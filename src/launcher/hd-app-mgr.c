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
#include "hd-launcher.h"
#include "hd-launcher-tree.h"
#include "home/hd-render-manager.h"

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

struct _HdAppMgrPrivate
{
  /* TODO: Remove this and use libgnome-menu. */
  HdLauncherTree *tree;

  DBusGProxy *dbus_proxy;
  DBusGProxy *dbus_sys_proxy;

  /* Each one of these lists contain different HdLauncherApps. */
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
  gboolean launcher_shown:1;
};

#define HD_APP_MGR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                     HD_TYPE_APP_MGR, HdAppMgrPrivate))

/* Signals */
enum
{
  APP_LAUNCHED,
  APP_RELAUNCHED,
  APP_SHOWN,
  NOT_ENOUGH_MEMORY,

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

#define STATE_CHECK_INTERVAL      (3)

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

/* Forward declarations */
static void hd_app_mgr_dispose (GObject *gobject);

static void hd_app_mgr_populate_tree_finished (HdLauncherTree *tree,
                                               gpointer data);

gboolean hd_app_mgr_start     (HdLauncherApp *app);
gboolean hd_app_mgr_prestart  (HdLauncherApp *app);
gboolean hd_app_mgr_hibernate (HdLauncherApp *app);
static gboolean hd_app_mgr_service_top (const gchar *service,
                                        const gchar *param);
static gboolean  hd_app_mgr_execute (const gchar *exec, GPid *pid);

static void hd_app_mgr_add_to_queue (HdAppMgrQueue queue,
                                     HdLauncherApp *app);
static void hd_app_mgr_remove_from_queue (HdAppMgrQueue queue,
                                          HdLauncherApp *app);
static void hd_app_mgr_move_queue (HdAppMgrQueue queue_from,
                                   HdAppMgrQueue queue_to,
                                   HdLauncherApp *app);

static size_t   hd_app_mgr_read_lowmem (const gchar *filename);
static HdAppMgrPrestartMode
hd_app_mgr_setup_prestart (size_t low_pages,
                           size_t nr_decay_pages,
                           size_t *prestart_required_pages);
static void
hd_app_mgr_setup_launch (size_t high_pages,
                         size_t nr_decay_pages,
                         size_t *launch_required_pages);
static gboolean hd_app_mgr_can_launch   (void);
static gboolean hd_app_mgr_can_prestart (void);
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
static DBusHandlerResult hd_app_mgr_signal_handler (DBusConnection *conn,
                                                    DBusMessage *msg,
                                                    void *data);

static void hd_app_mgr_request_app_pid (HdLauncherApp *app);

/* The HdLauncher singleton */
static HdAppMgr *the_app_mgr = NULL;

HdAppMgr *
hd_app_mgr_get (void)
{
  if (G_UNLIKELY (!the_app_mgr))
    the_app_mgr = g_object_new (HD_TYPE_APP_MGR, NULL);
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
  app_mgr_signals[NOT_ENOUGH_MEMORY] =
    g_signal_new (I_("not-enough-memory"),
                  HD_TYPE_APP_MGR,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0, NULL);

  /* Bind D-Bus info. */
  dbus_g_object_type_install_info (HD_TYPE_APP_MGR,
                                   &dbus_glib_hd_app_mgr_object_info);
}

/* TODO: Extend to use, in addition to an interface, a path and a signal
 * name.
 */
static gboolean
hd_app_mgr_add_signal_match (DBusGProxy *proxy, const gchar *interface)
{
  gboolean result;
  gchar *arg;
  arg = g_strdup_printf("type='signal', interface='%s'", interface);
  result = org_freedesktop_DBus_add_match (proxy, arg, NULL);
  g_free (arg);
  return result;
}

static void
hd_app_mgr_init (HdAppMgr *self)
{
  HdAppMgrPrivate *priv;

  self->priv = priv = HD_APP_MGR_GET_PRIVATE (self);

  /* Initialize the queues. */
  for (int i = 0; i < NUM_QUEUES; i++)
    priv->queues[i] = g_queue_new ();

  /* Connect to state changes. */
  g_signal_connect (hd_render_manager_get (), "notify::state",
                    G_CALLBACK (hd_app_mgr_hdrm_state_change),
                    priv);

  /* TODO: Move handling of HdLauncherTree here. */
  priv->tree = g_object_ref (hd_launcher_get_tree ());
  g_signal_connect (priv->tree, "finished",
                    G_CALLBACK (hd_app_mgr_populate_tree_finished),
                    self);

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
        g_warning ("%s: Failed to connect to session dbus.\n", __FUNCTION__);
    }
  else
    g_warning ("%s: Failed to proxy session dbus.\n", __FUNCTION__);

  connection = NULL;

  /* Connect to the memory management signals. */
  /* Note: It'd be a lot better to use DBusGProxies here, but the design of
   * the signals makes that very difficult.
   */
  connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
  if (connection)
    {
      priv->dbus_sys_proxy = dbus_g_proxy_new_for_name (connection,
                                                        DBUS_SERVICE_DBUS,
                                                        DBUS_PATH_DBUS,
                                                        DBUS_INTERFACE_DBUS);
      if (priv->dbus_sys_proxy)
        {
          hd_app_mgr_add_signal_match (priv->dbus_sys_proxy,
                                       LOWMEM_ON_SIGNAL_INTERFACE);
          hd_app_mgr_add_signal_match (priv->dbus_sys_proxy,
                                       LOWMEM_OFF_SIGNAL_INTERFACE);
          hd_app_mgr_add_signal_match (priv->dbus_sys_proxy,
                                       BGKILL_ON_SIGNAL_INTERFACE);
          hd_app_mgr_add_signal_match (priv->dbus_sys_proxy,
                                       BGKILL_OFF_SIGNAL_INTERFACE);
          hd_app_mgr_add_signal_match (priv->dbus_sys_proxy,
                                       INIT_DONE_SIGNAL_INTERFACE);
          dbus_connection_add_filter (dbus_g_connection_get_connection (connection),
                                      hd_app_mgr_signal_handler,
                                      self, NULL);
        }
      else
        g_warning ("%s: Failed to connect to system dbus.\n", __FUNCTION__);
    }
  else
    g_warning ("%s: Failed to proxy system dbus.\n", __FUNCTION__);
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

  if (priv->dbus_sys_proxy)
    {
      g_object_unref (priv->dbus_sys_proxy);
      priv->dbus_sys_proxy = NULL;
    }

  if (priv->tree)
    {
      g_object_unref (priv->tree);
      priv->tree = NULL;
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

  G_OBJECT_CLASS (hd_app_mgr_parent_class)->dispose (gobject);
}

static gint
_hd_app_mgr_compare_app_priority (gconstpointer a,
                                  gconstpointer b,
                                  gpointer user_data)
{
  gint a_priority = hd_launcher_app_get_priority (HD_LAUNCHER_APP (a));
  gint b_priority = hd_launcher_app_get_priority (HD_LAUNCHER_APP (b));

  return b_priority - a_priority;
}

static void
hd_app_mgr_add_to_queue (HdAppMgrQueue queue, HdLauncherApp *app)
{
  if (!app)
    return;

  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());

  g_queue_insert_sorted (priv->queues[queue],
                         g_object_ref (app),
                         _hd_app_mgr_compare_app_priority,
                         NULL);
}

static void
hd_app_mgr_remove_from_queue (HdAppMgrQueue queue, HdLauncherApp *app)
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
                       HdLauncherApp *app)
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

void hd_app_mgr_prestartable (HdLauncherApp *app, gboolean prestartable)
{
  if (prestartable)
    hd_app_mgr_add_to_queue (QUEUE_PRESTARTABLE, app);
  else
    hd_app_mgr_remove_from_queue (QUEUE_PRESTARTABLE, app);
}

void hd_app_mgr_hibernatable (HdLauncherApp *app, gboolean hibernatable)
{
  /* We can only hibernate apps that have a dbus service.
   */
  if (!hd_launcher_app_get_service (app))
    return;

  g_debug ("%s: Making app %s %s hibernatable", __FUNCTION__,
      hd_launcher_app_get_service (app),
      hibernatable ? "really" : "not");

  if (hibernatable)
    hd_app_mgr_add_to_queue (QUEUE_HIBERNATABLE, app);
  else
    hd_app_mgr_remove_from_queue (QUEUE_HIBERNATABLE, app);
}

/* Application management */

/* This function either:
 * - Relaunches an app if already running.
 * - Wakes up an app if it's hibernating.
 * - Starts the app if not running.
 */
gboolean
hd_app_mgr_launch (HdLauncherApp *app)
{
  gboolean result = FALSE;
  HdLauncherAppState state = hd_launcher_app_get_state (app);

  switch (state)
  {
    case HD_APP_STATE_INACTIVE:
    case HD_APP_STATE_LOADING:
    case HD_APP_STATE_PRESTARTED:
      result = hd_app_mgr_start (app);
      break;
    case HD_APP_STATE_SHOWN:
      result = hd_app_mgr_relaunch (app);
      break;
    case HD_APP_STATE_HIBERNATING:
      result = hd_app_mgr_wakeup (app);
      break;
    default:
      result = FALSE;
  }

  return result;
}

gboolean
hd_app_mgr_start (HdLauncherApp *app)
{
  gboolean result = FALSE;
  const gchar *service = hd_launcher_app_get_service (app);
  const gchar *exec;

  if (!hd_app_mgr_can_launch ())
    {
      g_debug ("%s: Not enough memory to start application %s.",
               __FUNCTION__, hd_launcher_item_get_id (HD_LAUNCHER_ITEM (app)));
      g_signal_emit (hd_app_mgr_get (), app_mgr_signals[NOT_ENOUGH_MEMORY], 0);
      return FALSE;
    }

  if (service)
    {
      result = hd_app_mgr_service_top (service, NULL);
      if (result)
        {
          if (!hd_launcher_app_get_pid (app))
            /* It's likely to fail because the service hasn't been
             * registered, but anyways. */
            hd_app_mgr_request_app_pid (app);

          /* As the app has been manually launched, stop considering it
           * for prestarting.
           */
          hd_app_mgr_prestartable (app, FALSE);
        }
    }
  else
    {
      exec = hd_launcher_app_get_exec (app);
      if (exec)
        {
          GPid pid = 0;
          result = hd_app_mgr_execute (exec, &pid);
          if (result)
            hd_launcher_app_set_pid (app, pid);
        }
    }

  if (result)
    {
      hd_launcher_app_set_state (app, HD_APP_STATE_LOADING);
      g_signal_emit (hd_app_mgr_get (), app_mgr_signals[APP_LAUNCHED],
          0, app, NULL);
    }
  return result;
}

gboolean
hd_app_mgr_relaunch (HdLauncherApp *app)
{
  /* we have to call hd_app_mgr_service_top when we relaunch an app,
   * but we can't do it straight away because the change in focus pulls
   * us out of our transitions. Instead, we get HdSwitcher to call
   * hd_app_mgr_relaunch_set_top after the task navigator has zoomed in
   * on the application. */

  g_signal_emit (hd_app_mgr_get (), app_mgr_signals[APP_RELAUNCHED],
      0, app, NULL);

  return TRUE;
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
hd_app_mgr_kill (HdLauncherApp *app)
{
  GPid pid = hd_launcher_app_get_pid (app);

  if (!hd_launcher_app_is_executing (app))
    return FALSE;

  if (!pid)
    {
      g_warning ("%s: No pid for app %s\n", __FUNCTION__,
          hd_launcher_item_get_id (HD_LAUNCHER_ITEM (app)));
      return FALSE;
    }

  if (kill (pid, SIGTERM) != 0)
    return FALSE;

  hd_app_mgr_closed (app);
  return TRUE;
}

void
hd_app_mgr_closed (HdLauncherApp *app)
{
  /* Remove from anywhere we keep executing apps. */
  hd_app_mgr_remove_from_queue (QUEUE_PRESTARTED, app);
  hd_app_mgr_remove_from_queue (QUEUE_HIBERNATED, app);
  hd_app_mgr_remove_from_queue (QUEUE_HIBERNATABLE, app);

  hd_launcher_app_set_pid (app, 0);
  hd_launcher_app_set_state (app, HD_APP_STATE_INACTIVE);
}

static void
_hd_app_mgr_collect_prestarted (HdLauncherItem *item, HdAppMgrPrivate *priv)
{
  if (hd_launcher_item_get_item_type (item) == HD_APPLICATION_LAUNCHER)
    {
      HdLauncherApp *app = HD_LAUNCHER_APP (item);
      if (hd_launcher_app_get_prestart_mode (app) == HD_APP_PRESTART_ALWAYS)
        hd_app_mgr_prestartable (app, TRUE);
    }
}

static void
hd_app_mgr_populate_tree_finished (HdLauncherTree *tree, gpointer data)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (HD_APP_MGR (data));
  GList *items = hd_launcher_tree_get_items (tree, NULL);

  g_list_foreach (items, (GFunc)_hd_app_mgr_collect_prestarted, priv);

  hd_app_mgr_state_check ();
}

static void
_hd_app_mgr_prestart_cb (DBusGProxy *proxy, guint result,
                        GError *error, gpointer data)
{
  HdLauncherApp *app = HD_LAUNCHER_APP (data);

  if (error || !result)
    {
      g_warning ("%s: Couldn't prestart service %s, error: %s\n",
          __FUNCTION__, hd_launcher_app_get_service (app),
          error ? error->message : "no result");
      /* TODO: Check number of times this has been tried and stop after
       * a while.
       */
    }
  else
    {
      g_debug ("%s: %s prestarted\n", __FUNCTION__,
          hd_launcher_app_get_service (app));
      hd_launcher_app_set_state (app, HD_APP_STATE_PRESTARTED);
      hd_app_mgr_add_to_queue (QUEUE_PRESTARTED, app);
      if (!hd_launcher_app_get_pid (app))
        hd_app_mgr_request_app_pid (app);
    }
}

gboolean
hd_app_mgr_prestart (HdLauncherApp *app)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  const gchar *service = hd_launcher_app_get_service (app);

  if (hd_launcher_app_is_executing (app))
    return TRUE;

  if (!service)
    {
      g_warning ("%s: Can't prestart an app without service.\n", __FUNCTION__);
      return FALSE;
    }

  hd_app_mgr_remove_from_queue (QUEUE_PRESTARTABLE, app);

  g_debug ("%s: Starting to prestart %s\n", __FUNCTION__,
           service);
  org_freedesktop_DBus_start_service_by_name_async (priv->dbus_proxy,
      service, 0,
      _hd_app_mgr_prestart_cb, (gpointer)app);

  /* We always return true because we don't know the result at this point. */
  return TRUE;
}

gboolean
hd_app_mgr_hibernate (HdLauncherApp *app)
{
  const gchar *service = hd_launcher_app_get_service (app);

  if (!service)
    /* Can't hibernate a non-dbus app. */
    return FALSE;

  g_debug ("%s: service %s\n", __FUNCTION__, service);
  if (hd_app_mgr_kill (app))
    {
      hd_launcher_app_set_state (app, HD_APP_STATE_HIBERNATING);
      hd_app_mgr_move_queue (QUEUE_HIBERNATABLE, QUEUE_HIBERNATED, app);
    }
  else
    {
      /* We couldn't kill it, so just take it out of the hibernatable queue. */
      hd_app_mgr_hibernatable (app, FALSE);
      return FALSE;
    }

  return TRUE;
}

gboolean
hd_app_mgr_wakeup   (HdLauncherApp *app)
{
  gboolean res = FALSE;
  const gchar *service = hd_launcher_app_get_service (app);

  /* If the app is not hibernating, do nothing. */
  if (hd_launcher_app_get_state (app) != HD_APP_STATE_HIBERNATING)
    return TRUE;

  if (!service)
    {
      g_warning ("%s: Can't wake up an app without service.\n", __FUNCTION__);
      return FALSE;
    }

  if (!hd_app_mgr_can_launch ())
    {
      g_debug ("%s: Not enough memory to start application %s.",
               __FUNCTION__, service);
      g_signal_emit (hd_app_mgr_get (), app_mgr_signals[NOT_ENOUGH_MEMORY], 0);
      return FALSE;
    }

  res = hd_app_mgr_service_top (service, "RESTORE");
  if (res) {
    hd_app_mgr_remove_from_queue (QUEUE_HIBERNATED, app);
    hd_app_mgr_request_app_pid (app);
    hd_launcher_app_set_state (app, HD_APP_STATE_LOADING);
  }

  return res;
}

#define OOM_DISABLE "0"

static void
_hd_app_mgr_child_setup(gpointer user_data)
{
  int priority;
  int fd;

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
    write (fd, OOM_DISABLE, sizeof (OOM_DISABLE));
    close (fd);
  }
}

static gboolean
hd_app_mgr_execute (const gchar *exec, GPid *pid)
{
  gboolean res = FALSE;
  gchar *space = strchr (exec, ' ');
  gchar *exec_cmd;
  gint argc;
  gchar **argv = NULL;
  GError *internal_error = NULL;

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

  if (!g_shell_parse_argv (exec_cmd, &argc, &argv, &internal_error))
  {
    g_free (exec_cmd);
    if (argv)
      g_strfreev (argv);

    return FALSE;
  }

  res = g_spawn_async (NULL,
                       argv, NULL,
                       0,
                       _hd_app_mgr_child_setup, NULL,
                       pid,
                       &internal_error);
  if (internal_error)

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
hd_app_mgr_can_launch (void)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  return !priv->bg_killing;
}

static gboolean hd_app_mgr_can_prestart (void)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  if (priv->prestart_mode == PRESTART_ALWAYS)
    return TRUE;
  else if (priv->prestart_mode == PRESTART_NEVER)
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
  if (launcher != priv->launcher_shown)
    {
      priv->launcher_shown = launcher;
      hd_app_mgr_state_check ();
    }
}

static void
hd_app_mgr_state_check (void)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());

  /* If it's already looping, it'll get there, so do nothing. */
  if (priv->state_check_looping)
    return;

  /* If not, call into it to see if we need looping. */
  hd_app_mgr_state_check_loop (NULL);
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
          HdLauncherApp *app = g_queue_peek_tail (priv->queues[QUEUE_PRESTARTED]);
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
          HdLauncherApp *app = g_queue_peek_tail (priv->queues[QUEUE_HIBERNATABLE]);
          hd_app_mgr_hibernate (app);
          if (!g_queue_is_empty (priv->queues[QUEUE_HIBERNATABLE]))
            loop = TRUE;
        }
    }
  /* If there's enough memory and hibernated apps, try to awake them.
   * TODO: Add some way to avoid waking-up apps from being shown immediately
   * to the user as if launched anew.
  else if (!g_queue_is_empty (priv->queues[QUEUE_HIBERNATED]) &&
           hd_app_mgr_can_launch ())
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
  else if (/* TODO: Add a timeout for waiting on the init_done signal. */
      /* priv->init_done &&
      !priv->lowmem &&
      !priv->bg_killing && */
      !priv->launcher_shown &&
      !g_queue_is_empty (priv->queues[QUEUE_PRESTARTABLE]) &&
      hd_app_mgr_can_prestart ())
    {
      HdLauncherApp *app = g_queue_peek_head (priv->queues[QUEUE_PRESTARTABLE]);
      hd_app_mgr_prestart (app);
      if (!g_queue_is_empty (priv->queues[QUEUE_PRESTARTABLE]))
        loop = TRUE;
    }

  /* Now the tricky part. This function is called by a timeout or by
   * changes in memory conditions. If we're already looping, return if we
   * need to loop. If not, and we need to loop, start the loop.
   */
  if (!priv->state_check_looping && loop)
    {
      g_timeout_add_seconds (STATE_CHECK_INTERVAL,
                             hd_app_mgr_state_check_loop,
                             NULL);
      priv->state_check_looping = TRUE;
    }
  else if (!loop)
    priv->state_check_looping = FALSE;

  return loop;
}

static void
hd_app_mgr_dbus_name_owner_changed (DBusGProxy *proxy,
                                    const char *name,
                                    const char *old_owner,
                                    const char *new_owner,
                                    gpointer data)
{
  GList *items;
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());

  /* Check only connections and disconnections. */
  if (!old_owner[0] == !new_owner[0])
    return;

  /* Check if the service is one we want always on. */
  items = hd_launcher_tree_get_items(priv->tree, NULL);
  while (items)
    {
      HdLauncherItem *item = items->data;

      if (hd_launcher_item_get_item_type (item) == HD_APPLICATION_LAUNCHER)
        {
          HdLauncherApp *app = HD_LAUNCHER_APP (item);

          if (!g_strcmp0 (name, hd_launcher_app_get_service (app)))
            {
              if (!new_owner[0])
                { /* Disconnection */
                  g_debug ("%s: App %s has fallen\n", __FUNCTION__,
                      hd_launcher_item_get_id (HD_LAUNCHER_ITEM (app)));
                  /* We have the correct app, deal accordingly. */

                  /* The app must have been hibernated or closed. */
                  if (hd_launcher_app_get_state (app) !=
                      HD_APP_STATE_HIBERNATING)
                    hd_app_mgr_closed (app);

                  /* Add to prestartable and check state if always-on. */
                  if (hd_launcher_app_get_prestart_mode (app) ==
                        HD_APP_PRESTART_ALWAYS)
                    {
                      hd_app_mgr_add_to_queue (QUEUE_PRESTARTABLE, app);
                      hd_app_mgr_state_check ();
                    }
                }
              else
                { /* Connection */
                  if (!hd_launcher_app_get_pid (app))
                    hd_app_mgr_request_app_pid (app);
                }
              break;
            }
        }

      items = g_list_next (items);
    }
}

gboolean
hd_app_mgr_dbus_launch_app (HdAppMgr *self, const gchar *id)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (self);
  HdLauncherItem *item = hd_launcher_tree_find_item (priv->tree, id);
  HdLauncherApp *app = NULL;

  if (!item)
    return FALSE;
  if (hd_launcher_item_get_item_type (item) != HD_APPLICATION_LAUNCHER)
    return FALSE;

  app = HD_LAUNCHER_APP (item);

  return hd_app_mgr_launch (app);
}

static DBusHandlerResult
hd_app_mgr_signal_handler (DBusConnection *conn,
                           DBusMessage *msg,
                           void *data)
{
  gboolean changed = TRUE;
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (HD_APP_MGR (data));

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
  else
    changed = FALSE;

  if (changed)
    hd_app_mgr_state_check ();

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void
_hd_app_mgr_request_app_pid_cb (DBusGProxy *proxy, guint pid,
    GError *error, gpointer data)
{
  HdLauncherApp *app = HD_LAUNCHER_APP (data);

  if (error)
    {
      g_warning ("%s: Couldn't get pid for app %s because %s\n",
                 __FUNCTION__, hd_launcher_app_get_service (app),
                 error->message);
      return;
    }

  g_debug ("%s: Got pid %d for %s\n", __FUNCTION__,
           pid, hd_launcher_app_get_service (app));
  hd_launcher_app_set_pid (app, pid);
}

static void
hd_app_mgr_request_app_pid (HdLauncherApp *app)
{
  DBusGProxy *proxy = (HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get()))->dbus_proxy;
  const gchar *service = hd_launcher_app_get_service (app);

  if (!service)
    {
      g_warning ("%s: Can't get the pid for a non-dbus app.\n", __FUNCTION__);
      hd_launcher_app_set_pid (app, 0);
    }

  org_freedesktop_DBus_get_connection_unix_process_id_async (proxy,
      service,
      _hd_app_mgr_request_app_pid_cb, (gpointer)app);
}

HdLauncherApp *
hd_app_mgr_match_window (const char *res_name,
                         const char *res_class,
                         GPid pid)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  GList *apps = hd_launcher_tree_get_items (priv->tree, NULL);
  HdLauncherApp *result = NULL;

  if (!res_name && !res_class)
    {
      g_warning ("%s: Can't match windows with no WM_CLASS set.\n", __FUNCTION__);
      return NULL;
    }

  while (apps)
    {
      HdLauncherApp *app;

      /* Filter non-applications. */
      if (hd_launcher_item_get_item_type (HD_LAUNCHER_ITEM (apps->data)) !=
          HD_APPLICATION_LAUNCHER)
        goto next;

      app = HD_LAUNCHER_APP (apps->data);

      /* First try to match the explicit WM_CLASS. */
      if (res_class &&
          hd_launcher_app_get_wm_class (app) &&
          g_strcmp0 (hd_launcher_app_get_wm_class (app), res_class) == 0)
        {
          result = app;
          break;
        }

      /* Now try the app's id with the class name, ignoring case. */
      if (res_class &&
          g_ascii_strncasecmp (res_class,
              hd_launcher_item_get_id (HD_LAUNCHER_ITEM (app)),
              strlen (res_class)) == 0)
        {
          result = app;
          break;
        }

      /* Try the executable as a last resource. */
      if (res_name &&
          g_strcmp0 (res_name, hd_launcher_app_get_exec (app)) == 0)
        {
          result = app;
          break;
        }

      next:
        apps = g_list_next (apps);
    }

  if (result)
    {
      hd_launcher_app_set_state (result, HD_APP_STATE_SHOWN);

      /* If we didn't have a pid, get the one from the window. */
      if (!hd_launcher_app_get_pid (result))
        hd_launcher_app_set_pid (result, pid);

      /* Signal that the app has appeared.
       * TODO: I'd prefer to signal this when the window is mapped,
       * but right now here's the only place HdAppMgr gets to know this.
       */
      g_signal_emit (hd_app_mgr_get (), app_mgr_signals[APP_SHOWN],
                     0, result, NULL);

      /* Remove it from prestarting lists, just in case it has been launched
       * from somewhere else.
       */
      hd_app_mgr_remove_from_queue (QUEUE_PRESTARTED, result);
      hd_app_mgr_remove_from_queue (QUEUE_PRESTARTABLE, result);
    }

  return result;
}

void
hd_app_mgr_dump_app_list (gboolean only_running)
{
  GList *apps;

  g_debug ("List of launched applications:");
  apps = hd_launcher_tree_get_items (hd_launcher_get_tree(), NULL);
  for (; apps; apps = apps->next)
    {
      HdLauncherApp *app;

      if (hd_launcher_item_get_item_type (apps->data) != HD_APPLICATION_LAUNCHER)
        continue;

      app = HD_LAUNCHER_APP (apps->data);
      if (!only_running || hd_launcher_app_get_state (app) == HD_APP_STATE_SHOWN)
        {
          g_debug("app=%p, pid=%d, wm_class=%s, service=%s, state=%d",
                  app, hd_launcher_app_get_pid (app),
                  hd_launcher_app_get_wm_class (app),
                  hd_launcher_app_get_service (app),
                  hd_launcher_app_get_state (app));
        }
    }
}
