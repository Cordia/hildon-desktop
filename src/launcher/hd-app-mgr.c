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
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/types.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#include "hd-launcher.h"
#include "hd-launcher-tree.h"

#define I_(str) (g_intern_static_string ((str)))

struct _HdAppMgrPrivate
{
  /* TODO: Remove this and use libgnome-menu. */
  HdLauncherTree *tree;

  DBusGProxy *dbus_proxy;
};

#define HD_APP_MGR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                     HD_TYPE_APP_MGR, HdAppMgrPrivate))

/* Signals */
enum
{
  APP_LAUNCHED,
  APP_SHOWN,

  LAST_SIGNAL
};
static guint app_mgr_signals[LAST_SIGNAL] = {0, };

G_DEFINE_TYPE (HdAppMgr, hd_app_mgr, G_TYPE_OBJECT);

/* Memory usage */
#define LOWMEM_PROC_ALLOWED    "/proc/sys/vm/lowmem_allowed_pages"
#define LOWMEM_PROC_USED       "/proc/sys/vm/lowmem_used_pages"
#define LOWMEM_LAUNCH_THRESHOLD_DISTANCE 2500

/* DBus names */
#define OSSO_BUS_ROOT          "com.nokia"
#define OSSO_BUS_TOP           "top_application"
#define PATH_NAME_LEN           255
#define DBUS_NAMEOWNERCHANGED_SIGNAL_NAME "NameOwnerChanged"

/* Forward declarations */
static void hd_app_mgr_dispose (GObject *gobject);

static gboolean hd_app_mgr_service_top (const gchar *service,
                                        const gchar *param);
static gboolean hd_app_mgr_execute (const gchar *exec);

static gboolean hd_app_mgr_memory_available (void);

static void hd_app_mgr_dbus_name_owner_changed (DBusGProxy *proxy,
                                                const char *name,
                                                const char *old_owner,
                                                const char *new_owner,
                                                gpointer data);

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
  app_mgr_signals[APP_SHOWN] =
    g_signal_new (I_("application-appeared"),
                  HD_TYPE_APP_MGR,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, HD_TYPE_LAUNCHER_APP);
}

static void
hd_app_mgr_init (HdAppMgr *self)
{
  HdAppMgrPrivate *priv;

  self->priv = priv = HD_APP_MGR_GET_PRIVATE (self);

  DBusGConnection *connection;
  connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
  if (!connection)
    {
      g_warning ("%s: Failed to connect to session dbus.\n", __FUNCTION__);
      return;
    }
  priv->dbus_proxy = dbus_g_proxy_new_for_name (connection,
                                                DBUS_SERVICE_DBUS,
                                                DBUS_PATH_DBUS,
                                                DBUS_INTERFACE_DBUS);
  if (!priv->dbus_proxy)
    {
      g_warning ("%s: Failed to connect to session dbus.\n", __FUNCTION__);
      return;
    }

  dbus_g_proxy_add_signal (priv->dbus_proxy,
      DBUS_NAMEOWNERCHANGED_SIGNAL_NAME,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (priv->dbus_proxy,
      DBUS_NAMEOWNERCHANGED_SIGNAL_NAME,
      (GCallback)hd_app_mgr_dbus_name_owner_changed,
      NULL, NULL);

  self->priv->tree = g_object_ref (hd_launcher_get_tree ());
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

  G_OBJECT_CLASS (hd_app_mgr_parent_class)->dispose (gobject);
}

/* Application management */

gboolean
hd_app_mgr_launch (HdLauncherApp *app)
{
  gboolean result = FALSE;
  const gchar *service = hd_launcher_app_get_service (app);
  const gchar *exec;

  if (!hd_app_mgr_memory_available ())
    {
      /*
       * TODO -- we probably should pop a dialog here asking the user to
       * kill some apps as the old TN used to do; check the current spec.
       */
      g_debug ("%s: Not enough memory to start application %s.",
               __FUNCTION__, service);
      return FALSE;
    }

  if (service)
    {
      result = hd_app_mgr_service_top (service, NULL);
    }
  else
    {
      exec = hd_launcher_app_get_exec (app);
      if (exec)
        {
          result = hd_app_mgr_execute (exec);
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

/* This just tells the app that it's been relaunched. */
gboolean
hd_app_mgr_relaunch (HdLauncherApp *app)
{
  const gchar *service = hd_launcher_app_get_service (app);

  if (service)
    return hd_app_mgr_service_top (service, NULL);

  /* If it's a plain old app, nothing to do. */
  return TRUE;
}

gboolean
hd_app_mgr_prestart (HdLauncherApp *app)
{
  DBusError derror;
  DBusConnection *conn;
  gboolean res;
  const gchar *service = hd_launcher_app_get_service (app);

  if (!service)
    {
      g_warning ("%s: Can't prestart an app without service.\n", __FUNCTION__);
      return FALSE;
    }

  if (!hd_app_mgr_memory_available ())
    {
      /* TODO: Check memory limits before prestarting apps. */
      g_debug ("%s: Not enough memory to prestart application %s.",
               __FUNCTION__, service);
      return FALSE;
    }

  dbus_error_init (&derror);
  conn = dbus_bus_get (DBUS_BUS_SESSION, &derror);
  if (dbus_error_is_set (&derror))
  {
    g_warning ("could not start: %s: %s", service, derror.message);
    dbus_error_free (&derror);
    return FALSE;
  }

  res = dbus_bus_start_service_by_name (conn, service, 0, NULL, &derror);
  if (dbus_error_is_set (&derror))
  {
    g_warning ("could not start: %s: %s", service, derror.message);
    dbus_error_free (&derror);
  }

  if (res)
    hd_launcher_app_set_state (app, HD_APP_STATE_PRESTARTED);
  return res;
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

  if (!hd_app_mgr_memory_available ())
    {
      /*
       * TODO -- we probably should pop a dialog here asking the user to
       * kill some apps as the old TN used to do; check the current spec.
       */
      g_debug ("%s: Not enough memory to start application %s.",
               __FUNCTION__, service);
      return FALSE;
    }

  res = hd_app_mgr_service_top (service, "RESTORE");
  if (res)
    hd_launcher_app_set_state (app, HD_APP_STATE_LOADING);
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
hd_app_mgr_execute (const gchar *exec)
{
  gboolean res = FALSE;
  gchar *space = strchr (exec, ' ');
  gchar *exec_cmd;
  gint argc;
  gchar **argv = NULL;
  GPid child_pid;
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
                       &child_pid,
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
    dbus_message_append_args (msg, DBUS_TYPE_STRING, param,
                              DBUS_TYPE_INVALID);

  if (!dbus_connection_send (conn, msg, NULL))
    {
      dbus_message_unref (msg);
      g_warning ("dbus_connection_send failed");
      return FALSE;
    }

  dbus_message_unref (msg);
  return TRUE;
}

static gboolean
hd_app_mgr_memory_available (void)
{
  guint    pages_allowed, pages_used;
  gboolean result = FALSE;
  FILE    *pages_allowed_f, *pages_used_f;

  pages_allowed_f = fopen (LOWMEM_PROC_ALLOWED, "r");
  pages_used_f    = fopen (LOWMEM_PROC_USED, "r");

  if (pages_allowed_f && pages_used_f)
    {
      fscanf (pages_allowed_f, "%u", &pages_allowed);
      fscanf (pages_used_f, "%u", &pages_used);

      if (pages_used < pages_allowed)
        result = TRUE;
    }
  else
    {
      g_warning ("%s: Could not read lowmem page stats, using scratchbox?",
          __FUNCTION__);
      result = TRUE;
    }

  if (pages_allowed_f)
    fclose(pages_allowed_f);

  if (pages_used_f)
    fclose(pages_used_f);

  return result;
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

  /* Check only disconnections. */
  if (strcmp(new_owner, ""))
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
              /* We have the correct app, deal accordingly. */
              if (hd_launcher_app_get_prestart_mode (app) ==
                    HD_APP_PRESTART_ALWAYS)
                {
                  hd_app_mgr_prestart (app);
                }
              else
                {
                  /* The app must have been hibernated or closed. */
                  if (hd_launcher_app_get_state (app) !=
                      HD_APP_STATE_HIBERNATING)
                    hd_launcher_app_set_state (app, HD_APP_STATE_INACTIVE);
                }
              break;
            }
        }

      items = g_list_next (items);
    }
}

HdLauncherApp *
hd_app_mgr_match_window (const char *res_name,
                         const char *res_class)
{
  HdAppMgrPrivate *priv = HD_APP_MGR_GET_PRIVATE (hd_app_mgr_get ());
  GList *apps = hd_launcher_tree_get_items (priv->tree, NULL);
  HdLauncherApp *result = NULL;

  if (!res_name && !res_class)
    {
      g_warning ("%s: Can't match windows with no WM_CLASS set.\n", __FUNCTION__);
      return NULL;
    }

  g_debug ("%s: Matching name: %s and class: %s", __FUNCTION__, res_name, res_class);

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
      g_debug ("%s: Matched window for %s\n", __FUNCTION__,
          hd_launcher_item_get_id (HD_LAUNCHER_ITEM (result)));

      /* Signal that the app has appeared.
       * TODO: I'd prefer to signal this when the window is mapped,
       * but right now here's the only place HdAppMgr gets to know this.
       */
      g_signal_emit (hd_app_mgr_get (), app_mgr_signals[APP_SHOWN],
                     0, result, NULL);
    }

  return result;
}
