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
#include "launcher/hd-app-mgr.h"

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
#include <mce/dbus-names.h>

#include <sys/types.h>
#include <signal.h>
#include <math.h>

/* TODO Is it a TODO? */
#define HIBERNATION_TIMEMOUT 3000 /* as suggested by 31410#10 */

#define OPERATOR_APPLET_ID "_HILDON_OPERATOR_APPLET"

/* We need to check whether @c neither is maximized nor is the systemui's
 * because these things pop up on startup and then don't go away,
 * leaving us with a needlessly blurred background. We don't blur for
 * fullscreen dialogs either because hdrm will put anything fullscreen
 * into the blur box (and we don't want to waste cycles needlessly
 * blurring anyway) */
#define BLUR_FOR_WINDOW(c) \
              (!hd_comp_mgr_ignore_window(c) \
               && !HD_COMP_MGR_CLIENT_IS_MAXIMIZED(c->frame_geometry))

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

  gboolean               stack_sync      : 1;

  MBWindowManagerClient *status_area_client;
  MBWindowManagerClient *status_menu_client;

  HdCompMgrClient       *current_hclient;

  /* Clients who need to block the Task button. */
  GHashTable            *tasks_button_blockers;

  /* Track changes to the PORTRAIT properties. */
  unsigned long          property_changed_cb_id;

  /* MCE D-Bus Proxy */
  DBusGProxy            *mce_proxy;
};

/*
 * A helper object to store manager's per-client data
 */

struct HdCompMgrClientPrivate
{
  HdLauncherApp *app;

  guint                 hibernation_key;
  gboolean              can_hibernate : 1;

  /*
   * Clients have the means to specify properties to indicate
   * portrait-capability, which are mapped as follows:
   *
   * #_HILDON_PORTRAIT_MODE_REQUEST => @portrait_mode_requested
   * #_HILDON_PORTRAIT_MODE_SUPPORT:
   *   if 0 => @portrait_mode_not_supported := 1
   *   else => @portrait_mode_not_supported == 0
   *
   * That is, all clients are assumed to be OK with portrait mode,
   * unless they say otherwise.
   */
  gboolean              portrait_not_supported;
  gboolean              portrait_requested;
};

HdLauncherApp *hd_comp_mgr_client_get_app_key (HdCompMgrClient *client,
                                               HdCompMgr *hmgr);

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

HdLauncherApp *
hd_comp_mgr_client_get_app_key (HdCompMgrClient *client, HdCompMgr *hmgr)
{
  MBWindowManagerClient *wm_client;
  MBWindowManager       *wm;
  XClassHint             class_hint;
  Status                 status = 0;
  HdLauncherApp          *app = NULL;
  HdCompMgrClientPrivate *priv = client->priv;

  wm = MB_WM_COMP_MGR (hmgr)->wm;
  wm_client = MB_WM_COMP_MGR_CLIENT (client)->wm_client;

  memset(&class_hint, 0, sizeof(XClassHint));

  mb_wm_util_trap_x_errors ();

  status = XGetClassHint(wm->xdpy, wm_client->window->xwindow, &class_hint);

  if (mb_wm_util_untrap_x_errors () || !status || !class_hint.res_name)
    goto out;

  app = hd_app_mgr_match_window (class_hint.res_name,
                                 class_hint.res_class);

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
              hd_launcher_item_get_id (HD_LAUNCHER_ITEM (app)),
              class_hint.res_class ? class_hint.res_class : "",
              role ? role : "",
              level);
      g_debug ("%s: app %s, window key: %s\n", __FUNCTION__,
                hd_launcher_item_get_id (HD_LAUNCHER_ITEM (app)),
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
  HdLauncherApp          *app;
  guint32                *prop;

  hmgr = HD_COMP_MGR (wm_client->wmref->comp_mgr);

  priv = client->priv = g_new0 (HdCompMgrClientPrivate, 1);

  app = hd_comp_mgr_client_get_app_key (client, hmgr);
  if (app)
    {
      GQuark appid = hd_launcher_item_get_id_quark (HD_LAUNCHER_ITEM (app));
      priv->app = g_object_ref (app);
      hd_comp_mgr_client_process_hibernation_prop (client);

      /* Look up if there were already windows for this app. */
      guint windows = (guint)g_hash_table_lookup (hmgr->priv->shown_apps,
                                           (gpointer)appid);
      if (!windows)
        hd_app_mgr_app_opened (app, wm_client->window->pid);

      g_hash_table_insert (hmgr->priv->shown_apps,
                           (gpointer)appid,
                           (gpointer)++windows);
    }

  /* Set portrait_* initially. */
  prop = hd_util_get_win_prop_data_and_validate (
                              wm_client->wmref->xdpy,
                              wm_client->window->xwindow,
                              hmgr->priv->atoms[HD_ATOM_WM_PORTRAIT_OK],
                              XA_CARDINAL, 32, 1, NULL);
  if (prop)
    {
      priv->portrait_not_supported = *prop == 0;
      XFree (prop);
    }
  else
    priv->portrait_not_supported = FALSE;

  prop = hd_util_get_win_prop_data_and_validate (
                       wm_client->wmref->xdpy,
                       wm_client->window->xwindow,
                       hmgr->priv->atoms[HD_ATOM_WM_PORTRAIT_REQUESTED],
                       XA_CARDINAL, 32, 1, NULL);
  if (prop)
    {
      priv->portrait_requested = *prop;
      XFree (prop);
    }

  g_debug ("portrait properties of %p: supported=%d requested=%d",
           wm_client,
           !priv->portrait_not_supported,
           priv->portrait_requested);

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
    return (hd_launcher_app_get_state (priv->app) == HD_APP_STATE_HIBERNATING);

  return FALSE;
}

gboolean
hd_comp_mgr_client_can_hibernate (HdCompMgrClient *hclient)
{
  HdCompMgrClientPrivate * priv = hclient->priv;

  return priv->can_hibernate;
}

ClutterActor *
hd_comp_mgr_client_get_actor (HdCompMgrClient *hclient)
{
  MBWMCompMgrClutterClient *cclient;
  cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (hclient);
  if (cclient)
    return mb_wm_comp_mgr_clutter_client_get_actor (cclient);
  return NULL;
}

HdLauncherApp *
hd_comp_mgr_client_get_app (HdCompMgrClient *hclient)
{
  g_return_val_if_fail(hclient, NULL);
  return hclient->priv->app;
}

const gchar *
hd_comp_mgr_client_get_app_local_name (HdCompMgrClient *hclient)
{
  HdLauncherApp *app = hclient->priv->app;
  if (app)
    return hd_launcher_item_get_local_name (HD_LAUNCHER_ITEM (app));
  return NULL;
}

static int  hd_comp_mgr_init (MBWMObject *obj, va_list vap);
static void hd_comp_mgr_class_init (MBWMObjectClass *klass);
static void hd_comp_mgr_destroy (MBWMObject *obj);
static void hd_comp_mgr_register_client (MBWMCompMgr *mgr,
                                         MBWindowManagerClient *c);
static void hd_comp_mgr_unregister_client (MBWMCompMgr *mgr, MBWindowManagerClient *c);
static void hd_comp_mgr_map_notify
                        (MBWMCompMgr *mgr, MBWindowManagerClient *c);
static void hd_comp_mgr_unmap_notify
                        (MBWMCompMgr *mgr, MBWindowManagerClient *c);
static void hd_comp_mgr_turn_on (MBWMCompMgr *mgr);
static void hd_comp_mgr_effect (MBWMCompMgr *mgr, MBWindowManagerClient *c, MBWMCompMgrClientEvent event);
static void hd_comp_mgr_home_clicked (HdCompMgr *hmgr, ClutterActor *actor);
static Bool hd_comp_mgr_client_property_changed (XPropertyEvent *event, HdCompMgr *hmgr);
static HdCompMgrClient *hd_comp_mgr_get_current_client (HdCompMgr *hmgr);

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
  cm_klass->restack           = hd_comp_mgr_restack;

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
  HdTaskNavigator      *task_nav;
  ClutterActor         *stage;
  ClutterActor         *arena;
  DBusGConnection      *system_connection;
  GError               *error = NULL;

  priv = hmgr->priv = g_new0 (HdCompMgrPrivate, 1);

  hd_atoms_init (wm->xdpy, priv->atoms);

  priv->dbus_connection = hd_dbus_init (hmgr);

  hd_gtk_style_init ();

  stage = clutter_stage_get_default ();

  /*
   * Create the home group before the switcher, so the switcher can
   * connect it's signals to it.
   */
  priv->home = g_object_new (HD_TYPE_HOME, "comp-mgr", cmgr, NULL);

  clutter_actor_set_reactive (priv->home, TRUE);

  g_signal_connect_swapped (priv->home, "background-clicked",
                            G_CALLBACK (hd_comp_mgr_home_clicked),
                            cmgr);

  clutter_actor_show (priv->home);

  task_nav = hd_task_navigator_new ();

  priv->render_manager = hd_render_manager_create(hmgr,
		                                  hd_launcher_get(),
		                                  hd_launcher_get_group(),
		                                  HD_HOME(priv->home),
						  task_nav);
  clutter_container_add_actor(CLUTTER_CONTAINER (stage),
                              CLUTTER_ACTOR(priv->render_manager));

  /* App manager must be created before switcher, but after render manager */
  priv->app_mgr = hd_app_mgr_get ();

  /* NB -- home must be constructed before constructing the switcher;
   */
  priv->switcher_group = g_object_new (HD_TYPE_SWITCHER,
				       "comp-mgr", cmgr,
				       "task-nav", task_nav,
				       NULL);

  /* Take our comp-mgr-clutter's 'arena' and hide it for now. We
   * don't want anything in it visible, as we'll take out what we want to
   * render. */
  arena = mb_wm_comp_mgr_clutter_get_arena(MB_WM_COMP_MGR_CLUTTER(cmgr));
  if (arena)
    {
      clutter_actor_hide(arena);
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

  priv->tasks_button_blockers = g_hash_table_new (NULL, NULL);

  /* Be notified about all X window property changes around here. */
  priv->property_changed_cb_id = mb_wm_main_context_x_event_handler_add (
                   cmgr->wm->main_ctx, None, PropertyNotify,
                   (MBWMXEventFunc)hd_comp_mgr_client_property_changed, cmgr);

  hd_render_manager_set_state(HDRM_STATE_HOME);


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
  g_object_unref( priv->render_manager );

  mb_wm_main_context_x_event_handler_remove (
                                     MB_WM_COMP_MGR (obj)->wm->main_ctx,
                                     PropertyNotify,
                                     priv->property_changed_cb_id);

  if (priv->mce_proxy)
    {
      g_object_unref (priv->mce_proxy);
      priv->mce_proxy = NULL;
    }
}

static HdCompMgrClient *
hd_comp_mgr_get_current_client (HdCompMgr *hmgr)
{
  HdCompMgrPrivate * priv = hmgr->priv;

  return priv->current_hclient;
}

/* Called on #PropertyNotify to handle changes to
 * _HILDON_PORTRAIT_MODE_SUPPORT and _HILDON_PORTRAIT_MODE_REQUEST. */
Bool
hd_comp_mgr_client_property_changed (XPropertyEvent *event, HdCompMgr *hmgr)
{
  static guint32 no[] = { 0 }, idontcare[] = { -1 };
  Atom pok, prq, killable, able_to_hibernate;
  guint32 *value;
  MBWindowManager *wm;
  HdCompMgrClient *cc;
  MBWindowManagerClient *c;

  g_return_val_if_fail (event->type == PropertyNotify, True);

  pok = hd_comp_mgr_get_atom (hmgr, HD_ATOM_WM_PORTRAIT_OK);
  prq = hd_comp_mgr_get_atom (hmgr, HD_ATOM_WM_PORTRAIT_REQUESTED);
  killable = hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_APP_KILLABLE);
  able_to_hibernate = hd_comp_mgr_get_atom (hmgr,
                          HD_ATOM_HILDON_ABLE_TO_HIBERNATE);

  wm = MB_WM_COMP_MGR (hmgr)->wm;

  /* Check for changes to the hibernable state. */
  if (event->atom == killable ||
      event->atom == able_to_hibernate)
    {
      HdLauncherApp *app, *current_app;
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
      if (app == current_app)
        return False;

      if (event->state == PropertyNewValue)
        hd_app_mgr_hibernatable(app, TRUE);
      else
        hd_app_mgr_hibernatable (app, FALSE);

      return False;
    }

  if (event->atom != pok && event->atom != prq)
    return True;

  /* Read the new property value. */
  value = event->state == PropertyNewValue
    ? hd_util_get_win_prop_data_and_validate (wm->xdpy, event->window,
                                              event->atom, XA_CARDINAL,
                                              32, 1, NULL)
    : event->atom == pok ? idontcare : no;
  if (!value)
    goto out0;
  if (!(c = mb_wm_managed_client_from_xwindow (wm, event->window)))
    goto out1;
  if (!c->cm_client)
    goto out1;
  cc = HD_COMP_MGR_CLIENT (c->cm_client);

  if (event->atom == pok)
    cc->priv->portrait_not_supported  = *value == 0;
  else
    cc->priv->portrait_requested      = *value != 0;

  g_debug ("portrait property of %p changed: supported=%d requested=%d", c,
           !cc->priv->portrait_not_supported, cc->priv->portrait_requested);

  /* Switch HDRM state if we need to. */
  if (STATE_IS_PORTRAIT (hd_render_manager_get_state()))
    { /* Portrait => landscape? */
      if (!*value && !hd_comp_mgr_should_be_portrait (hmgr))
        hd_render_manager_set_state_unportrait ();
    }
  else if (STATE_IS_PORTRAIT_CAPABLE (hd_render_manager_get_state()))
    { /* Landscape => portrait? */
      if (*value && hd_comp_mgr_should_be_portrait (hmgr))
        hd_render_manager_set_state_portrait ();
    }

out1:
  if (value != no && value != idontcare)
    XFree (value);
out0:
  return False;
}

/* Creates a region for anything of a type in client_mask which is
 * above the desktop - we can use this to mask off buttons by notifications,
 * etc.
 */
static XserverRegion
hd_comp_mgr_get_foreground_region(HdCompMgr *hmgr, MBWMClientType client_mask)
{
  XRectangle *rectangle;
  XserverRegion region;
  guint      i = 0, count = 0;
  MBWindowManagerClient *fg_client;
  MBWindowManager *wm = MB_WM_COMP_MGR(hmgr)->wm;

  fg_client = wm->desktop ? wm->desktop->stacked_above : 0;
  while (fg_client)
    {
      if (MB_WM_CLIENT_CLIENT_TYPE(fg_client) & client_mask)
        count++;
      fg_client = fg_client->stacked_above;
    }

  rectangle = g_new (XRectangle, count);
  fg_client = wm->desktop ? wm->desktop->stacked_above : 0;
  while (fg_client)
    {
      if (MB_WM_CLIENT_CLIENT_TYPE(fg_client) & client_mask)
        {
          MBGeometry geo = fg_client->window->geometry;
          rectangle[i].x      = geo.x;
          rectangle[i].y      = geo.y;
          rectangle[i].width  = geo.width;
          rectangle[i].height = geo.height;
        }

      fg_client = fg_client->stacked_above;
    }

  region = XFixesCreateRegion (wm->xdpy, rectangle, count);
  g_free (rectangle);
  return region;
}

void
hd_comp_mgr_setup_input_viewport (HdCompMgr *hmgr, ClutterGeometry *geom,
                                  int count)
{
  XserverRegion      region;
  Window             overlay;
  Window             clutter_window;
  MBWMCompMgr       *mgr = MB_WM_COMP_MGR (hmgr);
  MBWindowManager   *wm = mgr->wm;
  Display           *xdpy = wm->xdpy;
  ClutterActor      *stage;

  overlay = XCompositeGetOverlayWindow (xdpy, wm->root_win->xwindow);

  XSelectInput (xdpy,
                overlay,
                FocusChangeMask |
                ExposureMask |
                PropertyChangeMask |
                ButtonPressMask | ButtonReleaseMask |
                KeyPressMask | KeyReleaseMask |
                PointerMotionMask );

  /*g_debug("%s: setting viewport", __FUNCTION__);*/
  if (count > 0)
    {
      XRectangle *rectangle = g_new (XRectangle, count);
      guint      i;
      for (i = 0; i < count; i++)
        {
          rectangle[i].x      = geom[i].x;
          rectangle[i].y      = geom[i].y;
          rectangle[i].width  = geom[i].width;
          rectangle[i].height = geom[i].height;
          /*g_debug("%s: region %d, %d, %d, %d", __FUNCTION__,
              geom[i].x, geom[i].y, geom[i].width, geom[i].height);*/
        }
      region = XFixesCreateRegion (wm->xdpy, rectangle, count);
      g_free (rectangle);
    }
  else
    region = XFixesCreateRegion (wm->xdpy, NULL, 0);

  /* we must subtract the regions for any dialogs + notifications
   * from this input mask... if we are in the position of showing
   * any of them */
  if (STATE_UNGRAB_NOTES(hd_render_manager_get_state()))
    {
      XserverRegion subtract;

      subtract = hd_comp_mgr_get_foreground_region(hmgr,
          MBWMClientTypeNote | MBWMClientTypeDialog);
      XFixesSubtractRegion (wm->xdpy, region, region, subtract);
      XFixesDestroyRegion (xdpy, subtract);
    }

  XFixesSetWindowShapeRegion (xdpy,
                              overlay,
                              ShapeBounding,
                              0, 0,
                              None);

  XFixesSetWindowShapeRegion (xdpy,
                              overlay,
                              ShapeInput,
                              0, 0,
                              region);

  stage = clutter_stage_get_default();

  clutter_window = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));

  XSelectInput (xdpy,
                clutter_window,
                FocusChangeMask |
                ExposureMask |
                PropertyChangeMask |
                ButtonPressMask | ButtonReleaseMask |
                KeyPressMask | KeyReleaseMask |
                PointerMotionMask);

  XFixesSetWindowShapeRegion (xdpy,
                              clutter_window,
                              ShapeBounding,
                              0, 0,
                              None);

  XFixesSetWindowShapeRegion (xdpy,
                              clutter_window,
                              ShapeInput,
                              0, 0,
                              region);

  XFixesDestroyRegion (xdpy, region);
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


static void
hd_comp_mgr_register_client (MBWMCompMgr           * mgr,
			     MBWindowManagerClient * c)
{
  HdCompMgrPrivate              * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClass              * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));

  g_debug ("%s, c=%p ctype=%d", __FUNCTION__, c, MB_WM_CLIENT_CLIENT_TYPE (c));
  if (MB_WM_CLIENT_CLIENT_TYPE (c) == MBWMClientTypeDesktop)
    {
      priv->desktop = c;
      return;
    }

  if (parent_klass->register_client)
    parent_klass->register_client (mgr, c);
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

  /* In cases like the FKB, we don't get an unmap event, so we need to
   * update the state of blurring here. */
  hd_render_manager_update_blur_state(c);

  /* FIXME: shouldn't this be in hd_comp_mgr_unmap_notify?
   * hd_comp_mgr_unregister_client might not be called for all unmapped
   * clients */
  if (g_hash_table_remove (priv->tasks_button_blockers, c)
      && g_hash_table_size (priv->tasks_button_blockers) == 0)
    { /* Last system modal dialog being unmapped, undo evil. */
      hd_render_manager_set_reactive(TRUE);
    }

  /* Check if it's the last window for the app. */
  if (hclient->priv->app)
    {
      GQuark appid = hd_launcher_item_get_id_quark (
                      HD_LAUNCHER_ITEM (hclient->priv->app));
      guint windows = (guint)g_hash_table_lookup (priv->shown_apps,
                                                  (gpointer)appid);
      if (--windows == 0)
        {
          hd_app_mgr_app_closed (hclient->priv->app);
          g_hash_table_remove (priv->shown_apps, (gpointer)appid);
        }
      else
        {
          g_hash_table_insert (priv->shown_apps,
                               (gpointer)appid,
                               (gpointer) windows);
        }
    }

  /*
   * If the actor is an application, remove it also to the switcher
   */
  if (hclient->priv->app &&
      (hd_launcher_app_get_state (hclient->priv->app) == HD_APP_STATE_HIBERNATING) &&
      !g_hash_table_lookup (priv->hibernating_apps, (gpointer) hclient->priv->hibernation_key))
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
	           app->leader->followers && app == g_list_last (app->leader->followers)->data))
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
                    clutter_actor_show (
		  	mb_wm_comp_mgr_clutter_client_get_actor (prev));
		    g_debug ("%s: REPLACE ACTOR %p WITH %p", __func__, actor,
			     mb_wm_comp_mgr_clutter_client_get_actor (prev));
                    hd_switcher_replace_window_actor (
		        priv->switcher_group, actor,
                  	mb_wm_comp_mgr_clutter_client_get_actor (prev));
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
                                                   actor);

                  if (c->window->xwindow == hd_wm_current_app_is (NULL, 0))
                    /* We are in APP state and foreground application closed. */
                    hd_render_manager_set_state (hd_render_manager_has_apps ()
                                                 ? HDRM_STATE_TASK_NAV
                                                 : HDRM_STATE_HOME);
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
          g_object_set_data (G_OBJECT (actor),
                             "HD-MBWMCompMgrClutterClient", NULL);
          g_object_set_data (G_OBJECT (actor),
                             "HD-ApplicationId", NULL);
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

      /* Unregister applet from HomeView */
      if (applet)
        {
          HdHomeView * view = g_object_get_data (G_OBJECT (applet), "HD-HomeView");

          if (HD_IS_HOME_VIEW (view))
            hd_home_view_unregister_applet (view, applet);

          g_object_unref (applet);
        }
    }

  if (priv->current_hclient == hclient)
    priv->current_hclient = NULL;

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

static gboolean
hd_comp_mgr_is_client_screensaver (MBWindowManagerClient *c)
{
  /* FIXME: ugly. MBWM flag and change to systemui needed? */
  return c && c->window && c->window->name &&
         g_str_equal((c)->window->name, "systemui");
}

/* strange windows that never go away and mess up our checks for visibility */
gboolean
hd_comp_mgr_ignore_window (MBWindowManagerClient *c)
{
  return c && c->window && c->window->name &&
         g_str_equal((c)->window->name, "SystemUI root window");
}

static void
hd_comp_mgr_texture_update_area(HdCompMgr *hmgr,
                                int x, int y, int width, int height,
                                ClutterActor* actor)
{
  ClutterFixed offsetx = 0, offsety = 0;
  ClutterActor *parent, *it;
  HdCompMgrPrivate * priv;
  gboolean blur_update = FALSE;

  if (!CLUTTER_IS_ACTOR(actor) ||
      !CLUTTER_ACTOR_IS_VISIBLE(actor) ||
      hmgr == 0)
    return;

  priv = hmgr->priv;

  /* TFP textures are usually bundled into another group, and it is
   * this group that sets visibility - so we must check it too */
  parent = clutter_actor_get_parent(actor);
  while (parent && !CLUTTER_IS_STAGE(parent))
    {
      if (!CLUTTER_ACTOR_IS_VISIBLE(parent))
        return;
      /* if we're a child of a blur group, tell it that it has changed */
      if (TIDY_IS_BLUR_GROUP(parent) &&
          tidy_blur_group_source_buffered(parent))
        {
          /* we don't update blur on every change of
           * an application now as it causes
           * a flicker. */
          /* tidy_blur_group_set_source_changed(parent); */
          blur_update = TRUE;
        }
      parent = clutter_actor_get_parent(parent);
    }

  /* Check we're not in a mode where it's a bad idea.
   * Also skip if we're in some transition - instead just update normally
   * We also must update fully if blurring, as we must update the whole
   * blur testure. */
  if (!STATE_DO_PARTIAL_REDRAW(hd_render_manager_get_state()) ||
      blur_update)
  {
    ClutterActor *stage = clutter_stage_get_default();
    clutter_actor_queue_redraw(stage);
    return;
  }

  /* Assume no zoom/rotate is happening here as we have simple windows */
  it = actor;
  while (it && !CLUTTER_IS_STAGE(it))
    {
      ClutterFixed px,py;
      clutter_actor_get_positionu(it, &px, &py);
      offsetx += px;
      offsety += py;
      it = clutter_actor_get_parent(it);
    }

  {
    ClutterActor *stage = clutter_actor_get_stage(actor);
    /* CLUTTER_FIXED_TO_INT does no rounding, so add 0.5 here to help this */
    ClutterGeometry area = {x + CLUTTER_FIXED_TO_INT(offsetx+CFX_HALF),
                            y + CLUTTER_FIXED_TO_INT(offsety+CFX_HALF),
                            width, height};

    /*g_debug("%s: UPDATE %d, %d, %d, %d", __FUNCTION__,
            area.x, area.y, area.width, area.height);*/

    /* Queue a redraw, but without updating the whole area */
    clutter_stage_set_damaged_area(stage, area);
    hd_render_manager_queue_delay_redraw();
  }
}

/* Hook onto and X11 texture pixmap children of this actor */
static void
hd_comp_mgr_hook_update_area(HdCompMgr *hmgr, ClutterActor *actor)
{
  if (CLUTTER_IS_GROUP(actor))
    {
      gint i;
      gint n = clutter_group_get_n_children(CLUTTER_GROUP(actor));

      for (i=0;i<n;i++)
        {
          ClutterActor *child =
              clutter_group_get_nth_child(CLUTTER_GROUP(actor), i);
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

/* returns HdApp of client that was replaced, or NULL */
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

  app->stack_index = -1;  /* initially a non-stackable */

  stack_atom = hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_STACKABLE_WINDOW);

  XGetWindowProperty (wm->xdpy, win->xwindow,
                      stack_atom, 0, 1, False,
                      XA_INTEGER, &actual_type, &format,
                      &items, &left,
                      &prop);

  if (actual_type == XA_INTEGER)
    {
      MBWindowManagerClient *c_tmp;
      HdApp *old_leader = NULL;
      HdApp *last_follower = NULL;

      win_group = win->xwin_group;
      app->stack_index = (int)*prop;
      g_debug ("%s: STACK INDEX %d", __func__, app->stack_index);

      mb_wm_stack_enumerate (wm, c_tmp)
        if (c_tmp != client &&
            MB_WM_CLIENT_CLIENT_TYPE (c_tmp) == MBWMClientTypeApp &&
            HD_APP (c_tmp)->stack_index >= 0 /* == stackable window */ &&
            c_tmp->window->xwin_group == win_group)
          {
            old_leader = HD_APP (c_tmp)->leader;
            break;
          }

      if (old_leader && old_leader->followers)
        last_follower = HD_APP (g_list_last (old_leader->followers)->data);

      if (app->stack_index > 0 && old_leader &&
	  (!last_follower || last_follower->stack_index < app->stack_index))
        {
          g_debug ("%s: %p is NEW SECONDARY OF THE STACK", __FUNCTION__, app);
          app->leader = old_leader;

          app->leader->followers = g_list_append (old_leader->followers,
                                                  client);
	  if (last_follower)
	    *replaced = last_follower;
	  else
	    *replaced = old_leader;
        }
      else if (old_leader && app->stack_index <= old_leader->stack_index)
        {
          GList *l;

          app->leader = app;
          for (l = old_leader->followers; l; l = l->next)
          {
            HD_APP (l->data)->leader = app;
          }

          if (old_leader->stack_index == app->stack_index)
          {
            /* drop the old leader from the stack if we replace it */
            g_debug ("%s: DROPPING OLD LEADER %p OUT OF THE STACK", __func__,
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
            g_debug ("%s: OLD LEADER %p IS NOW A FOLLOWER", __func__, app);
            app->followers = g_list_prepend (old_leader->followers,
                                             old_leader);
            old_leader->followers = NULL;
            old_leader->leader = app;
          }
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
            g_debug ("%s: %p is FIRST FOLLOWER OF THE STACK", __func__, app);
            old_leader->followers = g_list_append (old_leader->followers, app);
	    *add_to_tn = app;
          }
          else if (f->stack_index == app->stack_index)
          {
	    if (f != app)
	    {
              g_debug ("%s: %p REPLACES A FOLLOWER OF THE STACK",
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
	      g_debug ("%s: %p is the SAME CLIENT", __FUNCTION__, app);
	    *replaced = f;
          }
          else if (f->stack_index > app->stack_index)
          {
            g_debug ("%s: %p PRECEEDS (index %d) A FOLLOWER (with index %d)"
	             " OF THE STACK", __func__, app, app->stack_index,
		     f->stack_index);
            old_leader->followers
                = g_list_insert_before (old_leader->followers, flink, app);
          }
	  else  /* f->stack_index < app->stack_index */
	  {
            if (flink->next)
	    {
              g_debug ("%s: %p PRECEEDS (index %d) A FOLLOWER (with index %d)"
	             " OF THE STACK", __func__, app, app->stack_index,
		     HD_APP (flink->next->data)->stack_index);
              old_leader->followers
                 = g_list_insert_before (old_leader->followers, flink->next,
				         app);
	    }
	    else
	    {
              g_debug ("%s: %p FOLLOWS LAST FOLLOWER OF THE STACK", __func__,
		       app);
              old_leader->followers = g_list_append (old_leader->followers,
                                                     app);
	      *add_to_tn = app;
	    }
	  }
        }
      else  /* we are the first window in the stack */
        {
          g_debug ("%s: %p is FIRST WINDOW OF THE STACK", __FUNCTION__, app);
          app->leader = app;
        }
    }

  if (prop)
    XFree (prop);

  /* all stackables have stack_index >= 0 */
  g_assert (!app->leader || (app->leader && app->stack_index >= 0));
}

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

  /*g_debug ("%s, c=%p ctype=%d", __FUNCTION__, c, MB_WM_CLIENT_CLIENT_TYPE (c));*/
  if (MB_WM_CLIENT_CLIENT_TYPE (c) == MBWMClientTypeDesktop)
    return;

  /* discard notification previews if in switcher */
  if (HD_IS_INCOMING_EVENT_PREVIEW_NOTE(c))
    {
      MBWindowManagerClient *current_app = hd_wm_determine_current_app (mgr->wm);

      if ((current_app && current_app->window &&
           mb_wm_client_window_is_state_set (current_app->window,
                                             MBWMClientWindowEWMHStateFullscreen)) ||
          STATE_DISCARD_PREVIEW_NOTE (hd_render_manager_get_state()))
        {
          g_debug ("%s. Discard notification", __FUNCTION__);
          mb_wm_client_hide (c);
          mb_wm_client_deliver_delete (c);
          return;
        }
    }

  /* #MBWMCompMgrClutterClient already has an actor, now it's time
   * for #MBWMCompMgrClutter to create its texture and bind it to
   * the window's pixmap. */
  if (parent_klass->map_notify)
    parent_klass->map_notify (mgr, c);

  /* Hide the Edit button if it is currently shown */
  if (priv->home)
    hd_home_hide_edit_button (HD_HOME (priv->home));

  /*
   * If the actor is an appliation, add it also to the switcher
   * If it is Home applet, add it to the home
   */
  ctype = MB_WM_CLIENT_CLIENT_TYPE (c);

  cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);
  hclient = HD_COMP_MGR_CLIENT (cclient);
  actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);

  g_object_set_data (G_OBJECT (actor),
		     "HD-MBWMCompMgrClutterClient", cclient);
  if (hclient->priv->app)
    g_object_set_data (G_OBJECT (actor),
           "HD-ApplicationId",
           (gchar *)hd_launcher_item_get_id (
                       HD_LAUNCHER_ITEM (hclient->priv->app)));

  hd_comp_mgr_hook_update_area(HD_COMP_MGR (mgr), actor);

  /* if we are in home_edit mode and we have stuff that will get in
   * our way and spoil our grab, move to home_edit_dlg so we can
   * look the same but remove our grab. */
  if ((hd_render_manager_get_state()==HDRM_STATE_HOME_EDIT) &&
      (ctype & (MBWMClientTypeDialog |
                HdWmClientTypeAppMenu |
                HdWmClientTypeStatusMenu)))
    {
      hd_render_manager_set_state(HDRM_STATE_HOME_EDIT_DLG);
    }

  /*
   * Leave switcher/launcher for home if we're mapping a system-modal
   * dialog, information note or confirmation note.  We need to leave now,
   * before we disable hdrm reactivity, because changing state after that
   * restores that.
   */
  if (!c->transient_for)
    if (ctype == MBWMClientTypeDialog
        || HD_IS_INFO_NOTE (c) || HD_IS_CONFIRMATION_NOTE (c))
      if (STATE_ONE_OF(hd_render_manager_get_state(),
                       HDRM_STATE_LAUNCHER | HDRM_STATE_TASK_NAV))
        hd_render_manager_set_state(HDRM_STATE_HOME);

  /* Do evil.  Do we need to block the Tasks button? */
  if ((ctype == MBWMClientTypeDialog ||
      (ctype == MBWMClientTypeNote &&
       HD_NOTE (c)->note_type != HdNoteTypeIncomingEvent))
      && hd_util_is_client_system_modal (c))
    {
      if (g_hash_table_size (priv->tasks_button_blockers) == 0)
        { /* First system modal client, allow it to receive events and
           * block the switcher buttons. */
          hd_render_manager_set_reactive(FALSE);
        }

      /*
       * Save the client's address and undo evil when the last of its
       * kind is unregistered.  We need to do this way because maps
       * and unmaps arrive unreliably, for example we don't get unmap
       * notification for VKB.  In other cases we may receive two
       * maps for the same client.
       * FIXME: I'm sure the extra/missing maps and unmaps are our bugs... (KH)
       */
      g_hash_table_insert (priv->tasks_button_blockers, c, GINT_TO_POINTER(1));
    }

  /* Hide status menu if any window except an applet is mapped */
  if (priv->status_menu_client &&
      ctype != HdWmClientTypeHomeApplet)
    mb_wm_client_deliver_delete (priv->status_menu_client);

  if (ctype == HdWmClientTypeHomeApplet)
    {
      HdHomeApplet * applet  = HD_HOME_APPLET (c);
      char         * applet_id = applet->applet_id;

      if (strcmp (OPERATOR_APPLET_ID, applet_id) != 0)
        {
          /* Normal applet */
          g_object_set_data_full (G_OBJECT (actor), "HD-applet-id",
                                  g_strdup (applet_id), (GDestroyNotify) g_free);

          hd_home_add_applet (HD_HOME (priv->home), actor);
        }
      else
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
                       HDRM_STATE_LAUNCHER | HDRM_STATE_TASK_NAV))
        hd_render_manager_set_state(HDRM_STATE_HOME);
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
      /* If we're in a state that needs a grab,
       * go back to home as the dialog will need the grab. */
      if (STATE_NEED_GRAB(hd_render_manager_get_state()))
        hd_render_manager_set_state(HDRM_STATE_HOME);
      return;
    }
  else if (ctype == MBWMClientTypeNote)
    {
      if (HD_NOTE (c)->note_type == HdNoteTypeIncomingEvent)
        {
          /* Unparent @actor from its desktop and leave it
           * up to the swithcer to show it wherever it wants. */
          ClutterActor *parent = clutter_actor_get_parent (actor);
          clutter_container_remove_actor (CLUTTER_CONTAINER (parent), actor);
          hd_switcher_add_notification (priv->switcher_group,
                                        HD_NOTE (c));
        }
      else if (c->transient_for)
        hd_switcher_add_dialog (priv->switcher_group, c, actor);
      else
        {
          /* Notes need to be pulled out right infront of the blur group
           * manually, as they are not given focus */
          hd_render_manager_add_to_front_group(actor);
        }
      /* Send dbus request to mce to turn display backlight on for banner notes */
      if (priv->mce_proxy && HD_IS_BANNER_NOTE (c))
        {
          g_debug ("%s. Call %s",
                   __FUNCTION__,
                   MCE_DISPLAY_ON_REQ);
          dbus_g_proxy_call_no_reply (priv->mce_proxy, MCE_DISPLAY_ON_REQ,
                                      G_TYPE_INVALID, G_TYPE_INVALID);
        }
      return;
    }
  else if (ctype == MBWMClientTypeDialog)
    {
      if (c->transient_for)
        hd_switcher_add_dialog (priv->switcher_group, c, actor);
      return;
    }
  else if (c->window->net_type ==
	   c->wmref->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_POPUP_MENU])
    {
      MBWindowManagerClient *transfor;

      if ((transfor = hd_comp_mgr_get_client_transient_for (c)) != NULL)
          hd_switcher_add_dialog_explicit (HD_SWITCHER (priv->switcher_group),
                                           c, actor, transfor);
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
  HdApp *app = HD_APP (c), *to_replace = NULL, *add_to_tn = NULL;

  hd_comp_mgr_handle_stackable (c, &to_replace, &add_to_tn);

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
      HdTaskNavigator *tasknav;
      MBWMCompMgrClutterClient *cclient;
      gboolean in_tasknav;

      tasknav = HD_TASK_NAVIGATOR (hd_switcher_get_task_navigator (
                                   priv->switcher_group));
      cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);
      if (hd_task_navigator_has_window (tasknav,
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
              if (hd_task_navigator_has_window (tasknav,
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

  if (to_replace && topmost)
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
        clutter_actor_hide (old_actor);
        /* and make sure we're in app mode and not transitioning as
         * we'll want to show this new app right away*/
        if (!STATE_IS_APP(hd_render_manager_get_state()))
          hd_render_manager_set_state(HDRM_STATE_APP);
        hd_render_manager_stop_transition();
        /* This forces the decors to be redone, taking into account the
         * stack index. */
        mb_wm_client_theme_change (c);
      }
    }
  else if (add_to_tn)
    {
      g_debug ("%s: ADD ACTOR %p", __func__, actor);
      hd_switcher_add_window_actor (priv->switcher_group, actor);
      /* and make sure we're in app mode and not transitioning as
       * we'll want to show this new app right away*/
      if (!STATE_IS_APP(hd_render_manager_get_state()))
        hd_render_manager_set_state(HDRM_STATE_APP);
      hd_render_manager_stop_transition();
      /* This forces the decors to be redone, taking into account the
       * stack index. */
      mb_wm_client_theme_change (c);
    }

  if (!(c->window->ewmh_state & MBWMClientWindowEWMHStateSkipTaskbar)
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

static void
hd_comp_mgr_unmap_notify (MBWMCompMgr *mgr, MBWindowManagerClient *c)
{
  HdCompMgrPrivate          * priv = HD_COMP_MGR (mgr)->priv;
  MBWMClientType            c_type = MB_WM_CLIENT_CLIENT_TYPE (c);
  MBWMCompMgrClutterClient *cclient;
  MBWindowManagerClient    *transfor = 0;

  cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);

  /* if we are in home_edit_dlg mode, check and see if there is stuff
   * that would spoil our grab now - and if not, return to home_edit mode.
   * TODO: could this code be modified to set blur correctly? */
  if (hd_render_manager_get_state()==HDRM_STATE_HOME_EDIT_DLG)
    {
      gboolean grab_spoil = FALSE;
      MBWindowManagerClient *above = mgr->wm->desktop;
      if (above) above = above->stacked_above;
      while (above)
        {
          if (above != c &&
              !hd_comp_mgr_ignore_window(above) &&
              MB_WM_CLIENT_CLIENT_TYPE(above)!=HdWmClientTypeHomeApplet &&
              MB_WM_CLIENT_CLIENT_TYPE(above)!=HdWmClientTypeStatusArea)
            grab_spoil = TRUE;
          above = above->stacked_above;
        }
      if (!grab_spoil)
        hd_render_manager_set_state(HDRM_STATE_HOME_EDIT);
    }

  if (HD_IS_INCOMING_EVENT_NOTE(c))
    {
      hd_switcher_remove_notification (priv->switcher_group,
                                       HD_NOTE (c));
      return;
    }
  else if (c_type == MBWMClientTypeNote || c_type == MBWMClientTypeDialog)
    transfor = c->transient_for;
  else if (c->window->net_type ==
	   c->wmref->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_POPUP_MENU])
    transfor = hd_comp_mgr_get_client_transient_for (c);
  else
    return;

  if (transfor)
    { /* Remove application-transient dialogs from the switcher. */
      ClutterActor *actor;
      actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);
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

  /*HdCompMgrPrivate *priv = HD_COMP_MGR (mgr)->priv;*/
  if (event == MBWMCompMgrClientEventUnmap)
    {
      if (c_type == HdWmClientTypeStatusMenu)
        hd_transition_popup(hmgr, c, MBWMCompMgrClientEventUnmap);
      else if (HD_IS_INCOMING_EVENT_PREVIEW_NOTE(c))
        hd_transition_notification(hmgr, c, MBWMCompMgrClientEventUnmap);
      else if (c_type == MBWMClientTypeDialog)
        hd_transition_popup(hmgr, c, MBWMCompMgrClientEventUnmap);
      else if (c_type == MBWMClientTypeNote)
        hd_transition_fade(hmgr, c, MBWMCompMgrClientEventUnmap);
      else if (c_type == MBWMClientTypeApp)
        if (!hd_comp_mgr_is_client_screensaver(c))
          {
            /* Look if it's a stackable window. */
            HdApp *app = HD_APP (c);
            if (app->stack_index > 0 && app->leader != app)
              {
                hd_transition_subview(hmgr, c,
                                      MB_WM_CLIENT(app->leader),
                                      MBWMCompMgrClientEventUnmap);
              }
            else
              hd_transition_close_app (hmgr, c);
            app->map_effect_before = FALSE;
          }
    }
  else if (event == MBWMCompMgrClientEventMap)
    {
      if (c_type == HdWmClientTypeStatusMenu)
        hd_transition_popup(hmgr, c, MBWMCompMgrClientEventMap);
      else if (c_type == MBWMClientTypeDialog)
        hd_transition_popup(hmgr, c, MBWMCompMgrClientEventMap);
      else if (HD_IS_INCOMING_EVENT_PREVIEW_NOTE(c))
        hd_transition_notification(hmgr, c, MBWMCompMgrClientEventMap);
      else if (c_type == MBWMClientTypeNote)
        hd_transition_fade(hmgr, c, MBWMCompMgrClientEventMap);
      else if (c_type == MBWMClientTypeApp)
        if (!hd_comp_mgr_is_client_screensaver(c))
          {
            /* Look if it's a stackable window. We don't do the subview
             * animation again if we have had a mapping without an unmap,
             * which is what happens in the Image Viewer when the same
             * window goes from Fullscreen to Windowed */
            HdApp *app = HD_APP (c);
            if (app->stack_index > 0 && !app->map_effect_before)
              hd_transition_subview(hmgr, c,
                                    MB_WM_CLIENT(app->leader),
                                    MBWMCompMgrClientEventMap);
            /* We're now showing this app, so remove our app
             * starting screen if we had one */
            hd_launcher_window_created();
            app->map_effect_before = TRUE;
          }
    }

  /* ignore this window when we set blur state if we're unmapping
   * - because it will be just about to disappear */
  hd_render_manager_update_blur_state(
      (event == MBWMCompMgrClientEventUnmap) ? c : NULL);
}

void
hd_comp_mgr_restack (MBWMCompMgr * mgr)
{
  HdCompMgrPrivate         * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClass         * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));

  /* g_debug ("%s", __FUNCTION__); */

  /* Hide the Edit button if it is currently shown */
  if (priv->home)
    hd_home_hide_edit_button (HD_HOME (priv->home));

  /*
   * We use the parent class restack() method to do the stacking, but as our
   * switcher shares actors with the CM, we cannot run this when the switcher
   * is showing, or an unmap effect is in progress; instead we set a flag, and
   * let the switcher request stack sync when it closes.
   */

  if (STATE_NEED_TASK_NAV(hd_render_manager_get_state()))
    {
      priv->stack_sync = TRUE;
    }
  else
    {
      if (parent_klass->restack)
	parent_klass->restack (mgr);

      /*MBWindowManager *wm = mgr->wm;
      MBWindowManagerClient *c = wm->stack_bottom;
      int i = 0;
      while (c)
        {
          ClutterActor *a = 0;

          if (c->cm_client)
            a = mb_wm_comp_mgr_clutter_client_get_actor(MB_WM_COMP_MGR_CLUTTER_CLIENT(c->cm_client));
          g_debug("%s: STACK %d : %s %s %s", __FUNCTION__, i,
              c->name?c->name:"?",
              (a && clutter_actor_get_name(a)) ?  clutter_actor_get_name(a) : "?",
              (wm->desktop==c) ? "DESKTOP" : "");
          i++;
          c = c->stacked_above;
        }*/

      /* Update _MB_CURRENT_APP_WINDOW if we're ready and it's changed. */
      if (mgr->wm && mgr->wm->root_win && mgr->wm->desktop)
        {
          MBWindowManagerClient *current_client =
                                  hd_wm_determine_current_app (mgr->wm);

          HdCompMgrClient *new_current_hclient =
            HD_COMP_MGR_CLIENT (current_client->cm_client);
          if (new_current_hclient != priv->current_hclient)
            {
              HdLauncherApp *old_current_app;
              HdLauncherApp *new_current_app;

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
                  if (new_current_app)
                    hd_app_mgr_hibernatable (new_current_app, FALSE);
                }

              priv->current_hclient = new_current_hclient;
            }

          hd_wm_current_app_is (mgr->wm, current_client->window->xwindow);
          /* If we have an app as the current client and we're not in
           * app mode - enter app mode. */
          if (!(MB_WM_CLIENT_CLIENT_TYPE(current_client) &
                                         MBWMClientTypeDesktop) &&
              !STATE_IS_APP(hd_render_manager_get_state()))
            hd_render_manager_set_state(HDRM_STATE_APP);

        }

      hd_render_manager_restack();

      /* Now that HDRM has sorted out the visibilities see if we need to
       * switch to/from portrait mode because of a new window. */
      if (!hd_render_manager_is_changing_state ())
        {
          /*
           * Change state if necessate:
           * APP <=> APP_PORTRAIT and HOME <=> HOME_PORTRAIT
           */
          if (STATE_IS_PORTRAIT_CAPABLE (hd_render_manager_get_state ()))
            { /* Landscape -> portrait? */
              if (hd_comp_mgr_should_be_portrait (HD_COMP_MGR (mgr)))
                hd_render_manager_set_state_portrait ();
            }
          else if (STATE_IS_PORTRAIT(hd_render_manager_get_state ()))
            { /* Portrait -> landscape? */
              if (!hd_comp_mgr_should_be_portrait (HD_COMP_MGR (mgr)))
                hd_render_manager_set_state_unportrait ();
            }
        }
    }
}

void
hd_comp_mgr_sync_stacking (HdCompMgr * hmgr)
{
  HdCompMgrPrivate * priv = hmgr->priv;

  /*
   * If the stack_sync flag is set, force restacking of the CM actors
   */
  if (priv->stack_sync)
    {
      priv->stack_sync = FALSE;
      hd_comp_mgr_restack (MB_WM_COMP_MGR (hmgr));
    }
}

static void
hd_comp_mgr_home_clicked (HdCompMgr *hmgr, ClutterActor *actor)
{
  hd_render_manager_set_state(HDRM_STATE_HOME);
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

      hd_switcher_remove_window_actor (priv->switcher_group,
				       actor);

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

  if (h_client->priv->app)
    {
      /* Notify HdAppMgr that the application has been closed. */
      hd_app_mgr_app_closed (h_client->priv->app);
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
  hd_app_mgr_wakeup (hclient->priv->app);
}

void
hd_comp_mgr_kill_all_apps (HdCompMgr *hmgr)
{
  GList *apps;

  apps = hd_launcher_tree_get_items (hd_launcher_get_tree(), NULL);
  for (; apps; apps = apps->next)
    {
      HdLauncherApp *app;

      if (hd_launcher_item_get_item_type (apps->data)
          != HD_APPLICATION_LAUNCHER)
        continue;

      app = HD_LAUNCHER_APP (apps->data);
      if (hd_launcher_app_get_state (app) != HD_APP_STATE_SHOWN)
        continue;

      hd_app_mgr_kill (app);
    }
}

/* Does any visible client request portrait mode?
 * Are all of them prepared for it? */
gboolean
hd_comp_mgr_should_be_portrait (HdCompMgr *hmgr)
{
  gboolean any_requests;
  MBWindowManager *wm;
  MBWindowManagerClient *cs, *ct;

  any_requests = FALSE;
  wm = MB_WM_COMP_MGR (hmgr)->wm;
  for (cs = wm->stack_top; cs && cs != wm->desktop; cs = cs->stacked_below)
    {
      if (cs == hmgr->priv->status_area_client)
        /* It'll be blocked anyway. */
        continue;
      if (MB_WM_CLIENT_CLIENT_TYPE (cs)
          & (HdWmClientTypeAppMenu | MBWMClientTypeMenu))
        /* Menus are not transient for their window nor they claim
         * portrait layout support.  Let's just assume they can. */
        continue;
      if (hd_comp_mgr_is_client_screensaver (cs))
        continue;
      if (hd_comp_mgr_ignore_window (cs))
        continue;
      if (!hd_render_manager_is_client_visible (cs)
          && !(cs->window
               && hd_wm_current_app_is (NULL, 0) == cs->window->xwindow))
        /* Ignore invisibles except if it's the current application. */
        continue;

      /* Let's suppose @ct requests portrait layout if any of the windows
       * it is transient for does. */
      for (ct = cs; ct; ct = ct->transient_for)
        {
          if (HD_COMP_MGR_CLIENT (ct->cm_client)->priv->portrait_not_supported
              && hd_render_manager_is_client_visible (ct))
            /* @cs is visible and doesn't support portrait layout. */
            /* Visibility had to be rechecked because @cs may not be visible. */
            return FALSE;
          any_requests |= HD_COMP_MGR_CLIENT (ct->cm_client)->priv->portrait_requested;
        }
    }

  return any_requests;
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

static void
dump_clutter_actor_tree (ClutterActor *actor, GString *indent)
{
  const gchar *name;
  MBWMCompMgrClient *cmgrc;
  ClutterGeometry geo;

  if (!indent)
    indent = g_string_new ("");

  if (!(name = clutter_actor_get_name (actor)) && CLUTTER_IS_LABEL (actor))
    name = clutter_label_get_text (CLUTTER_LABEL (actor));
  cmgrc = g_object_get_data(G_OBJECT (actor), "HD-MBWMCompMgrClutterClient");

  clutter_actor_get_geometry (actor, &geo);
  g_debug ("actor[%u]: %s%p (type=%s, name=%s, win=0x%lx), "
           "size: %ux%u%+d%+d, visible: %d, reactive: %d",
           indent->len, indent->str, actor,
           G_OBJECT_TYPE_NAME (actor), name,
           cmgrc && cmgrc->wm_client && cmgrc->wm_client->window
               ? cmgrc->wm_client->window->xwindow : 0,
           geo.width, geo.height, geo.x, geo.y,
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

void
hd_comp_mgr_dump_debug_info (const gchar *tag)
{
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

      geo = mbwmc->window ? mbwmc->window->geometry : mbwmc->frame_geometry;
      g_debug (" client=%p, type=%d, size=%dx%d%+d%+d, trfor=%p, layer=%d, "
               "win=0x%lx, group=0x%lx, name=%s",
               mbwmc, MB_WM_CLIENT_CLIENT_TYPE (mbwmc), MBWM_GEOMETRY (&geo),
               mbwmc->transient_for, mbwmc->stacking_layer,
               mbwmc->window ? mbwmc->window->xwindow : 0,
               mbwmc->window ? mbwmc->window->xwin_group : 0,
               mbwmc->window ? mbwmc->window->name : "<unset>");
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
}

void hd_comp_mgr_set_effect_running(HdCompMgr *hmgr, gboolean running)
{
  /* We don't need this now, but this might be useful in the future.
   * It is called when any transition begins or ends. */
}

guint
hd_comp_mgr_get_current_screen_width (void)
{
  return mb_wm_root_window_get (NULL)->wm->xdpy_width;
}

guint
hd_comp_mgr_get_current_screen_height(void)
{
  return mb_wm_root_window_get (NULL)->wm->xdpy_height;
}
