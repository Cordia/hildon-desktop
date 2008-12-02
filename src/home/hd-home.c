/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Tomas Frydrych <tf@o-hand.com>
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

#include "hd-home.h"
#include "hd-home-glue.h"
#include "hd-switcher.h"
#include "hd-home-view.h"
#include "hd-comp-mgr.h"
#include "hd-util.h"
#include "hd-layout-dialog.h"
#include "hd-gtk-style.h"
#include "hd-gtk-utils.h"

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>

#include <matchbox/core/mb-wm.h>

#include <gconf/gconf-client.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>

#define HDH_MOVE_DURATION 300
#define HDH_ZOOM_DURATION 400
#define HDH_EDIT_BUTTON_DURATION 200
#define HDH_EDIT_BUTTON_TIMEOUT 3000

#define HDH_LAYOUT_TOP_SCALE 0.5
#define HDH_LAYOUT_Y_OFFSET 60

/* FIXME -- match spec */
#define HDH_OPERATOR_PADDING 10
#define HDH_PAN_THRESHOLD 20

#define CLOSE_BUTTON "qgn_home_close"
#define BACK_BUTTON  "back-button"
#define NEW_BUTTON   "new-view-button"
#define APPLET_SETTINGS_BUTTON "applet-settings-button"
#define APPLET_RESIZE_BUTTON   "applet-resize-button"

#define EDIT_BUTTON  "edit-button.png"

#define HD_HOME_DBUS_NAME  "com.nokia.HildonDesktop.Home" 
#define HD_HOME_DBUS_PATH  "/com/nokia/HildonDesktop/Home"

#define CALL_UI_DBUS_NAME "com.nokia.CallUI"
#define CALL_UI_DBUS_PATH "/com/nokia/CallUI"
#define CALL_UI_DBUS_METHOD_SHOW_DIALPAD "ShowDialpad"

#undef WITH_SETTINGS_BUTTON

enum
{
  PROP_COMP_MGR = 1,
};

enum
{
  SIGNAL_BACKGROUND_CLICKED,
  SIGNAL_MODE_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

struct _HdHomePrivate
{
  MBWMCompMgrClutter    *comp_mgr;

  ClutterEffectTemplate *move_template;
  ClutterEffectTemplate *zoom_template;
  ClutterEffectTemplate *edit_button_template;

  ClutterActor          *main_group; /* Where the views + their buttons live */
  ClutterActor          *edit_group; /* An overlay group for edit mode */
  ClutterActor          *control_group;
  ClutterActor          *back_button;
  ClutterActor          *edit_button;

  ClutterActor          *applet_close_button;
#if 0
  ClutterActor          *applet_settings_button;
  ClutterActor          *applet_resize_button;
#endif

  ClutterActor          *layout_dialog;

  ClutterActor          *active_applet;

  ClutterActor          *grey_filter;

  ClutterActor          *operator;
  ClutterActor          *operator_icon;
  ClutterActor          *operator_label;

  /*
  ClutterActor          *status_area;
  */

  ClutterActor          *left_switch;
  ClutterActor          *right_switch;

  GList                 *views;
  GList                 *all_views;
  guint                  n_views;
  guint                  current_view;
  guint                  current_desktop;
  gint                   xwidth;
  gint                   xheight;

  HdHomeMode             mode;

  GList                 *pan_queue;

  gulong                 desktop_motion_cb;

  guint                  edit_button_cb;

#if 0
  gint                   applet_resize_start_x;
  gint                   applet_resize_start_y;
  gint                   applet_resize_last_x;
  gint                   applet_resize_last_y;
  guint                  applet_resize_width;
  guint                  applet_resize_height;

  guint                  applet_resize_motion_cb;
#endif

  /* Pan variables */
  gint                   last_x;
  gint                   initial_x;
  gint                   initial_y;
  gint                   cumulative_x;

  gint			 grab_count;
  gboolean               pan_handled         : 1;
  gboolean               showing_edit_button : 1;

  Window                 desktop;

  /* DBus Proxy for the call to com.nokia.CallUI.ShowDialpad */
  DBusGConnection       *connection;
  DBusGProxy            *call_ui_proxy;
};

static void hd_home_class_init (HdHomeClass *klass);
static void hd_home_init       (HdHome *self);
static void hd_home_dispose    (GObject *object);
static void hd_home_finalize   (GObject *object);

static void hd_home_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec);

static void hd_home_get_property (GObject      *object,
				  guint         prop_id,
				  GValue       *value,
				  GParamSpec   *pspec);

static void hd_home_constructed (GObject *object);

static void hd_home_start_pan (HdHome *home);

static void hd_home_pan_full (HdHome *home, gboolean left);

static void hd_home_show_edit_button (HdHome *home);

static void hd_home_store_n_views(HdHome *home);

static void hd_home_store_current_desktop(HdHome *home, guint new_desktop);

#if 0
static void hd_home_send_settings_message (HdHome *home, Window xwin);

static gboolean hd_home_applet_resize_button_release (ClutterActor *button,
						   ClutterButtonEvent *event,
						   HdHome *home);
#endif

G_DEFINE_TYPE (HdHome, hd_home, CLUTTER_TYPE_GROUP);

static void
hd_home_class_init (HdHomeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec   *pspec;

  g_type_class_add_private (klass, sizeof (HdHomePrivate));

  object_class->dispose      = hd_home_dispose;
  object_class->finalize     = hd_home_finalize;
  object_class->set_property = hd_home_set_property;
  object_class->get_property = hd_home_get_property;
  object_class->constructed  = hd_home_constructed;

  pspec = g_param_spec_pointer ("comp-mgr",
				"Composite Manager",
				"MBWMCompMgrClutter Object",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class, PROP_COMP_MGR, pspec);

  signals[SIGNAL_BACKGROUND_CLICKED] =
      g_signal_new ("background-clicked",
                    G_OBJECT_CLASS_TYPE (object_class),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (HdHomeClass, background_clicked),
                    NULL,
                    NULL,
                    g_cclosure_marshal_VOID__BOXED,
                    G_TYPE_NONE,
                    1,
		    CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  signals[SIGNAL_MODE_CHANGED] =
      g_signal_new ("mode-changed",
                    G_OBJECT_CLASS_TYPE (object_class),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (HdHomeClass, mode_changed),
                    NULL,
                    NULL,
                    g_cclosure_marshal_VOID__INT,
                    G_TYPE_NONE,
                    1,
		    G_TYPE_INT | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static gboolean
hd_home_edit_button_clicked (ClutterActor *button,
			     ClutterEvent *event,
			     HdHome       *home)
{
  hd_home_hide_edit_button (home);
  hd_home_set_mode (home, HD_HOME_MODE_EDIT);

  return TRUE;
}

static gboolean
hd_home_back_button_clicked (ClutterActor *button,
			     ClutterEvent *event,
			     HdHome       *home)
{
  HdHomePrivate *priv = home->priv;

  if (priv->mode == HD_HOME_MODE_EDIT || priv->mode == HD_HOME_MODE_LAYOUT)
    hd_home_set_mode (home, HD_HOME_MODE_NORMAL);

  return TRUE;
}

static void
hd_home_view_thumbnail_clicked (HdHomeView         *view,
				ClutterButtonEvent *ev,
				HdHome             *home)
{
  HdHomePrivate *priv = home->priv;
  gint           index = g_list_index (priv->views, view);

  hd_home_show_view (home, index);
}

static void
hd_home_view_background_clicked (HdHomeView         *view,
				 ClutterButtonEvent *event,
				 HdHome             *home)
{
  HdHomePrivate *priv = home->priv;

  g_debug ("hd_home_view_background_clicked, mode=%d\n", priv->mode);

  if (priv->mode != HD_HOME_MODE_EDIT)
    g_signal_emit (home, signals[SIGNAL_BACKGROUND_CLICKED], 0, event);
  else
    {
#if 0
      /*
       * When tracking resize motion, the pointer can get outside the
       * resize button, so if we get a button click on the background in edit
       * mode, and have the motion cb installed, we need to terminate the
       * resize.
       */
      if (priv->applet_resize_motion_cb)
	{
	  hd_home_applet_resize_button_release (priv->applet_resize_button,
						event, home);
	}
#endif
    }
}

static void
hd_home_view_applet_clicked (HdHomeView         *view,
			     ClutterActor       *applet,
			     HdHome             *home)
{
  HdHomePrivate *priv = home->priv;

  if (priv->mode == HD_HOME_MODE_EDIT)
    {
#if 0
      /*
       * When tracking resize motion, the pointer can get outside the
       * resize button, so if we get a button click on an applet in edit
       * mode, and have the motion cb installed, we need to terminate the
       * resize.
       */
      if (priv->applet_resize_motion_cb)
	{
	  hd_home_applet_resize_button_release (priv->applet_resize_button,
						NULL, home);
	}
      else
#endif
	hd_home_show_applet_buttons (home, applet);
    }
}

static Bool
hd_home_desktop_motion (XButtonEvent *xev, void *userdata)
{
  HdHome          *home = userdata;
  HdHomePrivate   *priv = home->priv;
  MBWindowManager *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;
  gint by_x;

  by_x = xev->x - priv->last_x;

  priv->cumulative_x += by_x;

  /*
   * When the motion gets over the pan threshold, we do a full pan
   * and disconnect the motion handler (next motion needs to be started
   * with another gesture).
   */
  if (priv->cumulative_x > 0 && priv->cumulative_x > HDH_PAN_THRESHOLD)
    {
      if (priv->desktop_motion_cb)
	mb_wm_main_context_x_event_handler_remove (wm->main_ctx,
						   MotionNotify,
						   priv->desktop_motion_cb);

      priv->desktop_motion_cb = 0;
      priv->cumulative_x = 0;
      priv->pan_handled = TRUE;

      hd_home_pan_full (home, FALSE);
    }
  else if (priv->cumulative_x < 0 && priv->cumulative_x < -HDH_PAN_THRESHOLD)
    {
      if (priv->desktop_motion_cb)
	mb_wm_main_context_x_event_handler_remove (wm->main_ctx,
						   MotionNotify,
						   priv->desktop_motion_cb);

      priv->desktop_motion_cb = 0;
      priv->cumulative_x = 0;
      priv->pan_handled = TRUE;

      hd_home_pan_full (home, TRUE);
    }

  priv->last_x = xev->x;

  return True;
}

static Bool
hd_home_desktop_release (XButtonEvent *xev, void *userdata)
{
  HdHome *home = userdata;
  HdHomePrivate *priv = home->priv;
  MBWindowManager *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;

  if (priv->desktop_motion_cb)
    mb_wm_main_context_x_event_handler_remove (wm->main_ctx,
					       MotionNotify,
					       priv->desktop_motion_cb);

  priv->desktop_motion_cb = 0;
  priv->cumulative_x = 0;

  if (!priv->pan_handled && priv->mode == HD_HOME_MODE_NORMAL)
      hd_home_show_edit_button (home);
  else
    priv->pan_handled = FALSE;

  return True;
}

static Bool
hd_home_desktop_key_press (XKeyEvent *xev, void *userdata)
{
  HdHome *home = userdata;
  HdHomePrivate *priv = home->priv;

  char buffer[10];

  XLookupString (xev, buffer, 10, NULL, NULL);

  if (priv->call_ui_proxy)
    {
      g_debug ("Call Dialpad via D-BUS. " CALL_UI_DBUS_NAME "." CALL_UI_DBUS_METHOD_SHOW_DIALPAD " (s: %s)", buffer);

      dbus_g_proxy_call_no_reply (priv->call_ui_proxy, CALL_UI_DBUS_METHOD_SHOW_DIALPAD,
                                  G_TYPE_STRING, buffer,
                                  G_TYPE_INVALID);
      return TRUE;
    }

  g_warning ("No Proxy for " CALL_UI_DBUS_NAME);

  return FALSE;
}

static Bool
hd_home_desktop_press (XButtonEvent *xev, void *userdata)
{
  HdHome *home = userdata;
  HdHomePrivate *priv = home->priv;
  MBWindowManager *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;

  if (priv->desktop_motion_cb)
    {
      mb_wm_main_context_x_event_handler_remove (wm->main_ctx,
						 MotionNotify,
						 priv->desktop_motion_cb);

      priv->desktop_motion_cb = 0;
    }

  priv->initial_x = priv->last_x = xev->x;
  priv->initial_y = xev->y;

  priv->cumulative_x = 0;
  priv->pan_handled = FALSE;

  priv->desktop_motion_cb =
    mb_wm_main_context_x_event_handler_add (wm->main_ctx,
					    priv->desktop,
					    MotionNotify,
					    (MBWMXEventFunc)
					    hd_home_desktop_motion,
					    userdata);

  return True;
}

static Bool
hd_home_desktop_client_message (XClientMessageEvent *xev, void *userdata)
{
  HdHome          *home = userdata;
  HdHomePrivate   *priv = home->priv;
  HdCompMgr       *hmgr = HD_COMP_MGR (priv->comp_mgr);
  Atom             pan_atom;

  pan_atom = hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_CLIENT_MESSAGE_PAN);

  if (xev->message_type == pan_atom)
    {
      gboolean left = (gboolean) xev->data.l[0];

      g_debug ("ClientMessage initiated pan.");

      hd_home_pan_full (home, left);

      /*
       * Return false, this is our private protocol and no-one else's business.
       */
      return False;
    }

  return True;
}

static void 
hd_home_status_area_allocation_changed (
		ClutterActor    *sa,
		GParamSpec      *arg1,
		HdHome          *home)
{
	g_debug ("The size of the status area has been changed.");
	hd_home_fixup_operator_position (home);
}

static gboolean
hd_home_applet_close_button_clicked (ClutterActor       *button,
				     ClutterButtonEvent *event,
				     HdHome             *home)
{
  HdHomePrivate *priv = home->priv;
  HdCompMgr     *hmgr = HD_COMP_MGR (priv->comp_mgr);
  MBWMCompMgrClutterClient *cc;
  GConfClient   *client = gconf_client_get_default ();
  gchar         *applet_id;
  gchar         *view_key;


  if (!priv->active_applet)
    {
      g_warning ("No active applet to close !!!");
      return FALSE;
    }

  cc = g_object_get_data (G_OBJECT (priv->active_applet),
			  "HD-MBWMCompMgrClutterClient");

  /* Unset GConf configuration */
  applet_id = g_object_get_data (G_OBJECT (priv->active_applet),
                                 "HD-applet-id");

  view_key = g_strdup_printf ("/apps/osso/hildon-desktop/applets/%s/view", applet_id);
  gconf_client_unset (client, view_key, NULL);

  hd_comp_mgr_close_client (hmgr, cc);

  return TRUE;
}

#ifdef WITH_SETTINGS_BUTTON
static gboolean
hd_home_applet_settings_button_clicked (ClutterActor       *button,
					ClutterButtonEvent *event,
					HdHome             *home)
{
  HdHomePrivate         *priv = home->priv;
  MBWindowManagerClient *client;
  MBWMCompMgrClient     *cclient;
  ClutterActor          *applet;

  g_debug ("Applet settings button clicked.");

  if (!priv->active_applet)
    return FALSE;

  applet = priv->active_applet;

  cclient =
    g_object_get_data (G_OBJECT (applet), "HD-MBWMCompMgrClutterClient");

  client = cclient->wm_client;

  /*
   * Dispatch ClientMessage to the applet to pop up settings dialog, and
   * return to Normal mode
   */
  hd_home_send_settings_message (home, client->window->xwindow);
  hd_home_set_mode (home, HD_HOME_MODE_NORMAL);

  return TRUE;
}
#endif

#if 0
static gboolean
hd_home_applet_resize_button_motion (ClutterActor       *button,
				     ClutterMotionEvent *event,
				     HdHome             *home)
{
  HdHomePrivate *priv = home->priv;
  gint x, y;
  guint w_a, h_a;
  ClutterActor *applet;
  gdouble scale_x, scale_y;

  if (!priv->active_applet)
    return FALSE;

  applet = priv->active_applet;

  x = event->x - priv->applet_resize_last_x;
  y = event->y - priv->applet_resize_last_y;

  priv->applet_resize_last_x = event->x;
  priv->applet_resize_last_y = event->y;

  /*
   * We only move the actor, not the applet per se, and only commit the motion
   * on button release.
   */
  priv->applet_resize_width += x;
  priv->applet_resize_height += y;

  clutter_actor_get_size (applet, &w_a, &h_a);

  scale_x = (gdouble)(priv->applet_resize_width + x)/(gdouble)w_a;
  scale_y = (gdouble)(priv->applet_resize_height + y)/(gdouble)h_a;

  clutter_actor_set_scale (applet, scale_x, scale_y);

  clutter_actor_move_by (priv->applet_resize_button, x, y);
  clutter_actor_move_by (priv->applet_close_button, x, 0);
#ifdef WITH_SETTINGS_BUTTON
  clutter_actor_move_by (priv->applet_settings_button, 0, y);
#endif
  return FALSE;
}

static gboolean
hd_home_applet_resize_button_release (ClutterActor       *button,
				      ClutterButtonEvent *event,
				      HdHome             *home)
{
  HdHomePrivate *priv = home->priv;
  MBWindowManagerClient *client;
  MBWMCompMgrClient *cclient;
  MBGeometry geom;
  ClutterActor *applet;
  gint x, y;

  if (!priv->active_applet)
    return FALSE;

  applet = priv->active_applet;

  if (priv->applet_resize_motion_cb)
    {
      g_signal_handler_disconnect (button, priv->applet_resize_motion_cb);
      priv->applet_resize_motion_cb = 0;
    }

  /* Move the underlying window to match the actor's position */
  cclient =
    g_object_get_data (G_OBJECT (applet), "HD-MBWMCompMgrClutterClient");

  client = cclient->wm_client;

  clutter_actor_get_position (applet, &x, &y);
  clutter_actor_set_scale (applet, 1.0, 1.0);

  geom.x = x;
  geom.y = y;
  geom.width = priv->applet_resize_width;
  geom.height = priv->applet_resize_height;

  mb_wm_client_request_geometry (client, &geom,
				 MBWMClientReqGeomIsViaUserAction);

  return TRUE;
}

static gboolean
hd_home_applet_resize_button_press (ClutterActor       *button,
				    ClutterButtonEvent *event,
				    HdHome             *home)
{
  HdHomePrivate *priv = home->priv;

  if (!priv->active_applet)
    return FALSE;

  if (priv->applet_resize_motion_cb)
    {
      g_signal_handler_disconnect (button, priv->applet_resize_motion_cb);
      priv->applet_resize_motion_cb = 0;
    }

  priv->applet_resize_motion_cb =
    g_signal_connect (button, "motion-event",
		      G_CALLBACK (hd_home_applet_resize_button_motion),
		      home);

  priv->applet_resize_start_x = event->x;
  priv->applet_resize_start_y = event->y;
  priv->applet_resize_last_x = event->x;
  priv->applet_resize_last_y = event->y;

  clutter_actor_get_size (priv->active_applet,
			  &priv->applet_resize_width,
			  &priv->applet_resize_height);

  return TRUE;
}
#endif

static void
hd_home_layout_dialog_ok_clicked (HdLayoutDialog *dialog, HdHome *home)
{
  hd_home_set_mode (home, HD_HOME_MODE_NORMAL);
}

/* Called when a client message is sent to the root window. */
static Bool
root_window_client_message (XClientMessageEvent *event, HdHome *home)
{
  HdHomePrivate *priv = home->priv;
  MBWindowManager *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;

  if (event->message_type == wm->atoms[MBWM_ATOM_NET_CURRENT_DESKTOP])
    {
      gint desktop = event->data.l[0];
      hd_home_show_view (home, desktop);
    }

  return False;
}

static void
hd_home_constructed (GObject *object)
{
  HdHomePrivate   *priv = HD_HOME (object)->priv;
  ClutterActor    *view;
  ClutterActor    *main_group;
  ClutterActor    *edit_group;
  MBWindowManager *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;
  gint             i;
  GError          *error = NULL;
  guint            button_width, button_height;
  ClutterColor     clr = {0,0,0,0xff};
  XSetWindowAttributes attr;
  ClutterColor     op_color = {0xff, 0xff, 0xff, 0xff};
  char		  *font_string;
  GtkIconTheme	  *icon_theme;

  icon_theme = gtk_icon_theme_get_default ();

  priv->xwidth  = wm->xdpy_width;
  priv->xheight = wm->xdpy_height;
  
  clutter_actor_set_name (CLUTTER_ACTOR(object), "HdHome");

  main_group = priv->main_group = clutter_group_new ();
  clutter_actor_set_name (main_group, "HdHome:main_group");
  clutter_container_add_actor (CLUTTER_CONTAINER (object), main_group);

  edit_group = priv->edit_group = clutter_group_new ();
  clutter_actor_set_name (edit_group, "HdHome:edit_group");
  clutter_container_add_actor (CLUTTER_CONTAINER (object), edit_group);
  clutter_actor_hide (edit_group);

  /* TODO -- see if the control group could be added directly to our parent,
   * so we would not have to move it about (it would mean to maintain it
   * in the correct order on the actor stack, which might be more difficult
   * than moving it).
   */
  priv->control_group = clutter_group_new ();
  clutter_actor_set_name (priv->control_group, "HdHome:control_group");
  clutter_container_add_actor (CLUTTER_CONTAINER (object),
			       priv->control_group);

  for (i = 0; i < 4; ++i)
    {
      view = g_object_new (HD_TYPE_HOME_VIEW,
			   "comp-mgr", priv->comp_mgr,
			   "home",     object,
			   "id",       i,
			   NULL);

      priv->all_views = g_list_append (priv->all_views, view);

      g_signal_connect (view, "thumbnail-clicked",
			G_CALLBACK (hd_home_view_thumbnail_clicked),
			object);

      g_signal_connect (view, "background-clicked",
			G_CALLBACK (hd_home_view_background_clicked),
			object);

      g_signal_connect (view, "applet-clicked",
			G_CALLBACK (hd_home_view_applet_clicked),
			object);

      priv->views = g_list_append (priv->views, view);

      clutter_actor_set_position (view, priv->xwidth * i, 0);
      clutter_container_add_actor (CLUTTER_CONTAINER (main_group), view);
    }

  priv->n_views = i;
  hd_home_store_n_views (HD_HOME (object));
  priv->current_desktop = 0;

  priv->back_button =
    hd_gtk_icon_theme_load_icon (icon_theme, BACK_BUTTON, 48, 0);

  clutter_container_add_actor (CLUTTER_CONTAINER (priv->control_group),
			       priv->back_button);
  clutter_actor_hide (priv->back_button);
  clutter_actor_set_reactive (priv->back_button, TRUE);

  clutter_actor_get_size (priv->back_button, &button_width, &button_height);
  clutter_actor_set_position (priv->back_button,
			      priv->xwidth - button_width - 5, 5);

  g_signal_connect (priv->back_button, "button-release-event",
		    G_CALLBACK (hd_home_back_button_clicked),
		    object);

  priv->edit_button =
    clutter_texture_new_from_file (
	g_build_filename (HD_DATADIR, EDIT_BUTTON, NULL),
	&error);

  clutter_container_add_actor (CLUTTER_CONTAINER (priv->control_group),
			       priv->edit_button);
  clutter_actor_hide (priv->edit_button);
  clutter_actor_set_reactive (priv->edit_button, TRUE);

  g_signal_connect (priv->edit_button, "button-release-event",
		    G_CALLBACK (hd_home_edit_button_clicked),
		    object);

  priv->operator = clutter_group_new ();
  clutter_actor_set_name(priv->operator, "HdHome:operator");
  clutter_actor_show (priv->operator);
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->control_group),
			       priv->operator);

  hd_gtk_style_get_text_color (HD_GTK_BUTTON_SINGLETON,
			       GTK_STATE_NORMAL,
			       &op_color);
  font_string = hd_gtk_style_get_font_string (HD_GTK_BUTTON_SINGLETON);
  priv->operator_label = clutter_label_new_full (font_string, "Operator",
						 &op_color);
  g_free (font_string);

  clutter_actor_show (priv->operator_label);
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->operator),
			       priv->operator_label);

  priv->left_switch = clutter_rectangle_new ();
  clutter_actor_set_name (priv->left_switch, "HdHome:left_switch");

  /* FIXME -- should the color come from theme ? */
  clr.red = 0;
  clr.green = 0xff;
  clr.blue  = 0xff;
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (priv->left_switch), &clr);

  clutter_actor_set_size (priv->left_switch, HDH_SWITCH_WIDTH, priv->xheight);
  clutter_actor_set_position (priv->left_switch, 0, 0);
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->control_group),
			       priv->left_switch);
  clutter_actor_hide (priv->left_switch);

  priv->right_switch = clutter_rectangle_new ();
  clutter_actor_set_name (priv->right_switch, "HdHome:right_switch");

  clutter_rectangle_set_color (CLUTTER_RECTANGLE (priv->right_switch), &clr);

  clutter_actor_set_size (priv->right_switch, HDH_SWITCH_WIDTH, priv->xheight);
  clutter_actor_set_position (priv->right_switch,
			      priv->xwidth - HDH_SWITCH_WIDTH, 0);
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->control_group),
			       priv->right_switch);
  clutter_actor_hide (priv->right_switch);

  priv->layout_dialog = g_object_new (HD_TYPE_LAYOUT_DIALOG,
				      "comp-mgr", priv->comp_mgr,
				      "home", object,
				       NULL);
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->control_group),
			       priv->layout_dialog);
  clutter_actor_hide (priv->layout_dialog);
  clutter_actor_set_reactive (priv->layout_dialog, TRUE);

  g_signal_connect (priv->layout_dialog, "ok-clicked",
		    G_CALLBACK (hd_home_layout_dialog_ok_clicked),
		    object);
  /*
   * Construct the grey rectangle for dimming of desktop in edit mode
   * This one is added directly to the home, so it is always on the top
   * all the other stuff in the main_group.
   */
  clr.alpha = 0x77;
  clr.red   = 0x77;
  clr.green = 0x77;
  clr.blue  = 0x77;

  priv->grey_filter = clutter_rectangle_new_with_color (&clr);
  clutter_actor_set_name (priv->grey_filter, "HdHome:grey_filter");

  clutter_actor_set_size (priv->grey_filter, priv->xwidth, priv->xheight);
  clutter_container_add_actor (CLUTTER_CONTAINER (edit_group),
			       priv->grey_filter);

  priv->applet_close_button =
    hd_gtk_icon_theme_load_icon (icon_theme, CLOSE_BUTTON, 48, 0);

  clutter_container_add_actor (CLUTTER_CONTAINER (edit_group),
			       priv->applet_close_button);
  clutter_actor_hide (priv->applet_close_button);
  clutter_actor_set_reactive (priv->applet_close_button, TRUE);

  g_signal_connect (priv->applet_close_button, "button-release-event",
		    G_CALLBACK (hd_home_applet_close_button_clicked),
		    object);
#ifdef WITH_SETTINGS_BUTTON
  priv->applet_settings_button =
    hd_gtk_icon_theme_load_icon (icon_theme, APPLET_SETTINGS_BUTTON, 48, 0);

  clutter_container_add_actor (CLUTTER_CONTAINER (edit_group),
			       priv->applet_settings_button);
  clutter_actor_hide (priv->applet_settings_button);
  clutter_actor_set_reactive (priv->applet_settings_button, TRUE);

  g_signal_connect (priv->applet_settings_button, "button-release-event",
		    G_CALLBACK (hd_home_applet_settings_button_clicked),
		    object);
#endif

#if 0
  priv->applet_resize_button =
    hd_gtk_icon_theme_load_icon (icon_theme, APPLET_RESIZE_BUTTON, 48, 0);

  clutter_container_add_actor (CLUTTER_CONTAINER (edit_group),
			       priv->applet_resize_button);
  clutter_actor_hide (priv->applet_resize_button);
  clutter_actor_set_reactive (priv->applet_resize_button, TRUE);

  g_signal_connect (priv->applet_resize_button, "button-press-event",
		    G_CALLBACK (hd_home_applet_resize_button_press),
		    object);

  g_signal_connect (priv->applet_resize_button, "button-release-event",
		    G_CALLBACK (hd_home_applet_resize_button_release),
		    object);
#endif

  hd_home_set_mode (HD_HOME (object), HD_HOME_MODE_NORMAL);

  /*
   * Create an InputOnly desktop window; we have a custom desktop client that
   * that will automatically wrap it, ensuring it is located in the correct
   * place.
   */
  attr.event_mask = MBWMChildMask |
    ButtonPressMask | ButtonReleaseMask | 
    PointerMotionMask | ExposureMask |
    KeyPressMask;

  priv->desktop = XCreateWindow (wm->xdpy,
				 wm->root_win->xwindow,
				 0, 0,
				 wm->xdpy_width,
				 wm->xdpy_height,
				 0,
				 CopyFromParent,
				 InputOnly,
				 CopyFromParent,
				 CWEventMask,
				 &attr);
  mb_wm_rename_window (wm, priv->desktop, "desktop");

  XChangeProperty (wm->xdpy, priv->desktop,
		   wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE],
		   XA_ATOM, 32, PropModeReplace,
		   (unsigned char *)
		   &wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_DESKTOP],
		   1);

  XMapWindow (wm->xdpy, priv->desktop);

  mb_wm_main_context_x_event_handler_add (wm->main_ctx,
					  priv->desktop,
					  ButtonPress,
					  (MBWMXEventFunc)
					  hd_home_desktop_press,
					  object);

  mb_wm_main_context_x_event_handler_add (wm->main_ctx,
					  priv->desktop,
					  ButtonRelease,
					  (MBWMXEventFunc)
					  hd_home_desktop_release,
					  object);

  mb_wm_main_context_x_event_handler_add (wm->main_ctx,
					  None,
					  ClientMessage,
					  (MBWMXEventFunc)
					  hd_home_desktop_client_message,
					  object);

  mb_wm_main_context_x_event_handler_add (wm->main_ctx,
					  priv->desktop,
					  KeyPress,
					  (MBWMXEventFunc)
					  hd_home_desktop_key_press,
					  object);

  mb_wm_main_context_x_event_handler_add (wm->main_ctx,
					  wm->root_win->xwindow,
					  ClientMessage,
					  (MBWMXEventFunc)
					  root_window_client_message,
					  object); 

}

static void
hd_home_init (HdHome *self)
{
  HdHomePrivate *priv;
  DBusGConnection *connection;
  DBusGProxy *bus_proxy = NULL;
  guint result;
  GError *error = NULL;

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
						   HD_TYPE_HOME, HdHomePrivate);

  priv->move_template =
    clutter_effect_template_new_for_duration (HDH_MOVE_DURATION,
					      CLUTTER_ALPHA_RAMP_INC);

  priv->zoom_template =
    clutter_effect_template_new_for_duration (HDH_ZOOM_DURATION,
					      CLUTTER_ALPHA_RAMP_INC);

  priv->edit_button_template =
    clutter_effect_template_new_for_duration (HDH_EDIT_BUTTON_DURATION,
					      CLUTTER_ALPHA_RAMP_INC);

  /* Listen to gconf notifications */
  gconf_client_add_dir (gconf_client_get_default (),
			HDH_GCONF_PREFIX,
			GCONF_CLIENT_PRELOAD_NONE,
			NULL);

  /* Register to D-Bus */
  connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (error != NULL)
    {
      g_warning ("Failed to open connection to session bus: %s", error->message);
      g_error_free (error);

      goto cleanup;
    }

  /* Request the well known name from the Bus */
  bus_proxy = dbus_g_proxy_new_for_name (connection,
                                         DBUS_SERVICE_DBUS,
                                         DBUS_PATH_DBUS,
                                         DBUS_INTERFACE_DBUS);

  if (!org_freedesktop_DBus_request_name (bus_proxy,
                                          HD_HOME_DBUS_NAME,
                                          DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                          &result, 
                                          &error))
    {
      g_warning ("Could not register name: %s", error->message);
      g_error_free (error);

      goto cleanup;
    }

  if (result == DBUS_REQUEST_NAME_REPLY_EXISTS)
    goto cleanup;

  dbus_g_object_type_install_info (HD_TYPE_HOME,
                                   &dbus_glib_hd_home_object_info);

  dbus_g_connection_register_g_object (connection,
                                       HD_HOME_DBUS_PATH,
                                       G_OBJECT (self));

  g_debug ("%s registered to session bus at %s",
           HD_HOME_DBUS_NAME,
           HD_HOME_DBUS_PATH);

  priv->connection = connection;
  priv->call_ui_proxy = dbus_g_proxy_new_for_name (priv->connection,
                                                   CALL_UI_DBUS_NAME,
                                                   CALL_UI_DBUS_PATH,
                                                   CALL_UI_DBUS_NAME);

cleanup:
  if (bus_proxy != NULL)
    g_object_unref (bus_proxy);
}

static void
hd_home_dispose (GObject *object)
{
  G_OBJECT_CLASS (hd_home_parent_class)->dispose (object);
}

static void
hd_home_finalize (GObject *object)
{
  G_OBJECT_CLASS (hd_home_parent_class)->finalize (object);
}

static void
hd_home_set_property (GObject       *object,
		      guint         prop_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
  HdHomePrivate *priv = HD_HOME (object)->priv;

  switch (prop_id)
    {
    case PROP_COMP_MGR:
      priv->comp_mgr = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hd_home_get_property (GObject      *object,
		      guint         prop_id,
		      GValue       *value,
		      GParamSpec   *pspec)
{
  HdHomePrivate *priv = HD_HOME (object)->priv;

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

void
hd_home_show_view (HdHome * home, guint view_index)
{
  HdHomePrivate   *priv = home->priv;
  HdCompMgr       *hmgr = HD_COMP_MGR (priv->comp_mgr);
  MBWindowManagerClient *desktop;
  ClutterTimeline *timeline1, *timeline2;

  if (view_index >= priv->n_views)
    {
      g_warning ("View %d requested, but desktop has only %d views.",
		 view_index, priv->n_views);
      return;
    }

  priv->current_view = view_index;
  hd_home_store_current_desktop (home, view_index);

  if (priv->mode == HD_HOME_MODE_NORMAL)
    {
      timeline1 = clutter_effect_move (priv->move_template,
				       CLUTTER_ACTOR (home),
				       - view_index * priv->xwidth, 0,
				       NULL, NULL);
      timeline2 = clutter_effect_move (priv->move_template,
				       priv->control_group,
				       view_index * priv->xwidth, 0,
				       NULL, NULL);

      clutter_timeline_start (timeline1);
      clutter_timeline_start (timeline2);
    }
  else
    {
      hd_home_set_mode (home, HD_HOME_MODE_NORMAL);
    }

  desktop = hd_comp_mgr_get_desktop_client (hmgr);

  if (desktop)
    mb_wm_client_stacking_mark_dirty (desktop);
}

void
hd_home_grab_pointer (HdHome *home)
{
  HdHomePrivate *priv = home->priv;
  ClutterActor  *stage = clutter_stage_get_default();
  Window         clutter_window;
  Display       *dpy = clutter_x11_get_default_display ();
  gint           status;

  clutter_window = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));

  status = XGrabPointer (dpy,
			 clutter_window,
			 False,
			 ButtonPressMask   |
			 ButtonReleaseMask |
			 PointerMotionMask,
			 GrabModeAsync,
			 GrabModeAsync,
			 None,
			 None,
			 CurrentTime);

  if (priv->grab_count != 0)
    g_warning ("We are issuing a grab when a grab is already in place. This\n"
	       "will override the previous and that's possibly a mistake.\n");

  g_debug ("Doing pointer grab on 0x%x (status %d and grab_count = %d)!!!",
	   (unsigned int) clutter_window, status, priv->grab_count++);
}

void
hd_home_ungrab_pointer (HdHome *home)
{
  HdHomePrivate *priv = home->priv;
  Display	*dpy = clutter_x11_get_default_display ();

  /* NB: X grabs can not be nested, but for our needs it is much easier
   * to manage things with nesting semantics */

  if (--priv->grab_count > 0)
    {
      g_debug ("Skipping ungrab (grab_count = %d) !!!", priv->grab_count);
      return;
    }
  if (priv->grab_count < 0)
    {
      g_debug ("Unbalanced ungrab!! (grub_count = %d) !!!", priv->grab_count);
      g_debug ("(Will now be reset to zero)\n");
    }

  XUngrabPointer (dpy, CurrentTime);

  /* NB: any return status is meaningless for an XUngrabPointer, it should
   * always == 1 (i.e. XUngrabPointer doesn't wait for a server response
   * before returning) */
  g_debug ("Doing pointer ungrab !!!");

  priv->grab_count = 0;
}

static void
hd_home_do_normal_layout (HdHome *home)
{
  HdHomePrivate *priv = home->priv;
  GList         *l = priv->views;
  gint           xwidth = priv->xwidth;
  gint           i = 0;

  clutter_actor_hide (priv->back_button);
  clutter_actor_hide (priv->edit_group);
  clutter_actor_hide (priv->layout_dialog);

  hd_home_hide_applet_buttons (home);

  while (l)
    {
      ClutterActor * view = l->data;

      clutter_actor_set_position (view, i * xwidth, 0);
      clutter_actor_set_scale (view, 1.0, 1.0);
      clutter_actor_set_depth (view, 0);
      hd_home_view_set_thumbnail_mode (HD_HOME_VIEW (view), FALSE);

      ++i;
      l = l->next;
    }

  clutter_actor_set_position (CLUTTER_ACTOR (home),
			      -priv->current_view * xwidth, 0);

  hd_home_hide_switches (home);

  hd_home_ungrab_pointer (home);
}

static void
hd_home_do_edit_layout (HdHome *home)
{
  HdHomePrivate *priv = home->priv;
  gint x;

  if (priv->mode == HD_HOME_MODE_EDIT)
    return;

  if (priv->mode != HD_HOME_MODE_NORMAL)
    hd_home_do_normal_layout (home);

  clutter_actor_hide (priv->layout_dialog);

  /*
   * Show the overlay edit_group and move it over the current view.
   */
  x = priv->xwidth * priv->current_view;

  clutter_actor_set_position (priv->edit_group, x, 0);
  clutter_actor_set_position (priv->control_group, x, 0);
  clutter_actor_show (priv->edit_group);

  clutter_actor_show (priv->back_button);
  clutter_actor_raise_top (priv->back_button);

  priv->mode = HD_HOME_MODE_EDIT;

  clutter_actor_show (priv->grey_filter);
  hd_home_show_switches (home);
  hd_home_grab_pointer (home);
}

static void
hd_home_do_layout_layout (HdHome * home)
{
  HdHomePrivate   *priv = home->priv;

  hd_home_do_normal_layout (home);
  clutter_actor_show (priv->layout_dialog);
  hd_home_grab_pointer (home);
}

void
hd_home_set_mode (HdHome *home, HdHomeMode mode)
{
  HdHomePrivate   *priv = home->priv;
  gboolean         change = FALSE;
  switch (mode)
    {
    case HD_HOME_MODE_NORMAL:
    default:
      hd_home_do_normal_layout (home);
      break;

    case HD_HOME_MODE_LAYOUT:
      hd_home_do_layout_layout (home);
      break;

    case HD_HOME_MODE_EDIT:
      hd_home_do_edit_layout (home);
      break;
    }

  if  (priv->mode == mode)
    change = TRUE;

  priv->mode = mode;

  g_signal_emit (home, signals[SIGNAL_MODE_CHANGED], 0, mode);
}

void
hd_home_show_activate_views_dialog (HdHome *home)
{
  hd_home_set_mode (home, HD_HOME_MODE_LAYOUT);
}

static void
hd_home_pan_stage_completed (HdHome *home)
{
  HdHomePrivate *priv = home->priv;

  if (priv->pan_queue)
    hd_home_start_pan (home);
  else
    {
      HdCompMgr *hmgr = HD_COMP_MGR (priv->comp_mgr);
      MBWindowManagerClient *desktop;

      desktop = hd_comp_mgr_get_desktop_client (hmgr);

      if (desktop)
	mb_wm_client_stacking_mark_dirty (desktop);
    }
}

static void
hd_home_start_pan (HdHome *home)
{
  HdHomePrivate   *priv = home->priv;
  GList           *l = priv->pan_queue;
  gint             move_by;
  ClutterTimeline *timeline1, *timeline2;

  move_by = clutter_actor_get_x (CLUTTER_ACTOR (home));

  while (l)
    {
      move_by += GPOINTER_TO_INT (l->data);
      l = l->next;
    }

  g_list_free (priv->pan_queue);
  priv->pan_queue = NULL;

  timeline1 = clutter_effect_move (priv->move_template,
				   CLUTTER_ACTOR (home),
				   move_by, 0,
				   (ClutterEffectCompleteFunc)
				   hd_home_pan_stage_completed, NULL);
  timeline2 = clutter_effect_move (priv->move_template,
				   priv->control_group,
				   -move_by, 0, NULL, NULL);

  clutter_timeline_start (timeline1);
  clutter_timeline_start (timeline2);
}

static void
hd_home_pan_by (HdHome *home, gint move_by)
{
  HdHomePrivate   *priv = home->priv;
  gboolean         in_progress = FALSE;

  if (priv->mode == HD_HOME_MODE_LAYOUT || !move_by)
    return;

  if (priv->pan_queue)
    in_progress = TRUE;

  priv->pan_queue = g_list_append (priv->pan_queue, GINT_TO_POINTER (move_by));

  if (!in_progress)
    {
      hd_home_start_pan (home);
    }
}

static void
hd_home_pan_full (HdHome *home, gboolean left)
{
  HdHomePrivate  *priv = home->priv;
  gint            by;
  gint            xwidth;

  if (priv->n_views < 2)
    return;

  /* Hide edit button */
  hd_home_hide_edit_button (home);

  by = xwidth = priv->xwidth;

  /*
   * Deal with view rollover.
   */
  if (left)
    {
      by *= -1;

      if (priv->current_view == priv->n_views - 1)
	{
	  gint          i = 0;
	  GList        *l = priv->views;
	  ClutterActor *view = g_list_first (l)->data;

	  l = g_list_remove (l, view);
	  l = g_list_append (l, view);

	  priv->views = l;

	  while (l)
	    {
	      view = l->data;
	      clutter_actor_set_position (view, i * xwidth, 0);
	      ++i;
	      l = l->next;
	    }

	  clutter_actor_set_position (CLUTTER_ACTOR (home),
				      -(priv->n_views-2)*xwidth, 0);
	}
      else
	{
	  ++priv->current_view;
	}

      hd_home_store_current_desktop (home, (priv->current_desktop+1)%priv->n_views);
    }
  else
    {
      if (priv->current_view == 0)
	{
	  gint          i = 0;
	  GList        *l = priv->views;
	  ClutterActor *view = g_list_last (l)->data;

	  l = g_list_remove (l, view);
	  l = g_list_prepend (l, view);

	  priv->views = l;

	  while (l)
	    {
	      view = l->data;
	      clutter_actor_set_position (view, i * xwidth, 0);
	      ++i;
	      l = l->next;
	    }

	  clutter_actor_set_position (CLUTTER_ACTOR (home), -xwidth, 0);
	}
      else
	{
	  --priv->current_view;
	}

      hd_home_store_current_desktop (home, (priv->current_desktop-1)%priv->n_views);
    }

  hd_home_pan_by (home, by);
}

void
hd_home_pan_and_move_applet (HdHome       *home,
			     gboolean      left,
			     ClutterActor *applet)
{
  HdHomePrivate *priv = home->priv;
  HdHomeView    *old_view;
  HdHomeView    *new_view;

  old_view = g_list_nth_data (priv->views, priv->current_view);

  hd_home_pan_full (home, left);

  new_view = g_list_nth_data (priv->views, priv->current_view);

  hd_home_view_move_applet (old_view, new_view, applet);
}

static void
hd_home_applet_destroyed (ClutterActor *original, ClutterActor *clone)
{
  clutter_actor_destroy (clone);
}

#if 0
static void
hd_home_status_area_destroyed (ClutterActor *original, HdHome *home)
{
  HdHomePrivate *priv = home->priv;
  g_debug ("hd_home_status_area_destroyed\n");
  clutter_container_remove (CLUTTER_CONTAINER (priv->control_group),
                            priv->status_area, NULL);
  clutter_actor_destroy (priv->status_area);
  priv->status_area = NULL;
}
#endif

void
hd_home_remove_status_area (HdHome *home, ClutterActor *sa)
{
  HdHomePrivate *priv = home->priv;
  ClutterActor *switcher;

  g_debug ("hd_home_remove_status_area, sa=%p\n", sa);
  switcher = hd_comp_mgr_get_switcher (HD_COMP_MGR (priv->comp_mgr));
  hd_switcher_remove_status_area (HD_SWITCHER (switcher), sa);

  clutter_container_remove_actor (CLUTTER_CONTAINER (priv->control_group), sa);
  hd_home_fixup_operator_position (home);
}

void
hd_home_add_status_menu (HdHome *home, ClutterActor *sa)
{
  HdHomePrivate *priv = home->priv;
  ClutterActor *switcher;

  g_debug ("hd_home_add_status_menu, sa=%p\n", sa);

  switcher = hd_comp_mgr_get_switcher (HD_COMP_MGR (priv->comp_mgr));
  hd_switcher_add_status_menu (HD_SWITCHER (switcher), sa);

  clutter_actor_unparent (sa);
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->control_group), sa);
}

void
hd_home_remove_status_menu (HdHome *home, ClutterActor *sa)
{
  HdHomePrivate *priv = home->priv;
  ClutterActor *switcher;

  g_debug ("hd_home_remove_status_menu, sa=%p\n", sa);

  switcher = hd_comp_mgr_get_switcher (HD_COMP_MGR (priv->comp_mgr));
  hd_switcher_remove_status_menu (HD_SWITCHER (switcher), sa);

  clutter_container_remove_actor (CLUTTER_CONTAINER (priv->control_group), sa);
}

void
hd_home_add_status_area (HdHome *home, ClutterActor *sa)
{
  HdHomePrivate *priv = home->priv;
  ClutterActor *switcher;

  g_debug ("hd_home_add_status_area, sa=%p\n", sa);
  /* FIXME: make a clone when FBOs work? */
  switcher = hd_comp_mgr_get_switcher (HD_COMP_MGR (priv->comp_mgr));
  hd_switcher_add_status_area (HD_SWITCHER (switcher), sa);

  clutter_actor_unparent (sa);
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->control_group), sa);

  hd_home_fixup_operator_position (home);

	g_signal_connect (sa, "notify::allocation",
			G_CALLBACK (hd_home_status_area_allocation_changed),
			home);
}

void
hd_home_add_applet (HdHome *home, ClutterActor *applet)
{
  HdHomePrivate *priv = home->priv;
  gint view_id;
  GList *l;
  GConfClient *client  = gconf_client_get_default ();
  gchar *applet_id;
  gchar *view_key, *position_key;
  GConfValue *value;
  GSList *position;
  MBGeometry geom;
  MBWMCompMgrClient *cclient;

  applet_id = g_object_get_data (G_OBJECT (applet), "HD-applet-id");

  view_key = g_strdup_printf ("/apps/osso/hildon-desktop/applets/%s/view", applet_id);
  position_key = g_strdup_printf ("/apps/osso/hildon-desktop/applets/%s/position", applet_id);

  value = gconf_client_get_without_default (client,
                                            view_key,
                                            NULL);

  if (value && value->type == GCONF_VALUE_INT)
    view_id = gconf_value_get_int (value);
  else
    {
      view_id = hd_home_get_current_view_id (home);

      gconf_client_set_int (client, view_key, view_id, NULL);
    }

  if (value)
    gconf_value_free (value);

  g_object_set_data (G_OBJECT (applet),
                     "HD-view-id", GINT_TO_POINTER (view_id));

  position = gconf_client_get_list (client,
                                    position_key,
                                    GCONF_VALUE_INT,
                                    NULL);

  if (position && position->next)
    {
      geom.x = GPOINTER_TO_INT (position->data);
      geom.y = GPOINTER_TO_INT (position->next->data);

      g_slist_free (position);
    }
  else
    {
      clutter_actor_get_position (applet, &geom.x, &geom.y);
    }

    {
      guint width, height;

      clutter_actor_get_size (applet, &width, &height);

      geom.width = width;
      geom.height = height;
    }

  cclient = g_object_get_data (G_OBJECT (applet), "HD-MBWMCompMgrClutterClient");

  mb_wm_client_request_geometry (cclient->wm_client, &geom,
				 MBWMClientReqGeomIsViaUserAction);

  g_object_unref (client);
  g_free (view_key);
  g_free (position_key);

  g_debug ("hd_home_add_applet (), view: %d", view_id);

  for (l = priv->views; l; l = l->next)
    {
      HdHomeView * view = l->data;
      gint         id = hd_home_view_get_view_id (view);

      if (id == view_id || (view_id < 0 && id == 0))
	{
	  hd_home_view_add_applet (view, applet);
	  break;
	}
      else if (view_id < 0)
	{
	  if (clutter_feature_available (CLUTTER_FEATURE_OFFSCREEN))
	    {
	      gpointer      client;
	      ClutterActor *clone = clutter_texture_new_from_actor (applet);

	      client = g_object_get_data (G_OBJECT (applet),
					  "HD-MBWMCompMgrClutterClient");

	      g_object_set_data (G_OBJECT (clone),
				 "HD-MBWMCompMgrClutterClient", client);

	      /*
	       * Connect to the destroy signal, so we can destroy the clone
	       * when the original is destroyed.
	       */
	      g_signal_connect (applet, "destroy",
				G_CALLBACK (hd_home_applet_destroyed), clone);

	      hd_home_view_add_applet (view, clone);
	    }
	  else
	    {
	      g_debug ("Sticky applets require FBO support in GL drivers.");
	    }
	}
    }
}


void
hd_home_remove_applet (HdHome *home, ClutterActor *applet)
{
  HdHomePrivate *priv = home->priv;
  gint           view_id;
  GList         *l;

  view_id =
    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (applet), "HD-view-id"));

  l = priv->views;

  while (l)
    {
      HdHomeView * view = l->data;
      gint         id = hd_home_view_get_view_id (view);

      if (id == view_id || view_id < 0)
	{
	  hd_home_view_remove_applet (view, applet);
	  break;
	}

      l = l->next;
    }

  if (applet == priv->active_applet)
    hd_home_hide_applet_buttons (home);
}

static void
hd_home_edit_button_move_completed (HdHome *home)
{
}

static gboolean
hd_home_edit_button_timeout (gpointer data)
{
  HdHome *home = data;

  hd_home_hide_edit_button (home);

  return FALSE;
}

static void
hd_home_show_edit_button (HdHome *home)
{
  HdHomePrivate   *priv = home->priv;
  guint            button_width, button_height;
  ClutterTimeline *timeline;
  gint             x;
  HdCompMgr       *comp_mgr = HD_COMP_MGR (priv->comp_mgr);
  ClutterGeometry  geom[2];

  if (priv->showing_edit_button)
    return;

  clutter_actor_get_size (priv->edit_button, &button_width, &button_height);

  x = priv->xwidth / 4 + priv->xwidth / 2;

  clutter_actor_set_position (priv->edit_button,
			      x,
			      -button_height);

  clutter_actor_show (priv->edit_button);
  clutter_actor_raise_top (priv->edit_button);

  g_debug ("moving edit button from %d, %d to %d, 0", x, -button_height, x);

  timeline = clutter_effect_move (priv->edit_button_template,
				  CLUTTER_ACTOR (priv->edit_button),
				  x, 0,
				  (ClutterEffectCompleteFunc)
				  hd_home_edit_button_move_completed, home);

  priv->showing_edit_button = TRUE;

  /* Add the area of the edit button to the input viewport */
  hd_switcher_get_button_geometry (HD_SWITCHER (hd_comp_mgr_get_switcher (comp_mgr)),
                                   &geom[0]);
  clutter_actor_get_geometry (priv->edit_button, &geom[1]);
  geom[1].y = 0;
  hd_comp_mgr_setup_input_viewport (comp_mgr, geom, 2);

  priv->edit_button_cb =
    g_timeout_add (HDH_EDIT_BUTTON_TIMEOUT, hd_home_edit_button_timeout, home);

  clutter_timeline_start (timeline);
}

void
hd_home_hide_edit_button (HdHome *home)
{
  HdHomePrivate   *priv = home->priv;
  HdCompMgr       *comp_mgr = HD_COMP_MGR (priv->comp_mgr);
  ClutterGeometry  geom;

  g_debug ("Hiding button");

  clutter_actor_hide (priv->edit_button);
  priv->showing_edit_button = FALSE;

  if (priv->edit_button_cb)
    {
      g_source_remove (priv->edit_button_cb);
      priv->edit_button_cb = 0;
    }

  /* Remove the area of the edit button from the input viewport */
  hd_switcher_get_button_geometry (HD_SWITCHER (hd_comp_mgr_get_switcher (comp_mgr)),
                                   &geom);
  hd_comp_mgr_setup_input_viewport (comp_mgr, &geom, 1);
}

void
hd_home_show_applet_buttons (HdHome *home, ClutterActor *applet)
{
  HdHomePrivate   *priv = home->priv;
  gint             x_a, y_a;
  guint            w_a, h_a, w_b, h_b;

  /* The applet buttons are placed within the edit_group. This group has the
   * same size as a single view and is automatically positioned over the active
   * view. This means that when placing the buttons, we do not need to consider
   * the position of the view they belong to.
   */
  clutter_actor_get_position (applet, &x_a, &y_a);
  clutter_actor_get_size (applet, &w_a, &h_a);

  clutter_actor_get_size (priv->applet_close_button, &w_b, &h_b);
  clutter_actor_set_position (priv->applet_close_button,
			      x_a + w_a - w_b/2,
			      y_a - h_b/2);
  clutter_actor_show (priv->applet_close_button);

#if 0
  clutter_actor_get_size (priv->applet_settings_button, &w_b, &h_b);
  clutter_actor_set_position (priv->applet_settings_button,
			      x_a - w_b/2,
			      y_a + h_a - h_b/2);
  clutter_actor_show (priv->applet_settings_button);

  clutter_actor_get_size (priv->applet_resize_button, &w_b, &h_b);
  clutter_actor_set_position (priv->applet_resize_button,
			      x_a + w_a - w_b/2,
			      y_a + h_a - h_b/2);
  clutter_actor_show (priv->applet_resize_button);
#endif

  priv->active_applet = applet;
}

void
hd_home_hide_applet_buttons (HdHome *home)
{
  HdHomePrivate   *priv = home->priv;

  clutter_actor_hide (priv->applet_close_button);
#if 0
  clutter_actor_hide (priv->applet_settings_button);
  clutter_actor_hide (priv->applet_resize_button);
#endif

  priv->active_applet = NULL;
}

void
hd_home_move_applet_buttons (HdHome *home, gint x_by, gint y_by)
{
  HdHomePrivate   *priv = home->priv;

  if (!priv->active_applet)
    return;

  clutter_actor_move_by (priv->applet_close_button, x_by, y_by);
#if 0
  clutter_actor_move_by (priv->applet_settings_button, x_by, y_by);
  clutter_actor_move_by (priv->applet_resize_button, x_by, y_by);
#endif
}

#if 0
static void
hd_home_send_settings_message (HdHome *home, Window xwin)
{
  HdHomePrivate   *priv = home->priv;
  HdCompMgr       *hmgr = HD_COMP_MGR (priv->comp_mgr);
  MBWindowManager *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;

  XEvent ev;
  Atom msg_atom;

  msg_atom =
    hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_CLIENT_MESSAGE_SHOW_SETTINGS);

  memset(&ev, 0, sizeof(ev));

  ev.xclient.type = ClientMessage;
  ev.xclient.window = xwin;
  ev.xclient.message_type = msg_atom;
  ev.xclient.format = 32;

  XSendEvent (wm->xdpy, xwin, False, StructureNotifyMask, &ev);

  XSync (wm->xdpy, False);
}
#endif

gint
hd_home_get_current_view_id (HdHome *home)
{
  HdHomePrivate   *priv = home->priv;
  HdHomeView      *top;

  top = g_list_nth_data (priv->views, priv->current_view);

  return hd_home_view_get_view_id (top);
}

HdHomeView *
hd_home_get_current_view (HdHome *home)
{
  HdHomePrivate   *priv = home->priv;

  return (HdHomeView *)g_list_nth_data (priv->views, priv->current_view);
}

void
hd_home_set_operator_label (HdHome *home, const char *text)
{
  HdHomePrivate *priv = home->priv;

  clutter_label_set_text (CLUTTER_LABEL (priv->operator_label), text);
  hd_home_fixup_operator_position (home);
}

void
hd_home_set_operator_icon (HdHome *home, const char *file)
{
  HdHomePrivate *priv = home->priv;
  ClutterActor  *icon = NULL;

  if (priv->operator_icon)
    clutter_actor_destroy (priv->operator_icon);

  if (file)
    icon = clutter_texture_new_from_file (file, NULL);

  priv->operator_icon = icon;

  if (icon)
    {
      clutter_actor_show (icon);
      clutter_container_add_actor (CLUTTER_CONTAINER (priv->operator), icon);
    }

  hd_home_fixup_operator_position (home);
}


void
hd_home_fixup_operator_position (HdHome *home)
{
  HdHomePrivate *priv = home->priv;
  guint          control_width, control_height;
  guint          icon_width = 0, icon_height = 0;
  guint          op_width, op_height = 0;
  guint          label_width, label_height;
  ClutterActor  *switcher;
  MBWindowManager *wm;

  switcher = hd_comp_mgr_get_switcher (HD_COMP_MGR (priv->comp_mgr));

  hd_switcher_get_control_area_size (HD_SWITCHER (switcher),
				     &control_width, &control_height);

  wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;
  if (wm)
    {
      g_debug ("Adjust dialogue position of %p", wm);
      mb_adjust_dialog_title_position (wm,
				       control_width);
    }
  else
    g_debug ("Don't adjust position because there's no WM");

  if (priv->operator_icon)
    clutter_actor_get_size (priv->operator_icon, &icon_width, &icon_height);

  clutter_actor_get_size (priv->operator_label, &label_width, &label_height);

  clutter_actor_get_size (priv->operator, &op_width, &op_height);

  clutter_actor_set_position (priv->operator_label,
			      icon_width + HDH_OPERATOR_PADDING,
			      (op_height - label_height)/2);

  clutter_actor_set_position (priv->operator,
			      control_width + HDH_OPERATOR_PADDING,
			      (control_height - op_height)/2);
}

void
hd_home_show_switches (HdHome *home)
{
  HdHomePrivate *priv = home->priv;

  clutter_actor_set_opacity (priv->left_switch, 0x7f);
  clutter_actor_set_opacity (priv->right_switch, 0x7f);
  clutter_actor_show (priv->left_switch);
  clutter_actor_show (priv->right_switch);
}

void
hd_home_hide_switches (HdHome *home)
{
  HdHomePrivate *priv = home->priv;

  clutter_actor_hide (priv->left_switch);
  clutter_actor_hide (priv->right_switch);
}

void
hd_home_highlight_switch (HdHome *home, gboolean left)
{
  HdHomePrivate *priv = home->priv;

  if (left)
    {
      clutter_actor_set_opacity (priv->left_switch, 0xff);
      clutter_actor_set_opacity (priv->right_switch, 0x7f);
    }
  else
    {
      clutter_actor_set_opacity (priv->left_switch, 0x7f);
      clutter_actor_set_opacity (priv->right_switch, 0xff);
    }
}

void
hd_home_unhighlight_switches (HdHome *home)
{
  HdHomePrivate *priv = home->priv;

  clutter_actor_set_opacity (priv->left_switch, 0x7f);
  clutter_actor_set_opacity (priv->right_switch, 0x7f);
}

GList *
hd_home_get_all_views (HdHome *home)
{
  HdHomePrivate *priv = home->priv;

  return priv->all_views;
}

GList *
hd_home_get_active_views (HdHome *home)
{
  HdHomePrivate *priv = home->priv;

  return priv->views;
}

static void
hd_home_activate_view (HdHome * home, guint id)
{
  g_debug ("%s: Activating view %d\n", __FUNCTION__, id);
  HdHomePrivate *priv = home->priv;
  ClutterActor  *view;
  GList         *l;
  gboolean       done = FALSE;

  view = g_list_nth_data (priv->all_views, id);

  l = g_list_find (priv->views, view);

  if (l)
    return;

  l = priv->views;
  while (l)
    {
      HdHomeView *v = l->data;
      guint       i = hd_home_view_get_view_id (v);

      if (i > id)
	{
	  priv->views = g_list_insert_before (priv->views, l, view);
	  done = TRUE;
	  break;
	}

      l = l->next;
    }

  if (!done)
    priv->views = g_list_append (priv->views, view);

  ++priv->n_views;
  hd_home_store_n_views (home);

  clutter_actor_show (view);
}

static void
hd_home_deactivate_view (HdHome * home, guint id)
{
  g_debug ("%s: Deactivating view %d\n", __FUNCTION__, id);
  HdHomePrivate *priv = home->priv;
  ClutterActor  *view;
  GList         *l;

  view = g_list_nth_data (priv->all_views, id);

  l = g_list_find (priv->views, view);

  if (!l)
    return;

  priv->views = g_list_remove (priv->views, view);
  --priv->n_views;
  hd_home_store_n_views (home);

  clutter_actor_hide (view);

  if (priv->current_view == priv->n_views)
    priv->current_view = 0;
}

void
hd_home_set_view_status (HdHome * home, guint id, gboolean active)
{
  if (active)
    hd_home_activate_view (home, id);
  else
    hd_home_deactivate_view (home, id);
}

static void
hd_home_store_n_views(HdHome *home)
{
  HdHomePrivate *priv = home->priv;
  MBWindowManager *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;
  long propvalue[1] = {priv->n_views, };

  g_debug ("------- Number of desktops is now %d", priv->n_views);

  XChangeProperty (wm->xdpy, wm->root_win->xwindow,
		   wm->atoms[MBWM_ATOM_NET_NUMBER_OF_DESKTOPS],
		   XA_CARDINAL, 32, PropModeReplace,
		   (unsigned char *) propvalue,
		   1);
}

static void
hd_home_store_current_desktop(HdHome *home, guint new_desktop)
{
  HdHomePrivate *priv = home->priv;
  MBWindowManager *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;
  long propvalue[1] = {new_desktop, };

  priv->current_desktop = new_desktop;

  g_debug ("------- Current desktop is now %d", new_desktop);

  XChangeProperty (wm->xdpy, wm->root_win->xwindow,
		   wm->atoms[MBWM_ATOM_NET_CURRENT_DESKTOP],
		   XA_CARDINAL, 32, PropModeReplace,
		   (unsigned char *) propvalue,
		   1);
}

