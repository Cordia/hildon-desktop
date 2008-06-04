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
#include "hd-home-view.h"
#include "hd-comp-mgr.h"
#include "hd-util.h"

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>

#include <matchbox/core/mb-wm.h>

#define HDH_MOVE_DURATION 300
#define HDH_ZOOM_DURATION 3000
#define HDH_LAYOUT_TOP_SCALE 0.5
#define HDH_LAYOUT_Y_OFFSET 60

#define CLOSE_BUTTON "close-button.png"
#define BACK_BUTTON  "back-button.png"
#define NEW_BUTTON   "new-view-button.png"

#define PAN_THRESHOLD 20
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

  ClutterEffectTemplate *move_template;
  ClutterEffectTemplate *zoom_template;

  ClutterActor          *main_group; /* Where the views + their buttons live */
  ClutterActor          *edit_group; /* An overlay group for edit mode */
  ClutterActor          *close_button;
  ClutterActor          *back_button;
  ClutterActor          *new_button;

  guint                  close_button_handler;

  ClutterActor          *grey_filter;

  GList                 *views;
  guint                  n_views;
  guint                  current_view;

  gint                   xwidth;
  gint                   xheight;

  HdHomeMode             mode;

  GList                 *pan_queue;

  gulong                 desktop_motion_cb;

  /* Pan variables */
  gint                   last_x;
  gint                   initial_x;
  gint                   initial_y;
  gint                   cumulative_x;

  gboolean               pointer_grabbed : 1;
  gboolean               pan_handled     : 1;

  Window                 desktop;
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

static void hd_home_remove_view (HdHome * home, guint view_index);

static void hd_home_new_view (HdHome * home);

static void hd_home_start_pan (HdHome *home);

static void hd_home_pan_full (HdHome *home, gboolean left);

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
hd_home_back_button_clicked (ClutterActor *button,
			     ClutterEvent *event,
			     HdHome       *home)
{
  g_debug ("back button pressed.");

  return TRUE;
}

static gboolean
hd_home_new_button_clicked (ClutterActor *button,
			    ClutterEvent *event,
			    HdHome       *home)
{
  hd_home_new_view (home);
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
  g_signal_emit (home, signals[SIGNAL_BACKGROUND_CLICKED], 0, event);
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

  g_debug ("Got desktop motion, by %d, cumulative %d.",
	   by_x, priv->cumulative_x);

  /*
   * When the motion gets over the pan threshold, we do a full pan
   * and disconnect the motion handler (next motion needs to be started
   * with another gesture).
   */
  if (priv->cumulative_x > 0 && priv->cumulative_x > PAN_THRESHOLD)
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
  else if (priv->cumulative_x < 0 && priv->cumulative_x < -PAN_THRESHOLD)
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

  g_debug ("Got desktop release (cb %d).", (int)priv->desktop_motion_cb);

  if (priv->desktop_motion_cb)
    mb_wm_main_context_x_event_handler_remove (wm->main_ctx,
					       MotionNotify,
					       priv->desktop_motion_cb);

  priv->desktop_motion_cb = 0;
  priv->cumulative_x = 0;

  return True;
}

static Bool
hd_home_desktop_press (XButtonEvent *xev, void *userdata)
{
  HdHome *home = userdata;
  HdHomePrivate *priv = home->priv;
  MBWindowManager *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;

  g_debug ("Got desktop press.");

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

  g_debug ("registered cb %d", (int)priv->desktop_motion_cb);

  return True;
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
  ClutterColor     clr;
  XSetWindowAttributes attr;

  priv->xwidth  = wm->xdpy_width;
  priv->xheight = wm->xdpy_height;

  main_group = priv->main_group = clutter_group_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (object), main_group);

  edit_group = priv->edit_group = clutter_group_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (object), edit_group);
  clutter_actor_hide (edit_group);

  for (i = 0; i < 3; ++i)
    {
      clr.alpha = 0xff;

      view = g_object_new (HD_TYPE_HOME_VIEW,
			   "comp-mgr", priv->comp_mgr,
			   "home",     object,
			   "id",       i,
			   NULL);

      g_signal_connect (view, "thumbnail-clicked",
		    G_CALLBACK (hd_home_view_thumbnail_clicked),
		    object);

      g_signal_connect (view, "background-clicked",
		    G_CALLBACK (hd_home_view_background_clicked),
		    object);

      priv->views = g_list_append (priv->views, view);

      clutter_actor_set_position (view, priv->xwidth * i, 0);
      clutter_container_add_actor (CLUTTER_CONTAINER (main_group), view);

      if (i % 4 == 0)
	{
	  clr.red   = 0xff;
	  clr.blue  = 0;
	  clr.green = 0;
	}
      else if (i % 4 == 1)
	{
	  clr.red   = 0;
	  clr.blue  = 0xff;
	  clr.green = 0;
	}
      else if (i % 4 == 2)
	{
	  clr.red   = 0;
	  clr.blue  = 0;
	  clr.green = 0xff;
	}
      else
	{
	  clr.red   = 0xff;
	  clr.blue  = 0xff;
	  clr.green = 0;
	}

      hd_home_view_set_background_color (HD_HOME_VIEW (view), &clr);
    }

  priv->n_views = i;

  /*
   * NB: we position the button in the hd_home_do_layout_layout() function;
   * this allows us to mess about with the layout in that one place only.
   */
  priv->close_button =
    clutter_texture_new_from_file (CLOSE_BUTTON, &error);

  clutter_container_add_actor (CLUTTER_CONTAINER (main_group),
			       priv->close_button);
  clutter_actor_hide (priv->close_button);
  clutter_actor_set_reactive (priv->close_button, TRUE);

  priv->back_button =
    clutter_texture_new_from_file (BACK_BUTTON, &error);

  clutter_container_add_actor (CLUTTER_CONTAINER (main_group),
			       priv->back_button);
  clutter_actor_hide (priv->back_button);
  clutter_actor_set_reactive (priv->back_button, TRUE);

  clutter_actor_get_size (priv->back_button, &button_width, &button_height);
  clutter_actor_set_position (priv->back_button,
			      priv->xwidth - button_width - 5, 5);

  g_signal_connect (priv->back_button, "button-release-event",
		    G_CALLBACK (hd_home_back_button_clicked),
		    object);

  priv->new_button =
    clutter_texture_new_from_file (NEW_BUTTON, &error);

  clutter_container_add_actor (CLUTTER_CONTAINER (main_group),
			       priv->new_button);
  clutter_actor_hide (priv->new_button);
  clutter_actor_set_reactive (priv->new_button, TRUE);

  g_signal_connect (priv->new_button, "button-release-event",
		    G_CALLBACK (hd_home_new_button_clicked),
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

  clutter_actor_set_size (priv->grey_filter, priv->xwidth, priv->xheight);
  clutter_container_add_actor (CLUTTER_CONTAINER (edit_group),
			       priv->grey_filter);

  hd_home_set_mode (HD_HOME (object), HD_HOME_MODE_LAYOUT);

  /*
   * Create an InputOnly desktop window; we have a custom desktop client that
   * that will automatically wrap it, ensuring it is located in the correct
   * place.
   */
  attr.override_redirect = True;
  attr.event_mask        = MBWMChildMask |
    ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask;

  priv->desktop = XCreateWindow (wm->xdpy,
				 wm->root_win->xwindow,
				 0, 0,
				 wm->xdpy_width,
				 wm->xdpy_height,
				 0,
				 CopyFromParent,
				 InputOnly,
				 CopyFromParent,
				 CWOverrideRedirect|CWEventMask,
				 &attr);

  XChangeProperty (wm->xdpy, priv->desktop,
		   wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE],
		   XA_ATOM, 32, PropModeReplace,
		   (unsigned char *)
		   &wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_DESKTOP],
		   1);

  XMapWindow (wm->xdpy, priv->desktop);

  g_debug ("Created desktop window %x", (unsigned int) priv->desktop);

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
}

static void
hd_home_init (HdHome *self)
{
  HdHomePrivate * priv;

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
						   HD_TYPE_HOME, HdHomePrivate);

  priv->move_template =
    clutter_effect_template_new_for_duration (HDH_MOVE_DURATION,
					      CLUTTER_ALPHA_RAMP_INC);

  priv->zoom_template =
    clutter_effect_template_new_for_duration (HDH_ZOOM_DURATION,
					      CLUTTER_ALPHA_RAMP_INC);
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
  ClutterTimeline *timeline;

  if (view_index >= priv->n_views)
    {
      g_warning ("View %d requested, but desktop has only %d views.",
		 view_index, priv->n_views);
      return;
    }

  priv->current_view = view_index;

  if (priv->mode == HD_HOME_MODE_NORMAL)
    {
      timeline = clutter_effect_move (priv->move_template,
				      CLUTTER_ACTOR (home),
				      - view_index * priv->xwidth, 0,
				      NULL, NULL);

      clutter_timeline_start (timeline);
    }
  else
    {
      hd_home_set_mode (home, HD_HOME_MODE_NORMAL);
    }
}

void
hd_home_grab_pointer (HdHome *home)
{
  HdHomePrivate *priv = home->priv;

  if (!priv->pointer_grabbed)
    {
      if (!hd_util_grab_pointer ())
	priv->pointer_grabbed = TRUE;
    }
}

void
hd_home_ungrab_pointer (HdHome *home)
{
  HdHomePrivate *priv = home->priv;

  if (priv->pointer_grabbed)
    {
      hd_util_ungrab_pointer ();
      priv->pointer_grabbed = FALSE;
    }
}

static void
hd_home_do_normal_layout (HdHome *home)
{
  HdHomePrivate *priv = home->priv;
  GList         *l = priv->views;
  gint           xwidth = priv->xwidth;
  gint           i = 0;

  clutter_actor_hide (priv->close_button);
  clutter_actor_hide (priv->back_button);
  clutter_actor_hide (priv->new_button);
  clutter_actor_hide (priv->edit_group);

  if (priv->close_button_handler)
    {
      g_signal_handler_disconnect (priv->close_button,
				   priv->close_button_handler);

      priv->close_button_handler = 0;
    }

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

  /*
   * Show the overlay edit_group and move it over the current view.
   */
  x = priv->xwidth * priv->current_view;

  clutter_actor_set_position (priv->edit_group, x, 0);
  clutter_actor_show (priv->edit_group);

  priv->mode = HD_HOME_MODE_EDIT;
}

static gboolean
hd_home_close_button_clicked (ClutterActor *button,
			      ClutterEvent *event,
			      HdHome       *home)
{
  HdHomePrivate * priv = home->priv;

  hd_home_remove_view (home, priv->current_view);

  return TRUE;
}

static void
hd_home_do_layout_contents (HdHomeView * top_view, HdHome * home)
{
  HdHomePrivate   *priv = home->priv;
  GList           *l = priv->views;
  gint             xwidth = priv->xwidth;
  gint             xheight = priv->xheight;
  gint             i = 0, n = 0;
  ClutterActor    *top;
  gint             x_top, y_top, w_top, h_top;
  gdouble          scale = HDH_LAYOUT_TOP_SCALE;
  guint            button_width, button_height;

  if (top_view)
    top = CLUTTER_ACTOR (top_view);
  else
    top = g_list_nth_data (priv->views, priv->current_view);

  w_top = (gint)((gdouble)xwidth * scale);
  h_top = (gint)((gdouble)xheight * scale) - xheight/16;
  x_top = (xwidth - w_top)/ 2;
  y_top = (xheight - h_top)/ 2 - HDH_LAYOUT_Y_OFFSET;

  clutter_actor_move_anchor_point_from_gravity (top,
						CLUTTER_GRAVITY_NORTH_WEST);

  clutter_actor_set_position (top, x_top, y_top);
  clutter_actor_set_depth (top, 0);
  clutter_actor_set_scale (top, scale, scale);
  hd_home_view_set_thumbnail_mode (HD_HOME_VIEW (top), TRUE);

  while (l)
    {
      ClutterActor * view = l->data;

      if (i != priv->current_view)
	{
	  if (!n)
	    {
	      clutter_actor_set_position (view, x_top - w_top, y_top);
	      clutter_actor_set_depth (view, -xwidth/2);
	    }
	  else
	    {
	      clutter_actor_set_position (view, x_top + w_top + (n-1)*w_top,
					  y_top);
	      clutter_actor_set_depth (view, - (n) * xwidth/2);
	    }

	  ++n;

	  clutter_actor_set_scale (view, scale, scale);
	  hd_home_view_set_thumbnail_mode (HD_HOME_VIEW (view), TRUE);
	}

      ++i;
      l = l->next;
    }

  if (priv->n_views > 1)
    {
      clutter_actor_get_size (priv->close_button,
			      &button_width, &button_height);

      clutter_actor_set_position (priv->close_button,
				  x_top + w_top - button_width / 4 -
				  button_width / 2,
				  y_top - button_height / 4);

      clutter_actor_show (priv->close_button);
    }
  else
    clutter_actor_hide (priv->close_button);

  clutter_actor_get_size (priv->new_button, &button_width, &button_height);
  clutter_actor_set_position (priv->new_button,
			      x_top + w_top / 2 - button_width / 2,
			      y_top + h_top + 2*HDH_LAYOUT_Y_OFFSET -
			      button_height);

  clutter_actor_show (priv->back_button);
  clutter_actor_raise_top (priv->back_button);

  clutter_actor_show (priv->new_button);
  clutter_actor_raise_top (priv->new_button);

  clutter_actor_raise_top (top);
  clutter_actor_raise (priv->close_button, top);

  clutter_actor_set_depth (top, 0);
  clutter_actor_set_depth (priv->close_button, 0);
  clutter_actor_set_depth (priv->back_button, 0);

  priv->close_button_handler =
    g_signal_connect (priv->close_button, "button-release-event",
		      G_CALLBACK (hd_home_close_button_clicked),
		      home);

  clutter_actor_hide (priv->grey_filter);

  hd_home_grab_pointer (home);
}

static void
hd_home_do_layout_layout (HdHome * home)
{
  HdHomePrivate   *priv = home->priv;
  ClutterTimeline *timeline;
  ClutterActor    *top;

  top = g_list_nth_data (priv->views, priv->current_view);

  g_assert (top);

  clutter_actor_move_anchor_point (top,
				   priv->xwidth / 2,
				   priv->xheight / 2 - HDH_LAYOUT_Y_OFFSET);

  timeline = clutter_effect_scale (priv->move_template,
				   top,
				   HDH_LAYOUT_TOP_SCALE, HDH_LAYOUT_TOP_SCALE,
				   (ClutterEffectCompleteFunc)
				   hd_home_do_layout_contents,
				   home);

  clutter_timeline_start (timeline);
}

void
hd_home_set_mode (HdHome *home, HdHomeMode mode)
{
  HdHomePrivate   *priv = home->priv;

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

  priv->mode = mode;
}

static void hd_home_new_view (HdHome * home)
{
  HdHomePrivate *priv = home->priv;
  ClutterActor  *view;
  ClutterColor   clr;
  gint           i = priv->n_views;

  clr.alpha = 0xff;

  if (i % 4 == 0)
    {
      clr.red   = 0xff;
      clr.blue  = 0;
      clr.green = 0;
    }
  else if (i % 4 == 1)
    {
      clr.red   = 0;
      clr.blue  = 0xff;
      clr.green = 0;
    }
  else if (i % 4 == 2)
    {
      clr.red   = 0;
      clr.blue  = 0;
      clr.green = 0xff;
    }
  else
    {
      clr.red   = 0xff;
      clr.blue  = 0xff;
      clr.green = 0;
    }

  view = g_object_new (HD_TYPE_HOME_VIEW,
		       "comp-mgr", priv->comp_mgr,
		       "home",     home,
		       "id",       i,
		       NULL);

  hd_home_view_set_background_color (HD_HOME_VIEW (view), &clr);

  g_signal_connect (view, "thumbnail-clicked",
		    G_CALLBACK (hd_home_view_thumbnail_clicked),
		    home);

  priv->views = g_list_append (priv->views, view);

  clutter_actor_set_position (view, priv->xwidth * priv->n_views, 0);
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->main_group), view);

  priv->current_view = priv->n_views;

  ++priv->n_views;

  hd_home_do_layout_contents (NULL, home);
}

static void
hd_home_remove_view (HdHome * home, guint view_index)
{
  HdHomePrivate * priv = home->priv;
  ClutterActor  * view;

  if (priv->n_views < 2)
    return;

  view = g_list_nth_data (priv->views, view_index);

  priv->views = g_list_remove (priv->views, view);
  --priv->n_views;

  if (view_index == priv->current_view)
    {
      if (view_index < priv->n_views-1)
	++priv->current_view;
      else
	priv->current_view = 0;
    }

  /*
   * Redo layout in the current mode
   *
   * We should really be LAYOUT mode, in which case, we only want to reorganize
   * the contents, not the scale effect.
   */
  if (priv->mode == HD_HOME_MODE_LAYOUT)
    hd_home_do_layout_contents (NULL, home);
  else
    hd_home_set_mode (home, priv->mode);

  /*
   * Only now remove the old actor; this way the new actor is in place before
   * the old one disappears and we avoid a temporary black void.
   *
   * This automatically destroys the actor, since we do not hold any
   * extra references to it.
   */
  clutter_container_remove_actor (CLUTTER_CONTAINER (priv->main_group), view);
}

static void
hd_home_pan_stage_completed (HdHome *home)
{
  HdHomePrivate *priv = home->priv;

  if (priv->pan_queue)
    hd_home_start_pan (home);
}

static void
hd_home_start_pan (HdHome *home)
{
  HdHomePrivate   *priv = home->priv;
  GList           *l = priv->pan_queue;
  gint             move_by;
  ClutterTimeline *timeline;

  move_by = clutter_actor_get_x (CLUTTER_ACTOR (home));

  while (l)
    {
      move_by += GPOINTER_TO_INT (l->data);
      l = l->next;
    }

  g_list_free (priv->pan_queue);
  priv->pan_queue = NULL;

  /*
   * TODO -- deal with view-rollover when we reach end of desktop
   */

  timeline = clutter_effect_move (priv->move_template,
				  CLUTTER_ACTOR (home),
				  move_by, 0,
				  (ClutterEffectCompleteFunc)
				  hd_home_pan_stage_completed, NULL);

  clutter_timeline_start (timeline);
}

static void
hd_home_pan_by (HdHome *home, gint move_by)
{
  HdHomePrivate   *priv = home->priv;
  gboolean         in_progress = FALSE;

  if (priv->mode != HD_HOME_MODE_NORMAL || !move_by)
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
    }

  hd_home_pan_by (home, by);
}

void
hd_home_add_applet (HdHome *home, ClutterActor *applet)
{
  HdHomePrivate *priv = home->priv;
  guint          view_id;
  GList         *l;

  view_id =
    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (applet), "HD-view-id"));

  l = priv->views;

  while (l)
    {
      HdHomeView * view = l->data;
      guint        id = hd_home_view_get_view_id (view);

      if (id == view_id)
	{
	  hd_home_view_add_applet (view, applet);
	  break;
	}

      l = l->next;
    }
}


void
hd_home_remove_applet (HdHome *home, ClutterActor *applet)
{
  HdHomePrivate *priv = home->priv;
  guint          view_id;
  GList         *l;

  view_id =
    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (applet), "HD-view-id"));

  l = priv->views;

  while (l)
    {
      HdHomeView * view = l->data;
      guint        id = hd_home_view_get_view_id (view);

      if (id == view_id)
	{
	  hd_home_view_remove_applet (view, applet);
	  break;
	}

      l = l->next;
    }
}
