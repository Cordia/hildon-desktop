/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Marc Ordinas i Llopis <marc.ordinasillopis@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hildon-desktop.h"
#include "hd-app-mgr.h"
#include "hd-app-mgr-glue.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/types.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <gconf/gconf-client.h>
#ifdef HAVE_DSME
/* TODO Convert MCE to DSME */
#include <mce/dbus-names.h>
#include <mce/mode-names.h>
#endif
#include "hd-launcher.h"
#include "hd-launcher-tree.h"
#include "home/hd-render-manager.h"
#include "hd-transition.h"
#include "hd-wm.h"

#undef  G_LOG_DOMAIN
#define G_LOG_DOMAIN "hd-app-mgr"

#define I_(str) (g_intern_static_string ((str)))

typedef enum
{
  QUEUE_PRESTARTABLE,
  QUEUE_PRESTARTED,
  QUEUE_HIBERNATABLE,
  QUEUE_HIBERNATED,

  NUM_QUEUES
} HdAppMgrQueue;

/* Prestarting depends on the env var HILDON_DESKTOP_APPS_PRESTART and the
 * amount of /proc/sys/vm/lowmem_free_pages up to
 * /proc/sys/vm/lowmem_notify_low_pages.
 * not set|false|no - Never prestart apps.
 * yes|auto|0 - Prestart if there are more free pages than stated in
 * /proc/sys/vm/lowmem_notify_low_pages.
 * number - Prestart if there are more than this number of free pages.
 */
typedef enum
{
  PRESTART_NEVER,
  PRESTART_AUTO,
  PRESTART_ALWAYS  /* Used in scratchbox where we don't have memory limits. */
} HdAppMgrPrestartMode;

/* Trying to launch an app can have different results. */
typedef enum
{
  LAUNCH_OK,
  LAUNCH_FAILED,
  LAUNCH_NO_MEM
} HdAppMgrLaunchResult;

struct _HdAppMgrPrivate
{
  HdLauncherTree *tree;

  DBusGProxy *dbus_proxy;

  /* All the running apps we know about. */
  GList *running_apps;

  /* Each one of these lists contain different HdRunningApps. */
  GQueue *queues[NUM_QUEUES];

  /* Is the state check already looping? */
  gboolean state_check_looping;

  /* Memory limits. */
  HdAppMgrPrestartMode prestart_mode;
  size_t prestart_required_pages;
  size_t launch_required_pages;
  size_t notify_low_pages;
  size_t notify_high_pages;
  size_t nr_decay_pages;

  /* Memory status and prestarting flags.*/
  gboolean bg_killing:1;
  gboolean lowmem:1;
  gboolean init_done:1;
  gboolean prestarting_stopped:1;
  gboolean prestarting;

  GConfClient *gconf_client;
  gboolean portrait;
  gboolean unlocked;
  gboolean slide_closed;
  gboolean accel_enabled;
};

#define HD_APP_MGR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                     HD_TYPE_APP_MGR, HdAppMgrPrivate))

/* Signals */
enum
{
  APP_LAUNCHED,
  APP_RELAUNCHED,
  APP_SHOWN,
  APP_LOADING_FAIL,
  APP_CRASHED,
  NOT_ENOUGH_MEMORY,  /* The boolean argument tells if it was waking up. */

  LAST_SIGNAL
};
static guint app_mgr_signals[LAST_SIGNAL] = {0, };

G_DEFINE_TYPE (HdAppMgr, hd_app_mgr, G_TYPE_OBJECT);

/* Memory usage */
#define LOWMEM_PROC_ALLOWED     "/proc/sys/vm/lowmem_allowed_pages"
#define LOWMEM_PROC_USED        "/proc/sys/vm/lowmem_used_pages"
#define LOWMEM_PROC_FREE        "/proc/sys/vm/lowmem_free_pages"
#define LOWMEM_PROC_NOTIFY_LOW  "/proc/sys/vm/lowmem_notify_low_pages"
#define LOWMEM_PROC_NOTIFY_HIGH "/proc/sys/vm/lowmem_notify_high_pages"
#define LOWMEM_PROC_NR_DECAY    "/proc/sys/vm/lowmem_nr_decay_pages"

#define LOADAVG_MAX               (1.0)
#define STATE_CHECK_INTERVAL      (1)
#define LOADING_TIMEOUT           (10)
#define INIT_DONE_TIMEOUT         (5)

#define PRESTART_ENV_VAR          "HILDON_DESKTOP_APPS_PRESTART"
#define NSIZE                     ((size_t)(-1))
#define PRESTART_ENV_AUTO         ((size_t)(-2))
#define PRESTART_ENV_NEVER        ((size_t)(-3))
/* This is used in scratchbox. */
#define PRESTART_ENV_ALWAYS       ((size_t)(-4))

/* DBus names */
#define OSSO_BUS_ROOT          "com.nokia"
#define OSSO_BUS_TOP           "top_application"
#define PATH_NAME_LEN           255
#define DBUS_NAMEOWNERCHANGED_SIGNAL_NAME "NameOwnerChanged"
#define HD_APP_MGR_DBUS_PATH   "/com/nokia/HildonDesktop/AppMgr"
#define HD_APP_MGR_DBUS_NAME   "com.nokia.HildonDesktop.AppMgr"

#define LOWMEM_ON_SIGNAL_INTERFACE  "com.nokia.ke_recv.lowmem_on"
#define LOWMEM_ON_SIGNAL_PATH       "/com/nokia/ke_recv/lowmem_on"
#define LOWMEM_ON_SIGNAL_NAME       "lowmem_on"

#define LOWMEM_OFF_SIGNAL_INTERFACE "com.nokia.ke_recv.lowmem_off"
#define LOWMEM_OFF_SIGNAL_PATH      "/com/nokia/ke_recv/lowmem_off"
#define LOWMEM_OFF_SIGNAL_NAME      "lowmem_off"

#define BGKILL_ON_SIGNAL_INTERFACE  "com.nokia.ke_recv.bgkill_on"
#define BGKILL_ON_SIGNAL_PATH       "/com/nokia/ke_recv/bgkill_on"
#define BGKILL_ON_SIGNAL_NAME       "bgkill_on"

#define BGKILL_OFF_SIGNAL_INTERFACE "com.nokia.ke_recv.bgkill_off"
#define BGKILL_OFF_SIGNAL_PATH      "/com/nokia/ke_recv/bgkill_off"
#define BGKILL_OFF_SIGNAL_NAME      "bgkill_off"

#define INIT_DONE_SIGNAL_INTERFACE "com.nokia.startup.signal"
#define INIT_DONE_SIGNAL_PATH      "/com/nokia/startup/signal"
#define INIT_DONE_SIGNAL_NAME      "init_done"

#define MAEMO_LAUNCHER_IFACE "org.maemo.launcher"
#define MAEMO_LAUNCHER_PATH  "/org/maemo/launcher"
#define MAEMO_LAUNCHER_APP_DIED_SIGNAL_NAME "ApplicationDied"

#define GCONF_SLIDE_OPEN_DIR     "/system/osso/af"
#define GCONF_SLIDE_OPEN_KEY     "/system/osso/af/slide-open"

/* Forward declarations */
static void hd_app_mgr_dispose (GObject *gobject);

static void hd_app_mgr_populate_tree_finished (HdLauncherTree *tree,
                                               gpointer data);

HdAppMgrLaunchResult hd_app_mgr_start     (HdRunningApp *app);
HdAppMgrLaunchResult hd_app_mgr_relaunch  (HdRunningApp *app);
HdAppMgrLaunchResult hd_app_mgr_wakeup    (HdRunningApp *app);
gboolean hd_app_mgr_prestart  (HdRunningApp *app);
gboolean hd_app_mgr_hibernate (HdRunningApp *app);

static gboolean hd_app_mgr_service_top (const gchar *service,
                                        const gchar *param);

void hd_app_mgr_prestartable     (HdRunningApp *app, gboolean prestartable);

static void hd_app_mgr_add_to_queue (HdAppMgrQueue queue,
                                     HdRunningApp *app);
static void hd_app_mgr_remove_from_queue (HdAppMgrQueue queue,
                                          HdRunningApp *app);
static void hd_app_mgr_move_queue (HdAppMgrQueue queue_from,
                                   HdAppMgrQueue queue_to,
                                   HdRunningApp *app);

static size_t   hd_app_mgr_read_lowmem (const gchar *filename);
static HdAppMgrPrestartMode
hd_app_mgr_setup_prestart (size_t low_pages,
                           size_t nr_decay_pages,
                           size_t *prestart_required_pages);
static void
hd_app_mgr_setup_launch (size_t high_pages,
                         size_t nr_decay_pages,
                         size_t *launch_required_pages);
static gboolean hd_app_mgr_can_launch   (HdLauncherApp *launcher);
static gboolean hd_app_mgr_can_prestart (HdLauncherApp *launcher);
static void     hd_app_mgr_hdrm_state_change (gpointer hdrm,
                                              GParamSpec *pspec,
                                              HdAppMgrPrivate *priv);
static void hd_app_mgr_state_check (void);
static gboolean hd_app_mgr_state_check_loop (gpointer data);

static void hd_app_mgr_dbus_name_owner_changed (DBusGProxy *proxy,
                                                const char *name,
                                                const char *old_owner,
                                                const char *new_owner,
                                                gpointer data);
static DBusHandlerResult hd_app_mgr_dbus_app_died (DBusConnection *conn,
                                                   DBusMessage *msg,
                                                   void *data);
static DBusHandlerResult hd_app_mgr_dbus_signal_handler (DBusConnection *conn,
                                                    DBusMessage *msg,
                                                    void *data);
static void hd_app_mgr_gconf_value_changed (GConfClient *client,
                                            guint cnxn_id,
                                            GConfEntry *entry,
                                            gpointer user_data);

static void     hd_app_mgr_request_app_pid (HdRunningApp *app);
static gboolean hd_app_mgr_loading_timeout (HdRunningApp *app);
static gboolean hd_app_mgr_init_done_timeout (HdAppMgr *self);

static void hd_app_mgr_kill_all_prestarted (void);

/* The HdLauncher singleton */
static HdAppMgr *the_app_mgr = NULL;

HdAppMgr *
hd_app_mgr_get (void)
{
  if (G_UNLIKELY (!the_app_mgr))
    { /* "Protect" against reentrancy. */
      static gboolean under_construction;

      g_assert(!under_construction);
      under_construction = TRUE;
      the_app_mgr = g_object_new (HD_TYPE_APP_MGR, NULL);
    }
  return the_app_mgr;
}

static void
hd_app_mgr_class_init (HdAppMgrClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdAppMgrPrivate));

  gobject_class->dispose     = hd_app_mgr_dispose;

  app_mgr_signals[APP_LAUNCHED] =
    g_signal_new (I_("application-launched"),
                  HD_TYPE_APP_MGR,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, HD_TYPE_LAUNCHER_APP);
  app_mgr_signals[APP_RELAUNCHED] =
    g_signal_new (I_("application-relaunched"),
                  HD_TYPE_APP_MGR,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, HD_TYPE_LAUNCHER_APP);
  app_mgr_signals[APP_SHOWN] =
    g_signal_new (I_("application-appeared"),
                  HD_TYPE_APP_MGR,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, HD_TYPE_LAUNCHER_APP);
  app_mgr_signals[APP_LOADING_FAIL] =
    g_signal_new (I_("application-loading-fail"),
                  HD_TYPE_APP_MGR,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, HD_TYPE_LAUNCHER_APP);
  app_mgr_signals[APP_CRASHED] =
    g_signal_new (I_("application-crashed"),
                  HD_TYPE_APP_MGR,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, HD_TYPE_LAUNCHER_APP);
  app_mgr_signals[NOT_ENOUGH_MEMORY] =
    g_signal_new (I_("not-enough-memory"),
                  HD_TYPE_APP_MGR,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  /* Bind D-Bus info. */
  dbus_g_object_type_install_info (HD_TYPE_APP_MGR,
                                   &dbus_glib_hd_app_mgr_object_info);
}

static gchar *
_hd_app_mgr_build_match (const gchar *interface,
                         const gchar *member)
{
  gchar *arg;
  if (member)
    arg = g_strdup_printf("type='signal', interface='%s', member='%s'",
                          interface, member);
  else
    arg = g_strdup_printf("type='signal', interface='%s'", interface);

  return arg;
}

/* TODO: Extend to use, in addition to an interface, a path and a signal
 * name.
 */
static void
hd_app_mgr_dbus_add_signal_match (DBusConnection *conn,
                                  const gchar *interface,
                                  const gchar *member)
{
  gchar *arg = _hd_app_mgr_build_match (interface, member);
  dbus_bus_add_match (conn, arg, NULL);
  g_free (arg);
}

#ifdef HAVE_DSME
static void
hd_app_mgr_dbus_remove_signal_match (DBusConnection *conn,
                                     const gchar *interface,
                                     const gchar *member)
{
  gchar *arg = _hd_app_mgr_build_match (interface, member);
  dbus_bus_remove_match (conn, arg, NULL);
  g_free (arg);
}
#endif

static void
hd_app_mgr_init (HdAppMgr *self)
{
  HdAppMgrPrivate *priv;

  self->priv = priv = HD_APP_MGR_GET_PRIVATE (self);

  /* Initialize the queues. */
  for (int i = 0; i < NUM_QUEUES; i++)
    priv->queues[i] = g_queue_new ();

  priv->tree = hd_launcher_tree_new ();
  hd_launcher_tree_ensure_user_menu ();
  g_signal_connect (priv->tree, "finished",
                    G_CALLBACK (hd_app_mgr_populate_tree_finished),
                    self);
  hd_launcher_tree_populate (priv->tree);

  /* NOTE: Can we assume this when we start up? */
  priv->unlocked = TRUE;
  priv->gconf_client = gconf_client_get_default ();
  if (priv->gconf_client)
    {
      priv->slide_closed = !gconf_client_get_bool (priv->gconf_client,
                                                   GCONF_SLIDE_OPEN_KEY,
                                                   NULL);
      gconf_client_add_dir (priv->gconf_client, GCONF_SLIDE_OPEN_DIR,
                            GCONF_CLIENT_PRELOAD_NONE, NULL);
      gconf_client_notify_add (priv->gconf_client, GCONF_SLIDE_OPEN_KEY,
                               hd_app_mgr_gconf_value_changed,
                               (gpointer) self,
                               NULL, NULL);
    }

  /* Start memory limits. */
  priv->notify_low_pages = hd_app_mgr_read_lowmem (LOWMEM_PROC_NOTIFY_LOW);
  priv->notify_high_pages = hd_app_mgr_read_lowmem (LOWMEM_PROC_NOTIFY_HIGH);
  priv->nr_decay_pages = hd_app_mgr_read_lowmem (LOWMEM_PROC_NR_DECAY);
  priv->prestart_mode = hd_app_mgr_setup_prestart (priv->notify_low_pages,
                                                   priv->nr_decay_pages,
                                                   &priv->prestart_required_pages);
  hd_app_mgr_setup_launch (priv->notify_high_pages,
                           priv->nr_decay_pages,
                           &priv->launch_required_pages);

  /* Start dbus signal tracking. */
  DBusGConnection *connection;
  connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
  if (connection)
    {
      /* Connect to NameOwnerChanged to track closing applications. */
      priv->dbus_proxy = dbus_g_proxy_new_for_name (connection,
                                                    DBUS_SERVICE_DBUS,
                                                    DBUS_PATH_DBUS,
                                                    DBUS_INTERFACE_DBUS);
      if (priv->dbus_proxy)
        {
          dbus_g_proxy_add_signal (priv->dbus_proxy,
              DBUS_NAMEOWNERCHANGED_SIGNAL_NAME,
              G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
          dbus_g_proxy_connect_signal (priv->dbus_proxy,
              DBUS_NAMEOWNERCHANGED_SIGNAL_NAME,
              (GCallback)hd_app_mgr_dbus_name_owner_changed,
              NULL, NULL);

          /* Serve the AppMgr interface. */
          guint result;
          if (!org_freedesktop_DBus_request_name (priv->dbus_proxy,
                                                  HD_APP_MGR_DBUS_NAME,
                                                  DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                                  &result,
                                                  NULL))
              {
                g_warning ("%s: Could not register name.\n", __FUNCTION__);
              }
          else
            {
              dbus_g_connection_register_g_object (connection,
                                                   HD_APP_MGR_DBUS_PATH,
                                                   G_OBJECT (self));
            }
        }
      else
        g_warning ("%s: Failed to proxy session dbus.", __FUNCTION__);

      /* Connect to the maemo launcher dbus interface. */
      hd_app_mgr_dbus_add_signal_match (
                                 dbus_g_connection_get_connection (connection),
                                 MAEMO_LAUNCHER_IFACE,
                                 MAEMO_LAUNCHER_APP_DIED_SIGNAL_NAME);
      dbus_connection_add_filter (dbus_g_connection_get_connection (connection),
                                  hd_app_mgr_dbus_app_died,
                                  self, NULL);
    }
  else
    g_warning ("%s: Failed to connect to session dbus.", __FUNCTION__);

  connection = NULL;

  /* Connect to the memory management signals. */
  DBusConnection *sys_conn = dbus_bus_get (DBUS_BUS_SYSTEM, NULL);
  if (sys_conn)
    {
      hd_app_mgr_dbus_add_signal_match (sys_conn,
                                   LOWMEM_ON_SIGNAL_INTERFACE,
                                   LOWMEM_ON_SIGNAL_NAME);
      hd_app_mgr_dbus_add_signal_match (sys_conn,
                                   LOWMEM_OFF_SIGNAL_INTERFACE,
                                   LOWMEM_OFF_SIGNAL_NAME);
      hd_app_mgr_dbus_add_signal_match (sys_conn,
                                   BGKILL_ON_SIGNAL_INTERFACE,
                                   BGKILL_ON_SIGNAL_NAME);
      hd_app_mgr_dbus_add_signal_match (sys_conn,
                                   BGKILL_OFF_SIGNAL_INTERFACE,
                                   BGKILL_OFF_SIGNAL_NAME);
      hd_app_mgr_dbus_add_signal_match (sys_conn,
                                   INIT_DONE_SIGNAL_INTERFACE,
                                   INIT_DONE_SIGNAL_NAME);
      dbus_connection_add_filter (sys_conn,
                                  hd_app_mgr_dbus_signal_handler,
                                  self, NULL);
    }
  else
    g_warning ("%s: Failed to connect to system dbus.\n", __FUNCTION__);

  /* Add a timeout in case init_done is never received. That can happen
   * when restarting, for example.
   */
  g_timeout_add_seconds (INIT_DONE_TIMEOUT,
                         (GSourceFunc)hd_app_mgr_init_done_timeout,
                         self);
}

void
hd_app_mgr_set_render_manager (GObject *rendermgr)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());

  /* Connect to state changes. */
  g_signal_connect (rendermgr, "notify::state",
                    G_CALLBACK (hd_app_mgr_hdrm_state_change),
                    priv);
}

static void
_hd_app_mgr_kill_prestarted (HdRunningApp *app, gpointer user_data)
{
  hd_app_mgr_kill (app);
}

static void
hd_app_mgr_kill_all_prestarted ()
{
  GQueue *prestarted;
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());

  prestarted = g_queue_copy (priv->queues[QUEUE_PRESTARTED]);
  g_queue_foreach (prestarted,
                   (GFunc)_hd_app_mgr_kill_prestarted,
                   NULL);
  g_queue_free (prestarted);
}

/* Called when exiting main() to close all prestarted apps. */
void
hd_app_mgr_stop ()
{
  if (!the_app_mgr)
    return;

  hd_app_mgr_kill_all_prestarted ();
}

static void
hd_app_mgr_dispose (GObject *gobject)
{
  HdAppMgr *self = HD_APP_MGR (gobject);
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (self);

  if (priv->dbus_proxy)
    {
      g_object_unref (priv->dbus_proxy);
      priv->dbus_proxy = NULL;
    }

  if (priv->tree)
    {
      g_object_unref (priv->tree);
      priv->tree = NULL;
    }

  if (priv->running_apps)
    {
      g_list_foreach (priv->running_apps, (GFunc)g_object_unref, NULL);
      g_list_free (priv->running_apps);
      priv->running_apps = NULL;
    }

  for (int i = 0; i < NUM_QUEUES; i++)
    {
      if (priv->queues[i])
        {
          g_queue_foreach (priv->queues[i], (GFunc)g_object_unref, NULL);
          g_queue_free (priv->queues[i]);
          priv->queues[i] = NULL;
        }
    }

  if (priv->gconf_client)
    {
      gconf_client_remove_dir (priv->gconf_client, GCONF_SLIDE_OPEN_DIR, NULL);
      g_object_unref (G_OBJECT (priv->gconf_client));
      priv->gconf_client = NULL;
    }

  G_OBJECT_CLASS (hd_app_mgr_parent_class)->dispose (gobject);
}

static gint
_hd_app_mgr_compare_app_priority (gconstpointer a,
                                  gconstpointer b,
                                  gpointer user_data)
{
  HdRunningApp *a_rapp = HD_RUNNING_APP (a);
  HdRunningApp *b_rapp = HD_RUNNING_APP (b);
  HdLauncherApp *a_launcher = hd_running_app_get_launcher_app (a_rapp);
  HdLauncherApp *b_launcher = hd_running_app_get_launcher_app (b_rapp);

  if (!a_launcher && !b_launcher)
    return 0;
  if (!a_launcher)
    return -1;
  if (!b_launcher)
    return 1;

  gint a_priority = hd_launcher_app_get_priority (a_launcher);
  gint b_priority = hd_launcher_app_get_priority (b_launcher);

  return b_priority - a_priority;
}

static void
hd_app_mgr_add_to_queue (HdAppMgrQueue queue, HdRunningApp *app)
{
  if (!app)
    return;

  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());

  /* Check if app's already there. */
  GList *link = g_queue_find (priv->queues[queue], app);
  if (link)
    return;

  g_queue_insert_sorted (priv->queues[queue],
                         g_object_ref (app),
                         _hd_app_mgr_compare_app_priority,
                         NULL);
}

static void
hd_app_mgr_remove_from_queue (HdAppMgrQueue queue, HdRunningApp *app)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  GList *link = g_queue_find (priv->queues[queue], app);

  if (link)
    {
      g_object_unref (app);
      g_queue_delete_link (priv->queues[queue], link);
    }
}

static void
hd_app_mgr_move_queue (HdAppMgrQueue queue_from,
                       HdAppMgrQueue queue_to,
                       HdRunningApp *app)
{
  if (!app)
    return;

  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  GList *link = g_queue_find (priv->queues[queue_from], app);

  if (link)
    {
      g_queue_delete_link (priv->queues[queue_from], link);
      g_queue_insert_sorted (priv->queues[queue_to],
                             app,
                             _hd_app_mgr_compare_app_priority,
                             NULL);

    }
  else
    hd_app_mgr_add_to_queue (queue_to, app);
}

void hd_app_mgr_prestartable (HdRunningApp *app, gboolean prestartable)
{
  if (prestartable)
    hd_app_mgr_add_to_queue (QUEUE_PRESTARTABLE, app);
  else
    hd_app_mgr_remove_from_queue (QUEUE_PRESTARTABLE, app);
}

void hd_app_mgr_hibernatable (HdRunningApp *app, gboolean hibernatable)
{
  /* We can only hibernate apps that have a dbus service.
   */
  HdLauncherApp *launcher = hd_running_app_get_launcher_app (app);
  if (!launcher || !hd_launcher_app_get_service (launcher))
    return;

  g_debug ("%s: Making app %s %s hibernatable", __FUNCTION__,
      hd_launcher_app_get_service (launcher),
      hibernatable ? "really" : "not");

  if (hibernatable)
    hd_app_mgr_add_to_queue (QUEUE_HIBERNATABLE, app);
  else
    hd_app_mgr_remove_from_queue (QUEUE_HIBERNATABLE, app);

  /* Go to state check, as marking an app hibernatable could mean we
   * have to bgkill it immediately.
   */
  hd_app_mgr_state_check ();
}

/* Application management */

static gint
_hd_app_mgr_compare_app_launcher (HdRunningApp *a,
                                  HdLauncherApp *b)
{
  HdLauncherApp *a_launcher = hd_running_app_get_launcher_app (a);

  if (a_launcher == b)
    return 0;

  return -1;
}

/* This function either:
 * - Relaunches an app if already running.
 * - Wakes up an app if it's hibernating.
 * - Starts the app if not running.
 */
gboolean
hd_app_mgr_launch (HdLauncherApp *launcher)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  gboolean result = FALSE;
  HdRunningApp *app = NULL;
  GList *link = NULL;

  /* Find if we already have a running app for this launcher. */
  link = g_list_find_custom (priv->running_apps, launcher,
                             (GCompareFunc)_hd_app_mgr_compare_app_launcher);
  if (link)
    /* We already have a running app for this one. */
    app = HD_RUNNING_APP (link->data);
  else
    {
      /* We need to create a new running app for it. */
      app = hd_running_app_new (launcher);
    }

  result = hd_app_mgr_activate (app);

  if (!link)
    {
      /* We just created this running app, so add to list or get rid of it. */
      if (result)
        priv->running_apps = g_list_prepend (priv->running_apps, app);
      else
        g_object_unref (app);
    }

  return result;
}

static gdouble
hd_app_mgr_timeout_backoff_factor ()
{
  return hd_transition_get_double("loading_timeout",
                                  "load_average_factor",
                                  0.0);
}

/*
 * Returns the current system load average.
 * Returns a negative value iff the load average
 * cannot be found.
 */
static gdouble
hd_app_mgr_system_load_average (void)
{
  int fd = open ("/proc/loadavg", O_RDONLY);

  if (fd >= 0)
    {
      char buffer[32];
      int size = read (fd, buffer, sizeof(buffer) -1);

      close (fd);
      if (size > 0)
        {
          gdouble load;
          buffer[size] = 0;
          load = g_ascii_strtod(buffer, NULL);

          return load;
        }
    }

  return -1.0;
}

/* This function either:
 * - Relaunches an app if already running.
 * - Wakes up an app if it's hibernating.
 * - Starts the app if not running.
 */
gboolean
hd_app_mgr_activate (HdRunningApp *app)
{
  HdAppMgrLaunchResult result = LAUNCH_FAILED;
  gboolean timer = FALSE;
  HdRunningAppState state;

  state = hd_running_app_get_state (app);
  switch (state)
  {
    case HD_APP_STATE_INACTIVE:
    case HD_APP_STATE_PRESTARTED:
      result = hd_app_mgr_start (app);
      timer = TRUE;
      break;
    case HD_APP_STATE_SHOWN:
      result = !STATE_IS_APP (hd_render_manager_get_state ()) || app != hd_comp_mgr_client_get_app (hd_comp_mgr_get_current_client (hd_comp_mgr_get ()))
        ? hd_app_mgr_relaunch (app) : LAUNCH_OK;
      break;
    case HD_APP_STATE_HIBERNATED:
      result = hd_app_mgr_wakeup (app);
      timer = TRUE;
      break;
    case HD_APP_STATE_LOADING:
    case HD_APP_STATE_WAKING:
      /* Send the launched signal so the loading screen will be shown again. */
      g_signal_emit (hd_app_mgr_get (), app_mgr_signals[APP_LAUNCHED],
                0, hd_running_app_get_launcher_app (app), NULL);
      result = LAUNCH_OK;
      break;
    default:
      result = LAUNCH_FAILED;
  }

  switch (result)
  {
    case LAUNCH_OK:
      if (timer)
          {
            /* Start a loading timer. */
            time_t now;
            gint timeout = (gint) (hd_app_mgr_timeout_backoff_factor () *
                                   hd_app_mgr_system_load_average ());

            if (timeout < LOADING_TIMEOUT)
              {
                timeout = LOADING_TIMEOUT;
	      }

            time (&now);
            hd_running_app_set_last_launch (app, now);
            g_timeout_add_seconds (timeout,
                                   (GSourceFunc)hd_app_mgr_loading_timeout,
                                   g_object_ref (app));
          }
      break;
    case LAUNCH_FAILED:
      g_signal_emit (hd_app_mgr_get (), app_mgr_signals[APP_LOADING_FAIL],
          0, hd_running_app_get_launcher_app (app), NULL);
      break;
    case LAUNCH_NO_MEM:
      g_signal_emit (hd_app_mgr_get (), app_mgr_signals[NOT_ENOUGH_MEMORY], 0,
          (state == HD_APP_STATE_HIBERNATED));
      break;
  }

  return (result == LAUNCH_OK)? TRUE : FALSE;
}

static void
_hd_app_mgr_child_exit (GPid pid, gint status, HdRunningApp *app)
{
  g_spawn_close_pid (pid);
  hd_app_mgr_app_closed (app);
  g_object_unref (app);
}

HdAppMgrLaunchResult
hd_app_mgr_start (HdRunningApp *app)
{
  gboolean result = FALSE;
  HdLauncherApp *launcher = hd_running_app_get_launcher_app (app);
  if (!launcher)
    return LAUNCH_FAILED;

  const gchar *service = hd_launcher_app_get_service (launcher);
  const gchar *exec;

  if (!hd_app_mgr_can_launch (launcher))
    return LAUNCH_NO_MEM;

  if (service)
    {
      result = hd_app_mgr_service_top (service, NULL);
      if (result)
        {
          /* As the app has been manually launched, stop considering it
           * for prestarting.
           */
          hd_app_mgr_prestartable (app, FALSE);
        }
    }
  else
    {
      exec = hd_launcher_app_get_exec (launcher);
      if (exec)
        {
          GPid pid = 0;
          result = hd_app_mgr_execute (exec, &pid, FALSE);
          if (result)
            {
              hd_running_app_set_pid (app, pid);
              /* Watch the child. */
              g_child_watch_add (pid,
                                 (GChildWatchFunc)_hd_app_mgr_child_exit,
                                 g_object_ref (app));
            }
        }
    }

  if (result)
    {
      hd_running_app_set_state (app, HD_APP_STATE_LOADING);
      g_signal_emit (hd_app_mgr_get (), app_mgr_signals[APP_LAUNCHED],
          0, launcher, NULL);
    }
  return result ? LAUNCH_OK : LAUNCH_FAILED;
}

HdAppMgrLaunchResult
hd_app_mgr_relaunch (HdRunningApp *app)
{
  /* we have to call hd_app_mgr_service_top when we relaunch an app,
   * but we can't do it straight away because the change in focus pulls
   * us out of our transitions. Instead, we get HdSwitcher to call
   * hd_app_mgr_relaunch_set_top after the task navigator has zoomed in
   * on the application. */

  HdLauncherApp *launcher = hd_running_app_get_launcher_app (app);
  if (launcher)
    g_signal_emit (hd_app_mgr_get (), app_mgr_signals[APP_RELAUNCHED],
                   0, launcher, NULL);

  return LAUNCH_OK;
}

static gboolean
hd_app_mgr_loading_timeout (HdRunningApp *app)
{
  time_t now;
  time (&now);
  HdRunningAppState state = hd_running_app_get_state (app);
  if ((state == HD_APP_STATE_LOADING || state == HD_APP_STATE_WAKING) &&
      difftime (now, hd_running_app_get_last_launch (app)) >= LOADING_TIMEOUT)
    {
      HdLauncherApp *launcher = hd_running_app_get_launcher_app (app);

      if (state == HD_APP_STATE_LOADING)
        /* If application hasn't appeared after this long, consider it
         * closed. */
        hd_app_mgr_app_closed (app);
      /* But not if hibernating, as we may need the pid later. */
      else
        hd_running_app_set_state (app, HD_APP_STATE_HIBERNATED);

      /* Tell the world, so something can be shown to the user.
       * Do this only if the app has a known service, because
       * it could be a command line. */
      if (hd_running_app_get_service(app) != NULL)
        {
          g_signal_emit (hd_app_mgr_get (), app_mgr_signals[APP_LOADING_FAIL],
              0, launcher, NULL);
        }
    }

  g_object_unref (app);
  return FALSE;
}

gboolean
hd_app_mgr_relaunch_set_top (HdLauncherApp *app)
{
  gboolean result = TRUE;
  const gchar *service = hd_launcher_app_get_service (app);

  if (service)
    {
      result = hd_app_mgr_service_top (service, NULL);
    }

  /* If it's a plain old app, nothing to do. */
  return result;
}

gboolean
hd_app_mgr_kill (HdRunningApp *app)
{
  GPid pid = hd_running_app_get_pid (app);

  if (!hd_running_app_is_executing (app))
    return FALSE;

  if (pid <= 0)
    {
      g_warning ("%s: No pid for app %s\n", __FUNCTION__,
          hd_launcher_item_get_id (HD_LAUNCHER_ITEM (app)));
      return FALSE;
    }

  if (kill (pid, SIGTERM) != 0)
    return FALSE;

  return TRUE;
}

void
hd_app_mgr_kill_all (void)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  /* We need to make a copy because the list is going to be changed. */
  GList *apps = g_list_copy(priv->running_apps);
  while (apps)
    {
      /* We only kill the shown apps. */
      if (hd_running_app_get_state (apps->data) == HD_APP_STATE_SHOWN)
        hd_app_mgr_kill (apps->data);
      apps = apps->next;
    }
  g_list_free(apps);
}

void hd_app_mgr_app_opened (HdRunningApp *app)
{
  HdLauncherApp *launcher = hd_running_app_get_launcher_app (app);
  hd_running_app_set_state (app, HD_APP_STATE_SHOWN);

  /* Signal that the app has appeared.
   */
  if (launcher)
    g_signal_emit (hd_app_mgr_get (), app_mgr_signals[APP_SHOWN],
                   0, launcher, NULL);

  /* Remove it from prestarting lists, just in case it has been launched
   * from somewhere else.
   */
  hd_app_mgr_remove_from_queue (QUEUE_HIBERNATED, app);
  hd_app_mgr_remove_from_queue (QUEUE_PRESTARTED, app);
  hd_app_mgr_remove_from_queue (QUEUE_PRESTARTABLE, app);

}

void
hd_app_mgr_app_closed (HdRunningApp *app)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  HdLauncherApp *launcher = hd_running_app_get_launcher_app (app);
  HdRunningAppState state = hd_running_app_get_state (app);

  if (state == HD_APP_STATE_HIBERNATED)
    {
      /* Do nothing with hibernating apps, as we need to keep them around. */
      return;
    }
  if (state == HD_APP_STATE_WAKING)
    {
      /* If the user switches really fast, h-d can try to wake up an app
       * that hasn't closed completely yet. As a dirty fix, try to
       * wake it up again. */
      hd_running_app_set_state (app, HD_APP_STATE_HIBERNATED);
      hd_app_mgr_activate (app);
      return;
    }

  /* Remove from anywhere we keep executing apps. */
  hd_app_mgr_remove_from_queue (QUEUE_PRESTARTED, app);
  hd_app_mgr_remove_from_queue (QUEUE_HIBERNATED, app);
  hd_app_mgr_remove_from_queue (QUEUE_HIBERNATABLE, app);

  hd_running_app_set_pid (app, 0);
  hd_running_app_set_state (app, HD_APP_STATE_INACTIVE);

  if (launcher &&
      hd_launcher_app_get_prestart_mode (launcher) == HD_APP_PRESTART_ALWAYS)
    {
      /* Add it to prestartable so it will be prestarted again. */
      hd_app_mgr_add_to_queue (QUEUE_PRESTARTABLE, app);
      hd_app_mgr_state_check ();
    }
  else
    {
      /* Take it out of the list of running apps. */
      GList *link = g_list_find (priv->running_apps, app);
      if (link)
        {
          g_object_unref (app);
          priv->running_apps = g_list_delete_link (priv->running_apps, link);
        }
    }
}

/* Called when an hibernating app is closed in the switcher. */
void
hd_app_mgr_app_stop_hibernation (HdRunningApp *app)
{
  hd_running_app_set_state (app, HD_APP_STATE_INACTIVE);
  hd_app_mgr_app_closed (app);
}

static void
hd_app_mgr_populate_tree_finished (HdLauncherTree *tree, gpointer data)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (HD_APP_MGR (data));
  /* We need to copy thess lists because we'll be modifying them. */
  GList *apps = g_list_copy (priv->running_apps);
  GList *items = g_list_copy (hd_launcher_tree_get_items (tree));
  GList *apps_to_free = apps;
  GList *items_to_free = items;

  /* First, traverse the already running apps to see if their HdLauncherApp
   * info has changed.
   */
  for (; apps; apps = apps->next)
    {
      HdRunningApp *app = apps->data;
      HdLauncherApp *old, *new;
      old = hd_running_app_get_launcher_app (app);
      if (!old)
        /* TODO? Try to recognize newly installed but already running apps? */
        continue;

      new = HD_LAUNCHER_APP (hd_launcher_tree_find_item (tree,
                                 hd_running_app_get_id (app)));
      hd_running_app_set_launcher_app (app, new);
      if (old && !new)
        {
          /* The .desktop file no longer exists, but the app could be running. */
          HdRunningAppState state = hd_running_app_get_state (app);
          if (state == HD_APP_STATE_PRESTARTED)
            /* Kill it, as it shouldn't be prestarted. */
            hd_app_mgr_kill (app);
          else if (state == HD_APP_STATE_INACTIVE)
            /* What's it doing here? */
            hd_app_mgr_app_closed (app);
        }
      if (old && new)
        {
          /* If the old was prestarted and the new one isn't, kill it. */
          if (hd_running_app_get_state (app) == HD_APP_STATE_PRESTARTED &&
              hd_launcher_app_get_prestart_mode (new) == HD_APP_PRESTART_NONE)
            hd_app_mgr_kill (app);
        }
    }

  g_list_free (apps_to_free);

  /* Now we need to look if we have new prestarted apps. */
  for (; items; items = items->next)
    {
      HdLauncherApp *launcher;
      GList *link = NULL;

      if (hd_launcher_item_get_item_type (HD_LAUNCHER_ITEM (items->data)) !=
                                          HD_APPLICATION_LAUNCHER)
        continue;

      launcher = HD_LAUNCHER_APP (items->data);
      if (priv->prestart_mode == PRESTART_NEVER ||
          hd_launcher_app_get_prestart_mode(launcher) != HD_APP_PRESTART_ALWAYS)
        continue;

      /* Look if we already have a running app for it. */
      link = g_list_find_custom (priv->running_apps, launcher,
                                 (GCompareFunc)_hd_app_mgr_compare_app_launcher);
      if (link)
        /* We dealt with it before. */
        continue;

      /* Create a new running app for it. */
      HdRunningApp *app = hd_running_app_new (launcher);
      priv->running_apps = g_list_prepend (priv->running_apps, app);
      hd_app_mgr_prestartable (app, TRUE);
    }

  g_list_free (items_to_free);
  hd_app_mgr_state_check ();
}

static void
_hd_app_mgr_prestart_cb (DBusGProxy *proxy, guint result,
                        GError *error, gpointer data)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  HdRunningApp *app = HD_RUNNING_APP (data);

  /* Prestarting can go ahead now. */
  priv->prestarting = FALSE;

  if (error || !result)
    {
      g_warning ("%s: Couldn't prestart service %s, error: %s\n",
          __FUNCTION__, hd_running_app_get_service (app),
          error ? error->message : "no result");
      /* TODO: Check number of times this has been tried and stop after
       * a while.
       */
    }
  else
    {
      g_debug ("%s: %s prestarted\n", __FUNCTION__,
          hd_running_app_get_service (app));
      hd_running_app_set_state (app, HD_APP_STATE_PRESTARTED);
      hd_app_mgr_add_to_queue (QUEUE_PRESTARTED, app);
      if (!hd_running_app_get_pid (app))
        hd_app_mgr_request_app_pid (app);
    }
}

gboolean
hd_app_mgr_prestart (HdRunningApp *app)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  const gchar *service = hd_running_app_get_service (app);

  if (hd_running_app_is_executing (app))
    return TRUE;

  if (!service)
    {
      g_warning ("%s: Can't prestart an app without service.\n", __FUNCTION__);
      return FALSE;
    }

  hd_app_mgr_remove_from_queue (QUEUE_PRESTARTABLE, app);

  g_debug ("%s: Starting to prestart %s\n", __FUNCTION__,
           service);
  priv->prestarting = TRUE;
  org_freedesktop_DBus_start_service_by_name_async (priv->dbus_proxy,
      service, 0,
      _hd_app_mgr_prestart_cb, (gpointer)app);

  /* We always return true because we don't know the result at this point. */
  return TRUE;
}

gboolean
hd_app_mgr_hibernate (HdRunningApp *app)
{
  const gchar *service = hd_running_app_get_service (app);

  if (!service)
    /* Can't hibernate a non-dbus app. */
    return FALSE;

  if (hd_app_mgr_kill (app))
    {
      hd_running_app_set_state (app, HD_APP_STATE_HIBERNATED);
      hd_app_mgr_move_queue (QUEUE_HIBERNATABLE, QUEUE_HIBERNATED, app);
      hd_running_app_set_pid (app, 0);
      hd_app_mgr_remove_from_queue (QUEUE_PRESTARTABLE, app);
    }
  else
    {
      /* We couldn't kill it, so just take it out of the hibernatable queue. */
      hd_app_mgr_hibernatable (app, FALSE);
      return FALSE;
    }

  return TRUE;
}

HdAppMgrLaunchResult
hd_app_mgr_wakeup   (HdRunningApp *app)
{
  g_return_val_if_fail (app, FALSE);

  gboolean res = FALSE;
  const gchar *service = hd_running_app_get_service (app);

  /* If the app is not hibernating, do nothing. */
  if (hd_running_app_get_state (app) != HD_APP_STATE_HIBERNATED)
    return LAUNCH_OK;

  if (!service)
    {
      g_warning ("%s: Can't wake up an app without service.\n", __FUNCTION__);
      return LAUNCH_FAILED;
    }

  if (!hd_app_mgr_can_launch (hd_running_app_get_launcher_app (app)))
    {
      return LAUNCH_NO_MEM;
    }

  res = hd_app_mgr_service_top (service, "RESTORE");
  if (res) {
    hd_running_app_set_state (app, HD_APP_STATE_WAKING);
  }

  return res ? LAUNCH_OK : LAUNCH_FAILED;
}

#define OOM_DISABLE "0"

static void
_hd_app_mgr_child_setup(gpointer user_data)
{
  int priority;
  int fd;
  int write_result;

  /* If the child process inherited desktop's high priority,
   * give child default priority */
  priority = getpriority (PRIO_PROCESS, 0);

  if (!errno && priority < 0)
  {
    setpriority (PRIO_PROCESS, 0, 0);
  }

  /* Unprotect from OOM */
  fd = open ("/proc/self/oom_adj", O_WRONLY);
  if (fd >= 0)
  {
    write_result = write (fd, OOM_DISABLE, sizeof (OOM_DISABLE));
    close (fd);
  }
}

gboolean
hd_app_mgr_execute (const gchar *exec, GPid *pid, gboolean auto_reap)
{
  gboolean res = FALSE;
  gchar *space = strchr (exec, ' ');
  gchar *exec_cmd;
  gint argc;
  gchar **argv = NULL;

  if (space)
  {
    gchar *cmd = g_strndup (exec, space - exec);
    gchar *exc = g_find_program_in_path (cmd);

    exec_cmd = g_strconcat (exc, space, NULL);

    g_free (cmd);
    g_free (exc);
  }
  else
    exec_cmd = g_find_program_in_path (exec);

  if (!exec_cmd ||
      !g_shell_parse_argv (exec_cmd, &argc, &argv, NULL))
  {
    g_free (exec_cmd);
    if (argv)
      g_strfreev (argv);

    return FALSE;
  }

  res = g_spawn_async (NULL,
                       argv, NULL,
                       auto_reap ? 0 : G_SPAWN_DO_NOT_REAP_CHILD,
                       _hd_app_mgr_child_setup, NULL,
                       pid,
                       NULL);
  g_free (exec_cmd);

  if (argv)
    g_strfreev (argv);

  return res;
}

static gboolean
hd_app_mgr_service_top (const gchar *service, const gchar *param)
{
  gchar path[PATH_NAME_LEN];
  DBusMessage *msg = NULL;
  DBusError derror;
  DBusConnection *conn;

  gchar *tmp = g_strdelimit(g_strdup (service), ".", '/');
  g_snprintf (path, PATH_NAME_LEN, "/%s", tmp);
  g_free (tmp);

  dbus_error_init (&derror);
  conn = dbus_bus_get (DBUS_BUS_SESSION, &derror);
  if (dbus_error_is_set (&derror))
  {
    g_warning ("could not start: %s: %s", service, derror.message);
    dbus_error_free (&derror);
    return FALSE;
  }

  msg = dbus_message_new_method_call (service, path, service, OSSO_BUS_TOP);
  if (msg == NULL)
  {
    g_warning ("failed to create message");
    return FALSE;
  }

  dbus_message_set_auto_start (msg, TRUE);
  dbus_message_set_no_reply (msg, TRUE);

  if (param)
    dbus_message_append_args (msg, DBUS_TYPE_STRING, &param,
                              DBUS_TYPE_INVALID);

  if (!dbus_connection_send (conn, msg, NULL))
    {
      dbus_message_unref (msg);
      g_warning ("dbus_connection_send failed");
      return FALSE;
    }

  g_debug ("%s: service: %s, param: %s", __FUNCTION__,
           service, param ? param : "null");
  dbus_message_unref (msg);
  return TRUE;
}

/* Memory management. */
static size_t
hd_app_mgr_read_lowmem (const gchar *filename)
{
  int fd = open (filename, O_RDONLY);

  if (fd >= 0)
    {
      char buffer[32];
      size_t size = read (fd, buffer, sizeof(buffer) -1);

      close (fd);
      if (size > 0)
        {
          buffer[size] = 0;
          return (size_t)strtol(buffer, NULL, 10);
        }
    }

  return NSIZE;
}

/*
 * Returns whether the load average is too high
 * to preload applications.
 */
static gboolean
hd_app_mgr_check_loadavg (void)
{
  gdouble load = hd_app_mgr_system_load_average ();

  return (load >= 0.0 &&
	  load <= LOADAVG_MAX);
}

static HdAppMgrPrestartMode
hd_app_mgr_setup_prestart (size_t low_pages,
                           size_t nr_decay_pages,
                           size_t *prestart_required_pages)
{
  gchar *prestart_env = NULL;
  HdAppMgrPrestartMode result = PRESTART_NEVER;
  *prestart_required_pages = NSIZE;

  prestart_env = getenv (PRESTART_ENV_VAR);

  if (!prestart_env ||
      !*prestart_env ||
      !g_strcmp0 (prestart_env, "no") ||
      !g_strcmp0 (prestart_env, "false"))
    {
      result = PRESTART_NEVER;
    }
  else if (low_pages == NSIZE || nr_decay_pages == NSIZE)
    {
      *prestart_required_pages = NSIZE;
      result = PRESTART_ALWAYS;
    }
  else
    {
      size_t reserved = (size_t)strtol (prestart_env, NULL, 10);
      if (reserved == 0)
        *prestart_required_pages =
                low_pages + hd_app_mgr_read_lowmem (LOWMEM_PROC_NR_DECAY);
      else
        *prestart_required_pages = low_pages + reserved;
      result = PRESTART_AUTO;
    }

  return result;
}

static void
hd_app_mgr_setup_launch (size_t high_pages,
                         size_t nr_decay_pages,
                         size_t *launch_required_pages)
{
  if (high_pages == NSIZE || nr_decay_pages == NSIZE)
    {
      g_debug ("%s: No memory limits, assuming scratchbox.\n", __FUNCTION__);
      *launch_required_pages = NSIZE;
      return;
    }

  *launch_required_pages = high_pages + nr_decay_pages;
}

static gboolean
hd_app_mgr_can_launch (HdLauncherApp *launcher)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());

  if (launcher && hd_launcher_app_get_ignore_lowmem (launcher))
    return TRUE;

  return !priv->lowmem;
}

static gboolean hd_app_mgr_can_prestart (HdLauncherApp *launcher)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  if (priv->prestart_mode == PRESTART_ALWAYS)
    return TRUE;
  else if (priv->prestart_mode == PRESTART_NEVER)
    return FALSE;

  if (hd_launcher_app_get_ignore_load (launcher))
    return TRUE;

  if (!hd_app_mgr_check_loadavg ())
    return FALSE;

  size_t free_pages = hd_app_mgr_read_lowmem (LOWMEM_PROC_FREE);
  if (free_pages == NSIZE)
    return TRUE;

  return free_pages >= priv->prestart_required_pages;
}

static void
hd_app_mgr_hdrm_state_change (gpointer hdrm,
                              GParamSpec *pspec,
                              HdAppMgrPrivate *priv)
{
  gboolean launcher = hd_render_manager_get_state () == HDRM_STATE_LAUNCHER;
  if (launcher != priv->prestarting_stopped)
    {
      priv->prestarting_stopped = launcher;
      hd_app_mgr_state_check ();
    }

  /* Also check if we should enable the accelerometer. */
  hd_app_mgr_mce_activate_accel_if_needed (TRUE);
}

static void
hd_app_mgr_state_check (void)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());

  /* If it's already looping, it'll get there, so do nothing. */
  if (priv->state_check_looping)
    return;

  /* If not, start looping. */
  priv->state_check_looping = TRUE;
  g_timeout_add_seconds (STATE_CHECK_INTERVAL,
                         hd_app_mgr_state_check_loop,
                         NULL);
}

/*
 * This function runs in a loop or whenever there's a change in memory
 * conditions. Depending on those conditions, it
 * - Kills prestarted apps.
 * - Hibernates apps.
 * - Prestarts apps.
 * It continues to loop if
 * - There are still apps to be prestarted.
 * - If memory is not low enough.
 */
static gboolean
hd_app_mgr_state_check_loop (gpointer data)
{
  gboolean loop = FALSE;
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());

  /* First check if we are really low on memory. */
  if (priv->lowmem)
    {
      /* If there are prestarted apps, kill one of them. */
      if (!g_queue_is_empty (priv->queues[QUEUE_PRESTARTED]))
        {
          HdRunningApp *app = g_queue_peek_tail (priv->queues[QUEUE_PRESTARTED]);
          hd_app_mgr_kill (app);
          if (!g_queue_is_empty (priv->queues[QUEUE_PRESTARTED]))
            loop = TRUE;
        }
    }

  /* If we're running low, hibernate an app. */
  else if (priv->bg_killing)
    {
      /* TODO: Hibernate an app and loop. */
      if (!g_queue_is_empty (priv->queues[QUEUE_HIBERNATABLE]))
        {
          HdRunningApp *app = g_queue_peek_tail (priv->queues[QUEUE_HIBERNATABLE]);
          hd_app_mgr_hibernate (app);
          if (!g_queue_is_empty (priv->queues[QUEUE_HIBERNATABLE]))
            loop = TRUE;
        }
    }
  /* If there's enough memory and hibernated apps, try to awake them.
   * TODO: Add some way to avoid waking-up apps from being shown immediately
   * to the user as if launched anew.
  else if (!g_queue_is_empty (priv->queues[QUEUE_HIBERNATED]) &&
           hd_app_mgr_can_launch (NULL))
    {
      HdLauncherApp *app = g_queue_peek_head (priv->queues[QUEUE_HIBERNATED]);
      hd_app_mgr_wakeup (app);
      if (!g_queue_is_empty (priv->queues[QUEUE_HIBERNATED]))
        loop = TRUE;
    }
   */
  /* If we have enough memory and there are apps waiting to be prestarted,
   * do that.
   */
  else if (priv->init_done &&
           priv->prestart_mode != PRESTART_NEVER &&
           !priv->prestarting_stopped &&
           !g_queue_is_empty (priv->queues[QUEUE_PRESTARTABLE])
      )
    {
      /* We make this tests here to loop even if we can't prestart right now.*/
      if (!priv->prestarting)
        {
          HdRunningApp *app = g_queue_peek_head (priv->queues[QUEUE_PRESTARTABLE]);
          HdLauncherApp *launcher = hd_running_app_get_launcher_app (app);
          if (launcher && hd_app_mgr_can_prestart (launcher))
            hd_app_mgr_prestart (app);
        }
      if (!g_queue_is_empty (priv->queues[QUEUE_PRESTARTABLE]))
        loop = TRUE;
    }

  /* Now the tricky part. This function is called by a timeout or by
   * changes in memory conditions. If we're already looping, return if we
   * need to loop. If not, and we need to loop, start the loop.
   */
  priv->state_check_looping = loop;

  return loop;
}

static void
hd_app_mgr_dbus_name_owner_changed (DBusGProxy *proxy,
                                    const char *name,
                                    const char *old_owner,
                                    const char *new_owner,
                                    gpointer data)
{
  GList *apps;
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());

  /* Check only connections and disconnections. */
  if (!old_owner[0] == !new_owner[0])
    return;

  /* Check if the service is one we want always on. */
  apps = priv->running_apps;
  while (apps)
    {
      HdRunningApp *app = HD_RUNNING_APP (apps->data);

      if (hd_running_app_is_inactive (app))
        goto next;

      if (!g_strcmp0 (name, hd_running_app_get_service (app)))
        {
          if (!new_owner[0])
            { /* Disconnection */
              g_debug ("%s: App %s has fallen\n", __FUNCTION__,
                                    hd_running_app_get_id (app));

              /* We have the correct app, deal accordingly. */
              hd_app_mgr_app_closed (app);
            }
          else
            { /* Connection */
              if (!hd_running_app_get_pid (app))
                    hd_app_mgr_request_app_pid (app);
            }
          break;
        }

      next:
      apps = g_list_next (apps);
    }
}

static gint
_hd_app_mgr_compare_launcher_exec (HdLauncherItem *item,
                                   gchar *filename)
{
  HdLauncherApp *launcher;

  if (hd_launcher_item_get_item_type (item) != HD_APPLICATION_LAUNCHER)
    return -1;

  launcher = HD_LAUNCHER_APP (item);
  return g_strcmp0 (hd_launcher_app_get_exec (launcher), filename);
}

static DBusHandlerResult hd_app_mgr_dbus_app_died (DBusConnection *conn,
                                                   DBusMessage *msg,
                                                   void *data)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  HdLauncherApp *launcher = NULL;
  GList *link;
  gchar *filename;
  GPid pid;
  gint status;
  DBusError err;

  if (!dbus_message_is_signal (msg,
                               MAEMO_LAUNCHER_IFACE,
                               MAEMO_LAUNCHER_APP_DIED_SIGNAL_NAME))
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  dbus_error_init(&err);

  dbus_message_get_args(msg, &err,
                        DBUS_TYPE_STRING, &filename,
                        DBUS_TYPE_INT32, &pid,
                        DBUS_TYPE_INT32, &status,
                        DBUS_TYPE_INVALID);

  if (dbus_error_is_set(&err))
  {
      g_warning ("%s: Error getting message args: %s\n",
                 __FUNCTION__, err.message);
      dbus_error_free (&err);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  /* Find which app died. */
  link = g_list_find_custom (hd_launcher_tree_get_items (priv->tree),
                             (gconstpointer)filename,
                             (GCompareFunc)_hd_app_mgr_compare_launcher_exec);

  /* NOTE: Should we report crashes of app we don't know about? */
  g_debug ("%s: app: %s, filename: %s", __FUNCTION__,
      link ? hd_launcher_item_get_id (HD_LAUNCHER_ITEM (link->data)) : "<unknown>",
      filename);

  if (link)
    {
      launcher = link->data;
      g_signal_emit (hd_app_mgr_get (), app_mgr_signals[APP_CRASHED],
                     0, launcher, NULL);
    }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

gboolean
hd_app_mgr_dbus_launch_app (HdAppMgr *self, const gchar *id)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (self);
  HdLauncherItem *item;
  HdLauncherApp *app;

  if (!id || !strlen(id))
    { /* If no ID was specified, all we want to do is trigger the
         app start animation with a blank window, not do anything fancy.
         We must check first that we haven't had a window mapped recently,
         because it could be that the dbus message got to us after the window
         appeared. */
      if (hd_render_manager_allow_dbus_launch_transition())
        hd_launcher_transition_app_start(NULL);
      return TRUE;
    }

  item = hd_launcher_tree_find_item (priv->tree, id);
  if (!item || hd_launcher_item_get_item_type (item) != HD_APPLICATION_LAUNCHER)
    app = hd_launcher_tree_find_app_by_service (priv->tree, id);
  else
    app = HD_LAUNCHER_APP (item);
  return app ? hd_app_mgr_launch (app) : FALSE;
}

gboolean
hd_app_mgr_dbus_prestart (HdAppMgr *self, const gboolean enable)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (self);

  if (enable)
    {
      priv->prestarting_stopped = FALSE;
      hd_app_mgr_state_check ();
    }
  else
    {
      priv->prestarting_stopped = TRUE;
      hd_app_mgr_kill_all_prestarted ();
    }

  return TRUE;
}

static gboolean
hd_app_mgr_init_done_timeout (HdAppMgr *self)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (self);

  if (!priv->init_done)
    {
      g_debug ("%s", __FUNCTION__);
      priv->init_done = TRUE;
      hd_app_mgr_state_check ();
    }

  return FALSE;
}

#ifdef HAVE_DSME
static gboolean
_hd_app_mgr_dbus_check_value (DBusMessage *msg,
                              const gchar *value)
{
  DBusError err;
  gchar *arg;

  dbus_error_init (&err);
  dbus_message_get_args (msg, &err,
                         DBUS_TYPE_STRING, &arg,
                         DBUS_TYPE_INVALID);

  if (dbus_error_is_set(&err))
  {
      g_warning ("%s: Error getting message args: %s\n",
                 __FUNCTION__, err.message);
      dbus_error_free (&err);
      return FALSE;
  }

  if (!g_strcmp0 (arg, value))
    return TRUE;

  return FALSE;
}
#endif

static void
hd_app_mgr_update_portraitness(HdAppMgr *self)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (self);
  HdCompMgr *hmgr = hd_comp_mgr_get ();
  // what about priv->unlocked/priv->display_on ?

  if (!hmgr)
    /* We're in some initialization state. */
    return;
  hd_comp_mgr_set_pip_flags (hmgr,
      priv->accel_enabled,
      priv->portrait && priv->slide_closed);
  hd_comp_mgr_portrait_or_not_portrait (MB_WM_COMP_MGR (hmgr), NULL);
}

static DBusHandlerResult
hd_app_mgr_dbus_signal_handler (DBusConnection *conn,
                           DBusMessage *msg,
                           void *data)
{
  gboolean changed = TRUE;
  HdAppMgr *self = HD_APP_MGR (data);
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (self);

  if (dbus_message_is_signal (msg,
                              LOWMEM_ON_SIGNAL_INTERFACE,
                              LOWMEM_ON_SIGNAL_NAME))
    priv->lowmem = TRUE;
  else if (dbus_message_is_signal (msg,
                                   LOWMEM_OFF_SIGNAL_INTERFACE,
                                   LOWMEM_OFF_SIGNAL_NAME))
    priv->lowmem = FALSE;
  else if (dbus_message_is_signal (msg,
                                   BGKILL_ON_SIGNAL_INTERFACE,
                                   BGKILL_ON_SIGNAL_NAME))
    priv->bg_killing = TRUE;
  else if (dbus_message_is_signal (msg,
                                   BGKILL_OFF_SIGNAL_INTERFACE,
                                   BGKILL_OFF_SIGNAL_NAME))
    priv->bg_killing = FALSE;
  else if (dbus_message_is_signal (msg,
                                   INIT_DONE_SIGNAL_INTERFACE,
                                   INIT_DONE_SIGNAL_NAME))
    priv->init_done = TRUE;
#ifdef HAVE_DSME
/* TODO Convert MCE to DSME */
  else
    {
      changed = FALSE;
      if (dbus_message_is_signal (msg,
                                  MCE_SIGNAL_IF,
                                  MCE_TKLOCK_MODE_SIG))
        {
          priv->unlocked = _hd_app_mgr_dbus_check_value (msg,
                                           MCE_DEVICE_UNLOCKED);
        }
      else if (dbus_message_is_signal (msg,
                                  MCE_SIGNAL_IF,
                                  MCE_DEVICE_ORIENTATION_SIG))
        {
          priv->portrait = _hd_app_mgr_dbus_check_value (msg,
                                           MCE_ORIENTATION_PORTRAIT);

          hd_app_mgr_update_portraitness(self);
        }
    }
#endif

  if (changed)
    hd_app_mgr_state_check ();

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* Activate the accelerometer when
 * - We are showing an app, and all visible windows support portrait mode
 */
void
hd_app_mgr_mce_activate_accel_if_needed (gboolean update_portraitness)
{
  extern gboolean hd_dbus_tklock_on;
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (the_app_mgr);
  DBusConnection *conn = NULL;
#ifdef HAVE_DSME
  DBusMessage *msg = NULL;
#endif
  gboolean activate = !hd_dbus_tklock_on;

  if (activate)
    activate = (STATE_IS_APP(hd_render_manager_get_state ())
          && hd_comp_mgr_can_be_portrait(hd_comp_mgr_get()));
  if (priv->accel_enabled == activate)
    return;

  conn = dbus_bus_get (DBUS_BUS_SYSTEM, NULL);
  if (!conn)
    {
      g_warning ("%s: Couldn't connect to session bus.", __FUNCTION__);
      return;
    }

#ifdef HAVE_DSME
/* TODO Convert MCE to DSME */
  /* We're only interested in these signals if we're going to rotate. */
  if (activate)
    {
      hd_app_mgr_dbus_add_signal_match (conn, MCE_SIGNAL_IF,
                                        MCE_TKLOCK_MODE_SIG);
      hd_app_mgr_dbus_add_signal_match (conn, MCE_SIGNAL_IF,
                                        MCE_DEVICE_ORIENTATION_SIG);
    }
  else
    {
      hd_app_mgr_dbus_remove_signal_match (conn, MCE_SIGNAL_IF,
                                           MCE_TKLOCK_MODE_SIG);
      hd_app_mgr_dbus_remove_signal_match (conn, MCE_SIGNAL_IF,
                                           MCE_DEVICE_ORIENTATION_SIG);
    }

  msg = dbus_message_new_method_call (
          MCE_SERVICE,
          MCE_REQUEST_PATH,
          MCE_REQUEST_IF,
          activate?
              MCE_ACCELEROMETER_ENABLE_REQ :
              MCE_ACCELEROMETER_DISABLE_REQ);
  if (!msg)
    {
      g_warning ("%s: Couldn't create message.", __FUNCTION__);
      return;
    }

  dbus_message_set_auto_start (msg, TRUE);
  if (activate)
    {
      DBusMessage *reply;

      /* @reply will contain the current orientation */
      if ((reply = dbus_connection_send_with_reply_and_block (
                                          conn, msg, -1, NULL)) != NULL)
        {
          priv->portrait = _hd_app_mgr_dbus_check_value (reply,
                                              MCE_ORIENTATION_PORTRAIT);
          dbus_message_unref (reply);
        }
      else
        g_warning ("%s: Couldn't send message.", __FUNCTION__);
      dbus_message_unref (msg);
    }
  else
    { /* Deactivate, expect no reply.  We can only deactivate in lscape. */
      dbus_message_set_no_reply (msg, TRUE);
      if (!dbus_connection_send (conn, msg, NULL))
        g_warning ("%s: Couldn't send message.", __FUNCTION__);
      dbus_message_unref (msg);
      priv->portrait = FALSE;
    }

  g_debug ("%s: %s", __FUNCTION__, activate ? "enabled" : "disabled");
  priv->accel_enabled = activate;
#endif

  if (update_portraitness)
    hd_app_mgr_update_portraitness(the_app_mgr);
  else
    hd_comp_mgr_set_pip_flags (hd_comp_mgr_get (), priv->accel_enabled,
                               priv->portrait && priv->slide_closed);
}

static void
hd_app_mgr_gconf_value_changed (GConfClient *client,
                                guint cnxn_id,
                                GConfEntry *entry,
                                gpointer user_data)
{
  HdAppMgr *self = HD_APP_MGR (user_data);
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (self);
  GConfValue *gvalue;
  gboolean value = FALSE;

  if (!entry)
    return;

  gvalue = gconf_entry_get_value (entry);
  if (gvalue->type == GCONF_VALUE_BOOL)
    value = gconf_value_get_bool (gvalue);

  if (!g_strcmp0 (gconf_entry_get_key (entry),
                  GCONF_SLIDE_OPEN_KEY))
    {
      priv->slide_closed = !value;

      hd_app_mgr_update_portraitness(self);
    }

  return;
}

static void
_hd_app_mgr_request_app_pid_cb (DBusGProxy *proxy, guint pid,
    GError *error, gpointer data)
{
  HdRunningApp *app = HD_RUNNING_APP (data);

  if (error)
    {
      g_warning ("%s: Couldn't get pid for service %s because %s\n",
                 __FUNCTION__, hd_running_app_get_service (app),
               error->message);
      return;
    }

  g_debug ("%s: Got pid %d for %s\n", __FUNCTION__,
           pid, hd_running_app_get_service (app));
  hd_running_app_set_pid (app, pid);
}

static void
hd_app_mgr_request_app_pid (HdRunningApp *app)
{
  DBusGProxy *proxy = (HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get()))->dbus_proxy;
  const gchar *service = hd_running_app_get_service (app);

  if (!service)
    {
      g_warning ("%s: Can't get the pid for a non-dbus app.\n", __FUNCTION__);
      hd_running_app_set_pid (app, 0);
    }

  org_freedesktop_DBus_get_connection_unix_process_id_async (proxy,
      service,
      _hd_app_mgr_request_app_pid_cb, (gpointer)app);
}

HdRunningApp *
hd_app_mgr_match_window (const char *res_name,
                         const char *res_class,
                         GPid pid)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  HdRunningApp *app = NULL;
  HdLauncherApp *launcher = NULL;
  GList *link = NULL;

  /* First we need to look if there's already a running app for this. */
  link = priv->running_apps;
  while (link)
    {
      app = HD_RUNNING_APP (link->data);
      launcher = hd_running_app_get_launcher_app (app);

      /* If we know the running app's pid and it's the same, we found it. */
      GPid app_pid = hd_running_app_get_pid (app);
      if (app_pid && (app_pid == pid))
        return app;

      /* Now we look if the app's launcher matches the window. */
      if (launcher)
        {
          if (hd_launcher_app_match_window (launcher, res_name, res_class))
            {
              /* Now we have a good pid. */
              if (!app_pid)
                hd_running_app_set_pid (app, pid);
              return app;
            }
        }

      /* Next. */
      link = link->next;
    }

  /* Well, there wasn't any already running app, so we'll have to look for
   * a launcher that matches.
   */
  GList *launchers = hd_launcher_tree_get_items (priv->tree);
  app = NULL;

  if (res_name || res_class)
    {
      while (launchers)
        {
          /* Filter non-applications. */
          if (hd_launcher_item_get_item_type (HD_LAUNCHER_ITEM (launchers->data)) !=
              HD_APPLICATION_LAUNCHER)
            goto next;

          launcher = HD_LAUNCHER_APP (launchers->data);
          if (hd_launcher_app_match_window (launcher, res_name, res_class))
            {
              /* Let's make a new running app for it. */
              app = hd_running_app_new (launcher);
              hd_running_app_set_pid (app, pid);
              priv->running_apps = g_list_prepend (priv->running_apps, app);
              return app;
            }

          next:
          launchers = g_list_next (launchers);
        }
    }

  /*
   * We didn't find any perfectly matching app, so try to see if we have
   * any loading one.
   * TODO: Review the wiseness of this move.
   */
  link = priv->running_apps;
  while (link)
    {
      app = HD_RUNNING_APP (link->data);
      if (hd_running_app_get_state (app) == HD_APP_STATE_LOADING)
        {
          if (!hd_running_app_get_pid (app))
            hd_running_app_set_pid (app, pid);
          return app;
        }

      link = link->next;
    }

  /* What? We haven't found any yet?
   * Well, let's just make one for this one and keep the pid in case we
   * have to kill it
   */
  app = hd_running_app_new (NULL);
  hd_running_app_set_pid (app, pid);
  priv->running_apps = g_list_prepend (priv->running_apps, app);

  return app;
}

HdLauncherTree *
hd_app_mgr_get_tree ()
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  return priv->tree;
}

#ifndef G_DEBUG_DISABLE
void
hd_app_mgr_dump_app_list (gboolean only_running)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  GList *apps = priv->running_apps;

  g_debug ("%s:\n", __FUNCTION__);
  for (; apps; apps = apps->next)
    {
      HdRunningApp *app = HD_RUNNING_APP (apps->data);

      if (!only_running || hd_running_app_get_state (app) == HD_APP_STATE_SHOWN)
        {
          g_debug("\tapp=%p, id=%s, pid=%d, state=%d\n",
                  app,
                  hd_running_app_get_id (app),
                  hd_running_app_get_pid (app),
                  hd_running_app_get_state (app));
        }
    }
}

void
hd_app_mgr_dump_tree ()
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  GList *items = hd_launcher_tree_get_items (priv->tree);

  g_debug ("%s:\n", __FUNCTION__);
  for (; items; items = items->next)
    {
      HdLauncherItem *item = HD_LAUNCHER_ITEM (items->data);

      g_debug("\titem=%p, id=%s, category=%s\n",
              item,
              hd_launcher_item_get_id (item),
              hd_launcher_item_get_category (item));
    }
}

#endif /* G_DEBUG_DISABLE */
