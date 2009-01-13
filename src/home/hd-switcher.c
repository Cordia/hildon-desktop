/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Johan Bilien <johan.bilien@nokia.com>
 *          Tomas Frydrych <tf@o-hand.com>
 *          Kimmo Hämäläinen <kimmo.hamalainen@nokia.com>
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

#include "hd-switcher.h"
#include "hd-task-navigator.h"
#include "hd-launcher.h"
#include "hd-comp-mgr.h"
#include "hd-util.h"
#include "hd-home.h"
#include "hd-gtk-utils.h"
#include "hd-render-manager.h"

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>

#include <matchbox/core/mb-wm.h>
#include <matchbox/core/mb-wm-object.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr-clutter.h>

#include <dbus/dbus-glib.h>

#define ICON_IMAGE_SWITCHER "qgn_tswitcher_application"
#define ICON_IMAGE_LAUNCHER "qgn_general_add"
#define BUTTON_IMAGE_MENU     "menu-button.png"

#define TOP_LEFT_BUTTON_HIGHLIGHT_TEXTURE "launcher-button-highlight.png"
#define TOP_LEFT_BUTTON_WIDTH	112
#define TOP_LEFT_BUTTON_HEIGHT	56

enum
{
  PROP_COMP_MGR = 1,
  PROP_TASK_NAV = 2
};

struct _HdSwitcherPrivate
{
  ClutterActor         *status_area;
  ClutterActor         *status_menu;

  HdLauncher           *launcher;
  HdTaskNavigator      *task_nav;

  DBusGConnection      *connection;
  DBusGProxy           *hildon_home_proxy;

  MBWMCompMgrClutter   *comp_mgr;

  /* FIXME: There should be a better way of knowing when not to respond
   * to user input because we're transitioning from one state to the
   * other.
   */
  gboolean in_transition;
};

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

static gboolean hd_switcher_clicked (HdSwitcher *switcher);

static ClutterActor * hd_switcher_top_left_button_new (const char *icon_name);

static gboolean hd_switcher_menu_clicked (HdSwitcher *switcher);

static void hd_switcher_item_selected (HdSwitcher *switcher,
				       ClutterActor *actor);

static void hd_switcher_item_closed (HdSwitcher *switcher,
                                     ClutterActor *actor);


static void hd_switcher_group_background_clicked (HdSwitcher   *switcher,
						  ClutterActor *actor);
static void hd_switcher_home_background_clicked (HdSwitcher   *switcher,
						 ClutterActor *actor);

static gboolean hd_switcher_notification_clicked (HdSwitcher *switcher,
                                                  HdNote *note);
static gboolean hd_switcher_notification_closed (HdSwitcher *switcher,
                                                 HdNote *note);

static void hd_switcher_launcher_cat_launched (HdLauncher *launcher,
                                               HdSwitcher *switcher);
static void hd_switcher_launcher_cat_hidden (HdLauncher *launcher,
                                             HdSwitcher *switcher);

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
  HdSwitcherPrivate *priv = HD_SWITCHER (data)->priv;

  g_debug("launcher_back_button_clicked\n");
  if (!hd_task_navigator_is_empty (priv->task_nav))
    hd_render_manager_set_state(HDRM_STATE_TASK_NAV);
  else
    hd_render_manager_set_state(HDRM_STATE_HOME);
}

static void
hd_switcher_constructed (GObject *object)
{
  GError            *error = NULL;
  HdSwitcherPrivate *priv = HD_SWITCHER (object)->priv;
  guint              button_width, button_height;
  HdHome	    *home =
    HD_HOME (hd_comp_mgr_get_home (HD_COMP_MGR (priv->comp_mgr)));
  ClutterActor      *actor;

  g_signal_connect_swapped (home, "background-clicked",
                            G_CALLBACK (hd_switcher_home_background_clicked),
                            object);
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

  /* Task Navigator Button */
  actor = hd_switcher_top_left_button_new (ICON_IMAGE_SWITCHER);
  hd_render_manager_set_button (HDRM_BUTTON_TASK_NAV, actor);
  clutter_actor_set_position (actor, 0, 0);
  clutter_actor_set_reactive (actor, TRUE);
  g_signal_connect_swapped (actor, "button-release-event",
                            G_CALLBACK (hd_switcher_clicked),
                            object);

  /* Task Launcher Button */
  actor = hd_switcher_top_left_button_new (ICON_IMAGE_LAUNCHER);
  hd_render_manager_set_button(HDRM_BUTTON_LAUNCHER, actor);
  clutter_actor_set_position (actor, 0, 0);
  clutter_actor_set_reactive (actor, TRUE);
  g_signal_connect_swapped (actor, "button-release-event",
                            G_CALLBACK (hd_switcher_clicked),
                            object);

  clutter_actor_get_size (actor, &button_width, &button_height);

  /* Home Menu Button */
  actor = clutter_texture_new_from_file (
		g_build_filename (HD_DATADIR, BUTTON_IMAGE_MENU, NULL),
	        &error);
  if (error)
    {
      g_error (error->message);
      actor = clutter_rectangle_new ();
      clutter_actor_set_size (actor, 200, 60);
    }
  hd_render_manager_set_button(HDRM_BUTTON_MENU, actor);
  clutter_actor_set_position (actor, 0, 0);
  clutter_actor_set_reactive (actor, TRUE);
  g_signal_connect_swapped (actor, "button-release-event",
                            G_CALLBACK (hd_switcher_menu_clicked),
                            object);
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

/* I wonder if this utility function should go somewhere else. I think
 * conceptually the top left button isn't owned by the switcher, so
 * I think it would make sense. */
static ClutterActor *
hd_switcher_top_left_button_new (const char *icon_name)
{
  ClutterActor	  *top_left_button;
  ClutterActor    *top_left_button_icon;
  ClutterActor    *top_left_button_highlight;
  ClutterGeometry  geom;
  GtkIconTheme	  *icon_theme;
  GError	  *error = NULL;

  icon_theme = gtk_icon_theme_get_default ();

  top_left_button = clutter_group_new ();
  clutter_actor_set_name (top_left_button, icon_name);

  top_left_button_highlight =
      clutter_texture_new_from_file (
        g_build_filename (HD_DATADIR, TOP_LEFT_BUTTON_HIGHLIGHT_TEXTURE, NULL),
        &error);
  if (error)
    {
      g_debug (error->message);
      g_error_free (error);
    }
  else
    {
      clutter_actor_set_size (top_left_button_highlight,
                              TOP_LEFT_BUTTON_WIDTH, TOP_LEFT_BUTTON_HEIGHT);
      clutter_container_add_actor (CLUTTER_CONTAINER (top_left_button),
                                   top_left_button_highlight);
    }

  top_left_button_icon =
    hd_gtk_icon_theme_load_icon (icon_theme, icon_name, 48, 0);
  clutter_actor_get_geometry (top_left_button_icon, &geom);
  clutter_actor_set_position (top_left_button_icon,
			      (TOP_LEFT_BUTTON_WIDTH/2)-(geom.width/2),
			      (TOP_LEFT_BUTTON_HEIGHT/2)-(geom.height/2));
  clutter_container_add_actor (CLUTTER_CONTAINER (top_left_button),
			       top_left_button_icon);



  return top_left_button;
}

static gboolean
hd_switcher_menu_clicked (HdSwitcher *switcher)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  HdHome	    *home =
    HD_HOME (hd_comp_mgr_get_home (HD_COMP_MGR (priv->comp_mgr)));

  g_debug("hd_switcher_menu_clicked, switcher=%p\n", switcher);

  if (priv->hildon_home_proxy)
    dbus_g_proxy_call_no_reply (priv->hildon_home_proxy, "ShowEditMenu",
                                G_TYPE_UINT, hd_home_get_current_view_id (home),
                                G_TYPE_INVALID);

  return TRUE;
}

static gboolean
hd_switcher_clicked (HdSwitcher *switcher)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  HdHome	    *home =
    HD_HOME (hd_comp_mgr_get_home (HD_COMP_MGR (priv->comp_mgr)));

  g_debug("entered hd_switcher_clicked: state=%d\n",
        hd_render_manager_get_state());

  /* Hide Home edit button */
  hd_home_hide_edit_button (home);

  /* Hide the status area */
  hd_switcher_hide_status_area (switcher);

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

      /* Only switch if we're not running any transition. */
      if (!priv->in_transition)
        hd_render_manager_set_state(HDRM_STATE_LAUNCHER);
    }
  else if (hd_render_manager_get_state() == HDRM_STATE_LAUNCHER)
    {
      g_debug("hd_switcher_clicked: show switcher, switcher=%p\n", switcher);
      hd_render_manager_set_state(HDRM_STATE_TASK_NAV);
            }
  else if (hd_task_navigator_is_empty(priv->task_nav))
    hd_render_manager_set_state(HDRM_STATE_LAUNCHER);
  else hd_render_manager_set_state(HDRM_STATE_TASK_NAV);

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

static void
hd_switcher_zoom_in_complete (ClutterActor *actor, HdSwitcher *switcher)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  MBWMCompMgrClient     *cclient;
  MBWindowManagerClient *c;
  MBWindowManager       *wm;
  HdCompMgrClient       *hclient;

  g_debug("hd_switcher_zoom_in_complete: switcher=%p actor=%p\n", switcher,
          actor);

  cclient =
    g_object_get_data (G_OBJECT (actor), "HD-MBWMCompMgrClutterClient");

  g_assert (cclient);

  c = cclient->wm_client;

  wm = c->wmref;
  hclient = HD_COMP_MGR_CLIENT (c->cm_client);

  priv->in_transition = FALSE;
  hd_render_manager_set_state(HDRM_STATE_APP);
  /* Stop any transition to app mode we may have had */
  hd_render_manager_stop_transition();

  if (!hd_comp_mgr_client_is_hibernating (hclient))
    {
      g_debug("hd_switcher_zoom_in_complete: calling "
              "mb_wm_activate_client c=%p\n", c);
      mb_wm_activate_client (wm, c);
    }
  else
    {
      g_debug("hd_switcher_zoom_in_complete: calling "
              "hd_comp_mgr_wakeup_client comp_mgr=%p hclient=%p\n",
              priv->comp_mgr, hclient);
      hd_comp_mgr_wakeup_client (HD_COMP_MGR (priv->comp_mgr), hclient);
    }
}

static void
hd_switcher_item_selected (HdSwitcher *switcher, ClutterActor *actor)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  priv->in_transition = TRUE;
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

void
hd_switcher_add_status_menu (HdSwitcher *switcher, ClutterActor *sa)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  priv->status_menu = sa;
}

void
hd_switcher_remove_status_menu (HdSwitcher *switcher, ClutterActor *sa)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  priv->status_menu = NULL;
}

void
hd_switcher_show_status_area (HdSwitcher *switcher)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  if (priv->status_area)
    clutter_actor_show (priv->status_area);
}

void
hd_switcher_hide_status_area (HdSwitcher *switcher)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  if (priv->status_area)
    clutter_actor_hide (priv->status_area);
}

void
hd_switcher_add_status_area (HdSwitcher *switcher, ClutterActor *sa)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  hd_render_manager_set_status_area(sa);
  priv->status_area = sa;
}

void
hd_switcher_remove_status_area (HdSwitcher *switcher, ClutterActor *sa)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  hd_render_manager_set_status_area(NULL);
  priv->status_area = NULL;
}

static gboolean
hd_switcher_notification_clicked (HdSwitcher *switcher, HdNote *note)
{
  Window xwin;
  Display *xdpy;
  XButtonEvent ev;

  /*
   * Deliver an #XButtonEvent to @win, so #HdIncomingEventWindow will know
   * there is a response and will invoke the appropriate action.
   * Note that %MBWindowManager->root_win->xwindow is different from what
   * DefaultRootWindow() returns, which may or may not be intentional.
   */
  xwin = MB_WM_CLIENT (note)->window->xwindow;
  xdpy = MB_WM_CLIENT (note)->wmref->xdpy;

  memset (&ev, 0, sizeof (ev));
  ev.type         = ButtonPress;
  ev.send_event   = True;
  ev.display      = xdpy;
  ev.window       = xwin;
  ev.root         = DefaultRootWindow (xdpy);
  ev.time         = CurrentTime;
  ev.button       = Button1;
  ev.same_screen  = True;

  XSendEvent(xdpy, xwin, False, ButtonPressMask, (XEvent *)&ev);
  XSync (xdpy, FALSE);

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
  if (hd_render_manager_get_state()==HDRM_STATE_TASK_NAV)
  if (HD_IS_NOTE (mbwmc) && HD_NOTE(mbwmc)->note_type == HdNoteTypeConfirmation)
      hd_switcher_item_selected (switcher, parent);
}

void
hd_switcher_add_dialog (HdSwitcher *switcher, MBWindowManagerClient *mbwmc,
                        ClutterActor *dialog)
{
  hd_switcher_add_dialog_explicit (switcher, mbwmc, dialog, mbwmc->transient_for);
}

/* Called when a window or a notification is removed from the switcher.
 * Exit the switcher if it's become empty and set up the switcher button
 * appropriately. */
static void
hd_switcher_something_removed (HdSwitcher * switcher)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  gboolean           have_children;

  have_children = !hd_task_navigator_is_empty(priv->task_nav);
  if (!have_children &&
      (hd_render_manager_get_state()==HDRM_STATE_TASK_NAV ||
       STATE_IS_APP(hd_render_manager_get_state())))
    {
      hd_render_manager_set_state(HDRM_STATE_HOME);
    }
}

void
hd_switcher_remove_notification (HdSwitcher * switcher, HdNote * note)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  hd_task_navigator_remove_notification (priv->task_nav, note);
  hd_switcher_something_removed (switcher);
}

void
hd_switcher_remove_dialog (HdSwitcher * switcher,
                           ClutterActor * dialog)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  hd_task_navigator_remove_dialog (priv->task_nav, dialog);
}

/* Called when #HdTaskNavigator has finished removing a thumbnail
 * from the navigator area. */
static void
hd_switcher_window_actor_removed (ClutterActor * unused, gpointer switcher)
{
  hd_switcher_something_removed (switcher);
}

void
hd_switcher_remove_window_actor (HdSwitcher * switcher, ClutterActor * actor)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  hd_task_navigator_remove_window (priv->task_nav, actor,
                                   hd_switcher_window_actor_removed,
                                   switcher);
  if (STATE_IS_APP(hd_render_manager_get_state()))
    {
      if (hd_task_navigator_is_empty(priv->task_nav))
        hd_render_manager_set_state(HDRM_STATE_HOME);
      else
        hd_render_manager_set_state(HDRM_STATE_TASK_NAV);
    }
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

ClutterActor *
hd_switcher_get_task_navigator (HdSwitcher *switcher)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  return CLUTTER_ACTOR(priv->task_nav);
}

/**
 * This function will get the height of the task switcher and the width of the
 * task switcher plus the width of the status area (if the status area is
 * present).
 */
void
hd_switcher_get_control_area_size (HdSwitcher *switcher,
				   guint *control_width,
				   guint *control_height)
{
  HdSwitcherPrivate *priv = switcher->priv;
  guint              button_width, button_height;
  guint              status_width = 0, status_height = 0;

  clutter_actor_get_size (
        hd_render_manager_get_button(HDRM_BUTTON_LAUNCHER),
			  &button_width, &button_height);
  /* FIXME */
  button_width = TOP_LEFT_BUTTON_WIDTH;

  if (priv->status_area)
    clutter_actor_get_size (priv->status_area,
			    &status_width, &status_height);

  if (control_width)
    *control_width = button_width + status_width;

  if (control_height)
    *control_height = button_height;
}

static void
hd_switcher_home_background_clicked (HdSwitcher   *switcher,
	   			         ClutterActor *actor)
{
  g_debug("hd_switcher_home_background_clicked: switcher=%p\n", switcher);
  hd_switcher_group_background_clicked (switcher, actor);
}

static void
hd_switcher_group_background_clicked (HdSwitcher   *switcher,
				      ClutterActor *actor)
{
  hd_render_manager_set_state(HDRM_STATE_HOME);
}

