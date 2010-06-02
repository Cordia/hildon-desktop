/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Johan Bilien <johan.bilien@nokia.com>
 *          Tomas Frydrych <tf@o-hand.com>
 *          Kimmo Hämäläinen <kimmo.hamalainen@nokia.com>
 *          80% of this file should be cut and pasted to /dev/null
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
#include "hd-switcher.h"
#include "hd-task-navigator.h"
#include "hd-app-mgr.h"
#include "hd-launcher.h"
#include "hd-launcher-app.h"
#include "hd-comp-mgr.h"
#include "hd-util.h"
#include "hd-home.h"
#include "hd-gtk-utils.h"
#include "hd-render-manager.h"
#include "hd-title-bar.h"
#include "hd-wm.h"

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>

#include <matchbox/core/mb-wm.h>
#include <matchbox/core/mb-wm-object.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr-clutter.h>

#include <dbus/dbus-glib.h>

#include <hildon/hildon-banner.h>

#define LONG_PRESS_DUR 1

enum
{
  PROP_COMP_MGR = 1,
  PROP_TASK_NAV = 2
};

struct _HdSwitcherPrivate
{
  HdLauncher           *launcher;
  HdTaskNavigator      *task_nav;

  DBusGConnection      *connection;
  DBusGProxy           *hildon_home_proxy;

  MBWMCompMgrClutter   *comp_mgr;

  gboolean long_press;
  gboolean pressed;
  guint press_timeout;
  guint wakeup_timeout;
};

/* used for a callback to trigger the relaunch animation */
typedef struct _HdSwitcherRelaunchAppData {
  HdSwitcher *switcher;
  ClutterActor *actor;
  HdLauncherApp *app;
} HdSwitcherRelaunchAppData;

static void hd_switcher_class_init (HdSwitcherClass *klass);
static void hd_switcher_init       (HdSwitcher *self);
static void hd_switcher_dispose    (GObject *object);
static void hd_switcher_finalize   (GObject *object);

static void hd_switcher_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec);

static void hd_switcher_get_property (GObject      *object,
                                      guint         prop_id,
                                      GValue       *value,
                                      GParamSpec   *pspec);

static void hd_switcher_constructed (GObject *object);

static void hd_switcher_clicked (HdSwitcher *switcher);
static gboolean hd_switcher_press (HdSwitcher *switcher);
static gboolean hd_switcher_leave (HdSwitcher *switcher);

static void hd_switcher_item_closed (HdSwitcher *switcher,
                                     ClutterActor *actor);


static void hd_switcher_group_background_clicked (HdSwitcher   *switcher,
						  ClutterActor *actor);

static gboolean hd_switcher_notification_clicked (HdSwitcher *switcher,
                                                  HdNote *note);
static gboolean hd_switcher_notification_closed (HdSwitcher *switcher,
                                                 HdNote *note);

static void hd_switcher_launcher_cat_launched (HdLauncher *launcher,
                                               HdSwitcher *switcher);
static void hd_switcher_launcher_cat_hidden (HdLauncher *launcher,
                                             HdSwitcher *switcher);
static void hd_switcher_launch_app (HdSwitcher *switcher,
                                    HdLauncherApp *app,
                                    gpointer data);
static void hd_switcher_relaunch_app (HdSwitcher *switcher,
                                      HdLauncherApp *app,
                                      gpointer data);
static void hd_switcher_loading_fail (HdSwitcher *switcher,
                                      HdLauncherApp *app,
                                      gpointer data);
static void hd_switcher_app_crashed (HdSwitcher *switcher,
                                     HdLauncherApp *app,
                                     gpointer data);
static void hd_switcher_insufficient_memory(HdSwitcher *switcher,
                                            gboolean waking_up,
                                            gpointer data);
static void
hd_switcher_zoom_in_complete (ClutterActor *actor, HdSwitcher *switcher);

G_DEFINE_TYPE (HdSwitcher, hd_switcher, G_TYPE_OBJECT);

static void
hd_switcher_class_init (HdSwitcherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec   *pspec;

  g_type_class_add_private (klass, sizeof (HdSwitcherPrivate));

  object_class->dispose      = hd_switcher_dispose;
  object_class->finalize     = hd_switcher_finalize;
  object_class->set_property = hd_switcher_set_property;
  object_class->get_property = hd_switcher_get_property;
  object_class->constructed  = hd_switcher_constructed;

  pspec = g_param_spec_pointer ("comp-mgr",
				"Composite Manager",
				"MBWMCompMgrClutter Object",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class,
                                   PROP_COMP_MGR,
                                   pspec);

  pspec = g_param_spec_pointer ("task-nav",
				"Task Navigator",
				"HdTaskNavigator Object",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class,
                                   PROP_TASK_NAV,
                                   pspec);
}

static void
launcher_back_button_clicked (HdLauncher *launcher,
                              gpointer *data)
{
  g_debug("launcher_back_button_clicked\n");
  hd_render_manager_set_state(HDRM_STATE_TASK_NAV);
}

static void
hd_switcher_constructed (GObject *object)
{
  GError            *error = NULL;
  HdSwitcherPrivate *priv = HD_SWITCHER (object)->priv;

  g_signal_connect_swapped (hd_render_manager_get_title_bar(),
                            "clicked-top-left",
                            G_CALLBACK (hd_switcher_clicked),
                            object);
  g_signal_connect_swapped (hd_render_manager_get_title_bar(),
                              "press-top-left",
                              G_CALLBACK (hd_switcher_press),
                              object);
  g_signal_connect_swapped (hd_render_manager_get_title_bar(),
                              "leave-top-left",
                              G_CALLBACK (hd_switcher_leave),
                              object);
  /* Task Launcher events */
  priv->launcher = hd_launcher_get ();
  g_signal_connect (priv->launcher, "launcher-hidden",
                    G_CALLBACK (launcher_back_button_clicked),
                    object);
  g_signal_connect (priv->launcher, "category-launched",
                    G_CALLBACK (hd_switcher_launcher_cat_launched),
                    object);
  g_signal_connect (priv->launcher, "category-hidden",
                    G_CALLBACK (hd_switcher_launcher_cat_hidden),
                    object);
  /* App manager events. */
  g_signal_connect_swapped (hd_app_mgr_get (), "application-launched",
                    G_CALLBACK (hd_switcher_launch_app),
                    object);
  g_signal_connect_swapped (hd_app_mgr_get (), "application-relaunched",
                    G_CALLBACK (hd_switcher_relaunch_app),
                    object);
  g_signal_connect_swapped (hd_app_mgr_get (), "application-loading-fail",
                    G_CALLBACK (hd_switcher_loading_fail),
                    object);
  g_signal_connect_swapped (hd_app_mgr_get (), "application-crashed",
                    G_CALLBACK (hd_switcher_app_crashed),
                    object);
  g_signal_connect_swapped (hd_app_mgr_get (), "not-enough-memory",
                    G_CALLBACK (hd_switcher_insufficient_memory),
                    object);

  /* Task navigator events */
  g_signal_connect_swapped (priv->task_nav, "thumbnail-clicked",
                            G_CALLBACK (hd_switcher_item_selected),
                            object);
  g_signal_connect_swapped (priv->task_nav, "thumbnail-closed",
                            G_CALLBACK (hd_switcher_item_closed),
                            object);
  g_signal_connect_swapped (priv->task_nav, "notification-clicked",
                            G_CALLBACK (hd_switcher_notification_clicked),
                            object);
  g_signal_connect_swapped (priv->task_nav, "notification-closed",
                            G_CALLBACK (hd_switcher_notification_closed),
                            object);
  g_signal_connect_swapped (priv->task_nav, "background-clicked",
                            G_CALLBACK (hd_switcher_group_background_clicked),
                            object);



  /* Connect to D-Bus */
  priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (G_UNLIKELY (error))
    {
      g_warning ("Could not connect to Session Bus. %s", error->message);
      g_error_free (error);
      error = NULL;
    }
  else
    {
      priv->hildon_home_proxy = dbus_g_proxy_new_for_name (priv->connection,
                                	"com.nokia.HildonHome",
                                        "/com/nokia/HildonHome",
                                        "com.nokia.HildonHome");
    }
}

static void
hd_switcher_init (HdSwitcher *self)
{
  HdSwitcherPrivate *priv;

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
					    HD_TYPE_SWITCHER,
					    HdSwitcherPrivate);
}

static void
hd_switcher_dispose (GObject *object)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (object)->priv;

  if (priv->press_timeout)
    {
      g_source_remove (priv->press_timeout);
      priv->press_timeout = 0;
    }

  if (priv->wakeup_timeout)
    {
      g_source_remove (priv->wakeup_timeout);
      priv->wakeup_timeout = 0;
    }

  G_OBJECT_CLASS (hd_switcher_parent_class)->dispose (object);
}

static void
hd_switcher_finalize (GObject *object)
{
  G_OBJECT_CLASS (hd_switcher_parent_class)->finalize (object);
}

static void
hd_switcher_set_property (GObject       *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (object)->priv;

  switch (prop_id)
    {
    case PROP_COMP_MGR:
      priv->comp_mgr = g_value_get_pointer (value);
      break;
    case PROP_TASK_NAV:
      priv->task_nav = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hd_switcher_get_property (GObject      *object,
                          guint         prop_id,
                          GValue       *value,
                          GParamSpec   *pspec)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (object)->priv;

  switch (prop_id)
    {
    case PROP_COMP_MGR:
      g_value_set_pointer (value, priv->comp_mgr);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hd_switcher_clicked (HdSwitcher *switcher)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  g_debug("entered hd_switcher_clicked: state=%s\n",
        hd_render_manager_get_state_str());

  if (priv->long_press || STATE_IS_EDIT_MODE (hd_render_manager_get_state ()))
    return;

  if (priv->press_timeout)
    {
      g_source_remove (priv->press_timeout);
      priv->press_timeout = 0;
    }

  if (!priv->pressed)
    return;
  priv->pressed = FALSE;
  /*
   * We have the following scenarios:
   *
   * 1. Showing Switcher: the active button is the launch button; we
   *    shutdown the switcher and execute the launcher instead.
   *
   * 2. Showing Launcher: the active button is the switcher button; we shutdown
   *    the the launcher and execute the switcher instead.
   *
   * 3. Neither switcher no launcher visible:
   *    a. We are in switcher mode: we launch the switcher.
   *    b. We are in launcher mode: we launch the launcher.
   */
  if (hd_render_manager_get_state() == HDRM_STATE_TASK_NAV)
    {
      g_debug("hd_switcher_clicked: show launcher, switcher=%p\n", switcher);

      hd_render_manager_set_state(HDRM_STATE_LAUNCHER);
    }
  else if (hd_render_manager_get_state() == HDRM_STATE_LAUNCHER)
    {
      g_debug("hd_switcher_clicked: show switcher, switcher=%p\n", switcher);
      hd_render_manager_set_state(HDRM_STATE_TASK_NAV);
    }
  else if (hd_task_navigator_is_empty())
    hd_render_manager_set_state(HDRM_STATE_LAUNCHER);
  else
    hd_render_manager_set_state(HDRM_STATE_TASK_NAV);
}

static gboolean
press_timeout_cb (gpointer data)
{
  HdSwitcher *switcher = HD_SWITCHER (data);
  HdSwitcherPrivate *priv = switcher->priv;

  if (priv->press_timeout)
    priv->press_timeout = 0;

  priv->long_press = TRUE;

  if (STATE_IS_APP (hd_render_manager_get_state ()))
    hd_render_manager_set_state (HDRM_STATE_HOME);

  return FALSE;
}

static gboolean
hd_switcher_press (HdSwitcher *switcher)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  priv->pressed = TRUE;

  if (priv->press_timeout)
    g_source_remove (priv->press_timeout);

  priv->long_press = FALSE;

  if (STATE_IS_APP (hd_render_manager_get_state ()))
    {
      priv->press_timeout = g_timeout_add_seconds (LONG_PRESS_DUR,
                                                   press_timeout_cb,
                                                   switcher);
    }
  else if (hd_render_manager_get_state() == HDRM_STATE_HOME_EDIT)
    {
      HdHome *home = HD_HOME (hd_comp_mgr_get_home (HD_COMP_MGR (priv->comp_mgr)));

      g_debug("%s: show edit menu, switcher=%p\n", __FUNCTION__, switcher);
      if (priv->hildon_home_proxy)
        dbus_g_proxy_call_no_reply (priv->hildon_home_proxy, "ShowEditMenu",
                                    G_TYPE_UINT, hd_home_get_current_view_id (home),
                                    G_TYPE_INVALID);
      /* Add an input blocker while we wait for hildon home to bring up
       * a window. Fixes NB#116375, NB#104558 */
      hd_render_manager_add_input_blocker();
    }

  return TRUE;
}

static gboolean
hd_switcher_leave (HdSwitcher *switcher)
{
  HdSwitcherPrivate *priv = switcher->priv;

  if (priv->press_timeout)
    {
      g_source_remove (priv->press_timeout);
      priv->press_timeout = 0;
    }

  return TRUE;
}

static void
hd_switcher_launcher_cat_launched (HdLauncher *launcher,
                                   HdSwitcher *switcher)
{
  hd_render_manager_set_launcher_subview(TRUE);
}

static void
hd_switcher_launcher_cat_hidden (HdLauncher *launcher,
                                 HdSwitcher *switcher)
{
  hd_render_manager_set_launcher_subview(FALSE);
}

/* called back after Task Navigator's zoom in transition has finished,
 * this sets app_top. We can't do this normally when zooming in on an app
 * because it's specced only for when relaunching. And we can't do it when
 * we zoom in, because hd-wm pulls us out of the task navigator abruptly */
static void
hd_switcher_relaunched_app_callback(ClutterActor *actor,
                                    HdSwitcherRelaunchAppData *data)
{
  hd_switcher_zoom_in_complete(data->actor, data->switcher);
  hd_app_mgr_relaunch_set_top(data->app);
  g_slice_free1(sizeof(*data), data);
}

/* called back after the transition to TASK_NAV has finished, and
 * this starts the zoom in on the thumbnail */
static void
hd_switcher_relaunch_app_callback(HdSwitcherRelaunchAppData *data)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (data->switcher)->priv;

  g_signal_handlers_disconnect_by_func(
      hd_render_manager_get(),
      G_CALLBACK(hd_switcher_relaunch_app_callback),
      data);

  /*
   * This is supposed to be called when a launcher->switcher blur transition
   * is complete.  However, we might not be in switcher state because...
   * of several reaons.  One of them is that a window was just mapped during
   * the transition and our state is APP now.  Anyway, tana doesn't tolerate
   * (for reasons of pedantry) requests for zooming in inappropriate states.
   */
  if (hd_render_manager_get_state () == HDRM_STATE_TASK_NAV)
    {
      hd_task_navigator_zoom_in (priv->task_nav, data->actor,
                  (ClutterEffectCompleteFunc)hd_switcher_relaunched_app_callback,
                  data);
    }
}

static void
hd_switcher_launch_app (HdSwitcher *switcher,
                        HdLauncherApp *app,
                        gpointer data)
{
  /* Add a full-screen input blocker to stop the user clicking
   * really quickly and starting something else. This will be removed
   * when a window appears, or after a timeout.   */
  hd_render_manager_add_input_blocker();

  hd_launcher_transition_app_start (app);
}

static void
hd_switcher_relaunch_app (HdSwitcher *switcher,
                          HdLauncherApp *app,
                          gpointer data)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  ClutterActor *actor;
  HdSwitcherRelaunchAppData *cb_data;

  /* Get the actor for the window and act as if the user had selected it. */
  actor = hd_task_navigator_find_app_actor (priv->task_nav,
              hd_launcher_item_get_id (HD_LAUNCHER_ITEM (app)));
  if (!actor)
    {
      g_debug ("%s: Weird! Trying to relaunch a non-existing app.\n",
          __FUNCTION__);
      return;
    }

  cb_data = g_slice_alloc(sizeof(*cb_data));
  cb_data->app = app;
  cb_data->actor = actor;
  cb_data->switcher = switcher;
  g_signal_connect_swapped(hd_render_manager_get(), "transition-complete",
        G_CALLBACK(hd_switcher_relaunch_app_callback),
        cb_data);

  /* Go to the task switcher view. After this is done, we'll do
   * our zoom in on the app view the callback above */
  hd_render_manager_set_state(HDRM_STATE_TASK_NAV);
}

/*
 * Called when HdAppMgr couldn't load an application.
 */
static void
hd_switcher_loading_fail (HdSwitcher *switcher,
                          HdLauncherApp *app,
                          gpointer data)
{
  hd_launcher_stop_loading_transition ();
  if (STATE_IS_LOADING(hd_render_manager_get_state ()))
    {
      if (hd_task_navigator_has_apps ())
        hd_render_manager_set_state (HDRM_STATE_TASK_NAV);
      else
        hd_render_manager_set_state (HDRM_STATE_HOME);
    }

#if 0 /* removed as of NB#140674 */
  GtkWidget* banner = hildon_banner_show_information (NULL, NULL,
                        _("ckct_ib_application_loading_failed"));
  hildon_banner_set_timeout (HILDON_BANNER (banner), 6000);
#endif
}

/*
 * Called when HdAppMgr says an app crashed.
 */
static void
hd_switcher_app_crashed (HdSwitcher *switcher,
                         HdLauncherApp *app,
                         gpointer data)
{
  gchar *text;

  text = g_strdup_printf (dgettext("ke-recv", "memr_ni_application_closed_no_resources"),
      app ? hd_launcher_item_get_local_name (HD_LAUNCHER_ITEM (app)) : "");
  GtkWidget* banner = hildon_banner_show_information (NULL, NULL, text);
  hildon_banner_set_timeout (HILDON_BANNER (banner), 6000);
  g_free (text);
}

/*
 * Called when the HdAppMgr doesn't have enough memory to launch an
 * application.
 */
static void
hd_switcher_insufficient_memory(HdSwitcher *switcher,
                                gboolean waking_up,
                                gpointer data)
{
  if (hd_task_navigator_has_apps ())
    hd_render_manager_set_state (HDRM_STATE_TASK_NAV);
  else
    hd_render_manager_set_state (HDRM_STATE_HOME);

  GtkWidget* banner = hildon_banner_show_information (NULL, NULL,
                        dgettext("ke-recv", (waking_up ?
                            "memr_ia_close_applications_switching" :
                            "memr_ia_close_applications_opening")));
  hildon_banner_set_timeout (HILDON_BANNER (banner), 6000);
}

/*
 * This function is called when a wakeup is initiated but the application is not
 * responding.
 */
static void
hd_switcher_waking_fail (HdSwitcher *switcher)
{
  GtkWidget *banner;

  banner = hildon_banner_show_information (NULL, NULL,
                        _("ckct_ib_application_loading_failed"));
  hildon_banner_set_timeout (HILDON_BANNER (banner), 6000);

  hd_render_manager_set_state(HDRM_STATE_TASK_NAV);
}

/*
 * A timeout function to monitor the wakeup.
 */
static gboolean
hd_switcher_wakeup_timeout(gpointer data)
{
  HdSwitcher *switcher = HD_SWITCHER (data);
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  if (STATE_IS_LOADING(hd_render_manager_get_state ())) {
    hd_switcher_waking_fail (switcher);
  }

  priv->wakeup_timeout = 0;
  return FALSE;
}

/*
 * This function is called when the render manager changes state during the
 * wakeup procedure of an application. The function will remove the timeout
 * that checks if the wake-up failed.
 */
static void
hd_switcher_render_manager_notify_state (
		GObject    *gobject,
		GParamSpec *pspec,
		gpointer    data)
{
  HdSwitcher *switcher = HD_SWITCHER (data);
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  g_signal_handlers_disconnect_by_func(
      priv->task_nav,
      G_CALLBACK(hd_switcher_render_manager_notify_state),
      data);

  if (priv->wakeup_timeout)
    {
      g_source_remove (priv->wakeup_timeout);
      priv->wakeup_timeout = 0;
    }
}

static void
hd_switcher_zoom_in_complete (ClutterActor *actor, HdSwitcher *switcher)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  HdCompMgrClient       *hclient;

  g_debug ("hd_switcher_zoom_in_complete(%p)", actor);

  if (hd_render_manager_get_state () != HDRM_STATE_TASK_NAV)
    /* Don't do anything if the user exited the switcher
     * during zooming. */
    return;

  hclient = g_object_get_data (G_OBJECT (actor),
                               "HD-MBWMCompMgrClutterClient");
  if (!hclient)
    {
      /* This may happen if the client was unmapped while we were zooming in.
       * The cmgrcc is destroyed and it's pointer is cleared from the actor. */
      g_warning("%s: Actor that has just been zoomed in on has no "
                "HD-MBWMCompMgrClutterClient", __FUNCTION__);
      /* this is a real problem - not a normal use case, so just return
       * to home, as everything should be ok there */
      hd_render_manager_set_state(HDRM_STATE_HOME);
      return;
    }

  if (!hd_comp_mgr_client_is_hibernating (hclient))
    {
      MBWindowManagerClient *c;

      if (!(c = MB_WM_COMP_MGR_CLIENT(hclient)->wm_client))
        {
          /*
           * A possible reason for this to happen is that the client has been
           * unregistered (this wm_client is cleared) but somebody still holds
           * a reference to it (eg. a hash table, so it hasn't been destroyed).
           * Treat it as if cmgrcc was NULL.
           */
          g_warning("%s: cclient->wm_client == NULL", __FUNCTION__);
          hd_render_manager_set_state(HDRM_STATE_HOME);
        }
      else
        hd_wm_activate_zoomed_client (c->wmref, c);
    }
  else
    {
      HdTitleBar *tbar = HD_TITLE_BAR (hd_render_manager_get_title_bar());
      gchar *text =
        g_strdup_printf (dgettext("maemo-af-desktop",
                                  "ckct_ib_application_resuming"),
                         hd_comp_mgr_client_get_app_local_name (hclient));
      hd_render_manager_set_loading (actor);
      hd_render_manager_set_state (HDRM_STATE_LOADING);
      hd_render_manager_stop_transition ();
      hd_title_bar_set_loading_title (tbar, text);

      /*
       * Implementing a timeout to see if the wakeup fails.
       */
      priv->wakeup_timeout = g_timeout_add (6000,
                                            hd_switcher_wakeup_timeout,
                                            switcher);
      g_signal_connect (hd_render_manager_get(), "notify::state",
          G_CALLBACK (hd_switcher_render_manager_notify_state), switcher);

      hd_comp_mgr_wakeup_client (HD_COMP_MGR (priv->comp_mgr), hclient);
      g_free (text);
    }
}

void
hd_switcher_item_selected (HdSwitcher *switcher, ClutterActor *actor)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  hd_task_navigator_zoom_in (priv->task_nav, actor,
              (ClutterEffectCompleteFunc) hd_switcher_zoom_in_complete,
              switcher);
}

static void
hd_switcher_item_closed (HdSwitcher *switcher, ClutterActor *actor)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  MBWMCompMgrClutterClient * cc;

  cc = g_object_get_data(G_OBJECT (actor), "HD-MBWMCompMgrClutterClient");
  if (cc == NULL)
    {
      g_warning("%s: Trying to close a window from an actor with no "
                "HD-MBWMCompMgrClutterClient", __FUNCTION__);
      return;
    }
  hd_comp_mgr_close_app (HD_COMP_MGR (priv->comp_mgr), cc, TRUE);
}

void
hd_switcher_add_window_actor (HdSwitcher * switcher, ClutterActor * actor)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  /*
   * Don't readd @actor if it's already there.  This is not an error:
   * the frame window of applications being unfullscreened is mapped again
   * and since HdCompMgr doesn't know the cause of the map it attempts to
   * add the same client again.
   */
  if (hd_task_navigator_has_window (priv->task_nav, actor))
    return;

  hd_task_navigator_add_window (priv->task_nav, actor);
}

/* Deliver an #XButtonEvent to @note, so #HdIncomingEventWindow will know
 * there is a response and will invoke the appropriate action. */
static gboolean
hd_switcher_notification_clicked (HdSwitcher *switcher, HdNote *note)
{
  hd_util_click (MB_WM_CLIENT (note));
  return TRUE;
}

static gboolean
hd_switcher_notification_closed (HdSwitcher *switcher, HdNote *note)
{
  mb_wm_client_deliver_delete (MB_WM_CLIENT (note));
  return TRUE;
}

void
hd_switcher_add_notification (HdSwitcher * switcher, HdNote * note)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  hd_task_navigator_add_notification (priv->task_nav, note);
}

void
hd_switcher_add_dialog_explicit (HdSwitcher *switcher, MBWindowManagerClient *mbwmc,
                                 ClutterActor *dialog, MBWindowManagerClient *transfor)
{
  ClutterActor *parent;
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  g_return_if_fail (transfor);

  parent = mb_wm_comp_mgr_clutter_client_get_actor (
       MB_WM_COMP_MGR_CLUTTER_CLIENT (transfor->cm_client));
  hd_task_navigator_add_dialog (priv->task_nav, parent, dialog);

  /* Zoom in the application @dialog belongs to if this is a confirmation
   * note.  This is to support closing applications that want to show a
   * confirmation before closing. */
  if (hd_render_manager_get_state() == HDRM_STATE_TASK_NAV
        && HD_IS_CONFIRMATION_NOTE (mbwmc))
      hd_switcher_item_selected (switcher, parent);
}

void
hd_switcher_add_dialog (HdSwitcher *switcher, MBWindowManagerClient *mbwmc,
                        ClutterActor *dialog)
{
  hd_switcher_add_dialog_explicit (switcher, mbwmc, dialog, mbwmc->transient_for);
}

/* Called when a window or a notification is removed from the switcher.
 * Exit the switcher if it's become empty. */
static void
hd_switcher_something_removed (void)
{
  if (hd_render_manager_get_state() == HDRM_STATE_TASK_NAV
      && hd_task_navigator_is_empty ())
    hd_render_manager_set_state (HDRM_STATE_HOME);
}

void
hd_switcher_remove_notification (HdSwitcher * switcher, HdNote * note)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  hd_task_navigator_remove_notification (priv->task_nav, note);
  hd_switcher_something_removed ();
}

void
hd_switcher_remove_dialog (HdSwitcher * switcher,
                           ClutterActor * dialog)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  hd_task_navigator_remove_dialog (priv->task_nav, dialog);
}

static void
hd_switcher_window_removed (ClutterActor * unused,
                            MBWMCompMgrClutterClient * cmgrcc)
{
  mb_wm_object_unref (MB_WM_OBJECT (cmgrcc));
  hd_switcher_something_removed ();
}

void
hd_switcher_remove_window_actor (HdSwitcher * switcher, ClutterActor * actor,
                                 MBWMCompMgrClutterClient * cmgrcc)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  /* Make sure @cmgrcc stays as long as %HdTaskNavigator animates. */
  mb_wm_object_ref (MB_WM_OBJECT (cmgrcc));
  hd_task_navigator_remove_window (priv->task_nav, actor,
                  (ClutterEffectCompleteFunc)hd_switcher_window_removed,
                  cmgrcc);
}

void
hd_switcher_replace_window_actor (HdSwitcher   * switcher,
				  ClutterActor * old,
				  ClutterActor * new)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  hd_task_navigator_replace_window (priv->task_nav, old, new);
}

void
hd_switcher_hibernate_window_actor (HdSwitcher   * switcher,
				    ClutterActor * actor)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  hd_task_navigator_hibernate_window (priv->task_nav, actor);
}

static void
hd_switcher_group_background_clicked (HdSwitcher   *switcher,
				      ClutterActor *actor)
{
  hd_render_manager_set_state(HDRM_STATE_HOME);
}

