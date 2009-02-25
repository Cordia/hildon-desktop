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
#include "hd-home-view-container.h"
#include "hd-comp-mgr.h"
#include "hd-util.h"
#include "hd-gtk-style.h"
#include "hd-gtk-utils.h"
#include "hd-home-applet.h"
#include "hd-render-manager.h"
#include "hd-clutter-cache.h"
#include "hd-theme.h"

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>

#include <matchbox/core/mb-wm.h>

#include <gconf/gconf-client.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>

#define HDH_EDIT_BUTTON_DURATION 200
#define HDH_EDIT_BUTTON_TIMEOUT 3000

#define HDH_LAYOUT_TOP_SCALE 0.5
#define HDH_LAYOUT_Y_OFFSET 60

/* FIXME -- match spec */
#define HDH_PAN_THRESHOLD 20
#define PAN_NEXT_PREVIOUS_PERCENTAGE 0.25

#define HD_HOME_DBUS_NAME  "com.nokia.HildonDesktop.Home"
#define HD_HOME_DBUS_PATH  "/com/nokia/HildonDesktop/Home"

#define CALL_UI_DBUS_NAME "com.nokia.CallUI"
#define CALL_UI_DBUS_PATH "/com/nokia/CallUI"
#define CALL_UI_DBUS_METHOD_SHOW_DIALPAD "ShowDialpad"

#define OSSO_ADDRESSBOOK_DBUS_NAME "com.nokia.osso_addressbook"
#define OSSO_ADDRESSBOOK_DBUS_PATH "/com/nokia/osso_addressbook"
#define OSSO_ADDRESSBOOK_DBUS_METHOD_SEARCH_APPEND "search_append"

#define INDICATION_WIDTH 50
#define HD_EDGE_INDICATION_COLOR "SelectionColor"

#define MAX_VIEWS 4

enum
{
  PROP_COMP_MGR = 1,
};

enum
{
  SIGNAL_BACKGROUND_CLICKED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

struct _HdHomePrivate
{
  MBWMCompMgrClutter    *comp_mgr;

  ClutterEffectTemplate *edit_button_template;

  ClutterActor          *edit_group; /* An overlay group for edit mode */
  ClutterActor          *edit_button;

  ClutterActor          *operator;
  ClutterActor          *operator_applet;

  ClutterActor          *edge_indication_left;
  ClutterActor          *edge_indication_right;

  ClutterActor          *view_container;

  /* container that sits infront of blurred home */
  ClutterGroup          *front;

  gulong                 desktop_motion_cb;

  guint                  edit_button_cb;

  /* Pan variables */
  gint                   last_x;
  gint                   initial_x;
  gint                   initial_y;
  gint                   cumulative_x;

  gboolean               moved_over_threshold : 1;

  Window                 desktop;

  /* DBus Proxy for the call to com.nokia.CallUI.ShowDialpad */
  DBusGProxy            *call_ui_proxy;
  DBusGProxy            *osso_addressbook_proxy;
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

static void hd_home_show_edit_button (HdHome *home);

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

}

static gboolean
hd_home_edit_button_clicked (ClutterActor *button,
			     ClutterEvent *event,
			     HdHome       *home)
{
  hd_render_manager_set_state(HDRM_STATE_HOME_EDIT);

  return TRUE;
}

gboolean
hd_home_back_button_clicked (ClutterActor *button,
			     ClutterEvent *event,
			     HdHome       *home)
{
  if (hd_render_manager_get_state()==HDRM_STATE_HOME_EDIT)
    hd_render_manager_set_state(HDRM_STATE_HOME);

  return FALSE;
}

static Bool
hd_home_desktop_motion (XButtonEvent *xev, void *userdata)
{
  HdHome          *home = userdata;
  HdHomePrivate   *priv = home->priv;
//  MBWindowManager *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;
  gint by_x;

  by_x = xev->x - priv->last_x;

  priv->cumulative_x += by_x;

  if (ABS (priv->cumulative_x) > HDH_PAN_THRESHOLD)
    priv->moved_over_threshold = TRUE;

  if (priv->moved_over_threshold)
    hd_home_view_container_set_offset (HD_HOME_VIEW_CONTAINER (priv->view_container),
                                  CLUTTER_UNITS_FROM_DEVICE (priv->cumulative_x));

  priv->last_x = xev->x;

  return True;
}

static Bool
hd_home_desktop_release (XButtonEvent *xev, void *userdata)
{
  HdHome *home = userdata;
  HdHomePrivate *priv = home->priv;
  MBWindowManager *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;

  g_debug("%s:", __FUNCTION__);

  if (priv->desktop_motion_cb)
    mb_wm_main_context_x_event_handler_remove (wm->main_ctx,
					       MotionNotify,
					       priv->desktop_motion_cb);

  priv->desktop_motion_cb = 0;

  if (priv->moved_over_threshold)
    {
      if (ABS (priv->cumulative_x) >= PAN_NEXT_PREVIOUS_PERCENTAGE * HD_COMP_MGR_LANDSCAPE_WIDTH) /* */
        {
          if (priv->cumulative_x > 0)
            hd_home_view_container_scroll_to_previous (HD_HOME_VIEW_CONTAINER (priv->view_container));
          else
            hd_home_view_container_scroll_to_next (HD_HOME_VIEW_CONTAINER (priv->view_container));
        }
      else
        hd_home_view_container_scroll_back (HD_HOME_VIEW_CONTAINER (priv->view_container));
    }
  else if (hd_render_manager_get_state() == HDRM_STATE_HOME)
    {
      hd_home_show_edit_button (home);
    }

  priv->cumulative_x = 0;
  priv->moved_over_threshold = FALSE;

  return True;
}

static Bool
hd_home_desktop_key_press (XKeyEvent *xev, void *userdata)
{
  HdHome *home = userdata;
  HdHomePrivate *priv = home->priv;
  static XComposeStatus *compose_status = NULL;

  char buffer[10] = {0,};

  XLookupString (xev, buffer, 10, NULL, compose_status);

  /* g_debug ("%s, key: %s, %x, %d", __FUNCTION__, buffer, buffer[0], g_ascii_isdigit (buffer[0])); */

  if (priv->call_ui_proxy && g_ascii_isdigit (buffer[0]))
    {
/*      g_debug ("Call Dialpad via D-BUS. " CALL_UI_DBUS_NAME "." CALL_UI_DBUS_METHOD_SHOW_DIALPAD " (s: %s)", buffer);*/

      dbus_g_proxy_call_no_reply (priv->call_ui_proxy, CALL_UI_DBUS_METHOD_SHOW_DIALPAD,
                                  G_TYPE_STRING, buffer,
                                  G_TYPE_INVALID);
    }
  else if (priv->osso_addressbook_proxy && buffer[0])
    {
/*      g_debug ("Call via D-BUS. " OSSO_ADDRESSBOOK_DBUS_NAME "." OSSO_ADDRESSBOOK_DBUS_METHOD_SEARCH_APPEND " (s: %s)", buffer); */

      dbus_g_proxy_call_no_reply (priv->osso_addressbook_proxy, OSSO_ADDRESSBOOK_DBUS_METHOD_SEARCH_APPEND,
                                  G_TYPE_STRING, buffer,
                                  G_TYPE_INVALID);
    }

  return TRUE;
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
hd_property_notify_message (XPropertyEvent *xev, void *userdata)
{
  HdHome          *home = userdata;
  HdHomePrivate *priv = home->priv;
  HdCompMgr     *hmgr = HD_COMP_MGR (priv->comp_mgr);

  if (xev->atom==hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_WM_WINDOW_PROGRESS_INDICATOR))
    { /* Redraw the title to display/remove the progress indicator. */
      MBWindowManagerClient *top;
      /* previous mb_wm_client_decor_mark_dirty didn't actually cause a redraw,
       * so mark the decor itself dirty */
      if ((top = mb_wm_get_visible_main_client(MB_WM_COMP_MGR (hmgr)->wm)) != NULL)
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

  return True;
}

/* Called when a client message is sent to the root window. */
static Bool
root_window_client_message (XClientMessageEvent *event, HdHome *home)
{
#if 0
  //  FIXME should we really support NET_CURRENT_DESKTOP?

  HdHomePrivate *priv = home->priv;
  MBWindowManager *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;

  if (event->message_type == wm->atoms[MBWM_ATOM_NET_CURRENT_DESKTOP])
    {
      gint desktop = event->data.l[0];
      hd_home_view_container_set_current_view (HD_HOME_VIEW_CONTAINER (priv->view_container),
                                               desktop);
    }
#endif

  return False;
}

static ClutterActor *
hd_home_create_edit_button (void)
{
  ClutterActor *edit_button;
  ClutterActor *bg_left, *bg_center, *bg_right, *icon;
  ClutterGeometry geom = { 0, };

  edit_button = clutter_group_new ();

  /* Load textures */
  bg_left = hd_clutter_cache_get_texture (HD_THEME_IMG_BUTTON_LEFT_HALF, TRUE);
  bg_right = hd_clutter_cache_get_texture (HD_THEME_IMG_BUTTON_RIGHT_HALF, TRUE);
  icon = hd_clutter_cache_get_texture (HD_THEME_IMG_EDIT_ICON, TRUE);

  /* Cut out the half of the texture */
  geom.width = clutter_actor_get_width (icon) / 2;
  geom.height = clutter_actor_get_height (icon);
  bg_center = hd_clutter_cache_get_sub_texture (HD_THEME_IMG_LEFT_ATTACHED, TRUE, &geom);

  /* Add textures to edit button */
  clutter_container_add (CLUTTER_CONTAINER (edit_button),
                         bg_left, bg_center, bg_right, icon, NULL);

  /* Layout textures */
  clutter_actor_set_position (bg_left, 0, 0);
  clutter_actor_set_position (bg_center, clutter_actor_get_width (bg_left), 0);
  clutter_actor_set_position (bg_right, clutter_actor_get_width (bg_left) + clutter_actor_get_width (bg_center), 0);

  /* Icon is centered on top the button */
  clutter_actor_set_position (icon,
                              (clutter_actor_get_width (edit_button) - clutter_actor_get_width (icon)) / 2,
                              0);

  return edit_button;
}

static void
hd_home_constructed (GObject *object)
{
  HdHomePrivate   *priv = HD_HOME (object)->priv;
  ClutterActor    *edit_group;
  MBWindowManager *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;
  ClutterColor     clr = {0,0,0,0xff};
  XSetWindowAttributes attr;
  XWMHints         wmhints;
  GtkStyle        *style;
  GdkColor         color;

  clutter_actor_set_name (CLUTTER_ACTOR(object), "HdHome");

  priv->front = CLUTTER_GROUP(clutter_group_new());
  clutter_actor_set_name (CLUTTER_ACTOR(priv->front), "HdHome:front");
  clutter_container_add_actor (CLUTTER_CONTAINER (object),
                               CLUTTER_ACTOR(priv->front));

  edit_group = priv->edit_group = clutter_group_new ();
  clutter_actor_set_name (edit_group, "HdHome:edit_group");
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->front), edit_group);
  clutter_actor_hide (edit_group);

  priv->view_container = hd_home_view_container_new (HD_COMP_MGR (priv->comp_mgr),
                                                CLUTTER_ACTOR (object));
  clutter_actor_set_name (priv->view_container, "HdHome:view_container");
  clutter_container_add_actor (CLUTTER_CONTAINER (object), priv->view_container);
  clutter_actor_set_size (CLUTTER_ACTOR (priv->view_container),
                          HD_COMP_MGR_LANDSCAPE_WIDTH,
                          HD_COMP_MGR_LANDSCAPE_HEIGHT);
  clutter_actor_show (priv->view_container);

  priv->edit_button = hd_home_create_edit_button ();
  clutter_actor_hide (priv->edit_button);
  clutter_actor_set_reactive (priv->edit_button, TRUE);

  g_signal_connect (priv->edit_button, "button-release-event",
		    G_CALLBACK (hd_home_edit_button_clicked),
		    object);

  priv->operator = clutter_group_new ();
  clutter_actor_set_name(priv->operator, "HdHome:operator");
  clutter_actor_show (priv->operator);
  clutter_actor_reparent (priv->operator, CLUTTER_ACTOR (object));
  clutter_actor_raise (priv->operator, priv->view_container);

  priv->edge_indication_left = clutter_rectangle_new ();
  clutter_actor_set_name (priv->edge_indication_left, "HdHome:left_switch");

  /* FIXME -- should the color come from theme ? */
  style = gtk_rc_get_style_by_paths (gtk_settings_get_default (),
                                     NULL, NULL,
                                     GTK_TYPE_WIDGET);
  gtk_style_lookup_color (style, HD_EDGE_INDICATION_COLOR, &color);
  clr.red = color.red >> 8;
  clr.green = color.green >> 8;
  clr.blue = color.blue >> 8;
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (priv->edge_indication_left), &clr);

  clutter_actor_set_size (priv->edge_indication_left, HD_EDGE_INDICATION_WIDTH, HD_COMP_MGR_LANDSCAPE_HEIGHT);
  clutter_actor_set_position (priv->edge_indication_left, 0, 0);
  clutter_actor_hide (priv->edge_indication_left);
  clutter_container_add_actor (CLUTTER_CONTAINER (edit_group), priv->edge_indication_left);

  priv->edge_indication_right = clutter_rectangle_new ();
  clutter_actor_set_name (priv->edge_indication_right, "HdHome:right_switch");

  clutter_rectangle_set_color (CLUTTER_RECTANGLE (priv->edge_indication_right), &clr);

  clutter_actor_set_size (priv->edge_indication_right, HD_EDGE_INDICATION_WIDTH, HD_COMP_MGR_LANDSCAPE_HEIGHT);
  clutter_actor_set_position (priv->edge_indication_right,
			      HD_COMP_MGR_LANDSCAPE_WIDTH - HD_EDGE_INDICATION_WIDTH, 0);
  clutter_actor_hide (priv->edge_indication_right);
  clutter_container_add_actor (CLUTTER_CONTAINER (edit_group), priv->edge_indication_right);

  clutter_actor_lower_bottom (priv->view_container);

  /*
   * Create an InputOnly desktop window; we have a custom desktop client that
   * that will automatically wrap it, ensuring it is located in the correct
   * place.  Make this window focusable ("Input" WM hint) so
   * hd_home_desktop_key_press() will actually work.
   */
  attr.event_mask = MBWMChildMask |
    ButtonPressMask | ButtonReleaseMask |
    PointerMotionMask | ExposureMask |
    KeyPressMask;
  wmhints.input = True;
  wmhints.flags = InputHint;

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
  XSetWMHints (wm->xdpy, priv->desktop, &wmhints);
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

  mb_wm_main_context_x_event_handler_add (wm->main_ctx,
					  None,
					  PropertyNotify,
					  (MBWMXEventFunc)
					  hd_property_notify_message,
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

  priv->edit_button_template =
    clutter_effect_template_new_for_duration (HDH_EDIT_BUTTON_DURATION,
					      CLUTTER_ALPHA_RAMP_INC);

  /* Listen to gconf notifications */
  gconf_client_add_dir (gconf_client_get_default (),
                        "/apps/osso/hildon-desktop",
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

  priv->call_ui_proxy = dbus_g_proxy_new_for_name (connection,
                                                   CALL_UI_DBUS_NAME,
                                                   CALL_UI_DBUS_PATH,
                                                   CALL_UI_DBUS_NAME);

  priv->osso_addressbook_proxy = dbus_g_proxy_new_for_name (connection,
                                                            OSSO_ADDRESSBOOK_DBUS_NAME,
                                                            OSSO_ADDRESSBOOK_DBUS_PATH,
                                                            OSSO_ADDRESSBOOK_DBUS_NAME);

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

/* FOR HDRM_STATE_HOME */
static void
_hd_home_do_normal_layout (HdHome *home)
{
  HdHomePrivate *priv = home->priv;

  clutter_actor_hide (priv->edit_group);

  hd_home_hide_edge_indication (home);
}

/* FOR HDRM_STATE_HOME_EDIT */
static void
_hd_home_do_edit_layout (HdHome *home)
{
  HdHomePrivate *priv = home->priv;

  _hd_home_do_normal_layout (home);

  clutter_actor_set_position (priv->edit_group, 0, 0);
  clutter_actor_show (priv->edit_group);
}

void
hd_home_update_layout (HdHome * home)
{
  /* FIXME: ideally all this should be done by HdRenderManager */
  HdHomePrivate *priv;
  guint i;

  if (!HD_IS_HOME(home))
    return;
  priv = home->priv;

  switch (hd_render_manager_get_state())
    {
    case HDRM_STATE_HOME:
      _hd_home_do_normal_layout(home);
      break;
    case HDRM_STATE_HOME_EDIT:
      _hd_home_do_edit_layout(home);
      break;
    default:
      g_warning("%s: should only be called for HDRM_STATE_HOME.*",
                __FUNCTION__);
    }

  for (i = 0; i < MAX_VIEWS; i++)
    {
      ClutterActor *view;
      
      view = hd_home_view_container_get_view (HD_HOME_VIEW_CONTAINER (priv->view_container),
                                              i);
      hd_home_view_update_state (HD_HOME_VIEW (view));
    }
}

void
hd_home_remove_status_area (HdHome *home, ClutterActor *sa)
{
  g_debug ("hd_home_remove_status_area, sa=%p\n", sa);

  hd_render_manager_set_status_area (NULL);
}

void
hd_home_add_status_menu (HdHome *home, ClutterActor *sa)
{
  g_debug ("hd_home_add_status_menu, sa=%p\n", sa);

  hd_render_manager_set_status_menu (sa);
}

void
hd_home_remove_status_menu (HdHome *home, ClutterActor *sa)
{
  g_debug ("hd_home_remove_status_menu, sa=%p\n", sa);

  hd_render_manager_set_status_menu (NULL);
}

void
hd_home_add_status_area (HdHome *home, ClutterActor *sa)
{
  g_debug ("hd_home_add_status_area, sa=%p\n", sa);

  hd_render_manager_set_status_area(sa);
}

void
hd_home_add_applet (HdHome *home, ClutterActor *applet)
{
  HdHomePrivate *priv = home->priv;
  gint view_id;
  GConfClient *client = gconf_client_get_default ();
  gchar *view_key;
  MBWMCompMgrClient *cclient;
  HdHomeApplet *wm_applet;

  cclient = g_object_get_data (G_OBJECT (applet), "HD-MBWMCompMgrClutterClient");
  wm_applet = (HdHomeApplet *) cclient->wm_client;

  view_key = g_strdup_printf ("/apps/osso/hildon-desktop/applets/%s/view", wm_applet->applet_id);

  view_id = gconf_client_get_int (client,
                                  view_key,
                                  NULL);

  if (view_id > 0 && view_id <= MAX_VIEWS)
    view_id--;
  else
    {
      view_id = hd_home_get_current_view_id (home);

      /* from 0 to 3 */
      gconf_client_set_int (client, view_key, view_id + 1, NULL);
    }

  wm_applet->view_id = view_id;

  g_object_set_data (G_OBJECT (applet),
                     "HD-view-id", GINT_TO_POINTER (view_id));

  g_object_unref (client);
  g_free (view_key);

  g_debug ("hd_home_add_applet (), view: %d", view_id);

  if (view_id >= 0)
    {
      ClutterActor *view;

      view = hd_home_view_container_get_view (HD_HOME_VIEW_CONTAINER (priv->view_container),
                                              view_id);
      hd_home_view_add_applet (HD_HOME_VIEW (view), applet);
    }
  else
    {
      g_debug ("%s FIXME: implement sticky applets", __FUNCTION__);
    }
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

  if (hd_render_manager_get_visible(HDRM_BUTTON_EDIT))
    return;

  clutter_actor_get_size (priv->edit_button, &button_width, &button_height);

  x = HD_COMP_MGR_LANDSCAPE_WIDTH - button_width - HD_COMP_MGR_TOP_RIGHT_BTN_WIDTH;

  clutter_actor_set_position (priv->edit_button,
                              x,
                              0);
  /* we must set the final position first so that the X input
   * area can be set properly */
  hd_render_manager_set_visible(HDRM_BUTTON_EDIT, TRUE);

  clutter_actor_set_position (priv->edit_button,
                              x,
                              -button_height);

  g_debug ("moving edit button from %d, %d to %d, 0", x, -button_height, x);

  timeline = clutter_effect_move (priv->edit_button_template,
                                  CLUTTER_ACTOR (priv->edit_button),
                                  x, 0,
                                  (ClutterEffectCompleteFunc)
                                  hd_home_edit_button_move_completed, home);

  priv->edit_button_cb =
    g_timeout_add (HDH_EDIT_BUTTON_TIMEOUT, hd_home_edit_button_timeout, home);

  clutter_timeline_start (timeline);
}

void
hd_home_hide_edit_button (HdHome *home)
{
  HdHomePrivate   *priv = home->priv;

  if (!hd_render_manager_get_visible(HDRM_BUTTON_EDIT))
    return;

  g_debug ("%s: Hiding button", __FUNCTION__);

  hd_render_manager_set_visible(HDRM_BUTTON_EDIT, FALSE);
  if (priv->edit_button_cb)
    {
      g_source_remove (priv->edit_button_cb);
      priv->edit_button_cb = 0;
    }
}

ClutterActor*
hd_home_get_edit_button (HdHome *home)
{
  HdHomePrivate *priv = home->priv;
  return priv->edit_button;
}

ClutterActor*
hd_home_get_operator (HdHome *home)
{
  HdHomePrivate *priv = home->priv;
  return priv->operator;
}

ClutterActor*
hd_home_get_front (HdHome *home)
{
  HdHomePrivate *priv = home->priv;
  return CLUTTER_ACTOR(priv->front);
}

guint
hd_home_get_current_view_id (HdHome *home)
{
  HdHomePrivate   *priv = home->priv;

  return hd_home_view_container_get_current_view (HD_HOME_VIEW_CONTAINER (priv->view_container));
}

ClutterActor *
hd_home_get_current_view (HdHome *home)
{
  HdHomePrivate   *priv = home->priv;

  return hd_home_view_container_get_view (HD_HOME_VIEW_CONTAINER (priv->view_container),
                                          hd_home_get_current_view_id (home));
}

void
hd_home_set_operator_applet (HdHome *home, ClutterActor *applet)
{
  HdHomePrivate *priv = home->priv;

  if (priv->operator_applet)
    clutter_actor_destroy (priv->operator_applet);

  priv->operator_applet = applet;

  if (applet)
    {
      clutter_actor_show (applet);
      clutter_actor_reparent (applet, priv->operator);
    }
}

void
hd_home_show_edge_indication (HdHome *home)
{
  HdHomePrivate *priv = home->priv;

  clutter_actor_set_opacity (priv->edge_indication_left, 0x7f);
  clutter_actor_set_opacity (priv->edge_indication_right, 0x7f);

  if (hd_home_view_container_get_previous_view (HD_HOME_VIEW_CONTAINER (priv->view_container)))
    clutter_actor_show (priv->edge_indication_left);
  if (hd_home_view_container_get_next_view (HD_HOME_VIEW_CONTAINER (priv->view_container)))
    clutter_actor_show (priv->edge_indication_right);
}

void
hd_home_hide_edge_indication (HdHome *home)
{
  HdHomePrivate *priv = home->priv;

  clutter_actor_hide (priv->edge_indication_left);
  clutter_actor_hide (priv->edge_indication_right);
}

void
hd_home_highlight_edge_indication (HdHome *home, gboolean left, gboolean right)
{
  HdHomePrivate *priv = home->priv;

  clutter_actor_set_opacity (priv->edge_indication_left, left ? 0xff : 0x7f);
  clutter_actor_set_opacity (priv->edge_indication_right, right ? 0xff : 0x7f);
}

void
hd_home_set_reactive (HdHome   *home,
                      gboolean  reactive)
{
  g_return_if_fail (HD_IS_HOME (home));

  hd_home_view_container_set_reactive (HD_HOME_VIEW_CONTAINER (home->priv->view_container),
                                       reactive);
}

