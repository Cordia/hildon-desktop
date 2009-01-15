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
#include "hd-home-view-container.h"
#include "hd-comp-mgr.h"
#include "hd-home.h"
#include "hd-util.h"
#include "hd-home-applet.h"
#include "hd-render-manager.h"

#include "hildon-desktop.h"

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>

#include <matchbox/core/mb-wm.h>

#include <glib/gstdio.h>
#include <gconf/gconf-client.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define BACKGROUND_COLOR {0, 0, 0, 0xff}

#define GCONF_BACKGROUND_KEY(i) g_strdup_printf ("/apps/osso/hildon-desktop/views/%u/bg-image", i + 1)

#define MAX_VIEWS 4

enum
{
  PROP_COMP_MGR = 1,
  PROP_HOME,
  PROP_BACKGROUND_IMAGE,
  PROP_BACKGROUND_MODE,
  PROP_ID,
  PROP_ACTIVE,
  PROP_CONTAINER
};

struct _HdHomeViewPrivate
{
  MBWMCompMgrClutter       *comp_mgr;
  HdHome                   *home;
  HdHomeViewContainer      *view_container;
  ClutterActor             *background_container;
  ClutterActor             *applets_container;

  ClutterActor             *background;
  gchar                    *background_image_file;
  gchar                    *processed_bg_image_file;

  gint                      xwidth;
  gint                      xheight;

  GList                    *applets; /* MBWMCompMgrClutterClient list */

  gint                      applet_motion_start_x;
  gint                      applet_motion_start_y;
  gint                      applet_motion_last_x;
  gint                      applet_motion_last_y;

  gboolean                  applet_motion_tap : 1;

  guint                     id;

  guint                     capture_cb;
  guint                     bg_image_notify;
  gboolean		    bg_image_skip_gconf;

  GThread                  *bg_image_thread;
  guint                     bg_image_set_source;
  gint                      bg_image_dest_width;
  gint                      bg_image_dest_height;
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

static void hd_home_view_refresh_bg (HdHomeView	 *self,
				     const gchar *image);

G_DEFINE_TYPE (HdHomeView, hd_home_view, CLUTTER_TYPE_GROUP);

static void
hd_home_view_allocate (ClutterActor          *actor,
                       const ClutterActorBox *box,
                       gboolean               absolute_origin_changed)
{
  HdHomeView        *view = HD_HOME_VIEW (actor);
  HdHomeViewPrivate *priv = view->priv;

  /* We've resized, refresh the background image to fit the new size */
  if ((CLUTTER_UNITS_TO_INT (box->x2 - box->x1) != priv->bg_image_dest_width) ||
      (CLUTTER_UNITS_TO_INT (box->y2 - box->y1) != priv->bg_image_dest_height))
    hd_home_view_refresh_bg (view,
                             priv->background_image_file);

  CLUTTER_ACTOR_CLASS (hd_home_view_parent_class)->
    allocate (actor, box, absolute_origin_changed);
}

static void
hd_home_view_class_init (HdHomeViewClass *klass)
{
  ClutterActorClass *actor_class  = CLUTTER_ACTOR_CLASS (klass);
  GObjectClass      *object_class = G_OBJECT_CLASS (klass);
  GParamSpec        *pspec;

  g_type_class_add_private (klass, sizeof (HdHomeViewPrivate));

  actor_class->allocate      = hd_home_view_allocate;

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
			    0, MAX_VIEWS - 1,
			    0,
			    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_property (object_class, PROP_ID, pspec);

  pspec = g_param_spec_string ("background-image",
			       "Background Image",
			       "Background Image",
			       NULL,
			       G_PARAM_READWRITE);

  g_object_class_install_property (object_class, PROP_BACKGROUND_IMAGE, pspec);


  g_object_class_install_property (object_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         "Active",
                                                         "View is active",
                                                         TRUE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_CONTAINER,
                                   g_param_spec_object ("view-container",
                                                        "View Container",
                                                        "Views are embedded in that container",
                                                        HD_TYPE_HOME_VIEW_CONTAINER,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static gboolean
hd_home_view_background_release (ClutterActor *self,
				 ClutterEvent *event,
				 HdHomeView   *view)
{
  HdHomeViewPrivate *priv = view->priv;
  GList         *l;

  g_debug ("Background release");


  /*
   * If the click started on an applet, we might have a motion callback
   * installed -- remove it.
   */
  l = priv->applets;
  while (l)
    {
      guint id;
      MBWMCompMgrClutterClient *cc = l->data;
      ClutterActor * applet = mb_wm_comp_mgr_clutter_client_get_actor (cc);

      id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (applet),
					       "HD-VIEW-motion-cb"));

      if (id)
	{
	  g_signal_handler_disconnect (applet, id);
	  g_object_set_data (G_OBJECT (applet), "HD-VIEW-motion-cb", NULL);
	}

      g_object_unref (applet);

      l = l->next;
    }

  if (hd_render_manager_get_state() != HDRM_STATE_HOME_EDIT)
    g_signal_emit_by_name (priv->home, "background-clicked", 0, event);

  return TRUE;
}

static void
hd_home_view_gconf_bgimage_notify (GConfClient *client,
				   guint        cnxn_id,
				   GConfEntry  *entry,
				   HdHomeView  *view)
{
  GConfValue   *value;

  value = gconf_entry_get_value (entry);
  if (value)
    {
      const gchar *image_string = gconf_value_get_string (value);
      if (image_string)
	{
	  HdHomeViewPrivate *priv = view->priv;
	  priv->bg_image_skip_gconf = TRUE;
	  g_object_set (view, "background-image", image_string, NULL);
	}
    }
}

static void
hd_home_view_constructed (GObject *object)
{
  ClutterColor              clr = BACKGROUND_COLOR;
  HdHomeView               *self = HD_HOME_VIEW (object);
  HdHomeViewPrivate        *priv = self->priv;
  MBWindowManager          *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;
  GConfClient              *default_client;
  gchar                    *gconf_path;

  priv->xwidth  = wm->xdpy_width;
  priv->xheight = wm->xdpy_height;

  priv->background_container = clutter_group_new ();
  clutter_actor_set_name (priv->background_container, "HdHomeView::background-container");
  clutter_actor_set_visibility_detect(priv->background_container, FALSE);
  clutter_actor_set_position (priv->background_container, 0, 0);
  clutter_actor_set_size (priv->background_container, priv->xwidth, priv->xheight);
  clutter_container_add_actor (CLUTTER_CONTAINER (object), priv->background_container);

  priv->applets_container = clutter_group_new ();
  clutter_actor_set_name (priv->applets_container, "HdHomeView::applets-container");
  clutter_actor_set_visibility_detect(priv->applets_container, FALSE);
  clutter_actor_set_position (priv->applets_container, 0, 0);
  clutter_actor_set_size (priv->applets_container, priv->xwidth, priv->xheight);
  clutter_container_add_actor (CLUTTER_CONTAINER (object), priv->applets_container);

  /* Raise applets container over background container */
  clutter_actor_raise (priv->applets_container, priv->background_container);

  /* By default the background is a black rectangle */
  priv->background = clutter_rectangle_new_with_color (&clr);
  clutter_actor_set_name (priv->background, "HdHomeView::background");
  clutter_actor_set_size (priv->background, priv->xwidth, priv->xheight);
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->background_container),
                               priv->background);

  clutter_actor_set_reactive (CLUTTER_ACTOR (object), TRUE);

  default_client = gconf_client_get_default ();

  /* Register gconf notification for background image */
  gconf_path = GCONF_BACKGROUND_KEY (priv->id);
  g_debug("hd_home_view_constructed: gconf path for bg image: %s\n", gconf_path);
  priv->bg_image_notify = gconf_client_notify_add (default_client,
                                                   gconf_path,
                                                   (GConfClientNotifyFunc) hd_home_view_gconf_bgimage_notify,
                                                   self,
                                                   NULL, NULL);
  gconf_client_notify (default_client, gconf_path);
  g_free (gconf_path);

  g_signal_connect (object, "button-release-event",
		    G_CALLBACK (hd_home_view_background_release),
		    object);
}

static void
hd_home_view_init (HdHomeView *self)
{
  self->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (self, HD_TYPE_HOME_VIEW, HdHomeViewPrivate);
  clutter_actor_set_name(CLUTTER_ACTOR(self), "HdHomeView");
}

static void
hd_home_view_dispose (GObject *object)
{
  HdHomeView         *self           = HD_HOME_VIEW (object);
  HdHomeViewPrivate  *priv	     = self->priv;
  GList              *l		     = priv->applets;
  GConfClient        *default_client = gconf_client_get_default ();

  /* Remove background image thread/source and delete processed image */
  hd_home_view_refresh_bg (self, NULL);

  /* Remove gconf notifications */
  if (priv->bg_image_notify)
    {
      gconf_client_notify_remove (default_client, priv->bg_image_notify);
      priv->bg_image_notify = 0;
    }

  /* Shutdown any applets associated with this view */
  while (l)
  {
    MBWMCompMgrClutterClient *cc = l->data;

    hd_comp_mgr_close_client (HD_COMP_MGR (priv->comp_mgr), cc);

    l = l->next;
  }

  g_list_free (priv->applets);
  priv->applets = NULL;

  g_free (priv->background_image_file);
  priv->background_image_file = NULL;

  G_OBJECT_CLASS (hd_home_view_parent_class)->dispose (object);
}

static void
hd_home_view_finalize (GObject *object)
{
  G_OBJECT_CLASS (hd_home_view_parent_class)->finalize (object);
}

static gchar *
get_bg_image_processed_name (HdHomeView *view, const gchar *filename)
{
  gchar		    *basename, *tmpname;
  ClutterActor      *actor = CLUTTER_ACTOR (view);

  basename = g_path_get_basename (filename);
  tmpname = g_strdup_printf ("%s/%dx%d-%s",
			     g_get_tmp_dir (),
                             clutter_actor_get_width (actor),
                             clutter_actor_get_height (actor),
			     basename);
  g_free (basename);
  g_debug ("%s: '%s'", __FUNCTION__, tmpname);

  return tmpname;
}

static gboolean
bg_image_set_idle_cb (gpointer data)
{
  HdHomeView	    *self  = HD_HOME_VIEW (data);
  HdHomeViewPrivate *priv  = self->priv;
  ClutterActor      *new_bg;
  ClutterColor       clr = BACKGROUND_COLOR;
  ClutterActor      *actor = CLUTTER_ACTOR (self);
  GError            *error = NULL;

  new_bg = clutter_texture_new_from_file (priv->processed_bg_image_file,
                                          &error);

  if (!new_bg)
    {
      g_warning ("Error loading background: %s", error->message);
      g_error_free (error);
      /* Add a black background */
      new_bg = clutter_rectangle_new_with_color (&clr);
      clutter_actor_set_size (new_bg, priv->xwidth, priv->xheight);
    }
  else
    {
      clutter_actor_set_position (new_bg,
                                  (clutter_actor_get_width (actor) -
                                   clutter_actor_get_width (new_bg))/2,
                                  (clutter_actor_get_height (actor) -
                                   clutter_actor_get_height (new_bg))/2);
    }

  clutter_actor_set_name (new_bg, "HdHomeView::background");

  /* Add new background to the background container */
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->background_container), new_bg);

  /* Raise the texture above the solid color */
  if (priv->background)
    clutter_actor_raise (new_bg, priv->background);

 /* Remove the old background (color or image) */
  if (priv->background)
    clutter_actor_destroy (priv->background);

  priv->background = new_bg;

  if (!priv->bg_image_skip_gconf)
    {
      gchar *gconf_path = GCONF_BACKGROUND_KEY (priv->id);
      gconf_client_set_string (gconf_client_get_default (),
			       gconf_path,
			       priv->background_image_file,
			       NULL);
      g_free (gconf_path);
    }
  else
    priv->bg_image_skip_gconf = FALSE;

  priv->bg_image_set_source = 0;

  return FALSE;
}

static gpointer
process_bg_image_thread (gpointer data)
{
  GError	    *error  = NULL;
  GdkPixbuf         *pixbuf = NULL;
  HdHomeView	    *self   = HD_HOME_VIEW (data);
  HdHomeViewPrivate *priv   = self->priv;

  pixbuf = gdk_pixbuf_new_from_file_at_scale (priv->background_image_file,
                                              priv->bg_image_dest_width,
                                              priv->bg_image_dest_height,
                                              TRUE,
                                              &error);
  if (!pixbuf)
    {
      g_warning ("Error loading background: %s", error->message);
      g_error_free (error);
      error = NULL;
    }

  if (pixbuf)
    {
      if (!g_file_test (priv->processed_bg_image_file, G_FILE_TEST_EXISTS))
        {
          g_debug ("%s: SAVING IMAGE %s", __FUNCTION__,
                   priv->processed_bg_image_file);
          if (!gdk_pixbuf_save (pixbuf, priv->processed_bg_image_file, "png",
                                &error, NULL))
            {
              g_warning ("Error saving background: %s", error->message);
              g_error_free (error);
            }
        }
      g_object_unref (pixbuf);
    }

  priv->bg_image_set_source = g_idle_add (bg_image_set_idle_cb, self);

  g_thread_exit (NULL);
  return NULL;
}

static void
hd_home_view_refresh_bg (HdHomeView  *self,
			 const gchar *image)
{
  HdHomeViewPrivate *priv = self->priv;

  if (g_strcmp0 (image, priv->background_image_file) == 0)
    {
      return;
    }

  /* Join with former thread */
  if (priv->bg_image_thread)
    {
      g_thread_join (priv->bg_image_thread);
      priv->bg_image_thread = NULL;
    }

  /* Remove image setting source */
  if (priv->bg_image_set_source)
    {
      g_source_remove (priv->bg_image_set_source);
      priv->bg_image_set_source = 0;
    }

  /* Delete the cached, processed background */
  if (priv->processed_bg_image_file)
    {
      g_remove (priv->processed_bg_image_file);
      g_free (priv->processed_bg_image_file);
      priv->processed_bg_image_file = NULL;
    }

  if (priv->background_image_file)
    {
      if (image != priv->background_image_file)
	g_free (priv->background_image_file);
    }


  if (!hd_disable_threads())
    priv->background_image_file = g_strdup (image);

  if (priv->background_image_file)
    {
      priv->processed_bg_image_file =
	get_bg_image_processed_name (self, priv->background_image_file);

      if (!g_file_test (priv->processed_bg_image_file, G_FILE_TEST_EXISTS))
	{
	  GError *error = NULL;

	  /* Start background processing thread */
	  priv->bg_image_dest_width =
	    clutter_actor_get_width (CLUTTER_ACTOR (self));
	  priv->bg_image_dest_height =
	    clutter_actor_get_height (CLUTTER_ACTOR (self));

	  priv->bg_image_thread = g_thread_create (process_bg_image_thread,
						   self,
						   TRUE,
						   &error);

	  if (!priv->bg_image_thread)
	    {
	      g_warning ("Error creating bg image thread: %s",
			 error->message);
	      g_error_free (error);
	    }
	}
      else
	{
	  /* Image already processed, load */
	  bg_image_set_idle_cb (self);
	}
    }
}

static void
hd_home_view_set_property (GObject       *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
  HdHomeView        *self = HD_HOME_VIEW (object);
  HdHomeViewPrivate *priv = self->priv;

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
    case PROP_BACKGROUND_IMAGE:
      hd_home_view_refresh_bg (self,
			       g_value_get_string (value));
      break;
    case PROP_CONTAINER:
      priv->view_container = g_value_get_object (value);
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
  HdHomeView *view = HD_HOME_VIEW (object);
  HdHomeViewPrivate *priv = view->priv;

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
    case PROP_BACKGROUND_IMAGE:
      g_value_set_string (value, priv->background_image_file);
      break;
    case PROP_ACTIVE:
      g_value_set_boolean (value, hd_home_view_container_get_active (priv->view_container,
                                                                     priv->id));
      break;
    case PROP_CONTAINER:
      g_value_set_object (value, priv->view_container);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
hd_home_view_set_background_image (HdHomeView *view, const gchar * path)
{
  g_object_set (G_OBJECT (view), "background-image", path, NULL);
}

guint
hd_home_view_get_view_id (HdHomeView *view)
{
  HdHomeViewPrivate *priv = view->priv;

  return priv->id;
}

static gboolean
hd_home_view_applet_motion (ClutterActor       *applet,
			    ClutterMotionEvent *event,
			    HdHomeView         *view)
{
  HdHomeViewPrivate *priv = view->priv;
  gint x, y;
  guint w, h;

  g_debug ("%s: %d, %d", __FUNCTION__, event->x, event->y);

  if (priv->applet_motion_tap)
    {
      if (ABS (priv->applet_motion_start_x - event->x) > 20 ||
          ABS (priv->applet_motion_start_y - event->y) > 20)
        priv->applet_motion_tap = FALSE;
      else
        return FALSE;
    }

  hd_home_show_switches (priv->home);
  hd_home_hide_applet_buttons (priv->home);

  x = event->x - priv->applet_motion_last_x;
  y = event->y - priv->applet_motion_last_y;

  priv->applet_motion_last_x = event->x;
  priv->applet_motion_last_y = event->y;

  /*
   * We only move the actor, not the applet per se, and only commit the motion
   * on button release.
   */
  clutter_actor_move_by (applet, x, y);
  hd_home_move_applet_buttons (priv->home, x, y);

  clutter_actor_get_position (applet, &x, &y);
  clutter_actor_get_size (applet, &w, &h);

  /*
   * If the applet entered the left/right switcher area, highlight
   * the switcher.
   */
  if (x < HDH_SWITCH_WIDTH)
    hd_home_highlight_switch (priv->home, TRUE);
  else if (x + w > priv->xwidth - HDH_SWITCH_WIDTH)
    hd_home_highlight_switch (priv->home, FALSE);
  else
    hd_home_unhighlight_switches (priv->home);


  /*
   * If the pointer entered the left/right switcher area, initiate pan.
   */
  /* FIXME: Move only the applet.
  if (event->x < HDH_SWITCH_WIDTH)
    hd_home_pan_and_move_applet (priv->home, TRUE, applet);
  else if (event->x > priv->xwidth - HDH_SWITCH_WIDTH)
    hd_home_pan_and_move_applet (priv->home, FALSE, applet);
    */

  return FALSE;
}

static gboolean
hd_home_view_applet_press (ClutterActor       *applet,
			   ClutterButtonEvent *event,
			   HdHomeView         *view)
{
  HdHomeViewPrivate *priv = view->priv;
  GConfClient *client = gconf_client_get_default ();
  gchar *modified_key, *modified;
  guint id;
  MBWMCompMgrClient *cclient;
  HdHomeApplet *wm_applet;
  MBWindowManagerClient *desktop_client;

  g_debug ("%s: %d, %d", __FUNCTION__, event->x, event->y);

  desktop_client = hd_comp_mgr_get_desktop_client (HD_COMP_MGR (priv->comp_mgr));
  cclient = g_object_get_data (G_OBJECT (applet), "HD-MBWMCompMgrClutterClient");
  wm_applet = (HdHomeApplet *) cclient->wm_client;

  /* Get all pointer events */
  clutter_grab_pointer (applet);

  id = g_signal_connect (applet, "motion-event",
			 G_CALLBACK (hd_home_view_applet_motion),
			 view);

  g_object_set_data (G_OBJECT (applet), "HD-VIEW-motion-cb",
		     GINT_TO_POINTER (id));

  /* Raise the applet */
  clutter_actor_raise_top (applet);

  /* Store the modifed time of the applet */
  time (&wm_applet->modified);

  modified = g_strdup_printf ("%ld", wm_applet->modified);
  modified_key = g_strdup_printf ("/apps/osso/hildon-desktop/applets/%s/modified", wm_applet->applet_id);

  gconf_client_set_string (client,
                           modified_key,
                           modified,
                           NULL);
  g_free (modified);
  g_object_unref (client);

  mb_wm_client_stacking_mark_dirty (desktop_client);

  priv->applet_motion_start_x = event->x;
  priv->applet_motion_start_y = event->y;
  priv->applet_motion_last_x = event->x;
  priv->applet_motion_last_y = event->y;
  priv->applet_motion_tap = TRUE;

  return FALSE;
}

static gboolean
hd_home_view_applet_release (ClutterActor       *applet,
			     ClutterButtonEvent *event,
			     HdHomeView         *view)
{
  HdHomeViewPrivate *priv = view->priv;
  guint id;

  g_debug ("%s: %d, %d", __FUNCTION__, event->x, event->y);

  /* Get all pointer events */
  clutter_ungrab_pointer ();

  id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (applet),
					   "HD-VIEW-motion-cb"));

  if (id)
    {
      g_signal_handler_disconnect (applet, id);
      g_object_set_data (G_OBJECT (applet), "HD-VIEW-motion-cb", NULL);
    }

  /*
   * If this was a simple press/release, with no intervening pointer motion,
   * emit the applet-clicked signal.
   */
  if (priv->applet_motion_tap)
    {
      if (hd_render_manager_get_state() == HDRM_STATE_HOME_EDIT)
        hd_home_show_applet_buttons (priv->home, applet);
    }
  else
    {
      /* Move the underlying window to match the actor's position */
      gint x, y;
      guint w, h;
      MBWindowManagerClient *client;
      MBWMCompMgrClient *cclient;
      MBGeometry geom;
      GConfClient *gconf_client;
      gchar *applet_id, *position_key;
      GSList *position_value;

      cclient =
	g_object_get_data (G_OBJECT (applet), "HD-MBWMCompMgrClutterClient");

      client = cclient->wm_client;

      clutter_actor_get_position (applet, &x, &y);
      clutter_actor_get_size (applet, &w, &h);

      geom.x = x;
      geom.y = y;
      geom.width = w;
      geom.height = h;

      mb_wm_client_request_geometry (client, &geom,
				     MBWMClientReqGeomIsViaUserAction);

      gconf_client = gconf_client_get_default ();
      applet_id = HD_HOME_APPLET (client)->applet_id;

      position_key = g_strdup_printf ("/apps/osso/hildon-desktop/applets/%s/position", applet_id);
      position_value = g_slist_prepend (g_slist_prepend (NULL, GINT_TO_POINTER (y)),
                                        GINT_TO_POINTER (x));
      gconf_client_set_list (gconf_client, position_key,
                             GCONF_VALUE_INT, position_value,
                             NULL);

      g_object_unref (gconf_client);
      g_free (position_key);
      g_slist_free (position_value);

      hd_home_hide_switches (priv->home);
    }

  return TRUE;
}

static gint
cmp_applet_modified (gconstpointer a,
                     gconstpointer b)
{
  const MBWMCompMgrClient *cc_a = a;
  const MBWMCompMgrClient *cc_b = b;

  return HD_HOME_APPLET (cc_a->wm_client)->modified - HD_HOME_APPLET (cc_b->wm_client)->modified;
}

static void
hd_home_view_restack_applets (HdHomeView *view)
{
  HdHomeViewPrivate *priv = view->priv;
  GList             *a;
  GSList            *sorted = NULL, *s;

  for (a = priv->applets; a; a = a->next)
    {
      MBWMCompMgrClient *cc = a->data;

      sorted = g_slist_insert_sorted (sorted, cc, cmp_applet_modified);
    }

  for (s = sorted; s; s = s->next)
    {
      MBWMCompMgrClutterClient *cc = s->data;
      ClutterActor *actor = mb_wm_comp_mgr_clutter_client_get_actor (cc);

      clutter_actor_raise_top (actor);
    }
}

void
hd_home_view_add_applet (HdHomeView *view, ClutterActor *applet)
{
  HdHomeViewPrivate *priv = view->priv;
  MBWMCompMgrClient *cc;
  guint              id;

  /*
   * Reparent the applet to ourselves; note that this automatically
   * gets us the correct position within the view.
   */
  clutter_actor_reparent (applet, priv->applets_container);
  clutter_actor_set_reactive (applet, TRUE);

  id = g_signal_connect (applet, "button-release-event",
			 G_CALLBACK (hd_home_view_applet_release),
			 view);

  g_object_set_data (G_OBJECT (applet), "HD-VIEW-release-cb",
		     GINT_TO_POINTER (id));

  id = g_signal_connect (applet, "button-press-event",
			 G_CALLBACK (hd_home_view_applet_press),
			 view);

  g_object_set_data (G_OBJECT (applet), "HD-VIEW-press-cb",
		     GINT_TO_POINTER (id));

  cc = g_object_get_data (G_OBJECT (applet), "HD-MBWMCompMgrClutterClient");

  priv->applets = g_list_prepend (priv->applets, cc);

  hd_home_view_restack_applets (view);
}

void
hd_home_view_remove_applet (HdHomeView *view, ClutterActor *applet)
{
  HdHomeViewPrivate        *priv = view->priv;
  MBWMCompMgrClutterClient *cc;
  guint                     id;


  cc = g_object_get_data (G_OBJECT (applet), "HD-MBWMCompMgrClutterClient");

  priv->applets = g_list_remove (priv->applets, cc);

  id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (applet),
					   "HD-VIEW-release-cb"));
  g_object_set_data (G_OBJECT (applet), "HD-VIEW-release-cb", NULL);
  g_signal_handler_disconnect (applet, id);

  id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (applet),
					   "HD-VIEW-press-cb"));
  g_object_set_data (G_OBJECT (applet), "HD-VIEW-press-cb", NULL);
  g_signal_handler_disconnect (applet, id);

  id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (applet),
					   "HD-VIEW-motion-cb"));
  if (id)
    {
      g_object_set_data (G_OBJECT (applet), "HD-VIEW-motion-cb", NULL);
      g_signal_handler_disconnect (applet, id);
    }
}

void
hd_home_view_move_applet (HdHomeView   *old_view,
			  HdHomeView   *new_view,
			  ClutterActor *applet)
{
  HdHomeViewPrivate *priv = new_view->priv;
  gint x, y;
/*   guint id; */

  hd_home_view_remove_applet (old_view, applet);

  /*
   * Position the applet on the new view, so that it would look like it was
   * dragged across the view boundary
   */
  clutter_actor_get_position (applet, &x, &y);

  if (x < 0)
    {
      /* Applet moved across left edge */
      x = priv->xwidth + x;
    }
  else
    {
      x = - (priv->xwidth - x);
    }

  clutter_actor_set_position (applet, x, y);

  /*
   * Add applet to the new view
   */
  hd_home_view_add_applet (new_view, applet);

#if 0
  /*
   * Now connect motion callback
   */
  id = g_signal_connect (applet, "motion-event",
			 G_CALLBACK (hd_home_view_applet_motion),
			 new_view);

  g_object_set_data (G_OBJECT (applet), "HD-VIEW-motion-cb",
		     GINT_TO_POINTER (id));

  priv->applet_motion_start_x = 0;
  priv->applet_motion_start_y = 0;
  priv->applet_motion_last_x = 0;
  priv->applet_motion_last_y = 0;
#endif
}

ClutterActor *
hd_home_view_get_background (HdHomeView *view)
{
  HdHomeViewPrivate *priv = view->priv;

  return priv->background;
}

gboolean
hd_home_view_get_active (HdHomeView *view)
{
  g_return_val_if_fail (HD_IS_HOME_VIEW (view), FALSE);
 
  return hd_home_view_container_get_active (view->priv->view_container,
                                            view->priv->id);
}

/*
static void
remove_applet_from_view (ClutterActor *applet,
                         HdHomeView   *view)
{
}

static void
scroll_next_completed_cb (ClutterTimeline *timeline,
                          HdHomeView      *view)
{
  HdHomeViewPrivate *priv = view->priv;

  clutter_actor_hide (CLUTTER_ACTOR (view));

  clutter_container_foreach (CLUTTER_CONTAINER (priv->applets_container),
                             CLUTTER_CALLBACK (remove_applet_from_view),
                             view);
 }
 */

