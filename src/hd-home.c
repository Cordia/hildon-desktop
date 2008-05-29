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

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>

#include <matchbox/core/mb-wm.h>

#define HDH_MOVE_DURATION 300

#define CLOSE_BUTTON "close-button.png"
#define BACK_BUTTON  "back-button.png"

enum
{
  PROP_COMP_MGR = 1,
};

struct _HdHomePrivate
{
  MBWMCompMgrClutter    *comp_mgr;

  ClutterEffectTemplate *move_template;

  ClutterActor          *close_button;
  ClutterActor          *back_button;

  guint                  close_button_handler;

  GList                 *views;
  guint                  n_views;
  guint                  current_view;

  gint                   xwidth;
  gint                   xheight;

  HdHomeMode             mode;

  gboolean               pointer_grabbed : 1;
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
}

static gboolean
hd_home_back_button_clicked (ClutterActor *button,
			     ClutterEvent *event,
			     HdHome       *home)
{
  g_debug ("back button pressed.");

  return FALSE;
}

static void
hd_home_constructed (GObject *object)
{
  HdHomePrivate   *priv = HD_HOME (object)->priv;
  ClutterActor    *view;
  MBWindowManager *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;
  gint             i;
  GError          *error = NULL;
  guint            button_width, button_height;

  priv->xwidth  = wm->xdpy_width;
  priv->xheight = wm->xdpy_height;

  for (i = 0; i < 3; ++i)
    {
      ClutterColor clr;

      clr.alpha = 0xff;

      view = g_object_new (HD_TYPE_HOME_VIEW,
			   "comp-mgr", priv->comp_mgr,
			   NULL);

      priv->views = g_list_append (priv->views, view);

      clutter_actor_set_position (view, priv->xwidth * i, 0);
      clutter_container_add_actor (CLUTTER_CONTAINER (object), view);

      if (i == 0)
	{
	  clr.red   = 0xff;
	  clr.blue  = 0;
	  clr.green = 0;
	}
      else if (i == 1)
	{
	  clr.red   = 0;
	  clr.blue  = 0xff;
	  clr.green = 0;
	}
      else if (i == 2)
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
   * NB: we position the button in the hd_home_do_layout_layout() function; this
   * allows us to mess about with the layout in that one place only.
   */
  priv->close_button =
    clutter_texture_new_from_file (CLOSE_BUTTON, &error);

  clutter_container_add_actor (CLUTTER_CONTAINER (object), priv->close_button);
  clutter_actor_hide (priv->close_button);
  clutter_actor_set_reactive (priv->close_button, TRUE);

  priv->back_button =
    clutter_texture_new_from_file (BACK_BUTTON, &error);

  clutter_container_add_actor (CLUTTER_CONTAINER (object), priv->back_button);
  clutter_actor_hide (priv->back_button);
  clutter_actor_set_reactive (priv->back_button, TRUE);

  clutter_actor_get_size (priv->back_button, &button_width, &button_height);
  clutter_actor_set_position (priv->back_button,
			      priv->xwidth - button_width - 5, 5);

  g_signal_connect (priv->back_button, "button-release-event",
		    G_CALLBACK (hd_home_back_button_clicked),
		    object);

  hd_home_set_mode (HD_HOME (object), HD_HOME_MODE_LAYOUT);
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

  timeline = clutter_effect_move (priv->move_template,
				  CLUTTER_ACTOR (home),
				  - view_index * priv->xwidth, 0,
				  NULL, NULL);

  priv->current_view = view_index;

  clutter_timeline_start (timeline);
}

static void
hd_home_grab_pointer (HdHomePrivate * priv)
{
  if (!priv->pointer_grabbed)
    {
      ClutterActor  *stage = clutter_stage_get_default();
      Window         clutter_window;
      Display       *dpy = clutter_x11_get_default_display ();
      int            status;

      clutter_window = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));

      status = XGrabPointer (dpy,
			     clutter_window,
			     False,
			     ButtonPressMask | ButtonReleaseMask,
			     GrabModeAsync,
			     GrabModeAsync,
			     None,
			     None,
			     CurrentTime);

      if (!status)
	priv->pointer_grabbed = TRUE;
    }
}

static void
hd_home_ungrab_pointer (HdHomePrivate * priv)
{
  if (priv->pointer_grabbed)
    {
      Display * dpy = clutter_x11_get_default_display ();

      XUngrabPointer (dpy, CurrentTime);

      priv->pointer_grabbed = FALSE;
    }
}

static void
hd_home_do_normal_layout (HdHomePrivate *priv)
{
  GList *l = priv->views;
  gint   xwidth = priv->xwidth;
  gint   i = 0;

  clutter_actor_hide (priv->close_button);
  clutter_actor_hide (priv->back_button);

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

      ++i;
      l = l->next;
    }

  hd_home_ungrab_pointer (priv);
}

static gboolean
hd_home_close_button_clicked (ClutterActor *button,
			      ClutterEvent *event,
			      HdHome       *home)
{
  HdHomePrivate * priv = home->priv;

  hd_home_remove_view (home, priv->current_view);

  return FALSE;
}

static void
hd_home_do_layout_layout (HdHome * home)
{
  HdHomePrivate   *priv = home->priv;
  GList           *l = priv->views;
  gint             xwidth = priv->xwidth;
  gint             xheight = priv->xheight;
  gint             i = 0, n = 0;
  ClutterActor    *top = NULL;
  gint             x_top, y_top, w_top, h_top;
  gdouble          scale = 0.5;

  w_top = (gint)((gdouble)xwidth * scale);
  h_top = (gint)((gdouble)xheight * scale) - xheight/16;
  x_top = (xwidth - w_top)/ 2;
  y_top = (xheight - h_top)/ 2 - xheight / 8;

  while (l)
    {
      ClutterActor * view = l->data;

      if (i == priv->current_view)
	{
	  top = view;

	  clutter_actor_set_position (view, x_top, y_top);
	  clutter_actor_set_depth (view, 0);
	}
      else
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
	}

      clutter_actor_set_scale (view, scale, scale);

      ++i;
      l = l->next;
    }

  if (priv->n_views > 1)
    {
      guint button_width, button_height;

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

  clutter_actor_show (priv->back_button);
  clutter_actor_raise_top (priv->back_button);

  if (top)
    {
      clutter_actor_raise_top (top);
      clutter_actor_raise (priv->close_button, top);

      clutter_actor_set_depth (top, 0);
      clutter_actor_set_depth (priv->close_button, 0);
      clutter_actor_set_depth (priv->back_button, 0);

      priv->close_button_handler =
	g_signal_connect (priv->close_button, "button-release-event",
                          G_CALLBACK (hd_home_close_button_clicked),
			  home);
    }

  hd_home_grab_pointer (priv);
}

void
hd_home_set_mode (HdHome* home, HdHomeMode mode)
{
  HdHomePrivate   *priv = home->priv;

  priv->mode = mode;

  switch (mode)
    {
    case HD_HOME_MODE_NORMAL:
    default:
      hd_home_do_normal_layout (priv);
      break;

    case HD_HOME_MODE_LAYOUT:
      hd_home_do_layout_layout (home);
      break;

    case HD_HOME_MODE_EDIT:
      break;
    }
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
      if (view_index > 0)
	--priv->current_view;
      else
	priv->current_view = 0;
    }

  /* This automatically destroys the actor, since we do not hold any
   * extra references to it.
   */
  clutter_container_remove_actor (CLUTTER_CONTAINER (home), view);

  /*
   * Redo layout in the current mode
   */
  hd_home_set_mode (home, priv->mode);
}
