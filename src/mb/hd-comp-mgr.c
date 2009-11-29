/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Johan Bilien <johan.bilien@nokia.com>
 *          Tomas Frydrych <tf@o-hand.com>
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

#include "hd-comp-mgr.h"
#include "hd-switcher.h"
#include "hd-task-navigator.h"
#include "hd-home.h"
#include "hd-dbus.h"
#include "hd-atoms.h"
#include "hd-util.h"
#include "hd-transition.h"
#include "hd-wm.h"
#include "hd-home-applet.h"
#include "hd-app.h"
#include "hd-gtk-style.h"
#include "hd-note.h"
#include "hd-animation-actor.h"
#include "hd-render-manager.h"
#include "hd-title-bar.h"
#include "launcher/hd-app-mgr.h"
#include "launcher/hd-launcher-editor.h"

#include <matchbox/core/mb-wm.h>
#include <matchbox/core/mb-window-manager.h>
#include <matchbox/core/mb-wm-client.h>
#include <matchbox/theme-engines/mb-wm-theme.h>

#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/shape.h>

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>

#include "../tidy/tidy-blur-group.h"

#include <dbus/dbus-glib-bindings.h>
#ifdef HAVE_DSME
/* TODO Convert MCE to DSME */
#include <mce/dbus-names.h>
#endif

#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

#define OPERATOR_APPLET_ID         "_HILDON_OPERATOR_APPLET"
#define STAMP_DIR                  "/tmp/hildon-desktop/"
#define STAMP_FILE                 STAMP_DIR "desktop-started.stamp"
#define GCONF_KEY_ORIENTATION_LOCK "/apps/osso/hildon-desktop/orientation_lock"
#define GCONF_KEY_DESKTOP_ORIENTATION_LOCK "/apps/osso/hildon-desktop/desktop_orientation_lock"

#if 0
# define PORTRAIT       g_debug
#else
# define PORTRAIT(...)  /* NOP */
#endif

HdTaskNavigator *hd_task_navigator;

struct HdCompMgrPrivate
{
  MBWindowManagerClient *desktop;
  HdRenderManager       *render_manager;
  HdAppMgr              *app_mgr;

  HdSwitcher            *switcher_group;
  ClutterActor          *home;

  GHashTable            *shown_apps;
  GHashTable            *hibernating_apps;

  Atom                   atoms[_HD_ATOM_LAST];

  DBusConnection        *dbus_connection;

  /* g_idle_add() event source, set by hd_comp_mgr_sync_stacking()
   * to call hd_comp_mgr_restack() some time. */
  guint                  stack_sync;

  /* Do Not Disturb flag */
  gboolean               do_not_disturb_flag : 1;

  MBWindowManagerClient *status_area_client;
  MBWindowManagerClient *status_menu_client;

  HdCompMgrClient       *current_hclient;

  /* Track changes to the PORTRAIT properties. */
  unsigned long          property_changed_cb_id;

#ifdef HAVE_DSME
/* TODO Convert MCE to DSME */
  /* MCE D-Bus Proxy */
  DBusGProxy            *mce_proxy;
#endif

  /* Time of last mapped window */
  struct timeval         last_map_time;

  /* Portrait If Possible. If Enabled, we change mode depending on
   * pip_portrait, IF we think it is possible. */
  gboolean pip_enabled;
  gboolean pip_portrait;

  /* GConf client for orientation lock. */
  GConfClient* gconf_client;
};

/*
 * A helper object to store manager's per-client data
 */

struct HdCompMgrClientPrivate
{
  HdRunningApp *app;

  guint                 hibernation_key;
  gboolean              can_hibernate : 1;

  gboolean              has_video_overlay;
};

extern gboolean hd_dbus_display_is_off;
static guint portrait_freshness_counter;

HdRunningApp *hd_comp_mgr_client_get_app_key (HdCompMgrClient *client,
                                               HdCompMgr *hmgr);

static void hd_comp_mgr_check_do_not_disturb_flag (HdCompMgr *hmgr);

static gboolean
hd_comp_mgr_client_prefers_compositing (MBWindowManagerClient *c);

static gboolean
hd_comp_mgr_is_non_composited (MBWindowManagerClient *client,
                               gboolean force_re_read);

static MBWindowManagerClient *hd_comp_mgr_determine_current_app (void);

static MBWMCompMgrClient *
hd_comp_mgr_client_new (MBWindowManagerClient * client)
{
  MBWMObject *c;

  c = mb_wm_object_new (HD_TYPE_COMP_MGR_CLIENT,
			MBWMObjectPropClient, client,
			NULL);

  return MB_WM_COMP_MGR_CLIENT (c);
}

static void
hd_comp_mgr_client_class_init (MBWMObjectClass *klass)
{
#if MBWM_WANT_DEBUG
  klass->klass_name = "HdCompMgrClient";
#endif
}

static void
hd_comp_mgr_client_process_hibernation_prop (HdCompMgrClient * hc)
{
  HdCompMgrClientPrivate * priv = hc->priv;
  MBWindowManagerClient  * wm_client = MB_WM_COMP_MGR_CLIENT (hc)->wm_client;
  HdCompMgr              * hmgr = HD_COMP_MGR (wm_client->wmref->comp_mgr);
  Atom                   * hibernable = NULL;

  /* NOTE:
   *       the prop has no 'value'; if set the app is killable (hibernatable),
   *       deletes to unset.
   */
  hibernable = hd_util_get_win_prop_data_and_validate
                     (wm_client->wmref->xdpy,
		      wm_client->window->xwindow,
                      hmgr->priv->atoms[HD_ATOM_HILDON_APP_KILLABLE],
                      XA_STRING,
                      8,
                      0,
                      NULL);

  if (!hibernable)
    {
      /*try the alias*/
      hibernable = hd_util_get_win_prop_data_and_validate
	            (wm_client->wmref->xdpy,
		     wm_client->window->xwindow,
                     hmgr->priv->atoms[HD_ATOM_HILDON_ABLE_TO_HIBERNATE],
                     XA_STRING,
                     8,
                     0,
                     NULL);
    }

  if (hibernable)
      priv->can_hibernate = TRUE;
  else
    priv->can_hibernate = FALSE;

  if (hibernable)
    XFree (hibernable);
}

HdRunningApp *
hd_comp_mgr_client_get_app_key (HdCompMgrClient *client, HdCompMgr *hmgr)
{
  MBWindowManagerClient *wm_client;
  MBWindowManager       *wm;
  XClassHint             class_hint;
  Status                 status = 0;
  HdRunningApp          *app = NULL;
  HdCompMgrClientPrivate *priv = client->priv;

  wm = MB_WM_COMP_MGR (hmgr)->wm;
  wm_client = MB_WM_COMP_MGR_CLIENT (client)->wm_client;

  /* We only lookup the app for main windows and dialogs. */
  if (MB_WM_CLIENT_CLIENT_TYPE (wm_client) != MBWMClientTypeApp &&
      MB_WM_CLIENT_CLIENT_TYPE (wm_client) != MBWMClientTypeDialog)
    return NULL;

  memset(&class_hint, 0, sizeof(XClassHint));

  /* We don't care about X errors here, because they will be reported
   * in the return value of XGetWindowAttributes */
  mb_wm_util_async_trap_x_errors (wm->xdpy);

  status = XGetClassHint(wm->xdpy, wm_client->window->xwindow, &class_hint);

  mb_wm_util_async_untrap_x_errors();
  if (!status)
    goto out;

  app = hd_app_mgr_match_window (class_hint.res_name,
                                 class_hint.res_class,
                                 wm_client->window->pid);

  if (app)
    {
      /* Calculate an hibernation key from:
       * - The app name.
       * - The role, if present.
       * - The window name.
       */
      gchar *role = NULL;
      gchar *key = NULL;
      gint level = 0;
      role = hd_util_get_win_prop_data_and_validate
                         (wm_client->wmref->xdpy,
                          wm_client->window->xwindow,
                          hmgr->priv->atoms[HD_ATOM_WM_WINDOW_ROLE],
                          XA_STRING,
                          8,
                          0,
                          NULL);

      if (MB_WM_CLIENT_CLIENT_TYPE (wm_client) == MBWMClientTypeApp)
        {
          HdApp *hdapp = HD_APP (wm_client);
          level = hdapp->stack_index;
        }

      key = g_strdup_printf ("%s/%s/%s/%d",
              hd_running_app_get_id (app),
              class_hint.res_class ? class_hint.res_class : "",
              role ? role : "",
              level);
      g_debug ("%s: app %s, window key: %s\n", __FUNCTION__,
                hd_running_app_get_id (app),
                key);
      priv->hibernation_key = g_str_hash (key);
      if (role)
        XFree (role);
      g_free (key);
    }

 out:
  if (class_hint.res_class)
    XFree(class_hint.res_class);

  if (class_hint.res_name)
    XFree(class_hint.res_name);

  return app;
}

static int
hd_comp_mgr_client_init (MBWMObject *obj, va_list vap)
{
  HdCompMgrClient        *client = HD_COMP_MGR_CLIENT (obj);
  HdCompMgrClientPrivate *priv;
  HdCompMgr              *hmgr;
  MBWindowManagerClient  *wm_client = MB_WM_COMP_MGR_CLIENT (obj)->wm_client;
  HdRunningApp          *app;

  hmgr = HD_COMP_MGR (wm_client->wmref->comp_mgr);

  priv = client->priv = g_new0 (HdCompMgrClientPrivate, 1);

  app = hd_comp_mgr_client_get_app_key (client, hmgr);
  if (app)
    {
      priv->app = g_object_ref (app);
      hd_comp_mgr_client_process_hibernation_prop (client);

      /* Look up if there were already windows for this app. */
      guint windows = (guint)g_hash_table_lookup (hmgr->priv->shown_apps,
                                                  (gpointer)app);
      if (!windows)
        hd_app_mgr_app_opened (app);

      g_hash_table_insert (hmgr->priv->shown_apps,
                           (gpointer)app,
                           (gpointer)++windows);
    }

  /* Initially get window overlay state */
  client->priv->has_video_overlay = hd_util_client_has_video_overlay(wm_client);

  return 1;
}

static void
hd_comp_mgr_client_destroy (MBWMObject* obj)
{
  HdCompMgrClientPrivate *priv = HD_COMP_MGR_CLIENT (obj)->priv;

  if (priv->app)
    {
      g_object_unref (priv->app);
      priv->app = NULL;
    }

  g_free (priv);
}

int
hd_comp_mgr_client_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdCompMgrClientClass),
	sizeof (HdCompMgrClient),
	hd_comp_mgr_client_init,
	hd_comp_mgr_client_destroy,
	hd_comp_mgr_client_class_init
      };

      type =
	mb_wm_object_register_class (&info,
				     MB_WM_TYPE_COMP_MGR_CLUTTER_CLIENT, 0);
    }

  return type;
}

gboolean
hd_comp_mgr_client_is_hibernating (HdCompMgrClient *hclient)
{
  HdCompMgrClientPrivate * priv = hclient->priv;

  if (priv->app)
    return (hd_running_app_is_hibernating (priv->app));

  return FALSE;
}

gboolean
hd_comp_mgr_client_can_hibernate (HdCompMgrClient *hclient)
{
  HdCompMgrClientPrivate * priv = hclient->priv;

  return priv->can_hibernate;
}

gboolean
hd_comp_mgr_client_has_video_overlay (HdCompMgrClient *hclient)
{
  HdCompMgrClientPrivate * priv = hclient->priv;

  return priv->has_video_overlay;
}

HdRunningApp *
hd_comp_mgr_client_get_app (HdCompMgrClient *hclient)
{
  if (!hclient) return NULL;
  return hclient->priv->app;
}

HdLauncherApp *
hd_comp_mgr_client_get_launcher (HdCompMgrClient *hclient)
{
  if (!hclient || !hclient->priv->app) return NULL;
  return hd_running_app_get_launcher_app(hclient->priv->app);
}

const gchar *
hd_comp_mgr_client_get_app_local_name (HdCompMgrClient *hclient)
{
  HdRunningApp *app = hclient->priv->app;
  if (app)
    {
      HdLauncherApp *launcher = hd_running_app_get_launcher_app (app);
      if (launcher)
        return hd_launcher_item_get_local_name (HD_LAUNCHER_ITEM (launcher));
    }
  return NULL;
}

static int  hd_comp_mgr_init (MBWMObject *obj, va_list vap);
static void hd_comp_mgr_class_init (MBWMObjectClass *klass);
static void hd_comp_mgr_destroy (MBWMObject *obj);
static void hd_comp_mgr_register_client (MBWMCompMgr *mgr,
                                         MBWindowManagerClient *c,
                                         Bool activate);
static void hd_comp_mgr_unregister_client (MBWMCompMgr *mgr,
                                           MBWindowManagerClient *c);
static void hd_comp_mgr_map_notify
                        (MBWMCompMgr *mgr, MBWindowManagerClient *c);
static void hd_comp_mgr_unmap_notify
                        (MBWMCompMgr *mgr, MBWindowManagerClient *c);
static void hd_comp_mgr_turn_on (MBWMCompMgr *mgr);
static void hd_comp_mgr_effect (MBWMCompMgr *mgr, MBWindowManagerClient *c,
                                MBWMCompMgrClientEvent event);
static Bool hd_comp_mgr_client_property_changed (XPropertyEvent *event,
                                                 HdCompMgr *hmgr);

int
hd_comp_mgr_class_type ()
{
  static int type = 0;

  if (type == 0)
    {
      static MBWMObjectClassInfo info = {
        sizeof (HdCompMgrClass),
        sizeof (HdCompMgr),
        hd_comp_mgr_init,
        hd_comp_mgr_destroy,
        hd_comp_mgr_class_init
      };

      type = mb_wm_object_register_class (&info,
					  MB_WM_TYPE_COMP_MGR_CLUTTER, 0);
    }

  return type;
}

static void
hd_comp_mgr_class_init (MBWMObjectClass *klass)
{
  MBWMCompMgrClass *cm_klass = MB_WM_COMP_MGR_CLASS (klass);
  MBWMCompMgrClutterClass * clutter_klass =
    MB_WM_COMP_MGR_CLUTTER_CLASS (klass);

  cm_klass->unregister_client = hd_comp_mgr_unregister_client;
  cm_klass->register_client   = hd_comp_mgr_register_client;
  cm_klass->client_event      = hd_comp_mgr_effect;
  cm_klass->turn_on           = hd_comp_mgr_turn_on;
  cm_klass->map_notify        = hd_comp_mgr_map_notify;
  cm_klass->unmap_notify      = hd_comp_mgr_unmap_notify;
  cm_klass->restack           = (void (*)(MBWMCompMgr*))hd_comp_mgr_restack;

  clutter_klass->client_new   = hd_comp_mgr_client_new;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HDCompMgr";
#endif
}

static int
hd_comp_mgr_init (MBWMObject *obj, va_list vap)
{
  MBWMCompMgr          *cmgr = MB_WM_COMP_MGR (obj);
  MBWindowManager      *wm = cmgr->wm;
  HdCompMgr            *hmgr = HD_COMP_MGR (obj);
  HdCompMgrPrivate     *priv;
  ClutterActor         *stage;
  ClutterActor         *arena;
#ifdef HAVE_DSME
/* TODO Convert MCE to DSME */
  DBusGConnection      *system_connection;
#endif
  GError               *error = NULL;
  extern MBWindowManager *hd_mb_wm;

  priv = hmgr->priv = g_new0 (HdCompMgrPrivate, 1);

  hd_mb_wm = wm;

  hd_atoms_init (wm->xdpy, priv->atoms);

  priv->dbus_connection = hd_dbus_init (hmgr);

  priv->gconf_client = gconf_client_get_default();
  g_assert(GCONF_IS_CLIENT(priv->gconf_client));

  hd_gtk_style_init ();

  stage = clutter_stage_get_default ();

  /*
   * Create the home group before the switcher, so the switcher can
   * connect it's signals to it.
   */
  priv->home = g_object_new (HD_TYPE_HOME, "comp-mgr", cmgr, NULL);

  clutter_actor_set_reactive (priv->home, TRUE);

  clutter_actor_show (priv->home);

  hd_task_navigator = hd_task_navigator_new ();

  priv->render_manager = hd_render_manager_create(hmgr,
		                                  hd_launcher_get(),
		                                  HD_HOME(priv->home),
						  hd_task_navigator);
  g_object_set(priv->home, "hdrm", priv->render_manager, NULL);
  clutter_container_add_actor(CLUTTER_CONTAINER (stage),
                              CLUTTER_ACTOR(priv->render_manager));

  /* Pass the render manager to the app mgr so it knows when it can't
   * prestart apps.
   */
  priv->app_mgr = g_object_ref (hd_app_mgr_get ());
  hd_app_mgr_set_render_manager (G_OBJECT (priv->render_manager));

  /* NB -- home must be constructed before constructing the switcher;
   */
  priv->switcher_group = g_object_new (HD_TYPE_SWITCHER,
				       "comp-mgr", cmgr,
				       "task-nav", hd_task_navigator,
				       NULL);

  /* When a MBWMCompMgrClutterClient is first created, it is added to the arena.
   * This will cause a redraw unless we stop the arena from causing a screen
   * redraw. When we want a window rendered, it is pulled out into
   * hd-render-manager.*/
  arena = mb_wm_comp_mgr_clutter_get_arena(MB_WM_COMP_MGR_CLUTTER(cmgr));
  if (arena)
    {
      clutter_actor_set_allow_redraw(arena, FALSE);
      clutter_actor_hide(arena);
      g_object_unref(arena); /* mb_wm_comp_mgr_clutter_get_arena refs us */
    }

  /*
   * Create hash tables for keeping active apps and hibernating windows.
   */
  priv->shown_apps =
    g_hash_table_new_full (g_direct_hash,
                           g_direct_equal,
                           NULL,
                           NULL);
  priv->hibernating_apps =
    g_hash_table_new_full (g_direct_hash,
			   g_direct_equal,
			   NULL,
			   (GDestroyNotify)mb_wm_object_unref);

  /* Be notified about all X window property changes around here. */
  priv->property_changed_cb_id = mb_wm_main_context_x_event_handler_add (
                   cmgr->wm->main_ctx, None, PropertyNotify,
                   (MBWMXEventFunc)hd_comp_mgr_client_property_changed, cmgr);

  hd_render_manager_set_state(HDRM_STATE_HOME);

#ifdef HAVE_DSME
/* TODO Convert MCE to DSME */
  /* Get D-Bus proxy for mce calls */
  system_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);

  if (error)
    {
      g_warning ("Could not connect to System D-Bus. %s", error->message);
      g_error_free (error);
    }
  else
    {
      priv->mce_proxy = dbus_g_proxy_new_for_name (system_connection,
                                                   MCE_SERVICE,
                                                   MCE_REQUEST_PATH,
                                                   MCE_REQUEST_IF);
      g_debug ("%s. Got mce Proxy", __FUNCTION__);
    }
#endif

  return 1;
}

static void
hd_comp_mgr_destroy (MBWMObject *obj)
{
  HdCompMgrPrivate * priv = HD_COMP_MGR (obj)->priv;

  if (priv->shown_apps)
    g_hash_table_destroy (priv->shown_apps);
  if (priv->hibernating_apps)
    g_hash_table_destroy (priv->hibernating_apps);
  if (priv->app_mgr)
    {
      g_object_unref (priv->app_mgr);
      priv->app_mgr = NULL;
    }
  g_object_unref( priv->render_manager );

  mb_wm_main_context_x_event_handler_remove (
                                     MB_WM_COMP_MGR (obj)->wm->main_ctx,
                                     PropertyNotify,
                                     priv->property_changed_cb_id);

#ifdef HAVE_DSME
/* TODO Convert MCE to DSME */
  if (priv->mce_proxy)
    {
      g_object_unref (priv->mce_proxy);
      priv->mce_proxy = NULL;
    }
#endif

  if (priv->stack_sync)
    g_source_remove (priv->stack_sync);
}

HdCompMgrClient *
hd_comp_mgr_get_current_client (HdCompMgr *hmgr)
{
  HdCompMgrPrivate * priv = hmgr->priv;

  return priv->current_hclient;
}

static gboolean
hd_comp_mgr_client_prefers_compositing (MBWindowManagerClient *c)
{
  if (MB_WM_CLIENT_CLIENT_TYPE (c)
      & (HdWmClientTypeStatusArea|MBWMClientTypeOverride|HdWmClientTypeHomeApplet))
    return FALSE;
  if (HD_IS_INCOMING_EVENT_NOTE (c))
    return FALSE;
  /* ...or application that wants non-composited mode */
  return !(HD_IS_APP (c) && hd_comp_mgr_is_non_composited (c, FALSE));
}

/* Called on #PropertyNotify to handle changes to
 * _HILDON_PORTRAIT_MODE_SUPPORT and _HILDON_PORTRAIT_MODE_REQUEST
 * and _HILDON_APP_KILLABLE and _HILDON_ABLE_TO_HIBERNATE
 * and _HILDON_DO_NOT_DISTURB and _HILDON_NOTIFICATION_THREAD. */
Bool
hd_comp_mgr_client_property_changed (XPropertyEvent *event, HdCompMgr *hmgr)
{
  Atom killable, able_to_hibernate, dnd, nothread;
  gboolean non_comp_changed;
  gint value;
  MBWindowManager *wm;
  HdCompMgrClient *cc;
  MBWindowManagerClient *c;

  if (event->type != PropertyNotify)
    return True;

  killable = hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_APP_KILLABLE);
  able_to_hibernate = hd_comp_mgr_get_atom (hmgr,
                          HD_ATOM_HILDON_ABLE_TO_HIBERNATE);
  dnd = hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_DO_NOT_DISTURB);

  wm = MB_WM_COMP_MGR (hmgr)->wm;

  if (event->atom == wm->atoms[MBWM_ATOM_HILDON_LIVE_DESKTOP_BACKGROUND])
    {
      HdCompMgrPrivate *priv = hmgr->priv;
      c = mb_wm_managed_client_from_xwindow (wm, event->window);
      if (c)
        {
          /* TODO: handle zero value */
          ClutterActor *actor;
          MBWMCompMgrClutterClient *cclient =
                MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);
          actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);
          /*g_printerr ("%s: client '%s' now has live-bg value %d\n", __func__,
                      mb_wm_client_get_name (c),
                      c->window->live_background);*/
          mb_wm_comp_mgr_clutter_client_set_flags (cclient,
                                   MBWMCompMgrClutterClientDontPosition);
          /* remove it from the switcher */
          if (hd_task_navigator_has_window (hd_task_navigator, actor))
            hd_switcher_remove_window_actor (priv->switcher_group,
                                             actor, cclient);
          hd_home_set_live_background (HD_HOME (priv->home), c);
         	if(STATE_IS_PORTRAIT (hd_render_manager_get_state ()))
	       		hd_render_manager_set_state (HDRM_STATE_HOME_PORTRAIT);
					else
		        hd_render_manager_set_state (HDRM_STATE_HOME);
          hd_launcher_hide ();
        }
      return False;
    }

  if (event->atom==
    hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_WM_WINDOW_PROGRESS_INDICATOR) ||
      event->atom==
    hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_WM_WINDOW_MENU_INDICATOR))
    {
      /* Redraw the title to display/remove the progress indicator or app
       * menu indicator. The title itself will check what the new state should
       * be. NOTE: we have to redo dialog titles here too, so we can't just
       * use hd_title_bar_update. */
      MBWindowManagerClient *top;
      /* previous mb_wm_client_decor_mark_dirty didn't actually cause a redraw,
       * so mark the decor itself dirty */
      top = mb_wm_managed_client_from_xwindow(MB_WM_COMP_MGR (hmgr)->wm,
                                              event->window);
      if (top)
        {
          MBWMList *l = top->decor;
          while (l)
            {
              MBWMDecor *decor = l->data;
              if (decor->type == MBWMDecorTypeNorth)
                mb_wm_decor_mark_dirty (decor);
              l = l->next;
            }
        }
    }

  non_comp_changed = event->atom ==
        hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_NON_COMPOSITED_WINDOW);
  if (event->atom == wm->atoms[MBWM_ATOM_NET_WM_STATE] || non_comp_changed)
    {
      c = mb_wm_managed_client_from_xwindow (wm, event->window);
      if (c && HD_IS_APP (c))
        {
          gboolean client_non_comp;
          MBWindowManagerClient *tmp;
          gboolean found = FALSE;
          /* check if there is a window above that needs compositing */
          for (tmp = c->stacked_above; tmp; tmp = tmp->stacked_above)
            if (mb_wm_client_is_map_confirmed (tmp) &&
                hd_comp_mgr_client_prefers_compositing (tmp))
              {
                found = TRUE;
                break;
              }
          client_non_comp = hd_comp_mgr_is_non_composited (c, non_comp_changed);
          if (hd_render_manager_get_state () == HDRM_STATE_NON_COMPOSITED &&
	      !client_non_comp)
            hd_render_manager_set_state (HDRM_STATE_APP);
          else if (hd_render_manager_get_state () == HDRM_STATE_NON_COMP_PORT
                   && !client_non_comp)
            hd_render_manager_set_state (HDRM_STATE_APP_PORTRAIT);
          else if (hd_render_manager_get_state () == HDRM_STATE_APP &&
                   !hd_transition_is_rotating () &&
	           client_non_comp && !found)
            hd_render_manager_set_state (HDRM_STATE_NON_COMPOSITED);
          else if (hd_render_manager_get_state () == HDRM_STATE_APP_PORTRAIT &&
                   !hd_transition_is_rotating () &&
	           client_non_comp && !found)
            hd_render_manager_set_state (HDRM_STATE_NON_COMP_PORT);
        }
    }
  /* Check for changes to the hibernable state. */
  if (event->atom == killable ||
      event->atom == able_to_hibernate)
    {
      HdRunningApp *app, *current_app;
      c = mb_wm_managed_client_from_xwindow (wm, event->window);
      if (!c || !c->cm_client)
        return False;
      cc = HD_COMP_MGR_CLIENT (c->cm_client);
      if (event->state == PropertyNewValue)
        cc->priv->can_hibernate = TRUE;
      else
        cc->priv->can_hibernate = FALSE;

      /* Change the hibernable state of the app only if it's not the
       * current app.
       */
      app = cc->priv->app;
      if (!app)
        return False;
      current_app =
        hd_comp_mgr_client_get_app (hd_comp_mgr_get_current_client (hmgr));
      if (!current_app || app == current_app)
        return False;

      if (event->state == PropertyNewValue)
        hd_app_mgr_hibernatable(app, TRUE);
      else
        hd_app_mgr_hibernatable (app, FALSE);

      return False;
    }

  if (event->atom == dnd)
    {
      hd_comp_mgr_check_do_not_disturb_flag (hmgr);
      return FALSE;
    }

  nothread = hd_comp_mgr_get_atom (hmgr, HD_ATOM_NOTIFICATION_THREAD);
  if (event->atom == nothread)
    {
      char *str;
      ClutterActor *a;

      c = mb_wm_managed_client_from_xwindow (wm, event->window);
      if (!c || !c->cm_client)
        return False;
      a = mb_wm_comp_mgr_clutter_client_get_actor (
                          MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client));
      str = event->state == PropertyNewValue
        ? hd_util_get_x_window_string_property (wm, c->window->xwindow,
                                                HD_ATOM_NOTIFICATION_THREAD)
        : NULL;
      if (event->state != PropertyNewValue || str)
        /* Otherwise don't mess up more. */
        hd_task_navigator_notification_thread_changed (hd_task_navigator,
                                                       a, str);
      return False;
    }

  /* Process XVIDEO flag. If this changed then we'll want to look again at
   * how we should blur. */
  if (event->atom == hd_comp_mgr_get_atom (hmgr, HD_ATOM_OMAP_VIDEO_OVERLAY))
    {
      c = mb_wm_managed_client_from_xwindow (wm, event->window);

      if (c && (cc = HD_COMP_MGR_CLIENT(c->cm_client)))
        {
          cc->priv->has_video_overlay = hd_util_client_has_video_overlay(c);
          hd_render_manager_update_blur_state();
        }
    }

  /* Process PORTRAIT flags */
  if (event->atom == wm->atoms[MBWM_ATOM_HILDON_PORTRAIT_MODE_SUPPORT])
    {
      if (!(c = mb_wm_managed_client_from_xwindow (wm, event->window)))
        return False;
      value = c->window->portrait_supported;
    }
  else if (event->atom == wm->atoms[MBWM_ATOM_HILDON_PORTRAIT_MODE_REQUEST])
    {
      if (!(c = mb_wm_managed_client_from_xwindow (wm, event->window)))
        return False;
      value = c->window->portrait_requested;
    }
  else
    return True;

  if (event->atom == wm->atoms[MBWM_ATOM_HILDON_PORTRAIT_MODE_REQUEST])
    hd_task_navigator_update_win_orientation(event->window, TRUE);
  else if (event->atom == wm->atoms[MBWM_ATOM_HILDON_PORTRAIT_MODE_SUPPORT])
    hd_task_navigator_update_win_orientation(event->window, value);

  /* Switch HDRM state if we need to.  Don't consider changing the state if
   * it is approved by the new value of the property.  We must reconsider
   * if we don't know if the property appoves or not. */

  if (STATE_IS_PORTRAIT (hd_render_manager_get_state()))
    { /* Portrait => landscape? */
      hd_app_mgr_mce_activate_accel_if_needed (FALSE);
      if (value <= 0 && !hd_comp_mgr_should_be_portrait (hmgr))
        hd_render_manager_set_state_unportrait ();
    }
  else if (STATE_IS_PORTRAIT_CAPABLE (hd_render_manager_get_state()))
    { /* Landscape => portrait? */
      hd_app_mgr_mce_activate_accel_if_needed (FALSE);
      if (value != 0 && hd_comp_mgr_should_be_portrait (hmgr))
        hd_render_manager_set_state_portrait ();
    }

  return False;
}

static void
hd_comp_mgr_turn_on (MBWMCompMgr *mgr)
{
  MBWMCompMgrClass * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));

  /*
   * The parent class turn_on method deals with setting up the input shape on
   * the overlay window; so we first call it, and then change the shape to
   * suit our custom needs.
   */
  if (parent_klass->turn_on)
    parent_klass->turn_on (mgr);

}


/* Returns whether @client is an App, Dialog or Confirmation Note.
 * Nothing else matters concerning portraitness. */
static gboolean __attribute__((pure))
is_interesting_client (MBWindowManagerClient *client)
{
  return (MB_WM_CLIENT_CLIENT_TYPE (client)
          & (MBWMClientTypeApp|MBWMClientTypeDialog))
    || HD_IS_CONFIRMATION_NOTE (client);
}

/* Returns the number of children, grandchildren, etc. of @client,
 * not including @client itself. */
static unsigned
cntchildren (MBWindowManagerClient *client)
{
  unsigned i;
  MBWMList *li;

  for (i = 0, li = client->transients; li; i++, li = li->next)
    i += cntchildren (li->data);

  return i;
}

/* Returns the stacking layer of @client, taking state and transiency
 * into account. */
static MBWMStackLayerType
layer_of (MBWindowManagerClient *client,
          gboolean needs_desktop, gboolean goto_app_state)
{
  /* Transients inherit the stacking layer. */
  while (client->transient_for)
    client = client->transient_for;

  /* Don't call desktop->stacking_layer() because it has side effects. */
  if (!(MB_WM_CLIENT_CLIENT_TYPE (client) & MBWMClientTypeDesktop))
    return mb_wm_client_get_stacking_layer (client);
  else if (!needs_desktop || goto_app_state)
    /* desktop is/will be at the bottom */
    return MBWMStackLayerBottom;
  else /* needs desktop now && !goto app state */
    return MBWMStackLayerMid;
}

/* Returns whether @lc is ok to be higher in the stack than @rc. */
static gboolean
ordered (MBWindowManagerClient *lc, MBWindowManagerClient *rc,
          gboolean needs_desktop, gboolean goto_app_state)
{
  MBWMStackLayerType llc, lrc;

  /* The desktop window is more equal amongst the equals. */
  llc = layer_of (lc, needs_desktop, goto_app_state);
  lrc = layer_of (rc, needs_desktop, goto_app_state);
  return llc != lrc
    ? llc > lrc
    : !(MB_WM_CLIENT_CLIENT_TYPE (rc) & MBWMClientTypeDesktop);
}

/* Guess whether we'd go to portrait or landscape when the newly mapped
 * @client is finally settled and rotate as soon as soon we can to either
 * direction.  Try hard not to guess wrong. */
static void
lp_forecast (MBWindowManager *wm, MBWindowManagerClient *client)
{
  MBWMClientType ctype;
  GPtrArray *stack;
  MBWindowManagerClient *c;
  gboolean goto_app_state;
  HDRMStateEnum state;
  unsigned l, r;
  /* GConf client for orientation lock. */
  GConfClient* gconf_client = gconf_client_get_default();;
  g_assert(GCONF_IS_CLIENT(gconf_client));

  /* Don't bother with anything but application windows, dialogs
   * and confirmation notes.  We simply don't have any other type
   * of interesting clients. */
  if (!is_interesting_client (client))
    return;

  /* We don't know where @client would be stacked, so construct the plausible
   * new window @stack:ing.  (&stack[0] == top) */
  stack = g_ptr_array_new ();
  for (c = wm->stack_top; c; c = c->stacked_below)
    g_ptr_array_add (stack, c);

  /*
   * Ensure that @client is stacked somewhere in its pile.
   * When a @client is mapped in practice three things may happen:
   * 1. if it's not transient it's stacked on the top
   * 2. otherwise it may not be stacked, thus it remains on the top
   * 3. or it may be stacked together with its application so that
   *    the pile is brought to the top
   * In the 2nd case there may be unrelated clients between the new
   * @client and its pile.  Fix this.
   */
  if (client->transient_for && wm->stack_top == client)
    {
      MBWMList *li;
      unsigned src, dst;
      MBWindowManagerClient *want;

      /* Find @client's oldest sibling, or if it's the first child
       * it will be closely above its parent. */
      for (want = client->transient_for, li = want->transients; ;
           want = li->data, li = li->next)
        {
          g_assert (li != NULL);
          if (li->data == client)
            break;
        }
      g_assert (want != client);

      /* @dst <- index of @want, where @client should be in @stack.
       * index of stack_top == 0 && want != client ==> index of want > 0 */
      for (dst = 1; ; dst++)
        {
          g_assert (dst < stack->len);
          if (stack->pdata[dst] == want)
            break;
        }

      /* dst <- where @client should be @stack:ed. */
      if (want != client->transient_for)
        {
          g_assert (dst >= cntchildren(want));
          dst -= cntchildren(want);
        }
      g_assert (dst > 0);
      dst--;

      /* @client is on the top, but if it's not destined there
       * move it to @dst. */
      src = 0;
      if (dst != src)
        {
          g_assert (src < dst);
          memmove (&stack->pdata[src], &stack->pdata[src+1],
                   sizeof (stack->pdata[0]) * (dst - src));
          stack->pdata[dst] = client;
        }
    }

  /* Are we @goto_app_state? */
  state = hd_render_manager_get_state ();
  ctype = MB_WM_CLIENT_CLIENT_TYPE (client);
  goto_app_state = (ctype & MBWMClientTypeApp);
  if (client->transient_for)
    {
      goto_app_state |= HD_IS_CONFIRMATION_NOTE (client);
      goto_app_state |= (ctype & MBWMClientTypeDialog)
	&& !STATE_IS_TASK_NAV(state);
    }

  /* Sort @stack by stacking layers and mb_wm_stack_ensure() would do
   * (except for the desktop window which is ordered to the highest
   *  position in its layer). */
  for (l = 1, r = 2; l < stack->len; l = r++)
    {
      while (!ordered(stack->pdata[l-1], stack->pdata[l],
                      STATE_NEED_DESKTOP (state), goto_app_state))
        {
          MBWindowManagerClient *tmp;

          tmp = stack->pdata[l-1];
          stack->pdata[l-1] = stack->pdata[l];
          stack->pdata[l] = tmp;
          if (!--l)
            break;
        }
    }

  /* Find the topmost interesting client and see its portrait preferences. */
  portrait_freshness_counter++;
  for (l = 0; stack->pdata[l] != wm->desktop; l++)
    {
      g_assert (l < stack->len);
      if (!is_interesting_client (c = stack->pdata[l]))
        continue;
      if ( (state == HDRM_STATE_HOME_EDIT_DLG || state == HDRM_STATE_HOME_EDIT_DLG_PORTRAIT) 
					&& goto_app_state && hd_is_hildon_home_dialog (c))
        /* Leaving EDIT_DLG state would close hildon-home dialogs. */
        continue;

      mb_wm_client_update_portrait_flags (c, portrait_freshness_counter);
      if ((!hd_transition_get_int("thp_tweaks", "forcerotation", 0) 
              && !c->portrait_supported)
              || gconf_client_get_bool (gconf_client, GCONF_KEY_ORIENTATION_LOCK, NULL)
              || hd_launcher_is_editor_in_landscape ())
        {
          hd_transition_rotate_screen (wm, FALSE);
          break;
        }
      else if (!c->portrait_requested_inherited)
        break;
      else if (c->portrait_requested)
        {
          hd_transition_rotate_screen (wm, TRUE);
          break;
        }
    }

  g_ptr_array_free (stack, TRUE);
  g_object_unref(gconf_client);
}

static void
hd_comp_mgr_register_client (MBWMCompMgr           * mgr,
			     MBWindowManagerClient * c,
                             Bool                    activate)
{
  HdCompMgrPrivate              * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClass              * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));
  MBWindowManager               * wm = mgr->wm;
  unsigned                        was;

  g_debug ("%s, c=%p ctype=%d", __FUNCTION__, c, MB_WM_CLIENT_CLIENT_TYPE (c));
  if (MB_WM_CLIENT_CLIENT_TYPE (c) == MBWMClientTypeDesktop)
    {
      priv->desktop = c;
      mb_wm_client_show (c);
      return;
    }

  if (parent_klass->register_client)
    parent_klass->register_client (mgr, c, activate);

  if (!activate)
    {
      mb_wm_client_show (c);
      return;
    }

  /* Rotate early if we need to.  If lp_forecast() did postpone client
   * activation until the root window is reconfigured, otherwise do it
   * now. */
  was = gdk_screen_get_width (gdk_screen_get_default());
  lp_forecast (wm, c);
  if (was == gdk_screen_get_width (gdk_screen_get_default()))
    mb_wm_activate_client (wm, c);
}


static void
hd_comp_mgr_unregister_client (MBWMCompMgr *mgr, MBWindowManagerClient *c)
{
  ClutterActor                  * actor;
  HdCompMgrPrivate              * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClass              * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));
  MBWMCompMgrClutterClient      * cclient =
    MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);
  HdCompMgrClient               * hclient = HD_COMP_MGR_CLIENT (c->cm_client);

  g_debug ("%s, c=%p ctype=%d", __FUNCTION__, c, MB_WM_CLIENT_CLIENT_TYPE (c));
  actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);

  /* Check if it's the last window for the app. */
  if (hclient->priv->app)
    {
      HdRunningApp *app = hclient->priv->app;
      guint windows = (guint)g_hash_table_lookup (priv->shown_apps,
                                                  (gpointer)app);
      if (--windows == 0)
        {
          hd_app_mgr_app_closed (app);
          g_hash_table_remove (priv->shown_apps, (gpointer)app);
        }
      else
        {
          g_hash_table_insert (priv->shown_apps,
                               (gpointer)app,
                               (gpointer)windows);
        }
    }

  /*
   * If the actor is an application, remove it also to the switcher
   */
  if (hclient->priv->app &&
      hd_running_app_is_hibernating (hclient->priv->app) &&
      !g_hash_table_lookup (priv->hibernating_apps,
                            (gpointer) hclient->priv->hibernation_key))
    {
      /*
       * We want to hold onto the CM client object, so we can continue using
       * the actor.
       */
      mb_wm_comp_mgr_clutter_client_set_flags (cclient,
                                MBWMCompMgrClutterClientDontUpdate);
      mb_wm_object_ref (MB_WM_OBJECT (cclient));

      g_hash_table_insert (priv->hibernating_apps,
			   (gpointer) hclient->priv->hibernation_key,
			   hclient);

      hd_switcher_hibernate_window_actor (priv->switcher_group,
					  actor);
    }
  else if (MB_WM_CLIENT_CLIENT_TYPE (c) == MBWMClientTypeApp)
    {
      HdApp *app = HD_APP(c);
      MBWMCompMgrClutterClient * prev = NULL;

      if (actor)
        {
	      gboolean topmost;

	      if (app->stack_index < 0 /* non-stackable */
		  /* leader without secondarys: */
	          || (app->leader == app && !app->leader->followers) ||
	          /* or a secondary window on top of the stack: */
	          (app->leader != NULL &&
	           app->leader->followers &&
                   app == g_list_last (app->leader->followers)->data))
	        topmost = 1;
	      else
	        topmost = 0;

              /* if we are secondary, there must be leader and probably
	       * even followers */
              if (app->stack_index > 0 && app->leader != app)
                {
                  g_assert(app->leader);
                  g_debug ("%s: %p is STACKABLE SECONDARY", __func__, app);
                  /* show the topmost follower and replace switcher actor
		   * for the stackable */

		  /* remove this window from the followers list */
		  app->leader->followers
		  	= g_list_remove (app->leader->followers, app);

		  if (app->leader->followers)
		    prev = MB_WM_COMP_MGR_CLUTTER_CLIENT (
			     MB_WM_CLIENT (
		               g_list_last (app->leader->followers)->data)
			                     ->cm_client);
		  else
		    prev = MB_WM_COMP_MGR_CLUTTER_CLIENT (
		             MB_WM_CLIENT (app->leader)->cm_client);

		  if (topmost) /* if we were on top, update the switcher */
		  {
                    ClutterActor *pactor;
                    pactor = mb_wm_comp_mgr_clutter_client_get_actor (prev);
                    if (pactor)
                    {
                      clutter_actor_show (pactor);
		      g_debug ("%s: REPLACE ACTOR %p WITH %p", __func__,
                               actor, pactor);
                      hd_switcher_replace_window_actor (priv->switcher_group,
                                                        actor, pactor);
                    }
                    else
                      g_warning ("%s: leader or next secondary not found",
                                 __func__);
		  }
                }
              else if (!(c->window->ewmh_state &
		         MBWMClientWindowEWMHStateSkipTaskbar) &&
		       (app->stack_index < 0 ||
		       (app->leader == app && !app->followers)))
                {
                  g_debug ("%p: NON-STACKABLE OR FOLLOWERLESS LEADER"
			   " (index %d), REMOVE ACTOR %p",
		           __func__, app->stack_index, actor);
                  /* We are the leader or a non-stackable window,
                   * just remove the actor from the switcher.
                   * NOTE The test above breaks if the client changed
                   * the flag after it's been mapped. */
                  hd_switcher_remove_window_actor (priv->switcher_group,
                                                   actor, cclient);

                  if (c->window->xwindow == hd_wm_current_app_is (NULL, 0) &&
                       (app->detransitised_from == None ||
                        !mb_wm_managed_client_from_xwindow (mgr->wm, app->detransitised_from)))
		    {
		      /* We are in APP state and foreground application closed.
                       * hdrm is grown-up enough to figure out if it shouldn't
                       * go to tasw for some reason. */
                      hd_render_manager_set_state (HDRM_STATE_TASK_NAV);
		    }
                }
	      else if (app->leader == app && app->followers)
	        {
                  GList *l;
		  HdApp *new_leader;
                  g_debug ("%s: STACKABLE LEADER %p (index %d) WITH CHILDREN",
			   __func__, app, app->stack_index);

                  prev = MB_WM_COMP_MGR_CLUTTER_CLIENT (
			   hd_app_get_prev_group_member(app)->cm_client);
		  new_leader = HD_APP (app->followers->data);
                  for (l = app->followers; l; l = l->next)
                  {
		    /* bottommost secondary is the new leader */
                    HD_APP (l->data)->leader = new_leader;
		  }
                  /* set the new leader's followers list */
                  new_leader->followers = g_list_remove (app->followers,
                                                         new_leader);
		  /* disconnect the app */
                  app->followers = NULL; /* list is now in new_leader */
                  app->leader = NULL;
                  app->stack_index = -1;
		}
	      else /* e.g. non-stackable with
                      MBWMClientWindowEWMHStateSkipTaskbar */
                {
                  MBWindowManagerClient *current_client =
                          hd_comp_mgr_determine_current_app ();

                  if (STATE_IS_APP (hd_render_manager_get_state ()) &&
                      MB_WM_CLIENT_CLIENT_TYPE (current_client) &
                      MBWMClientTypeDesktop)
                    hd_render_manager_set_state (HDRM_STATE_TASK_NAV);
                }
          g_object_set_data (G_OBJECT (actor), "HD-ApplicationId", NULL);
        }
    }
  else if (MB_WM_CLIENT_CLIENT_TYPE (c) == HdWmClientTypeStatusArea)
    {
      hd_home_remove_status_area (HD_HOME (priv->home), actor);
      priv->status_area_client = NULL;
    }
  else if (MB_WM_CLIENT_CLIENT_TYPE (c) == HdWmClientTypeStatusMenu)
    {
      hd_home_remove_status_menu (HD_HOME (priv->home), actor);
      priv->status_menu_client = NULL;
    }
  else if (MB_WM_CLIENT_CLIENT_TYPE (c) == HdWmClientTypeHomeApplet)
    {
      ClutterActor *applet = mb_wm_comp_mgr_clutter_client_get_actor (cclient);

      /* Unregister applet from Home */
      if (applet)
        hd_home_unregister_applet (HD_HOME (priv->home),
                                   applet);
    }

  if (priv->current_hclient == hclient)
    priv->current_hclient = NULL;

  /*
   * We have the following situation to prevent: a client is forecast
   * as portrait-wanting, and we started to rotate but we're not in a
   * STATE_IS_PORTRAIT yet, but the client disappears in the meantime
   * without even being mapped.  If we don't do anything about it we
   * will remain rotated but not in STATE_IS_PORTRAIT().
   */
  if (!STATE_IS_PORTRAIT (hd_render_manager_get_state ())
      && hd_transition_is_rotating_to_portrait ()
      && !mb_wm_client_is_map_confirmed (c)
      && c->window->portrait_requested > 0)
    hd_transition_rotate_screen (mgr->wm, FALSE);

  /* Dialogs and Notes (including notifications) have already been dealt
   * with in hd_comp_mgr_effect().  This is because by this time we don't
   * have information about transiency. */

  if (parent_klass->unregister_client)
    parent_klass->unregister_client (mgr, c);
}

/* Returns the client @c is transient for.  Some clients (notably menus)
 * don't have their c->transient_for field set even though they are
 * transient.  Figure it out from the c->window in this case. */
static MBWindowManagerClient *
hd_comp_mgr_get_client_transient_for (MBWindowManagerClient *c)
{
  Window xtransfor;

  if (c->transient_for)
    return c->transient_for;

  xtransfor = c->window->xwin_transient_for;
  return xtransfor && xtransfor != c->window->xwindow
      && xtransfor != c->wmref->root_win->xwindow
    ? mb_wm_managed_client_from_xwindow (c->wmref, xtransfor)
    : NULL;
}

static void
hd_comp_mgr_texture_update_area(HdCompMgr *hmgr,
                                int x, int y, int width, int height,
                                ClutterActor* actor)
{
  ClutterActor *parent;
  HdCompMgrPrivate * priv;
  gboolean blur_update = FALSE;
  ClutterActor *actors_stage;

  if (!actor || !CLUTTER_ACTOR_IS_VISIBLE(actor) || hmgr == 0)
    return;

  if (hd_dbus_display_is_off)
    {
            /*
      g_printerr ("%s: update for actor %p (%d,%d) %dx%d '%s'"
                  " while display is off\n", __func__, actor, x, y,
                  width, height, clutter_actor_get_name (actor));
                  */
      return;
    }

  /* If we are in the blanking period of the rotation transition
   * then we don't want to issue a redraw every time something changes.
   * This function also assumes that it is called because there was damage,
   * and makes sure it prolongs the blanking period a bit.
   */
  if (hd_transition_rotate_ignore_damage())
    return;

  priv = hmgr->priv;

  /* TFP textures are usually bundled into another group, and it is
   * this group that sets visibility - so we must check it too */
  parent = clutter_actor_get_parent(actor);
  actors_stage = clutter_actor_get_stage(actor);
  if (!actors_stage)
    /* if it's not on stage, it's not visible */
    return;

  while (parent && parent != actors_stage)
    {
      if (!CLUTTER_ACTOR_IS_VISIBLE(parent))
        return;
      /* if we're a child of a blur group, tell it that it has changed */
      if (TIDY_IS_BLUR_GROUP(parent))
        {
          /* we don't update blur on every change of
           * an application now as it causes a flicker, so
           * instead we just hint that next time we become
           * unblurred, we need to recalculate. */
          tidy_blur_group_hint_source_changed(parent);
          /* ONLY set blur_update if the image is buffered ->
           * we are actually blurred */
          if (tidy_blur_group_source_buffered(parent))
            blur_update = TRUE;
        }
      parent = clutter_actor_get_parent(parent);
    }

  /* We no longer display changes that occur on blurred windows, so if
   * this damage was actually on a blurred window, forget about it. */
  if (blur_update)
    return;

  /* Update the screen. This function checks for scaling/visibility and
   * chooses the area to update accordingly */
  {
    ClutterGeometry area = {x,y,width, height};
    hd_util_partial_redraw_if_possible(actor, &area);
  }
}

/* Hook onto and X11 texture pixmap children of this actor */
static void
hd_comp_mgr_hook_update_area(HdCompMgr *hmgr, ClutterActor *actor)
{
  if (CLUTTER_IS_GROUP(actor))
    {
      ClutterActor *child;
      gint i;

      for (i = 0, child = clutter_group_get_nth_child(CLUTTER_GROUP(actor), 0);
           child;
           child = clutter_group_get_nth_child(CLUTTER_GROUP(actor), ++i))
        {
          if (CLUTTER_X11_IS_TEXTURE_PIXMAP(child))
            {
              g_signal_connect_swapped(
                      G_OBJECT(child), "update-area",
                      G_CALLBACK(hd_comp_mgr_texture_update_area), hmgr);
              clutter_actor_set_allow_redraw(child, FALSE);
            }
        }
    }
}

static void
fix_transiency (MBWindowManagerClient *client)
{
  MBWindowManager       *wm = client->wmref;
  MBWMClientWindow      *win = client->window;

  if (win->xwin_transient_for
      && win->xwin_transient_for != win->xwindow
      && win->xwin_transient_for != wm->root_win->xwindow)
    {
      MBWindowManagerClient *trans_parent;

      trans_parent = mb_wm_managed_client_from_xwindow (wm,
                      win->xwin_transient_for);

      if (trans_parent)
        {
          g_debug("%s: setting %lx transient to %lx\n", __FUNCTION__,
                 win->xwindow, win->xwin_transient_for);
          mb_wm_client_add_transient (trans_parent, client);
        }

      /* this change can affect stacking order */
      mb_wm_client_stacking_mark_dirty (client);
    }
  else
    g_debug("%s: DO NOTHING %lx is transient to %lx\n", __FUNCTION__,
                 win->xwindow, win->xwin_transient_for);
}

/* set composite overlay shape according to our state */
void hd_comp_mgr_reset_overlay_shape (HdCompMgr *hmgr)
{
  static gboolean    fs_comp = TRUE;
  gboolean           want_fs_comp;
  MBWMCompMgr       *mgr = MB_WM_COMP_MGR (hmgr);
  MBWindowManager   *wm;
  Window             clutter_window;
  ClutterActor      *stage;

  want_fs_comp = !STATE_IS_NON_COMP (hd_render_manager_get_state ());
  if (want_fs_comp == fs_comp)
    return;

  wm = mgr->wm;
  stage = clutter_stage_get_default ();
  clutter_window = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));

  if (want_fs_comp) {
    /* Recreate the overlay window and move stuff back */
    mgr->disabled = True;
    XSetWindowBackgroundPixmap(wm->xdpy, clutter_window, None);
    hd_comp_mgr_turn_on(mgr);
    XMoveWindow(wm->xdpy, clutter_window, 0, 0);
    XSetWindowBackground(wm->xdpy, clutter_window,
                         BlackPixel(wm->xdpy, DefaultScreen(wm->xdpy)));

    /* g_printerr ("%s: COMPOSITING: FULL SCREEN\n", __FUNCTION__); */
    clutter_stage_set_shaped_mode (stage, 0);
  } else {
    /* g_printerr ("%s: COMPOSITING: ZERO REGION\n", __FUNCTION__); */
    /* Change the stage background to None before we do anything, to avoid
     * ugly black flashes. */
    XSetWindowBackgroundPixmap(wm->xdpy, clutter_window, None);
    /* tell Clutter not to draw on the window */
    clutter_stage_set_shaped_mode (stage, 1);
    /* Reparent X back to the root window - and move it offscreen, then
     * reset its background to black. */
    XReparentWindow (wm->xdpy, clutter_window, wm->root_win->xwindow, 0, 0);
    XMoveWindow(wm->xdpy, clutter_window, 0, -800);
    XSetWindowBackground(wm->xdpy, clutter_window,
                         BlackPixel(wm->xdpy, DefaultScreen(wm->xdpy)));
    /* Kill the overlay window */
    XCompositeReleaseOverlayWindow (wm->xdpy, wm->root_win->xwindow);
    mb_wm_comp_mgr_clutter_set_overlay_window(
        MB_WM_COMP_MGR_CLUTTER(hmgr), None);
  }

  fs_comp = want_fs_comp;
}

/* 'force' allows unredirecting non-fullscreen applications, it is used
 * for the key shortcut (handy when checking for compositing glitches) */
void
hd_comp_mgr_unredirect_topmost_client (MBWindowManager *wm, gboolean force)
{
  MBWindowManagerClient *c;

  for (c = wm->stack_top; c && c != wm->desktop; c = c->stacked_below)
    {
      if (mb_wm_client_is_unmap_confirmed (c))
        /* client is already unmapped but remains in our stack structure */
        continue;

      /* unredirect and do not track damage of the topmost
       * application window that is fullscreen */
      if (c->cm_client && c->window->net_type ==
            wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_NORMAL] &&
          (c->window->ewmh_state & MBWMClientWindowEWMHStateFullscreen
           || force))
        {
          if (!mb_wm_comp_mgr_clutter_client_is_unredirected (c->cm_client))
            {
              mb_wm_comp_mgr_clutter_client_track_damage (
                MB_WM_COMP_MGR_CLUTTER_CLIENT(c->cm_client), False);
              mb_wm_comp_mgr_clutter_set_client_redirection (c->cm_client,
                                                             FALSE);
            }
          break;
        }
    }
}

static void
hd_comp_mgr_unredirect_client (MBWindowManagerClient *c)
{
  if (c->cm_client &&
      !mb_wm_comp_mgr_clutter_client_is_unredirected (c->cm_client))
    {
      mb_wm_comp_mgr_clutter_client_track_damage (
        MB_WM_COMP_MGR_CLUTTER_CLIENT(c->cm_client), False);
      mb_wm_comp_mgr_clutter_set_client_redirection (c->cm_client,
                                                     FALSE);
    }
  else
    g_printerr("%s: ain't no doing no unredirection\n", __func__);
}

/* returns TRUE if the client wants non-composited mode */
static gboolean
hd_comp_mgr_is_non_composited (MBWindowManagerClient *client,
                               gboolean force_re_read)
{
  MBWindowManager *wm;
  HdCompMgr *hmgr;
  MBWMClientWindow *win;
  Atom atom, actual_type;
  int format;
  unsigned long items, left;
  unsigned char *prop;
  Status ret;
  int value = 1;

  if (!HD_IS_APP (client))
    return FALSE;

  wm = client->wmref;

  if (!HD_APP (client)->non_composited_read)
    {
      /* check if the window is blacklisted */
      XClassHint class_hint;
      memset (&class_hint, 0, sizeof (XClassHint));
      mb_wm_util_async_trap_x_errors (wm->xdpy);
      ret = XGetClassHint (wm->xdpy, client->window->xwindow, &class_hint);
      mb_wm_util_async_untrap_x_errors ();

      if (ret && class_hint.res_class)
        {
          if (!strcmp (class_hint.res_class, "Chessui") ||
              !strcmp (class_hint.res_class, "Mahjong"))
            {
              /* g_printerr ("%s: mahjong or chess\n", __func__); */
              HD_APP (client)->non_composited_read = True;
              HD_APP (client)->non_composited = False;
              HD_APP (client)->force_composited = True;
            }
        }

      if (class_hint.res_class)
        XFree (class_hint.res_class);

      if (class_hint.res_name)
        XFree (class_hint.res_name);
    }

  if (HD_APP (client)->force_composited)
    return FALSE;

  if (HD_APP (client)->non_composited_read && !force_re_read)
    {
      if ((client->window->ewmh_state & MBWMClientWindowEWMHStateFullscreen)
          && HD_APP (client)->non_composited)
        return TRUE;
      else
        return FALSE;
    }

  hmgr = HD_COMP_MGR (wm->comp_mgr);
  win = client->window;
  prop = NULL;

  atom = hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_NON_COMPOSITED_WINDOW);

  mb_wm_util_async_trap_x_errors (wm->xdpy);
  ret = XGetWindowProperty (wm->xdpy, win->xwindow,
                            atom, 0, 1, False,
                            XA_INTEGER, &actual_type, &format,
                            &items, &left, &prop);
  mb_wm_util_async_untrap_x_errors ();
  if (ret != Success)
    return FALSE;

  HD_APP (client)->non_composited_read = True;

  if (prop)
    {
      value = (int)*prop;
      XFree (prop);
    }

  if (actual_type == XA_INTEGER)
    {
      if (value)
        {
          HD_APP (client)->non_composited = True;
          if (client->window->ewmh_state & MBWMClientWindowEWMHStateFullscreen)
            return TRUE;
        }
      else
        HD_APP (client)->non_composited = False;
    }
  else
   {
     /* non-stackable "prefers" non-compositing in fullscreen mode */
     if (HD_APP (client)->stack_index < 0)
       {
         HD_APP (client)->non_composited = True;
         if (client->window->ewmh_state & MBWMClientWindowEWMHStateFullscreen)
           return TRUE;
       }
     else
       HD_APP (client)->non_composited = False;
   }
  return FALSE;
}

/* returns HdApp of client that was replaced (because the stack_index
 * was the same) in 'replaced', or NULL.
 * 'add_to_tn' returns a client if that client is a new window on a stack, or
 * the first window in a stack.
 *
 * NOTICE THIS: 'replacing' here does NOT mean the same as replacing actors
 * in the switcher! 'replaced' just means that one stack member was replaced
 * with another. */
static void
hd_comp_mgr_handle_stackable (MBWindowManagerClient *client,
		              HdApp **replaced, HdApp **add_to_tn)
{
  MBWindowManager       *wm = client->wmref;
  MBWMClientWindow      *win = client->window;
  HdCompMgr             *hmgr = HD_COMP_MGR (wm->comp_mgr);
  HdApp                 *app = HD_APP (client);
  Window                 win_group;
  unsigned char         *prop = NULL;
  unsigned long          items, left;
  int			 format;
  Atom                   stack_atom, actual_type;
  Status                ret;

  app->stack_index = -1;  /* initially a non-stackable */
  *replaced = *add_to_tn = NULL;

  fix_transiency (client);
  stack_atom = hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_STACKABLE_WINDOW);

  mb_wm_util_async_trap_x_errors (wm->xdpy);
  /*
   * XGetWindowProperty() is a synchronization point so any errors reported
   * through the X error handler is most probably due to a bug earlier.
   * Let's hide the crap under the carpet and ignore it.
   *
   * It doesn't make much a difference because the errors would be trapped
   * and ignored anyway because of various bugs in matchbox (errors being
   * trapped twice).  Let's pretend they're not.
   */
  ret = XGetWindowProperty (wm->xdpy, win->xwindow,
                            stack_atom, 0, 1, False,
                            XA_INTEGER, &actual_type, &format,
                            &items, &left, &prop);
  mb_wm_util_async_untrap_x_errors ();
  if (ret != Success)
    /* Now, the call really failed. */
    return;

  if (actual_type == XA_INTEGER)
    {
      MBWindowManagerClient *c_tmp;
      HdApp *old_leader = NULL;
      HdApp *last_follower = NULL;

      win_group = win->xwin_group;
      app->stack_index = (int)*prop;
      g_debug ("%s: STACK INDEX %d\n", __func__, app->stack_index);

      mb_wm_stack_enumerate (wm, c_tmp)
        if (c_tmp != client &&
            MB_WM_CLIENT_CLIENT_TYPE (c_tmp) == MBWMClientTypeApp &&
            HD_APP (c_tmp)->stack_index >= 0 /* == stackable window */ &&
            c_tmp->window->xwin_group == win_group)
          {
	    /*
	     * It is possible that the bottommost window is mapped but we did
	     * not get a map notify yet. In this case the leader can be found
	     * higher (see NB#121902).
	     */
            if (mb_wm_client_is_map_confirmed(c_tmp))
              {
                old_leader = HD_APP (c_tmp)->leader;
                break;
              }
          }

      if (old_leader && old_leader->followers)
        last_follower = HD_APP (g_list_last (old_leader->followers)->data);

      if (old_leader && app->stack_index <= old_leader->stack_index)
        {
          GList *l;

          if (app == old_leader)
            /* ... like killing app->followers */
            g_critical ("%s: app == old_leader == %p, "
                        "i'm about to do silly things",
                        __FUNCTION__, app);

          app->leader = app;
          for (l = old_leader->followers; l; l = l->next)
          {
            HD_APP (l->data)->leader = app;
          }

          if (old_leader->stack_index == app->stack_index)
          {
            /* drop the old leader from the stack if we replace it */
            g_debug ("%s: DROPPING OLD LEADER %p OUT OF THE STACK\n", __func__,
		     app);
            app->followers = old_leader->followers;
            old_leader->followers = NULL;
            old_leader->leader = NULL;
            old_leader->stack_index = -1; /* mark it non-stackable */
	    *replaced = old_leader;
          }
          else
          {
            /* the new leader is now a follower */
            g_debug ("%s: OLD LEADER %p IS NOW A FOLLOWER\n", __func__, app);
            app->followers = g_list_prepend (old_leader->followers,
                                             old_leader);
            old_leader->followers = app->followers;
            old_leader->leader = app;
            fix_transiency ((MBWindowManagerClient*)old_leader);

            /* This forces the decors to be redone, taking into account the
             * stack index. */
            mb_wm_client_theme_change ((MBWindowManagerClient*)old_leader);
            mb_wm_client_theme_change ((MBWindowManagerClient*)app);
	    *replaced = old_leader;
	    if (HD_APP(*replaced)->followers)
	        *replaced = g_list_last (HD_APP(*replaced)->followers)->data;
          }
        }
      else if (app->stack_index > 0 && old_leader &&
	  (!last_follower || last_follower->stack_index < app->stack_index))
        {
          /* no replacement possible in this branch
           * (replacing of the leader is handled in the first branch) */
          g_debug ("%s: %p is NEW SECONDARY OF THE STACK\n", __FUNCTION__, app);
          app->leader = old_leader;

          app->leader->followers = g_list_append (old_leader->followers,
                                                  client);
        }
      else if (old_leader && app->stack_index > old_leader->stack_index)
        {
          GList *flink;
          HdApp *f = NULL;

          app->leader = old_leader;
          /* find the follower that the new window replaces or follows */
          for (flink = old_leader->followers; flink; flink = flink->next)
          {
            f = flink->data;
            if (f->stack_index >= app->stack_index)
	    {
	      if (flink->prev &&
	          HD_APP (flink->prev->data)->stack_index == app->stack_index)
	      {
	        f = flink->prev->data;
		flink = flink->prev;
	      }
              break;
	    }
          }
	  if (!f && old_leader->followers &&
	      HD_APP (old_leader->followers->data)->stack_index
	                                             == app->stack_index)
	  {
	    f = old_leader->followers->data;
	    flink = old_leader->followers;
	  }

          if (!f)
          {
            g_debug ("%s: %p is FIRST FOLLOWER OF THE STACK\n", __func__, app);
            old_leader->followers = g_list_append (old_leader->followers, app);
	    *add_to_tn = app;
          }
          else if (f->stack_index == app->stack_index)
          {
	    if (f != app)
	    {
              g_debug ("%s: %p REPLACES A FOLLOWER OF THE STACK\n",
		       __func__, app);
              old_leader->followers
                = g_list_insert_before (old_leader->followers, flink, app);
              old_leader->followers
                = g_list_remove_link (old_leader->followers, flink);
              g_list_free (flink);
              /* drop the replaced follower from the stack */
              f->leader = NULL;
              f->stack_index = -1; /* mark it non-stackable */
	    }
	    else
	      g_debug ("%s: %p is the SAME CLIENT\n", __FUNCTION__, app);
	    *replaced = f;
          }
          else if (f->stack_index > app->stack_index)
          {
            GList *l;
            g_debug ("%s: %p PRECEEDS (index %d) A FOLLOWER (with index %d)"
	             " OF THE STACK\n", __func__, app, app->stack_index,
		     f->stack_index);
            old_leader->followers
                = g_list_insert_before (old_leader->followers, flink, app);

            /* fix up transiency of the window on top of this one */
            l = g_list_find (old_leader->followers, app);
            if (l && l->next)
            {
              mb_wm_client_detransitise (MB_WM_CLIENT (l->next->data));
              mb_wm_client_add_transient (MB_WM_CLIENT (app),
                                          MB_WM_CLIENT (l->next->data));
            }

            mb_wm_client_theme_change ((MBWindowManagerClient*)app);
          }
	  else  /* f->stack_index < app->stack_index */
	  {
            if (flink && flink->next)
	    {
              GList *l;
              g_debug ("%s: %p PRECEEDS (index %d) A FOLLOWER (with index %d)"
	             " OF THE STACK\n", __func__, app, app->stack_index,
		     HD_APP (flink->next->data)->stack_index);
              old_leader->followers
                 = g_list_insert_before (old_leader->followers, flink->next,
				         app);

              /* fix up transiency of the window on top of this one */
              l = g_list_find (old_leader->followers, app);
              if (l && l->next)
              {
                mb_wm_client_detransitise (MB_WM_CLIENT (l->next->data));
                mb_wm_client_add_transient (MB_WM_CLIENT (app),
                                            MB_WM_CLIENT (l->next->data));
              }

              mb_wm_client_theme_change ((MBWindowManagerClient*)app);
	    }
	    else
	    {
              g_debug ("%s: %p FOLLOWS LAST FOLLOWER OF THE STACK\n", __func__,
		       app);
              old_leader->followers = g_list_append (old_leader->followers,
                                                     app);
	      *add_to_tn = app;
	    }
	  }
        }
      else  /* we are the first window in the stack */
        {
          g_debug ("%s: %p is FIRST WINDOW OF THE STACK\n",
                      __FUNCTION__, app);
          app->leader = app;
	  *add_to_tn = app;
        }
    }

  if (prop)
    XFree (prop);

  /* all stackables have stack_index >= 0 */
  g_assert (!app->leader || (app->leader && app->stack_index >= 0));
}

static void
create_stampfile (void)
{
  static gboolean done = FALSE;
  int fd;

  if (G_LIKELY (done))
    return;

#ifdef __i386__
  /* Fail silently if STAMP_DIR doesn't exist. */
  close (fd = creat (STAMP_FILE, 0644));
  done = TRUE;
#else /* __arm__ */
  mkdir (STAMP_DIR, 0755);
  if ((fd = creat (STAMP_FILE, 0644)) >= 0)
    {
      close (fd);
      done = TRUE;
    }
  else
    g_critical ("couldn't create %s: %m", STAMP_FILE);
#endif /* __arm__ */
}

extern gboolean hd_dbus_tklock_on;

static void
hd_comp_mgr_map_notify (MBWMCompMgr *mgr, MBWindowManagerClient *c)
{
  ClutterActor             * actor;
  HdCompMgrPrivate         * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClutterClient * cclient;
  MBWMCompMgrClass         * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));
  HdCompMgrClient          * hclient;
  HdCompMgrClient          * hclient_h;
  guint                      hkey;
  MBWMClientType             ctype;
  MBWindowManagerClient    * transient_for;

  g_debug ("%s: 0x%lx '%s'\n", __FUNCTION__,
           c && c->window ? c->window->xwindow : 0,
           mb_wm_client_get_name (c));
  create_stampfile();

  /* Log the time this window was mapped */
  gettimeofday(&priv->last_map_time, NULL);

  /* if *anything* is mapped, remove our full-screen input blocker */
  hd_render_manager_remove_input_blocker();

  /* We want to make sure the rotation transition is notified of a map event.
   * It may have happened during blanking, and if so we want to increase the
   * blanking time. */
  hd_transition_rotate_ignore_damage();

  /*g_debug ("%s, c=%p ctype=%d", __FUNCTION__, c,
             MB_WM_CLIENT_CLIENT_TYPE (c));*/
  if (MB_WM_CLIENT_CLIENT_TYPE (c) == MBWMClientTypeDesktop)
    return;

  if (c->window->live_background)
    {
      /*g_printerr ("%s: client '%s' is live background\n", __func__,
                  mb_wm_client_get_name (c)); */
      parent_klass->map_notify (mgr, c);

      hd_home_set_live_background (HD_HOME (priv->home), c);
      mb_wm_comp_mgr_clutter_client_set_flags (
                          MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client),
                          MBWMCompMgrClutterClientDontPosition);

      cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);
      actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);
      hd_comp_mgr_hook_update_area (HD_COMP_MGR (mgr), actor);
      hd_render_manager_restack ();

      if (STATE_IS_LOADING(hd_render_manager_get_state ()))
        {
          /* make sure to hide the loading screen and go to a sane state */
          hd_launcher_stop_loading_transition ();
          hd_render_manager_set_loading (NULL);
        }
      /* Move to HDRM_STATE_HOME even if we don't have a loading screen,
       * otherwise we hide the launcher and go to a broken state. */
			if(STATE_IS_PORTRAIT (hd_render_manager_get_state () ))
	      hd_render_manager_set_state (HDRM_STATE_HOME_PORTRAIT);
			else
				hd_render_manager_set_state (HDRM_STATE_HOME);
      hd_launcher_hide ();
      return;
    }

  /* discard notification previews if necessary */
  if (HD_IS_INCOMING_EVENT_PREVIEW_NOTE(c))
    {
      if (priv->do_not_disturb_flag
          || STATE_DISCARD_PREVIEW_NOTE (hd_render_manager_get_state())
          || hd_dbus_tklock_on)
        {
          g_debug ("%s. Discard notification", __FUNCTION__);
          mb_wm_client_hide (c);
          mb_wm_client_deliver_delete (c);
          return;
        }
    }

  transient_for = mb_wm_client_get_transient_for (c);

  /* Discard notification banners if do not disturb flag is set
   * and the dnd override flag is not set on the information banner
   * and the client is not topmost
   */
  if (priv->do_not_disturb_flag && HD_IS_BANNER_NOTE (c) &&
      (!transient_for ||
       mb_wm_client_get_next_focused_app (transient_for) != NULL))
    {
      guint32 *value;
      Atom dnd_override = hd_comp_mgr_get_atom (HD_COMP_MGR (mgr),
						HD_ATOM_HILDON_DO_NOT_DISTURB_OVERRIDE);

      value = hd_util_get_win_prop_data_and_validate (c->wmref->xdpy,
                c->window->xwindow, dnd_override, XA_INTEGER, 32, 1, NULL);

      if (!value || *value != 1)
        {
          g_debug ("%s. Discard information banner (Do not Disturb flag set)",
                   __FUNCTION__);
          mb_wm_client_hide (c);
          mb_wm_client_deliver_delete (c);

         if (value)
            XFree (value);
          return;
        }

      if (value)
        XFree (value);
    }

  ctype = MB_WM_CLIENT_CLIENT_TYPE (c);

  /*
   * #MBWMCompMgrClutterClient already has an actor, now it's time
   * for #MBWMCompMgrClutter to create its texture and bind it to
   * the window's pixmap.  This is not necessary for notifications
   * whose windows we don't use for anything at all and not creating
   * the texture saves precious miliseconds.
   */
  if (!HD_IS_INCOMING_EVENT_NOTE(c))
    {
      /* start compositing if the client prefers that */
      if (hd_comp_mgr_client_prefers_compositing (c))
        {
          /* TODO: should check that this client really is above the
           * non-composited client */
          /* possibly switch away from non-composited mode to enable creating
           * the texture */
          hd_render_manager_switch_to_composited_state ();
        }

      parent_klass->map_notify (mgr, c);
    }

  /* Now the actor has been created and added to the desktop, make sure we
   * call hdrm_restack to put it in the correct group in hd-render-manager */
  hd_render_manager_restack();
  hd_comp_mgr_portrait_or_not_portrait(mgr, c);

  /* Hide the Edit button if it is currently shown */
  if (priv->home)
    hd_home_hide_edit_button (HD_HOME (priv->home));

  /*
   * If the actor is an application, add it also to the switcher
   * If it is Home applet, add it to the home
   */

  cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);
  hclient = HD_COMP_MGR_CLIENT (cclient);
  actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);

  if (hclient->priv->app)
    g_object_set_data (G_OBJECT (actor),
           "HD-ApplicationId",
           (gchar *)hd_running_app_get_id (hclient->priv->app));

  hd_comp_mgr_hook_update_area(HD_COMP_MGR (mgr), actor);

  /* if we are in home_edit mode and we have stuff that will get in
   * our way and spoil our grab, move to home_edit_dlg so we can
   * look the same but remove our grab. */
  if ((hd_render_manager_get_state()==HDRM_STATE_HOME_EDIT || hd_render_manager_get_state()==HDRM_STATE_HOME_EDIT_PORTRAIT) &&
      (ctype & (MBWMClientTypeDialog |
                HdWmClientTypeAppMenu |
                HdWmClientTypeStatusMenu)))
    {
			if(STATE_IS_PORTRAIT(hd_render_manager_get_state()))
	      hd_render_manager_set_state(HDRM_STATE_HOME_EDIT_DLG_PORTRAIT);
			else
				hd_render_manager_set_state(HDRM_STATE_HOME_EDIT_DLG);
    }

  /*
   * Leave switcher/launcher for home if we're mapping a system-modal
   * dialog, information note or confirmation note.  We need to leave now,
   * before we disable hdrm reactivity, because changing state after that
   * restores that.
   */
  if (!transient_for)
    if (ctype == MBWMClientTypeDialog
        || HD_IS_INFO_NOTE (c) || HD_IS_CONFIRMATION_NOTE (c))
      if (STATE_ONE_OF(hd_render_manager_get_state(),
                       HDRM_STATE_LAUNCHER | HDRM_STATE_LAUNCHER_PORTRAIT |
		       HDRM_STATE_TASK_NAV | HDRM_STATE_TASK_NAV_PORTRAIT))
        {
					if(STATE_IS_PORTRAIT (hd_render_manager_get_state () ))
	      		hd_render_manager_set_state (HDRM_STATE_HOME_PORTRAIT);
					else
						hd_render_manager_set_state (HDRM_STATE_HOME);
          if (hd_comp_mgr_client_is_maximized(c->window->geometry))
            /*
             * So we are in switcher view and want to get to home view
             * to show a dialog or something.  hdrm_set_visibilities()
             * is smart enough to leave alone dialogs (normally they
             * are in app_top) but the case with maximized clients is
             * different and they are subject to the regular checking.
             * This checking would fail because the transition from
             * switcher to home view is, well, a transition and it
             * takes time, during which set_visibilities() refuses
             * to show windows, even dialogs, if they are in home_blur.
             * Soooo, let's stop the transition and problem solved.
             */
            hd_render_manager_stop_transition();
        }

  /* Hide status menu if any window except an applet is mapped */
  if (priv->status_menu_client &&
      ctype != HdWmClientTypeHomeApplet &&
      ctype != MBWMClientTypeOverride &&
      !HD_IS_BANNER_NOTE(c))
    mb_wm_client_deliver_delete (priv->status_menu_client);

  if (ctype == HdWmClientTypeHomeApplet)
    {
      HdHomeApplet * applet  = HD_HOME_APPLET (c);
      char         * applet_id = applet->applet_id;

      if (priv->home && strcmp (OPERATOR_APPLET_ID, applet_id) != 0)
        {
          /* Normal applet */
          g_object_set_data_full (G_OBJECT (actor), "HD-applet-id",
                                  g_strdup (applet_id),
                                  (GDestroyNotify) g_free);
          hd_home_add_applet (HD_HOME (priv->home), actor);
        }
      else if (priv->home)
        {
          /* Special operator applet */
          hd_home_set_operator_applet (HD_HOME (priv->home), actor);
        }
      return;
    }
  else if (ctype == HdWmClientTypeStatusArea)
    {
      hd_home_add_status_area (HD_HOME (priv->home), actor);
      priv->status_area_client = c;
      return;
    }
  else if (ctype == HdWmClientTypeStatusMenu)
    { /* Either status menu OR power menu. */
      if (STATE_ONE_OF(hd_render_manager_get_state(),
                       HDRM_STATE_LAUNCHER | HDRM_STATE_LAUNCHER_PORTRAIT |
		       HDRM_STATE_TASK_NAV | HDRM_STATE_TASK_NAV_PORTRAIT))
			{
				if(STATE_IS_PORTRAIT (hd_render_manager_get_state () ))
	      	hd_render_manager_set_state (HDRM_STATE_HOME_PORTRAIT);
				else
					hd_render_manager_set_state (HDRM_STATE_HOME);
			}
      hd_home_add_status_menu (HD_HOME (priv->home), actor);
      priv->status_menu_client = c;
      return;
    }
  else if (ctype == HdWmClientTypeAnimationActor)
    {
      return;
    }
  else if (ctype == HdWmClientTypeAppMenu)
    {
      /* This is mainly for the power key menu, but we must not allow
       * menus is general when not in APP state because they are not
       * added to the switcher.  This can be considered a shortcoming. */
      if (STATE_NEED_WHOLE_SCREEN_INPUT(hd_render_manager_get_state()))
        {
          if (STATE_IS_PORTRAIT(hd_render_manager_get_state()))
            hd_render_manager_set_state(HDRM_STATE_HOME_PORTRAIT);
          else
            hd_render_manager_set_state(HDRM_STATE_HOME);
        }
      return;
    }
  else if (ctype == MBWMClientTypeNote)
    {
      gboolean unblank = FALSE;
      if (HD_IS_BANNER_NOTE (c))
        {
          if (transient_for)
            {
              /* don't put the banner to front group if it's transient to
               * a background window */
              MBWindowManagerClient *tmp;
              for (tmp = hd_comp_mgr_determine_current_app (); tmp;
                   tmp = tmp->stacked_above)
                if (tmp == transient_for)
                  {
                    hd_render_manager_add_to_front_group (actor);
                    unblank = TRUE;
                    break;
                  }
            }
          else
            {
              hd_render_manager_add_to_front_group (actor);
              unblank = TRUE;
            }
        }
      else if (HD_IS_INCOMING_EVENT_PREVIEW_NOTE (c))
        hd_render_manager_add_to_front_group(actor);

      if (HD_IS_INCOMING_EVENT_PREVIEW_NOTE (c))
        { /* let's be us who decide when the previews can be clicked */
          clutter_actor_set_reactive (actor, TRUE);
          g_signal_connect_swapped (actor, "button-press-event",
                                    G_CALLBACK (hd_note_clicked), c);
        }

      if (HD_IS_INCOMING_EVENT_NOTE (c))
        hd_switcher_add_notification (priv->switcher_group, HD_NOTE (c));

      if (!HD_IS_INCOMING_EVENT_NOTE (c)
          && !HD_IS_INCOMING_EVENT_PREVIEW_NOTE (c)
          && transient_for)
        hd_switcher_add_dialog (priv->switcher_group, c, actor);

#ifdef HAVE_DSME
/* TODO Convert MCE to DSME */
      if (unblank && priv->mce_proxy)
        { /* Turn display backlight on for banner notes. */
          g_debug ("%s. Call %s", __FUNCTION__, MCE_DISPLAY_ON_REQ);
          dbus_g_proxy_call_no_reply (priv->mce_proxy, MCE_DISPLAY_ON_REQ,
                                      G_TYPE_INVALID, G_TYPE_INVALID);
        }
#endif
      return;
    }
  else if (ctype == MBWMClientTypeDialog)
    {
      if (transient_for)
        {
          hd_switcher_add_dialog (priv->switcher_group, c, actor);
        }
      return;
    }
  else if (c->window->net_type ==
	   c->wmref->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_POPUP_MENU])
    {
      MBWindowManagerClient *transfor;

      if ((transfor = hd_comp_mgr_get_client_transient_for (c)) != NULL)
        {
          hd_switcher_add_dialog_explicit (HD_SWITCHER (priv->switcher_group),
                                           c, actor, transfor);
        }
      return;
    }
  else if (ctype == MBWMClientTypeOverride
           && STATE_IS_NON_COMP (hd_render_manager_get_state ()))
    {
      /* we need to unredirect this to screen */
      hd_comp_mgr_unredirect_client (c);
      return;
    }
  else if (ctype != MBWMClientTypeApp)
    return;

  hkey = hclient->priv->hibernation_key;

  hclient_h = g_hash_table_lookup (priv->hibernating_apps, (gpointer)hkey);

  if (hclient_h)
    {
      MBWMCompMgrClutterClient *cclient_h;
      ClutterActor *actor_h;
      cclient_h = MB_WM_COMP_MGR_CLUTTER_CLIENT (hclient_h);
      actor_h = mb_wm_comp_mgr_clutter_client_get_actor (cclient_h);
      hd_switcher_replace_window_actor (priv->switcher_group,
                                        actor_h, actor);
      mb_wm_object_unref (MB_WM_OBJECT (hclient_h));
      g_hash_table_remove (priv->hibernating_apps, (gpointer)hkey);
    }

  int topmost;
  HdApp *app = HD_APP (c), *to_replace, *add_to_tn;
  gboolean actor_handled = FALSE;

  hd_comp_mgr_handle_stackable (c, &to_replace, &add_to_tn);

  /* have to re-read the property here because stack_index
   * is now initialised */
  if (hd_comp_mgr_is_non_composited (c, TRUE))
    {
      MBWindowManagerClient *tmp;
      gboolean found = FALSE;
      /* first check that this client is not below some client that needs
       * compositing */
      for (tmp = c->stacked_above; tmp; tmp = tmp->stacked_above)
        if (mb_wm_client_is_map_confirmed (tmp) &&
            hd_comp_mgr_client_prefers_compositing (tmp))
          {
            found = TRUE;
            break;
          }

      if (!found &&
          hd_render_manager_get_state () != HDRM_STATE_NON_COMPOSITED &&
          !STATE_IS_PORTRAIT (hd_render_manager_get_state ()))
        hd_render_manager_set_state (HDRM_STATE_NON_COMPOSITED);
      else if (!found &&
               hd_render_manager_get_state () != HDRM_STATE_NON_COMP_PORT &&
               STATE_IS_PORTRAIT (hd_render_manager_get_state ()))
        hd_render_manager_set_state (HDRM_STATE_NON_COMP_PORT);
      else if (!found && STATE_IS_NON_COMP (hd_render_manager_get_state ()))
        hd_comp_mgr_unredirect_topmost_client (c->wmref, FALSE);
    }
  else
    hd_render_manager_switch_to_composited_state ();

  if (app->stack_index < 0 /* non-stackable */
      /* leader without followers: */
      || (!app->leader->followers && app->leader == app) ||
      /* or a secondary window on top of the stack: */
      app == g_list_last (app->leader->followers)->data)
    topmost = 1;
  else
    topmost = 0;

  /* handle the restart case when the stack does not have any window
   * in the switcher yet */
  if (app->stack_index >= 0 && !topmost)
    {
      MBWMCompMgrClutterClient *cclient;
      gboolean in_tasknav;

      cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);
      if (hd_task_navigator_has_window (hd_task_navigator,
          mb_wm_comp_mgr_clutter_client_get_actor (cclient)))
        in_tasknav = TRUE;
      else
        in_tasknav = FALSE;

      if (!app->leader->followers && !in_tasknav)
        /* lonely leader */
        topmost = TRUE;
      else if (app->leader->followers && !in_tasknav)
        {
          GList *l;
          gboolean child_found = FALSE;

          for (l = app->leader->followers; l; l = l->next)
            {
              cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (
                             MB_WM_CLIENT (l->data)->cm_client);
              if (hd_task_navigator_has_window (hd_task_navigator,
                  mb_wm_comp_mgr_clutter_client_get_actor (cclient)))
                {
                  child_found = TRUE;
                  break;
                }
            }

          if (!child_found)
            topmost = TRUE;
        }
    }

  if (to_replace &&
		  to_replace->leader == NULL &&
		  to_replace->stack_index == -1)
    {
      ClutterActor *old_actor;

      /*
       * If we replaced an existing follower and made it a non stackable.
       */
      old_actor = mb_wm_comp_mgr_clutter_client_get_actor (
                  MB_WM_COMP_MGR_CLUTTER_CLIENT (
			  MB_WM_CLIENT (to_replace)->cm_client));

      g_debug ("%s: REPLACE %p WITH %p and ADD %p BACK", __func__,
		      old_actor, actor, old_actor);
      hd_switcher_replace_window_actor (priv->switcher_group, old_actor, actor);
      hd_switcher_add_window_actor (priv->switcher_group, old_actor);

      /* and make sure we're in app mode and not transitioning as
       * we'll want to show this new app right away*/
      if (!STATE_IS_APP(hd_render_manager_get_state()))
        hd_render_manager_set_state(HDRM_STATE_APP);
      hd_render_manager_stop_transition();
      /* This forces the decors to be redone, taking into account the
       * stack index. */
      mb_wm_client_theme_change (c);
      mb_wm_client_theme_change (MB_WM_CLIENT(to_replace));
      actor_handled = TRUE;
    }
  else if (to_replace && topmost)
    {
      ClutterActor *old_actor;
      old_actor = mb_wm_comp_mgr_clutter_client_get_actor (
                  MB_WM_COMP_MGR_CLUTTER_CLIENT (
                    MB_WM_CLIENT (to_replace)->cm_client));
      if (old_actor != actor)
      {
        g_debug ("%s: REPLACE ACTOR %p WITH %p", __func__, old_actor,
               actor);
        hd_switcher_replace_window_actor (priv->switcher_group,
                                        old_actor, actor);
        /* and make sure we're in app mode and not transitioning as
         * we'll want to show this new app right away*/
        if (!STATE_IS_APP(hd_render_manager_get_state()))
          hd_render_manager_set_state(HDRM_STATE_APP);
        hd_render_manager_stop_transition();
        /* This forces the decors to be redone, taking into account the
         * stack index. */
        mb_wm_client_theme_change (c);
        actor_handled = TRUE;
      }
    }
  else if (!to_replace && app->stack_index >= 0
           && app->leader != app && topmost)
    {
      ClutterActor *old_actor;
      g_debug ("%s: new follower that is on top\n", __func__);
      if (app->leader->followers)
        {
          /* find the follower to replace in the switcher
           * (note that app is the last on the list) */
          GList *l;

          for (l = app->leader->followers; l->next && l->next->data != app;
               l = l->next) ;

          if (l->data == app)
            old_actor = mb_wm_comp_mgr_clutter_client_get_actor (
		        MB_WM_COMP_MGR_CLUTTER_CLIENT (
			      MB_WM_CLIENT (app->leader)->cm_client));
          else
            old_actor = mb_wm_comp_mgr_clutter_client_get_actor (
		        MB_WM_COMP_MGR_CLUTTER_CLIENT (
			      MB_WM_CLIENT (l->data)->cm_client));
        }
      else
        {
          old_actor = mb_wm_comp_mgr_clutter_client_get_actor (
		        MB_WM_COMP_MGR_CLUTTER_CLIENT (
			      MB_WM_CLIENT (app->leader)->cm_client));
        }
      hd_switcher_replace_window_actor (priv->switcher_group,
                                        old_actor, actor);
      mb_wm_client_theme_change (c);
      actor_handled = TRUE;
    }
  else if (to_replace && to_replace->leader == app)
    {
      ClutterActor *old_actor;

      /*
       * This is the 'old leader become a follower' use case. In this situation
       * the visible actor remains a follower, but we need to do something with
       * the new actor otherwise it will be unhandled by the task switcher and
       * it will be shown in the background.
       */
      g_debug ("%s: ADD ACTOR %p BEHIND THE LEADER", __func__, actor);
      old_actor = mb_wm_comp_mgr_clutter_client_get_actor (
		      MB_WM_COMP_MGR_CLUTTER_CLIENT (
			      MB_WM_CLIENT (to_replace)->cm_client));
      hd_switcher_replace_window_actor (priv->switcher_group, old_actor, actor);
      hd_switcher_replace_window_actor (priv->switcher_group, actor, old_actor);
      /* and make sure we're in app mode and not transitioning as
       * we'll want to show this new app right away*/
      if (!STATE_IS_APP(hd_render_manager_get_state()))
        hd_render_manager_set_state(HDRM_STATE_APP);
      hd_render_manager_stop_transition();
      /* This forces the decors to be redone, taking into account the
       * stack index. */
      mb_wm_client_theme_change (c);
      mb_wm_client_theme_change (MB_WM_CLIENT(to_replace));
      actor_handled = TRUE;
    }
  else if (add_to_tn)
    {
      g_debug ("%s: ADD ACTOR %p\n", __func__, actor);
      hd_switcher_add_window_actor (priv->switcher_group, actor);
      /* and make sure we're in app mode and not transitioning as
       * we'll want to show this new app right away*/
      if (!STATE_IS_APP(hd_render_manager_get_state()))
        hd_render_manager_set_state(HDRM_STATE_APP);
      hd_render_manager_stop_transition();
      /* This forces the decors to be redone, taking into account the
       * stack index. */
      mb_wm_client_theme_change (c);
      actor_handled = TRUE;
    }
  else if (app->leader && app->leader != app &&
		    app->leader->followers && !topmost)
    {
      ClutterActor *old_actor;
      HdApp *last_follower;

      /*
       * If this window belongs to a window stack, but it is not visible we
       * still have to do something with it, otherwise it will appear in the
       * background.
       */
      last_follower = g_list_last (app->leader->followers)->data;
      old_actor = mb_wm_comp_mgr_clutter_client_get_actor (
		      MB_WM_COMP_MGR_CLUTTER_CLIENT (
			      MB_WM_CLIENT (last_follower)->cm_client));
      g_debug ("%s: 1 REPLACE %p WITH %p and ADD %p BACK\n", __func__,
		      old_actor, actor, old_actor);
      hd_switcher_replace_window_actor (priv->switcher_group, old_actor, actor);
      hd_switcher_replace_window_actor (priv->switcher_group, actor, old_actor);
      actor_handled = TRUE;
    }


  if (!actor_handled
      && !(c->window->ewmh_state & MBWMClientWindowEWMHStateSkipTaskbar)
      && !to_replace && !add_to_tn && topmost)
    {
            /*
      printf("non-stackable, stackable leader, "
             "or secondary acting as leader\n");
             */
      g_debug ("%s: ADD CLUTTER ACTOR %p", __func__, actor);
      hd_switcher_add_window_actor (priv->switcher_group, actor);
      /* and make sure we're in app mode and not transitioning as
       * we'll want to show this new app right away*/
      if (!STATE_IS_APP(hd_render_manager_get_state()))
        hd_render_manager_set_state(HDRM_STATE_APP);
      hd_render_manager_stop_transition();

      /* This forces the decors to be redone, taking into account the
       * stack index (if any). */
      mb_wm_client_theme_change (c);
    }
}

static MBWindowManagerClient *
hd_comp_mgr_determine_current_app ()
{
  extern MBWindowManager *hd_mb_wm;
  MBWindowManagerClient *c;

  /* Select the topmost client that is either the desktop
   * or a %HdApp with full screen coverage. */
  for (c = hd_mb_wm->stack_top; c; c = c->stacked_below)
    {
      if (MB_WM_CLIENT_CLIENT_TYPE (c) & MBWMClientTypeDesktop)
        return c;
      if (!HD_IS_APP (c))
        continue;
      if (mb_wm_client_is_unmap_confirmed (c))
        continue;
      if (!mb_wm_client_is_map_confirmed (c) &&
          mb_wm_client_is_geometry_requested (c) &&
          !hd_comp_mgr_client_is_maximized (c->frame_geometry))
        /* Not covering the whole application area. */
        continue;
      if (!c->window)
        continue;
      if (c->window->name && !g_strncasecmp (c->window->name, "systemui", 8))
        /* systemui is not an application. */
        continue;
      return c;
    }
  return hd_mb_wm->desktop;
}

/* returns TRUE if state was changed */
gboolean
hd_comp_mgr_reconsider_compositing (MBWMCompMgr *mgr)
{
  HDRMStateEnum hdrm_state = hd_render_manager_get_state ();
  MBWindowManagerClient *c = hd_comp_mgr_determine_current_app ();

  if (c && c != mgr->wm->desktop && !hd_transition_is_rotating () &&
      (hdrm_state == HDRM_STATE_APP || hdrm_state == HDRM_STATE_APP_PORTRAIT)
      && hd_comp_mgr_is_non_composited (c, FALSE))
    {
      MBWindowManagerClient *tmp;
      gboolean found = FALSE;

      /* check if there is a window that wishes composited mode above */
      for (tmp = c->stacked_above; tmp; tmp = tmp->stacked_above)
        if (mb_wm_client_is_map_confirmed (tmp) &&
            hd_comp_mgr_client_prefers_compositing (tmp))
          {
            found = TRUE;
            break;
          }

      if (!found)
        {
          if (hdrm_state == HDRM_STATE_APP)
            hd_render_manager_set_state (HDRM_STATE_NON_COMPOSITED);
          else
            hd_render_manager_set_state (HDRM_STATE_NON_COMP_PORT);
          return TRUE;
        }
    }
  else if (STATE_IS_NON_COMP (hdrm_state))
    {
      if (c && c == mgr->wm->desktop)
			{
				if(STATE_IS_PORTRAIT (hdrm_state ))
		      hd_render_manager_set_state (HDRM_STATE_HOME_PORTRAIT);
				else
					hd_render_manager_set_state (HDRM_STATE_HOME);
			}
      else if (c)
        {
          MBWindowManagerClient *tmp;
          gboolean found = FALSE;

          /* check if there is a window that needs composited mode above */
          for (tmp = c->stacked_above; tmp; tmp = tmp->stacked_above)
            if (mb_wm_client_is_map_confirmed (tmp) &&
                hd_comp_mgr_client_prefers_compositing (tmp))
              {
                found = TRUE;
                break;
              }

          if (found || !hd_comp_mgr_is_non_composited (c, FALSE))
            {
              hd_render_manager_switch_to_composited_state ();
              return TRUE;
            }
          /* this is for the case of two clients on top of each other,
           * where the top client is unredirected and unmapped but the
           * lower client is not unredirected yet */
          else if (hd_comp_mgr_is_non_composited (c, FALSE) && c->cm_client &&
                 !mb_wm_comp_mgr_clutter_client_is_unredirected (c->cm_client))
            hd_comp_mgr_unredirect_client (c);
        }
      else /* no application -> we should be composited */
        {
          g_warning ("non-composited but no application, should not happen");
          hd_render_manager_switch_to_composited_state ();
          return TRUE;
        }
    }
  return FALSE;
}

static void
hd_comp_mgr_unmap_notify (MBWMCompMgr *mgr, MBWindowManagerClient *c)
{
  HdCompMgrPrivate          * priv = HD_COMP_MGR (mgr)->priv;
  MBWMClientType            c_type = MB_WM_CLIENT_CLIENT_TYPE (c);
  MBWMCompMgrClutterClient *cclient;
  HDRMStateEnum             hdrm_state;

  g_debug ("%s: 0x%lx '%s'\n", __FUNCTION__,
           c && c->window ? c->window->xwindow : 0,
           mb_wm_client_get_name (c));

  if (c->window->live_background)
    {
      /*g_printerr ("%s: remove live_bg\n", __func__);*/
      c->window->live_background = 0;
      hd_home_set_live_background (HD_HOME (priv->home), c);
      return;
    }

  cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);
  hdrm_state = hd_render_manager_get_state ();

  /* if we are in home_edit_dlg mode, check and see if there is stuff
   * that would spoil our grab now - and if not, return to home_edit mode */
  if (hdrm_state == HDRM_STATE_HOME_EDIT_DLG || hdrm_state == HDRM_STATE_HOME_EDIT_DLG_PORTRAIT)
    {
      gboolean grab_spoil = FALSE;
      MBWindowManagerClient *above = mgr->wm->desktop;
      if (above) above = above->stacked_above;
      for (; above; above = above->stacked_above)
        {
          if (above != c && !mb_wm_client_is_unmap_confirmed (above) &&
              MB_WM_CLIENT_CLIENT_TYPE(above)!=MBWMClientTypeOverride &&
              MB_WM_CLIENT_CLIENT_TYPE(above)!=HdWmClientTypeHomeApplet &&
              MB_WM_CLIENT_CLIENT_TYPE(above)!=HdWmClientTypeStatusArea &&
              MB_WM_CLIENT_CLIENT_TYPE(above)!=HdWmClientTypeAnimationActor &&
              !HD_IS_BANNER_NOTE (above))
          {
            g_debug ("spoiler=%p", above);
            grab_spoil = TRUE;
            break;
          }
        }
      if (!grab_spoil)
				{
					if(STATE_IS_PORTRAIT(hdrm_state))
	        	hd_render_manager_set_state(HDRM_STATE_HOME_EDIT_PORTRAIT);
					else
	        	hd_render_manager_set_state(HDRM_STATE_HOME_EDIT);
				}
    }

  if (HD_IS_APP (c) && HD_APP (c)->stack_index > 0 &&
      HD_APP (c)->leader != HD_APP (c))
    {
      GList *l;
      g_debug ("%s: detransitise stackable secondary %lx\n", __FUNCTION__,
               c->window->xwindow);
      /* stackable window: detransitise if it is not the leader, so we
       * don't unmap the secondaries above us */
      mb_wm_client_detransitise (MB_WM_CLIENT (c));

      l = g_list_find (HD_APP (c)->leader->followers, c);
      if (l && l->next)
      {
        /* remove link from the window above, so that it is not unmapped
         * by libmatchbox2 */
        mb_wm_client_detransitise (MB_WM_CLIENT (l->next->data));
        /* add link from that window to the window below */
        if (l->prev)
          {
            g_debug("%s: re-link stackable %lx to secondary\n", __FUNCTION__,
                   MB_WM_CLIENT (l->next->data)->window->xwindow);
            mb_wm_client_add_transient (MB_WM_CLIENT (l->prev->data),
                                        MB_WM_CLIENT (l->next->data));
          }
        else
          {
            g_debug("%s: re-link stackable %lx to leader\n", __FUNCTION__,
                   MB_WM_CLIENT (l->next->data)->window->xwindow);
            mb_wm_client_add_transient (MB_WM_CLIENT (HD_APP (c)->leader),
                                        MB_WM_CLIENT (l->next->data));
          }
      }
    }
  else if (HD_IS_APP (c) && HD_APP (c)->stack_index >= 0 &&
           HD_APP (c)->leader == HD_APP (c))
    {
      g_debug ("%s: detransitise stackable leader %lx\n", __FUNCTION__,
               c->window->xwindow);
      if (HD_APP (c)->followers)
        {
          /* remove link from the first secondary, so that it is not unmapped
           * by libmatchbox2 */
          mb_wm_client_detransitise (
                MB_WM_CLIENT (HD_APP (c)->followers->data));
        }
    }
  else if (HD_IS_INCOMING_EVENT_NOTE(c))
    {
      hd_switcher_remove_notification (priv->switcher_group,
                                       HD_NOTE (c));
      return;
    }
  else if (c_type == MBWMClientTypeNote || c_type == MBWMClientTypeDialog ||
           c->window->net_type ==
           c->wmref->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_POPUP_MENU])
    {
      ClutterActor *actor;
      actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);
      /* checking for transiency is not enough because nowadays dialogs
       * can become non-transient after they have been transient */
      if (actor)
        hd_switcher_remove_dialog (priv->switcher_group, actor);
    }
}


static void
hd_comp_mgr_effect (MBWMCompMgr                *mgr,
                    MBWindowManagerClient      *c,
                    MBWMCompMgrClientEvent      event)
{
  HdCompMgr      *hmgr = HD_COMP_MGR(mgr);
  MBWMClientType c_type = MB_WM_CLIENT_CLIENT_TYPE (c);
  /*g_debug ("%s, c=%p ctype=%d event=%d",
            __FUNCTION__, c, MB_WM_CLIENT_CLIENT_TYPE (c), event);*/

  if ((c->window->allowed_actions & MBWMClientWindowActionNoTransitions)
      || c->window->live_background)
    {
      /* restack because this window could be in the blur group */
      g_debug("%s: no transition effect for this one\n", __func__);
      hd_render_manager_restack ();
      hd_comp_mgr_reconsider_compositing (mgr);
      return;
    }

  /*HdCompMgrPrivate *priv = HD_COMP_MGR (mgr)->priv;*/
  if (event == MBWMCompMgrClientEventUnmap)
    {
      hd_transition_play_tactile (FALSE, c_type);
      if (c_type == HdWmClientTypeStatusMenu)
        hd_transition_popup(hmgr, c, MBWMCompMgrClientEventUnmap);
      else if (HD_IS_INCOMING_EVENT_PREVIEW_NOTE(c))
        hd_transition_notification(hmgr, c, MBWMCompMgrClientEventUnmap);
      else if (c_type == MBWMClientTypeDialog ||
               c_type == HdWmClientTypeAppMenu)
        {
          if (!hd_util_client_obscured(c))
            hd_transition_popup(hmgr, c, MBWMCompMgrClientEventUnmap);
        }
      else if (c_type == MBWMClientTypeNote && !HD_IS_INCOMING_EVENT_NOTE(c))
        hd_transition_fade(hmgr, c, MBWMCompMgrClientEventUnmap);
      else if (c_type == MBWMClientTypeApp)
        {
          /* Look if it's a stackable window. */
          HdApp *app = HD_APP (c);
          if (app->stack_index > 0 && app->leader != app)
            {
              /* Find the next window to transition to. We haven't yet been
               * removed from the stack so want the window second from the
               * top */
              MBWindowManagerClient *next = MB_WM_CLIENT(app->leader);
              if (app->leader->followers &&
                  g_list_last(app->leader->followers)->prev)
                next = MB_WM_CLIENT(
                    g_list_last(app->leader->followers)->prev->data);
              /* Start actual transition */
              hd_transition_subview(hmgr, c,
                                    next,
                                    MBWMCompMgrClientEventUnmap);
            }
          else if ((app->stack_index < 0
                    || (app->leader == app && !app->followers))
                   && hd_task_navigator_is_crowded ()
                   && c->window->xwindow == hd_wm_current_app_is (NULL, 0)
                   && !hd_wm_has_modal_blockers (mgr->wm)
                   && !c->transient_for)
	  {
		hd_render_manager_set_state (HDRM_STATE_TASK_NAV);
	  }
          else
            {
              HdCompMgrClient *hclient = HD_COMP_MGR_CLIENT (c->cm_client);
              HdRunningApp *app = hd_comp_mgr_client_get_app (hclient);

              /* Avoid this transition if app is being hibernated,
               * or if it's the launcher editor */
              if (!app ||
                  !hd_running_app_is_hibernating (app))
                {
                  /* unregister_client() will switch state if it thinks so */
                  gboolean window_on_top = FALSE;
                  MBWindowManagerClient *cit = c->stacked_above;
                  while (cit)
                    {
                      MBWMClientType cit_type = MB_WM_CLIENT_CLIENT_TYPE(cit);
                      if (cit_type == MBWMClientTypeApp ||
                          cit_type == MBWMClientTypeDialog ||
                          cit_type == MBWMClientTypeDesktop)
                        {
                          window_on_top = TRUE;
                          break;
                        }
                      cit = cit->stacked_above;
                    }

                  /* if the window has another app/dialog on top of it then
                   * don't show the closing animation - but still play the
                   * sound */
                  if (!window_on_top)
                    hd_transition_close_app (hmgr, c);
                  else
                    {
                      hd_comp_mgr_reconsider_compositing (mgr);
                      hd_transition_play_sound (HDCM_WINDOW_CLOSED_SOUND);
                    }
                }
              else
                hd_comp_mgr_reconsider_compositing (mgr);
            }
          app->map_effect_before = FALSE;
        }
      else
        hd_comp_mgr_reconsider_compositing (mgr);
    }
  else if (event == MBWMCompMgrClientEventMap)
    {
      hd_transition_play_tactile (TRUE, c_type);
      if (c_type == HdWmClientTypeStatusMenu)
        hd_transition_popup(hmgr, c, MBWMCompMgrClientEventMap);
      else if ((c_type == MBWMClientTypeDialog) ||
               (c_type == HdWmClientTypeAppMenu))
        hd_transition_popup(hmgr, c, MBWMCompMgrClientEventMap);
      else if (HD_IS_INCOMING_EVENT_PREVIEW_NOTE(c))
        hd_transition_notification(hmgr, c, MBWMCompMgrClientEventMap);
      else if (c_type == MBWMClientTypeNote && !HD_IS_INCOMING_EVENT_NOTE(c))
        /* std event notes go direct to the switcher, so we don't want to
         * use a transition for them */
        hd_transition_fade(hmgr, c, MBWMCompMgrClientEventMap);
      else if (c_type == MBWMClientTypeApp)
        {
          /* Look if it's a stackable window. We don't do the subview
           * animation again if we have had a mapping without an unmap,
           * which is what happens in the Image Viewer when the same
           * window goes from Fullscreen to Windowed */
          HdApp *app = HD_APP (c);
          if (app->stack_index > 0 && !app->map_effect_before)
            {
              /* Find the window to transition from (we have already been
               * added to the stack, so the window is second from the
               * top of the stack */
              MBWindowManagerClient *next = MB_WM_CLIENT(app->leader);
              if (app->leader->followers &&
                  g_list_last(app->leader->followers)->prev)
                next = MB_WM_CLIENT(
                    g_list_last(app->leader->followers)->prev->data);
              /* Start actual transition - We may have the case where 2
               * stackable windows are created at the same time, and we are
               * mapping the subview before the main view is mapped. In this
               * case the first window will not be in the followers list, and
               * app->leader == app, so we don't want any transition.
               * Solves NB#112411 */
              if (c!=next)
                hd_transition_subview(hmgr, c,
                                      next,
                                      MBWMCompMgrClientEventMap);
            }
          /* We're now showing this app, so remove our app
           * starting screen if we had one */
          hd_launcher_window_created();
          app->map_effect_before = TRUE;
        }
    }
}

gboolean
hd_comp_mgr_restack (MBWMCompMgr * mgr)
{
  HdCompMgrPrivate         * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClass         * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));

  /* g_debug ("%s", __FUNCTION__); */

  /*
   * We use the parent class restack() method to do the stacking, but as our
   * switcher shares actors with the CM, we cannot run this when the switcher
   * is showing, or an unmap effect is in progress; instead we set a flag, and
   * let the switcher request stack sync when it closes.
   */
  if (priv->stack_sync)
    {
      g_source_remove (priv->stack_sync);
      priv->stack_sync = 0;
    }

  if (STATE_NEED_TASK_NAV (hd_render_manager_get_state()))
    {
      hd_comp_mgr_check_do_not_disturb_flag (HD_COMP_MGR (mgr));
      /* current_hclient should be desktop now */
      if (mgr->wm->desktop)
        priv->current_hclient = HD_COMP_MGR_CLIENT (
                                    mgr->wm->desktop->cm_client);

      return FALSE;
    }

  /* Hide the Edit button if it is currently shown */
  if (priv->home)
    hd_home_hide_edit_button (HD_HOME (priv->home));

  if (parent_klass->restack)
    parent_klass->restack (mgr);

  /* Update _MB_CURRENT_APP_WINDOW if we're ready and it's changed.
   * Don't if we're in the middle of a transition which will change
   * state one again because getting is-topmost wrong is frowned upon. */
  if (mgr->wm && mgr->wm->root_win && mgr->wm->desktop
      && !hd_transition_rotation_will_change_state ())
    {
      gboolean current_client_changed = FALSE;
      MBWindowManagerClient *current_client =
                              hd_comp_mgr_determine_current_app ();

      HdCompMgrClient *new_current_hclient =
        HD_COMP_MGR_CLIENT (current_client->cm_client);
      if (new_current_hclient != priv->current_hclient)
        {
          HdRunningApp *old_current_app;
          HdRunningApp *new_current_app;

          current_client_changed = TRUE;

          /* Reset our 'map' timer, so that if we're asked to do a starting
           * transition, we'll know if jitter could have meant the app was
           * already showing when we got the request */
          gettimeofday(&priv->last_map_time, NULL);


          /* Switch the hibernatable state for the new current client. */
          if (priv->current_hclient &&
              hd_comp_mgr_client_can_hibernate (priv->current_hclient))
            {
              old_current_app =
                hd_comp_mgr_client_get_app (priv->current_hclient);
              if (old_current_app)
                hd_app_mgr_hibernatable (old_current_app, TRUE);
            }

          if (new_current_hclient)
            {
              new_current_app =
                hd_comp_mgr_client_get_app (new_current_hclient);
              /* re-check compositing for the case that we raise composited
               * client on top of non-composited client */
              hd_comp_mgr_reconsider_compositing (mgr);
              if (new_current_app)
                hd_app_mgr_hibernatable (new_current_app, FALSE);
            }

          priv->current_hclient = new_current_hclient;
        }

      hd_wm_current_app_is (mgr->wm, current_client->window->xwindow);

      /* If we have a new app as the current client and we're not in
       * app mode - enter app mode. */
      if (current_client_changed &&
          !(MB_WM_CLIENT_CLIENT_TYPE(current_client) &
                                     MBWMClientTypeDesktop) &&
          !STATE_IS_APP(hd_render_manager_get_state()))
        hd_render_manager_set_state(HDRM_STATE_APP);
    }

  /* Decide about portraitification in case a blocking window was unmapped. */
  hd_comp_mgr_check_do_not_disturb_flag (HD_COMP_MGR (mgr));
  hd_render_manager_restack ();
  hd_app_mgr_mce_activate_accel_if_needed (FALSE);
  hd_comp_mgr_portrait_or_not_portrait (mgr, NULL);

  return FALSE;
}

/* Do a restack some time.  Used in cases when multiple parties
 * want restacking, not knowing about each other. */
void
hd_comp_mgr_sync_stacking (HdCompMgr * hmgr)
{
  HdCompMgrPrivate * priv = hmgr->priv;

  if (!priv->stack_sync)
    /* We need higher priority than idles usually have because
     * the effect has higher priority too and it could starve us. */
    priv->stack_sync = g_idle_add_full (0, (GSourceFunc)hd_comp_mgr_restack,
                                        hmgr, NULL);
}

/*
 * Shuts down a client, handling hibernated applications correctly.
 * if @close_all and @cc is associated with a window stack then
 * close all windows in the stack, otherwise only @cc's.
 */
void
hd_comp_mgr_close_app (HdCompMgr *hmgr, MBWMCompMgrClutterClient *cc,
                       gboolean close_all)
{
  HdCompMgrPrivate      * priv = hmgr->priv;
  HdCompMgrClient       * h_client = HD_COMP_MGR_CLIENT (cc);

  g_return_if_fail (cc != NULL);
  g_return_if_fail (h_client != NULL);
  if (hd_comp_mgr_client_is_hibernating(h_client))
    {
      ClutterActor * actor;

      actor = mb_wm_comp_mgr_clutter_client_get_actor (cc);

      hd_switcher_remove_window_actor (priv->switcher_group, actor, cc);

      g_hash_table_remove (priv->hibernating_apps,
                           (gpointer)h_client->priv->hibernation_key);

      if (h_client->priv->app)
        {
          hd_app_mgr_app_stop_hibernation (h_client->priv->app);
        }

      mb_wm_object_unref (MB_WM_OBJECT (cc));
    }
  else
    {
      MBWindowManagerClient * c = MB_WM_COMP_MGR_CLIENT (cc)->wm_client;

      if (close_all && HD_IS_APP (c) && HD_APP (c)->leader)
        {
          c = MB_WM_CLIENT (HD_APP (c)->leader);
          hd_app_close_followers (HD_APP (c));
	  if (HD_APP (c)->leader == HD_APP (c))
	    /* send delete to the leader */
            mb_wm_client_deliver_delete (c);
        }
      else /* Either primary or a secondary who's lost its leader. */
        mb_wm_client_deliver_delete (c);
    }
}

void
hd_comp_mgr_close_client (HdCompMgr *hmgr, MBWMCompMgrClutterClient *cc)
{
  hd_comp_mgr_close_app (hmgr, cc, FALSE);
}

void
hd_comp_mgr_wakeup_client (HdCompMgr *hmgr, HdCompMgrClient *hclient)
{
  hd_app_mgr_activate (hclient->priv->app);
}

void
hd_comp_mgr_kill_all_apps (HdCompMgr *hmgr)
{
  hd_app_mgr_kill_all ();
}

/* Does any visible client request portrait mode? Or if assume_requested==TRUE
 * we only return false if someone doesn't support portrait mode.
 * Are all of them concerned prepared for it? */
static gboolean
hd_comp_mgr_may_be_portrait (HdCompMgr *hmgr, gboolean assume_requested)
{
  MBWindowManager *wm;
  MBWindowManagerClient *c;
  HdCompMgrPrivate *priv = hmgr->priv;
  gboolean any_supports, any_requests;

  /* Invalidate all cached, inherited portrait flags at once. */
  portrait_freshness_counter++;

  PORTRAIT ("SHOULD BE PORTRAIT?");
  any_supports = any_requests = FALSE;
  wm = MB_WM_COMP_MGR (hmgr)->wm;

  for (c = wm->stack_top; c && c != wm->desktop; c = c->stacked_below)
    {
      PORTRAIT ("CLIENT %p", c);
      PORTRAIT ("IS IGNORABLE?");
      if (MB_WM_CLIENT_CLIENT_TYPE (c) & HdWmClientTypeStatusArea)
        /* It'll be blocked anyway. */
        continue;
      if (MB_WM_CLIENT_CLIENT_TYPE (c)
          & (HdWmClientTypeAppMenu | MBWMClientTypeMenu))
        /* Menus are not transient for their window nor they claim
         * portrait layout support.  Let's just assume they can. */
        continue;
      if (MB_WM_CLIENT_CLIENT_TYPE (c) & HdWmClientTypeHomeApplet)
        /* Make an exception for applets. */
        continue;
      if (MB_WM_CLIENT_CLIENT_TYPE (c) & MBWMClientTypeOverride)
        /* For the beloved systemui windows. */
        continue;
      if (HD_IS_BANNER_NOTE (c) || HD_IS_INCOMING_EVENT_PREVIEW_NOTE (c))
        /* Assume it for now. */
        continue;

      PORTRAIT ("IS VISIBLE OR CURRENT?");
      if (!hd_render_manager_is_client_visible (c) && !(c->window
          && hd_wm_current_app_is (NULL, 0) == c->window->xwindow))
        /*
         * Ignore invisibles except if it's the current application.
         * This is for cases when the topmost client requests pmode
         * and you launch another program.  When we're invoked the
         * new client doesn't have an actor yet but it needs to be
         * taken into account.
         */
        continue;

      /* Get @portrait_supported/requested updated. */
      mb_wm_client_update_portrait_flags (c, portrait_freshness_counter);
      PORTRAIT ("SUPPORT IS %d", c->portrait_supported);

      /* Check if current app is blacklisted. If it's then prevent it from portrait. */
      if((c == hd_comp_mgr_determine_current_app()) && hd_comp_mgr_is_blacklisted(wm, c))
        return FALSE;

      gboolean is_whitelisted = FALSE;

      if(c == hd_comp_mgr_determine_current_app() && hd_comp_mgr_is_whitelisted(wm, c))
        is_whitelisted = TRUE;

      if (((!hd_transition_get_int("thp_tweaks", "forcerotation", 0)
              && !is_whitelisted)
              && !c->portrait_supported)
              || gconf_client_get_bool (priv->gconf_client, GCONF_KEY_ORIENTATION_LOCK, NULL)
              || hd_launcher_is_editor_in_landscape ())
        return FALSE;

      any_supports  = TRUE;
      any_requests |= c->portrait_requested != 0;
      if (!c->portrait_requested && !c->portrait_requested_inherited)
        { /* Client explicity !REQUESTED portrait, obey. */
          PORTRAIT ("PROHIBITED");
          if (!hd_transition_get_int("thp_tweaks", "forcerotation", 0)
              || !is_whitelisted
              || gconf_client_get_bool (priv->gconf_client, GCONF_KEY_ORIENTATION_LOCK, NULL)
              || hd_launcher_is_editor_in_landscape ())
              return FALSE;
        }

      /*
       * This is a workaround for the fullscreen incoming call dialog.
       * Since it's fullscreen we can safely assume it will cover
       * everything underneath, even if that's still visible in
       * clutter sense.  This is an evidence that we just cannot
       * rely on visibility checking entirely. TODO remove later
       */
      if (c->portrait_requested > 1
          || (c->portrait_requested && c->window
              && c->window->ewmh_state & MBWMClientWindowEWMHStateFullscreen))
        {
          PORTRAIT ("DEMANDED");
          break;
        }
    }

  any_requests |= assume_requested && any_supports;
  PORTRAIT ("SHOULD BE: %d", any_requests);
  return any_requests;
}

void hd_comp_mgr_set_pip_flags (HdCompMgr *hmgr,
                                gboolean enabled, gboolean portrait)
{
  hmgr->priv->pip_enabled = enabled;
  hmgr->priv->pip_portrait = portrait;
}

/* Does any visible client request portrait mode?
 * Are all of them concerned prepared for it?
 * Also, is HDRM state LAUNCHER/LAUNCHER_PORTRAIT and environment (conf and
 * device orientation) allows launcher in portrait mode? */
gboolean
hd_comp_mgr_should_be_portrait (HdCompMgr *hmgr)
{
	HdCompMgrPrivate *priv = hmgr->priv;

  /* LAUNCHER can be portraited but does not handle the normal XProperties so
   * we need to check it explicitely */
  if (STATE_IS_LAUNCHER (hd_render_manager_get_state ()))
    {
      if (hd_app_mgr_ui_can_rotate () && hd_app_mgr_is_portrait ())
        return TRUE;
      else
        return FALSE;
    }
  else if (STATE_IS_TASK_NAV (hd_render_manager_get_state ()))
    {
	if(hd_task_navigator_mode())
	    return TRUE;
	  else
	    return FALSE;
    }
	else if (STATE_IS_HOME (hd_render_manager_get_state ()))
	{
    /* Check if desktop is not prevented from switching to portrait mode */
    if (gconf_client_get_bool (priv->gconf_client, GCONF_KEY_DESKTOP_ORIENTATION_LOCK, NULL))
      return FALSE;

		/* Let's honour orientation lock, prevents freezing desktop in portrait
		 * mode */
		gboolean orientation_lock = gconf_client_get_bool (priv->gconf_client, 
																												GCONF_KEY_ORIENTATION_LOCK, NULL);

	 /* hd_comp_mgr_may_be_portrait tells also if there's an app _requesting_ 
		* portrait mode (mostly call-ui) */
		if((hd_home_is_portrait_capable () && !orientation_lock) || hd_comp_mgr_may_be_portrait(hmgr, FALSE))
			return TRUE;
		else
			return FALSE;
	}
	else if (STATE_IS_EDIT_MODE (hd_render_manager_get_state ()))
	{
    /* Check if desktop is not prevented from switching to portrait mode */
    if (gconf_client_get_bool (priv->gconf_client, GCONF_KEY_DESKTOP_ORIENTATION_LOCK, NULL))
      return FALSE;

		/* Check if we are in portrait desktop edit mode and block screen orientation 
		 * if it's true */
		gboolean is_edit_portrait = (hd_render_manager_get_state () == HDRM_STATE_HOME_EDIT_PORTRAIT 
																|| hd_render_manager_get_state () == HDRM_STATE_HOME_EDIT_DLG_PORTRAIT);

		if((hd_home_is_portrait_capable () || is_edit_portrait) && is_edit_portrait)
			return TRUE;
    else
      return FALSE;
	}
  else
    {
      return hd_comp_mgr_may_be_portrait(hmgr, FALSE);
    }
}

/* Are all clients concerned prepared for portrait mode?
 * In case of HDRM in any LAUNCHER state: can it rotate (by conf)? */
gboolean
hd_comp_mgr_can_be_portrait (HdCompMgr *hmgr)
{
  if (STATE_IS_LAUNCHER (hd_render_manager_get_state ()))
    {
      if (hd_app_mgr_ui_can_rotate ())
        return TRUE;
      else
        return FALSE;
    }
  else if (STATE_IS_TASK_NAV (hd_render_manager_get_state ()))
    {
      return hd_app_mgr_slide_is_open();
    }
	else if (STATE_IS_HOME (hd_render_manager_get_state ()))
	{
		return hd_app_mgr_slide_is_open();
	}
	else if (STATE_IS_EDIT_MODE (hd_render_manager_get_state ()))
	{
		return hd_app_mgr_slide_is_open();
	}
  else
    {
      /* compute it normally if not in LAUNCHER */
      return hd_comp_mgr_may_be_portrait(hmgr, TRUE);
    }
}

/*
 * Based on the visible windows decide whether we should be portrait or not.
 * Requires that the visibilities be sorted out.  Otherwise it doesn't work
 * correctly.  @c is the client being mapped, if the context is appropriate,
 * otherwise NULL.
 */
void
hd_comp_mgr_portrait_or_not_portrait (MBWMCompMgr *mgr,
                                      MBWindowManagerClient *c)
{
  HdCompMgrPrivate *priv = HD_COMP_MGR(mgr)->priv;
  /* I think this is a guard for cases when we do a
   * set_state() -> portrait/unportrait() -> restack() */
  if (hd_render_manager_is_changing_state ())
    return;


  /* Undo hd_comp_mgr_portrait_forecast() if in the end it was false. */
  if (c && mb_wm_client_wants_portrait (c)
      && !STATE_IS_PORTRAIT (hd_render_manager_get_state ())
      && hd_transition_is_rotating_to_portrait ()
      && !hd_comp_mgr_should_be_portrait (HD_COMP_MGR (mgr)))
    {
      hd_transition_rotate_screen (mgr->wm, FALSE);
      return;
    }

  /*
   * Change state if necessary:
   * APP <=> APP_PORTRAIT and HOME <=> HOME_PORTRAIT
   */
  if (STATE_IS_PORTRAIT_CAPABLE (hd_render_manager_get_state ()))
    { /* Landscape -> portrait?
         Do this if an app says it really wants to be portrait OR
         if we want to be in 'portrait if possible', and it is poissible */
      if (hd_comp_mgr_should_be_portrait(HD_COMP_MGR (mgr)) ||
          (priv->pip_enabled && priv->pip_portrait &&
           hd_comp_mgr_can_be_portrait(HD_COMP_MGR (mgr))))
          {
            hd_render_manager_set_state_portrait ();
          }
    }
  else if (STATE_IS_PORTRAIT (hd_render_manager_get_state ()))
    { /* Portrait -> landscape?
         Do this if no app is specifically asking for portrait mode AND
         'portrait if possible' wasn't enabled, or we're in landscape mode */
      if (!hd_comp_mgr_should_be_portrait (HD_COMP_MGR (mgr)) &&
          (!priv->pip_enabled || !priv->pip_portrait ||
           !hd_comp_mgr_can_be_portrait(HD_COMP_MGR (mgr))))
          {
            hd_render_manager_set_state_unportrait ();
          }
    }
}

gboolean
hd_comp_mgr_client_supports_portrait (MBWindowManagerClient *mbwmc)
{
  /* Don't mess with hd_comp_mgr_should_be_portrait()'s @counter. */
  mb_wm_client_update_portrait_flags (mbwmc, G_MAXUINT); 

  if(hd_comp_mgr_is_whitelisted(mbwmc->wmref, mbwmc))
    return TRUE;

  if(hd_comp_mgr_is_blacklisted(mbwmc->wmref, mbwmc))
    return FALSE;

  return hd_transition_get_int("thp_tweaks", "forcerotation", 0) ?
      TRUE : mbwmc->portrait_supported;
}

static void
hd_comp_mgr_check_do_not_disturb_flag (HdCompMgr *hmgr)
{
  HdCompMgrPrivate *priv = hmgr->priv;
  MBWindowManager *wm;
  Window xwindow;
  gboolean do_not_disturb_flag = FALSE;

  wm = MB_WM_COMP_MGR (hmgr)->wm;
  xwindow = hd_wm_current_app_is (NULL, 0);

  /* xwindow may be ~0 here if we are in TASK_NAV. In this case, we just
   * assume the DND flag is not set. */
  if (xwindow && (xwindow!=~0) && wm->desktop && xwindow != wm->desktop->window->xwindow)
    {
      guint32 *value;
      Atom dnd;

      dnd = hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_DO_NOT_DISTURB);

      value = hd_util_get_win_prop_data_and_validate (wm->xdpy, xwindow,
                                                      dnd, XA_INTEGER,
                                                      32, 1, NULL);
      do_not_disturb_flag = (value && *value == 1);

      if (value)
        XFree (value);
    }

  /* Check change */
  if (priv->do_not_disturb_flag != do_not_disturb_flag)
    {
      priv->do_not_disturb_flag = do_not_disturb_flag;
      g_debug ("DND: %d", priv->do_not_disturb_flag);

      hd_dbus_disable_display_blanking (do_not_disturb_flag);
    }
}

Atom
hd_comp_mgr_get_atom (HdCompMgr *hmgr, HdAtoms id)
{
  HdCompMgrPrivate *priv = hmgr->priv;

  if (id >= _HD_ATOM_LAST)
    return (Atom) 0;

  return priv->atoms[id];
}

ClutterActor *
hd_comp_mgr_get_home (HdCompMgr *hmgr)
{
  HdCompMgrPrivate *priv = hmgr->priv;

  return priv->home;
}

GObject *
hd_comp_mgr_get_switcher (HdCompMgr *hmgr)
{
  HdCompMgrPrivate *priv = hmgr->priv;

  return G_OBJECT(priv->switcher_group);
}

gint
hd_comp_mgr_get_current_home_view_id (HdCompMgr *hmgr)
{
  HdCompMgrPrivate *priv = hmgr->priv;

  return hd_home_get_current_view_id (HD_HOME (priv->home));
}

MBWindowManagerClient *
hd_comp_mgr_get_desktop_client (HdCompMgr *hmgr)
{
  HdCompMgrPrivate *priv = hmgr->priv;

  return priv->desktop;
}

#ifndef G_DEBUG_DISABLE
static void
dump_clutter_actor_tree (ClutterActor *actor, GString *indent)
{
  const gchar *name;
  MBWMCompMgrClient *cmgrc;
  ClutterGeometry geo;
  gint ax, ay;

  if (!indent)
    indent = g_string_new ("");

  if (!(name = clutter_actor_get_name (actor)) && CLUTTER_IS_LABEL (actor))
    name = clutter_label_get_text (CLUTTER_LABEL (actor));
  cmgrc = g_object_get_data(G_OBJECT (actor), "HD-MBWMCompMgrClutterClient");

  clutter_actor_get_geometry (actor, &geo);
  clutter_actor_get_anchor_point (actor, &ax, &ay);
  g_debug ("actor[%u]: %s%p (type=%s, name=%s, win=0x%lx), "
           "size: %ux%u%+d%+d[%d,%d], visible: %d, reactive: %d",
           indent->len, indent->str, actor,
           G_OBJECT_TYPE_NAME (actor), name,
           cmgrc && cmgrc->wm_client && cmgrc->wm_client->window
               ? cmgrc->wm_client->window->xwindow : 0,
           geo.width, geo.height, geo.x, geo.y, ax, ay,
           CLUTTER_ACTOR_IS_VISIBLE (actor) != 0,
           CLUTTER_ACTOR_IS_REACTIVE (actor) != 0);
  if (CLUTTER_IS_CONTAINER (actor))
    {
      g_string_append_c (indent, ' ');
      clutter_container_foreach (CLUTTER_CONTAINER (actor),
                                 (ClutterCallback)dump_clutter_actor_tree,
                                 indent);
      g_string_truncate (indent, indent->len-1);
    }
}
#endif

void
hd_comp_mgr_dump_debug_info (const gchar *tag)
{
#ifndef G_DEBUG_DISABLE
  Window focus;
  MBWMRootWindow *root;
  MBWindowManagerClient *mbwmc;
  int i, revert, ninputshapes, unused;
  XRectangle *inputshape;
  ClutterActor *stage;

  if (tag)
    g_debug ("%s", tag);

  g_debug ("Windows:");
  root = mb_wm_root_window_get (NULL);
  mb_wm_stack_enumerate_reverse (root->wm, mbwmc)
    {
      MBGeometry geo;
      const HdApp *app;

      geo = mbwmc->window ? mbwmc->window->geometry : mbwmc->frame_geometry;
      g_debug (" client=%p, type=%d, size=%dx%d%+d%+d, trfor=%p, layer=%d, "
               "stkidx=%d, win=0x%lx, group=0x%lx, name=%s",
               mbwmc, MB_WM_CLIENT_CLIENT_TYPE (mbwmc), MBWM_GEOMETRY (&geo),
               mbwmc->transient_for, mbwmc->stacking_layer,
               HD_IS_APP (mbwmc) ? HD_APP (mbwmc)->stack_index : -1,
               mbwmc->window ? mbwmc->window->xwindow : 0,
               mbwmc->window ? mbwmc->window->xwin_group : 0,
               mbwmc->window ? mbwmc->window->name : "<unset>");
      if (HD_IS_APP (mbwmc) && (app = HD_APP (mbwmc))->followers)
        {
          const GList *li;

          g_debug ("  followers:");
          for (li = app->followers; li; li = li->next)
            g_debug ("   %p", li->data);
        }
    }
  mb_wm_object_unref (MB_WM_OBJECT (root));

  g_debug ("input:");
  XGetInputFocus (clutter_x11_get_default_display (), &focus, &revert);
  g_debug ("  focus: 0x%lx", focus);
  if (revert == RevertToParent)
    g_debug ("  reverts to parent");
  else if (revert == RevertToPointerRoot)
    g_debug ("  reverts to pointer root");
  else if (revert == RevertToNone)
    g_debug ("  reverts to none");
  else
    g_debug ("  reverts to %d", revert);

  g_debug ("  shape:");
  stage = clutter_stage_get_default ();
  inputshape = XShapeGetRectangles(root->wm->xdpy,
                   clutter_x11_get_stage_window (CLUTTER_STAGE (stage)),
                   ShapeInput, &ninputshapes, &unused);
  for (i = 0; i < ninputshapes; i++)
    g_debug ("    %dx%d%+d%+d", MBWM_GEOMETRY(&inputshape[i]));
  XFree(inputshape);

  dump_clutter_actor_tree (clutter_stage_get_default (), NULL);
  hd_app_mgr_dump_app_list (TRUE);
#endif
}

void hd_comp_mgr_set_effect_running(HdCompMgr *hmgr, gboolean running)
{
  /* We don't need this now, but this might be useful in the future.
   * It is called when any transition begins or ends. */
}

/* Return the time since the last window was mapped (in ms). This
 * is used in the _launch dbus call to check that the window we
 * were asked to do a transition for hasn't actually mapped before
 * we got the dbug message.
 */
gint hd_comp_mgr_time_since_last_map(HdCompMgr *hmgr)
{
  struct timeval current;
  gettimeofday(&current, NULL);

  return ((current.tv_sec - hmgr->priv->last_map_time.tv_sec) * 1000) +
         ((current.tv_usec - hmgr->priv->last_map_time.tv_usec) / 1000);
}

extern gboolean hd_dbus_display_is_off;
extern MBWindowManager *hd_mb_wm;

void
hd_comp_mgr_update_applets_on_current_desktop_property (HdCompMgr *hmgr)
{
  HdHome *home = HD_HOME (hmgr->priv->home);
  GSList *applets = NULL, *a;
  GSList *views, *v;

  applets = hd_home_view_get_all_applets (HD_HOME_VIEW (
                                          hd_home_get_current_view (home)));


  mb_wm_util_async_trap_x_errors (MB_WM_COMP_MGR(hmgr)->wm->xdpy);
  /* Handle applets on current view */
  for (a = applets; a; a = a->next)
    {
      MBWindowManagerClient *wm_client
              = MB_WM_COMP_MGR_CLIENT (a->data)->wm_client;
      guint32 on_desktop = 1;
      if (STATE_NEED_DESKTOP (hd_render_manager_get_state ()) &&
          STATE_SHOW_APPLETS (hd_render_manager_get_state ()) &&
          !hd_dbus_display_is_off)
        {
          XChangeProperty (wm_client->wmref->xdpy,
                           wm_client->window->xwindow,
                           hd_comp_mgr_get_atom (hmgr,
                                HD_ATOM_HILDON_APPLET_ON_CURRENT_DESKTOP),
                           XA_CARDINAL,
                           32,
                           PropModeReplace,
                           (const guchar *) &on_desktop,
                           1);
        }
      else
        {
          XDeleteProperty (wm_client->wmref->xdpy,
                           wm_client->window->xwindow,
                           hd_comp_mgr_get_atom (hmgr,
                                HD_ATOM_HILDON_APPLET_ON_CURRENT_DESKTOP));
        }
    }
  g_slist_free (applets);

  views = hd_home_get_not_visible_views (home);
  for (v = views; v; v = v->next)
    {
      applets = hd_home_view_get_all_applets (HD_HOME_VIEW (v->data));
      for (a = applets; a; a = a->next)
        {
          MBWindowManagerClient *wm_client
                  = MB_WM_COMP_MGR_CLIENT (a->data)->wm_client;
          XDeleteProperty (wm_client->wmref->xdpy,
                           wm_client->window->xwindow,
                           hd_comp_mgr_get_atom (hmgr,
                                HD_ATOM_HILDON_APPLET_ON_CURRENT_DESKTOP));
        }
      g_slist_free (applets);
    }
  g_slist_free (views);

  mb_wm_util_async_untrap_x_errors ();
}

gboolean
hd_comp_mgr_is_whitelisted(MBWindowManager *wm, MBWindowManagerClient *c)
{
  gchar *whitelist;
  XClassHint class_hint;
  Status ret;
  gchar *wname = NULL;
  gboolean is_on_whitelist = FALSE;

  if (!HD_IS_APP (c) || !MB_WINDOW_MANAGER(wm) || c == wm->desktop)
    return FALSE;

  whitelist = g_strdup(hd_transition_get_string("thp_tweaks", "whitelist", ""));
  memset(&class_hint, 0, sizeof(XClassHint));
  mb_wm_util_async_trap_x_errors (wm->xdpy);
  ret = XGetClassHint (wm->xdpy, c->window->xwindow, &class_hint);
	mb_wm_util_async_untrap_x_errors ();	

  if (ret && class_hint.res_class)
    wname = g_strdup(class_hint.res_name);

  if (class_hint.res_class)
    XFree(class_hint.res_class);

  if (class_hint.res_name)
    XFree(class_hint.res_name);


  if (g_strrstr(whitelist, wname))
    is_on_whitelist = TRUE;

  g_free(whitelist);
  g_free(wname);

  return is_on_whitelist;
}

gboolean
hd_comp_mgr_is_blacklisted(MBWindowManager *wm, MBWindowManagerClient *c)
{
  gchar *blacklist;
  XClassHint class_hint;
  Status ret;
  gchar *wname = NULL;

  if (!HD_IS_APP (c) || !MB_WINDOW_MANAGER(wm) || c == wm->desktop)
    return FALSE;

  /* We don't want blacklisted windows when forcerotation == 0. */
  if(!hd_transition_get_int("thp_tweaks", "forcerotation", 0))
    return FALSE;

  /* Let's check if the window is on the whitelist. */
  if(hd_comp_mgr_is_whitelisted(wm, c))
    return FALSE;

  blacklist = g_strdup(hd_transition_get_string("thp_tweaks", "blacklist", ""));
  memset(&class_hint, 0, sizeof(XClassHint));
  mb_wm_util_async_trap_x_errors (wm->xdpy);
  ret = XGetClassHint (wm->xdpy, c->window->xwindow, &class_hint);
	mb_wm_util_async_untrap_x_errors ();	

  if (ret && class_hint.res_class)
    wname = g_strdup(class_hint.res_name);

  /* Check, if X-CSSU-Force-Landscape=true. */
  gboolean blacklisted = hd_comp_mgr_is_blacklisted_parse_desktop_file(wname, class_hint.res_class, c->window->pid);


  if (class_hint.res_class)
    XFree(class_hint.res_class);

  if (class_hint.res_name)
    XFree(class_hint.res_name);


  if (g_strrstr(blacklist, wname) && !(c->portrait_supported || c->portrait_requested))
    blacklisted = TRUE;

  if(c->stacked_below && (wname == NULL))
    if(hd_comp_mgr_is_blacklisted(wm, c->stacked_below))
      blacklisted = TRUE;

  g_free(blacklist);
  g_free(wname);

  return blacklisted;
}

gboolean
hd_comp_mgr_is_blacklisted_parse_desktop_file(char *res_name, 
                                              char *res_class, GPid pid)
{
  HdLauncherTree *tree;
  HdLauncherItem *item;
  HdRunningApp *app;

  /* With the informations from XClassHint, we can match our window to an application. */
  app = hd_app_mgr_match_window (res_name, res_class, pid);
  /* Get application list. */
  tree = hd_app_mgr_get_tree ();
  /* Find our app in the app list. */
  item = hd_launcher_tree_find_item(tree, hd_running_app_get_id(app));
  
  if(item)
    {
      /* Check, if X-CSSU-Force-Landscape=true. */
      if(hd_launcher_item_get_cssu_force_landscape (item))
        return TRUE;
    }
  return FALSE;
}
