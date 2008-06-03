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

#include "hd-home-view.h"
#include "hd-comp-mgr.h"
#include "hd-home.h"
#include "hd-util.h"

#include <clutter/clutter.h>

#include <matchbox/core/mb-wm.h>

enum
{
  SIGNAL_THUMBNAIL_CLICKED,
  SIGNAL_BACKGROUND_CLICKED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

enum
{
  PROP_COMP_MGR = 1,
  PROP_HOME,
  PROP_ID,
};

struct _HdHomeViewPrivate
{
  MBWMCompMgrClutter   *comp_mgr;
  HdHome               *home;

  ClutterActor         *background;
  ClutterActor         *mouse_trap;

  gint                  xwidth;
  gint                  xheight;

  GList                *applets; /* MBWMCompMgrClutterClient list */

  gboolean              thumbnail_mode   : 1;
  gboolean              active_input     : 1;
  gboolean              forwarding_event : 1;

  guint                 id;
};

static void hd_home_view_class_init (HdHomeViewClass *klass);
static void hd_home_view_init       (HdHomeView *self);
static void hd_home_view_dispose    (GObject *object);
static void hd_home_view_finalize   (GObject *object);

static void hd_home_view_set_property (GObject      *object,
				       guint         prop_id,
				       const GValue *value,
				       GParamSpec   *pspec);

static void hd_home_view_get_property (GObject      *object,
				       guint         prop_id,
				       GValue       *value,
				       GParamSpec   *pspec);

static void hd_home_view_constructed (GObject *object);


G_DEFINE_TYPE (HdHomeView, hd_home_view, CLUTTER_TYPE_GROUP);

static void
hd_home_view_class_init (HdHomeViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec   *pspec;

  g_type_class_add_private (klass, sizeof (HdHomeViewPrivate));

  object_class->dispose      = hd_home_view_dispose;
  object_class->finalize     = hd_home_view_finalize;
  object_class->set_property = hd_home_view_set_property;
  object_class->get_property = hd_home_view_get_property;
  object_class->constructed  = hd_home_view_constructed;

  pspec = g_param_spec_pointer ("comp-mgr",
				"Composite Manager",
				"MBWMCompMgrClutter Object",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class, PROP_COMP_MGR, pspec);

  pspec = g_param_spec_pointer ("home",
				"HdHome",
				"Parent HdHome object",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class, PROP_HOME, pspec);

  pspec = g_param_spec_int ("id",
			    "id",
			    "Numerical id for this view",
			    0, G_MAXINT,
			    0,
			    G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class, PROP_ID, pspec);

  signals[SIGNAL_THUMBNAIL_CLICKED] =
      g_signal_new ("thumbnail-clicked",
                    G_OBJECT_CLASS_TYPE (object_class),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (HdHomeViewClass, thumbnail_clicked),
                    NULL,
                    NULL,
                    g_cclosure_marshal_VOID__BOXED,
                    G_TYPE_NONE,
                    1,
		    CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  signals[SIGNAL_BACKGROUND_CLICKED] =
      g_signal_new ("background-clicked",
                    G_OBJECT_CLASS_TYPE (object_class),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (HdHomeViewClass, background_clicked),
                    NULL,
                    NULL,
                    g_cclosure_marshal_VOID__BOXED,
                    G_TYPE_NONE,
                    1,
		    CLUTTER_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

}

static gboolean
hd_home_view_background_clicked (ClutterActor *background,
				 ClutterEvent *event,
				 HdHomeView   *view)
{
  g_signal_emit (view, signals[SIGNAL_BACKGROUND_CLICKED], 0, event);
  return TRUE;
}

static gboolean
hd_home_view_mouse_trap_press (ClutterActor       *trap,
			       ClutterButtonEvent *event,
			       HdHomeView         *view)
{
  HdHomeViewPrivate * priv = view->priv;

  g_debug ("Mousetrap pressed, input mode %d", priv->active_input);

  /*
   * Make sure we clear the forwarding_event flag if set
   */
  priv->forwarding_event = FALSE;

  /*
   * If in active input mode, start tracking motion events
   */
  if (priv->active_input)
    {
      hd_home_connect_pan_handler (priv->home, trap, event->x, event->y);

      return TRUE;
    }

  return FALSE;
}

static gboolean
hd_home_view_mouse_trap_release (ClutterActor       *trap,
				 ClutterButtonEvent *event,
				 HdHomeView         *view)
{
  HdHomeViewPrivate * priv = view->priv;

  /*
   * We use a flag to avoid responding to a release event that we sometimes
   * get when we fake events.
   */
  if (priv->forwarding_event)
    {
      g_debug ("FWD release.");
      priv->forwarding_event = FALSE;

      /*
       * Must return false here to allow processing to proceed, otherwise
       * things get screwed up.
       */
      return FALSE;
    }

  g_debug ("Mousetrap released, input mode %d", priv->active_input);

  /*
   * If the active input is set, we resend this event, otherwise emit the
   * thumbnail-clicked signal.
   */
  if (!hd_home_disconnect_pan_handler (priv->home))
    {
      GList * l = priv->applets;

      while (l)
	{
	  MBWMCompMgrClutterClient *cc = l->data;
	  ClutterActor             *applet;
	  ClutterGeometry           geom;

	  applet = mb_wm_comp_mgr_clutter_client_get_actor (cc);

	  clutter_actor_get_geometry (applet, &geom);

	  if ((event->x >= geom.x) && (event->x <= geom.x + geom.width) &&
	      (event->y >= geom.y) && (event->y <= geom.y + geom.height))
	    {
	      MBWindowManagerClient *c  = MB_WM_COMP_MGR_CLIENT (cc)->wm_client;
	      Display               *xdpy = c->wmref->xdpy;

	      priv->forwarding_event = TRUE;

	      hd_home_ungrab_pointer (priv->home);
	      XSync (xdpy, False);

	      g_debug ("Faking button press & release.");
	      hd_util_fake_button_event (xdpy, event->button, True,
					 event->x, event->y);
	      hd_util_fake_button_event (xdpy, event->button, False,
					 event->x, event->y);
	      XSync (xdpy, False);

	      hd_home_grab_pointer (priv->home);
	      break;
	    }

	  l = l->next;
	}

      return TRUE;
    }

  if (!priv->active_input)
    g_signal_emit (view, signals[SIGNAL_THUMBNAIL_CLICKED], 0, event);

  return TRUE;
}

static void
hd_home_view_constructed (GObject *object)
{
  ClutterActor        *rect;
  ClutterColor         clr = {0xff, 0, 0, 0xff};
  HdHomeViewPrivate   *priv = HD_HOME_VIEW (object)->priv;
  MBWindowManager     *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;

  priv->xwidth  = wm->xdpy_width;
  priv->xheight = wm->xdpy_height;

  /*
   * TODO -- for now, just add a rectangle, so we can see
   * where the home view is. Later we will need to be able to use
   * a texture.
   */

  rect = clutter_rectangle_new_with_color (&clr);

  clutter_actor_set_size (rect, priv->xwidth, priv->xheight);
  clutter_actor_show (rect);
  clutter_actor_set_reactive (rect, TRUE);
  clutter_container_add_actor (CLUTTER_CONTAINER (object), rect);

  g_signal_connect (rect, "button-release-event",
		    G_CALLBACK (hd_home_view_background_clicked),
		    object);

  priv->background = rect;

  clr.alpha = 0;
  rect = clutter_rectangle_new_with_color (&clr);

  clutter_actor_set_size (rect, priv->xwidth, priv->xheight);
  clutter_actor_hide (rect);
  clutter_actor_set_reactive (rect, TRUE);

  clutter_container_add_actor (CLUTTER_CONTAINER (object), rect);

  g_signal_connect (rect, "button-release-event",
		    G_CALLBACK (hd_home_view_mouse_trap_release),
		    object);

  g_signal_connect (rect, "button-press-event",
		    G_CALLBACK (hd_home_view_mouse_trap_press),
		    object);

  priv->mouse_trap = rect;
}

static void
hd_home_view_init (HdHomeView *self)
{
  self->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (self, HD_TYPE_HOME_VIEW, HdHomeViewPrivate);
}

static void
hd_home_view_dispose (GObject *object)
{
  HdHomeViewPrivate  *priv = HD_HOME_VIEW (object)->priv;
  GList              *l    = priv->applets;

  /* Shutdown any applets associated with this view */
  while (l)
  {
    MBWMCompMgrClutterClient *cc = l->data;

    hd_comp_mgr_close_client (HD_COMP_MGR (priv->comp_mgr), cc);

    l = l->next;
  }

  g_list_free (priv->applets);

  priv->applets = NULL;

  G_OBJECT_CLASS (hd_home_view_parent_class)->dispose (object);
}

static void
hd_home_view_finalize (GObject *object)
{
  G_OBJECT_CLASS (hd_home_view_parent_class)->finalize (object);
}

static void
hd_home_view_set_property (GObject       *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
  HdHomeViewPrivate *priv = HD_HOME_VIEW (object)->priv;

  switch (prop_id)
    {
    case PROP_COMP_MGR:
      priv->comp_mgr = g_value_get_pointer (value);
      break;
    case PROP_HOME:
      priv->home = g_value_get_pointer (value);
      break;
    case PROP_ID:
      priv->id = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hd_home_view_get_property (GObject      *object,
			   guint         prop_id,
			   GValue       *value,
			   GParamSpec   *pspec)
{
  HdHomeViewPrivate *priv = HD_HOME_VIEW (object)->priv;

  switch (prop_id)
    {
    case PROP_COMP_MGR:
      g_value_set_pointer (value, priv->comp_mgr);
      break;
    case PROP_HOME:
      g_value_set_pointer (value, priv->home);
      break;
    case PROP_ID:
      g_value_set_int (value, priv->id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
hd_home_view_set_background_color (HdHomeView *view, ClutterColor *color)
{
  HdHomeViewPrivate *priv = view->priv;

  if (priv->background && CLUTTER_IS_RECTANGLE (priv->background))
    {
      clutter_rectangle_set_color (CLUTTER_RECTANGLE (priv->background), color);
    }
  else
    {
      ClutterActor * background;

      background = clutter_rectangle_new_with_color (color);
      clutter_actor_set_size (background, priv->xwidth, priv->xheight);
      clutter_container_add_actor (CLUTTER_CONTAINER (view), background);

      if (priv->background)
	clutter_actor_destroy (priv->background);

      priv->background = background;
    }
}

void
hd_home_view_set_background_image (HdHomeView *view, const gchar * path)
{
  HdHomeViewPrivate *priv = view->priv;
  ClutterActor      *background;
  GError            *error = NULL;

  background = clutter_texture_new_from_file (path, &error);

  if (error)
    {
      g_warning ("Could not load image %s: %s.",
		 path, error->message);

      g_error_free (error);
      return;
    }

  clutter_actor_set_size (background, priv->xwidth, priv->xheight);
  clutter_container_add_actor (CLUTTER_CONTAINER (view), background);

  if (priv->background)
    clutter_actor_destroy (priv->background);

  priv->background = background;
}

/*
 * The thumbnail mode is one in which the view acts as a single actor in
 * which button events are intercepted globally.
 */
void
hd_home_view_set_thumbnail_mode (HdHomeView * view, gboolean on)
{
  HdHomeViewPrivate *priv = view->priv;

  if (priv->thumbnail_mode && !on)
    {
      priv->thumbnail_mode = FALSE;

      clutter_actor_hide (priv->mouse_trap);
    }
  else if (!priv->thumbnail_mode && on)
    {
      priv->thumbnail_mode = TRUE;

      clutter_actor_show (priv->mouse_trap);
      clutter_actor_raise_top (priv->mouse_trap);
    }
}

/*
 * Active input mode is similar to the thumbnail mode (i.e., all events are
 * intercepted globally, but in addition, events get forwarded to
 * children if appropriate.
 */
void
hd_home_view_set_input_mode (HdHomeView * view, gboolean active)
{
  HdHomeViewPrivate *priv = view->priv;

  priv->active_input = active;
  hd_home_view_set_thumbnail_mode (view, active);
}

guint
hd_home_view_get_view_id (HdHomeView *view)
{
  HdHomeViewPrivate *priv = view->priv;

  return priv->id;
}

void
hd_home_view_add_applet (HdHomeView *view, ClutterActor *applet)
{
  HdHomeViewPrivate        *priv = view->priv;
  MBWMCompMgrClutterClient *cc;

  /*
   * Reparent the applet to ourselves; note that this automatically
   * gets us the correct position within the view.
   */
  clutter_actor_reparent (applet, CLUTTER_ACTOR (view));

  cc = g_object_get_data (G_OBJECT (applet), "HD-MBWMCompMgrClutterClient");

  priv->applets = g_list_prepend (priv->applets, cc);
}

