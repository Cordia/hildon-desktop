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
#include <clutter/x11/clutter-x11.h>

#include <matchbox/core/mb-wm.h>

#include <glib/gstdio.h>
#include <gconf/gconf-client.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

enum
{
  SIGNAL_THUMBNAIL_CLICKED,
  SIGNAL_BACKGROUND_CLICKED,
  SIGNAL_APPLET_CLICKED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

enum
{
  PROP_COMP_MGR = 1,
  PROP_HOME,
  PROP_BACKGROUND_COLOR,
  PROP_BACKGROUND_IMAGE,
  PROP_BACKGROUND_MODE,
  PROP_ID,
};

struct _HdHomeViewPrivate
{
  MBWMCompMgrClutter       *comp_mgr;
  HdHome                   *home;

  ClutterActor             *background;
  ClutterActor             *background_image;
  gchar                    *background_image_file;
  gchar                    *processed_bg_image_file;
  HdHomeViewBackgroundMode  background_mode;

  gint                      xwidth;
  gint                      xheight;

  GList                    *applets; /* MBWMCompMgrClutterClient list */

  gint                      applet_motion_start_x;
  gint                      applet_motion_start_y;
  gint                      applet_motion_last_x;
  gint                      applet_motion_last_y;

  gboolean                  thumbnail_mode        : 1;
  gboolean                  applet_motion_handled : 1;

  guint                     id;

  guint                     capture_cb;
  
  guint                     bg_color_notify;
  guint                     bg_image_notify;
  guint                     bg_style_notify;
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

static void hd_home_view_refresh_bg (HdHomeView		      *self,
				     gchar		      *image,
				     HdHomeViewBackgroundMode  mode);

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
                             priv->background_image_file,
                             priv->background_mode);
  
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
			    0, G_MAXINT,
			    0,
			    G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class, PROP_ID, pspec);

  pspec = g_param_spec_boxed ("background-color",
			      "Background Color",
			      "Background Color",
			      CLUTTER_TYPE_COLOR,
			      G_PARAM_READWRITE);

  g_object_class_install_property (object_class, PROP_BACKGROUND_COLOR, pspec);

  pspec = g_param_spec_string ("background-image",
			       "Background Image",
			       "Background Image",
			       NULL,
			       G_PARAM_READWRITE);

  g_object_class_install_property (object_class, PROP_BACKGROUND_IMAGE, pspec);

  pspec = g_param_spec_enum ("background-mode",
			     "Background mode",
			     "Background mode",
			     hd_home_view_background_mode_get_type (),
			     HDHV_BACKGROUND_STRETCHED,
			     G_PARAM_READWRITE);

  g_object_class_install_property (object_class, PROP_BACKGROUND_MODE, pspec);

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

  signals[SIGNAL_APPLET_CLICKED] =
      g_signal_new ("applet-clicked",
                    G_OBJECT_CLASS_TYPE (object_class),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (HdHomeViewClass, applet_clicked),
                    NULL,
                    NULL,
                    g_cclosure_marshal_VOID__OBJECT,
                    G_TYPE_NONE,
                    1,
		    CLUTTER_TYPE_ACTOR | G_SIGNAL_TYPE_STATIC_SCOPE);

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

  g_signal_emit (view, signals[SIGNAL_BACKGROUND_CLICKED], 0, event);
  return TRUE;
}

static gboolean
hd_home_view_captured_event (ClutterActor       *self,
			     ClutterEvent       *event,
			     HdHomeView         *view)
{
  HdHomeViewPrivate *priv = view->priv;

  /*
   * In thumbnail mode, we swallow all button presses and releases, and
   * emit thumbnail-clicked on release.
   */
  if (!priv->thumbnail_mode ||
      (event->type != CLUTTER_BUTTON_PRESS &&
       event->type != CLUTTER_BUTTON_RELEASE))
    {
      return FALSE;
    }

  if (event->type == CLUTTER_BUTTON_RELEASE)
    g_signal_emit (view, signals[SIGNAL_THUMBNAIL_CLICKED], 0, event);

  /* Swallow it */
  return TRUE;
}

static gchar *
hd_home_view_get_gconf_path (HdHomeView *view, const gchar *key)
{
  HdHomeViewPrivate *priv = view->priv;
  return g_strdup_printf (HDH_GCONF_PREFIX "/view_%d%s", priv->id, key);
}

static void
hd_home_view_gconf_bgcolor_notify (GConfClient *client,
				   guint        cnxn_id,
				   GConfEntry  *entry,
				   HdHomeView  *view)
{
  GConfValue   *value;
  
  value = gconf_entry_get_value (entry);
  if (value)
    {
      const gchar *color_string = gconf_value_get_string (value);
      if (color_string)
	{
	  HdHomeViewPrivate *priv = view->priv;
	  ClutterColor       color;
	  
	  clutter_color_parse (color_string, &color);
	  clutter_rectangle_set_color (CLUTTER_RECTANGLE (priv->background),
				       &color);
	}
    }
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
hd_home_view_gconf_bgstyle_notify (GConfClient *client,
				   guint        cnxn_id,
				   GConfEntry  *entry,
				   HdHomeView  *view)
{
  GConfValue   *value;
  
  value = gconf_entry_get_value (entry);
  if (value)
    {
      HdHomeViewPrivate *priv = view->priv;
      
      hd_home_view_refresh_bg (view,
			       priv->background_image_file,
			       gconf_value_get_int (value));
    }
}

static void
hd_home_view_constructed (GObject *object)
{
  ClutterActor             *rect;
  ClutterColor              clr = {0xff, 0, 0, 0xff};
  HdHomeView               *self = HD_HOME_VIEW (object);
  HdHomeViewPrivate        *priv = self->priv;
  MBWindowManager          *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;
  GConfClient              *default_client;
  gchar                    *gconf_path;

  priv->xwidth  = wm->xdpy_width;
  priv->xheight = wm->xdpy_height;

  /*
   * TODO -- for now, just add a rectangle, so we can see
   * where the home view is. Later we will need to be able to use
   * a texture.
   */

  /* Some default colours when gconf values aren't set */
  if (priv->id % 4 == 0)
    {
      clr.red   = 0xff;
      clr.blue  = 0;
      clr.green = 0;
    }
  else if (priv->id % 4 == 1)
    {
      clr.red   = 0;
      clr.blue  = 0xff;
      clr.green = 0;
    }
  else if (priv->id % 4 == 2)
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
  rect = clutter_rectangle_new_with_color (&clr);

  clutter_actor_set_size (rect, priv->xwidth, priv->xheight);
  clutter_container_add_actor (CLUTTER_CONTAINER (object), rect);

  priv->background = rect;

  clutter_actor_set_reactive (CLUTTER_ACTOR (object), TRUE);

  default_client = gconf_client_get_default ();

  /* Register gconf notification for background colour */
  gconf_path = hd_home_view_get_gconf_path (self, HDHV_GCONF_BG_COLOR);
  priv->bg_color_notify =
    gconf_client_notify_add (default_client,
			     gconf_path,
			     (GConfClientNotifyFunc)
			     hd_home_view_gconf_bgcolor_notify,
			     self,
			     NULL, NULL);
  gconf_client_notify (default_client, gconf_path);
  g_free (gconf_path);
  
  /* Register gconf notification for background image */
  gconf_path = hd_home_view_get_gconf_path (self, HDHV_GCONF_BG_IMAGE);
  priv->bg_image_notify =
    gconf_client_notify_add (default_client,
			     gconf_path,
			     (GConfClientNotifyFunc)
			     hd_home_view_gconf_bgimage_notify,
			     self,
			     NULL, NULL);
  gconf_client_notify (default_client, gconf_path);
  g_free (gconf_path);

  /* Register gconf notification for background display style */
  gconf_path = hd_home_view_get_gconf_path (self, HDHV_GCONF_BG_STYLE);
  priv->bg_style_notify =
    gconf_client_notify_add (default_client,
			     gconf_path,
			     (GConfClientNotifyFunc)
			     hd_home_view_gconf_bgstyle_notify,
			     self,
			     NULL, NULL);
  gconf_client_notify (default_client, gconf_path);
  g_free (gconf_path);

  g_signal_connect (object, "captured-event",
		    G_CALLBACK (hd_home_view_captured_event),
		    object);

  g_signal_connect (object, "button-release-event",
		    G_CALLBACK (hd_home_view_background_release),
		    object);
}

static void
hd_home_view_init (HdHomeView *self)
{
  self->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (self, HD_TYPE_HOME_VIEW, HdHomeViewPrivate);
  self->priv->background_mode = HDHV_BACKGROUND_STRETCHED;
}

static void
hd_home_view_dispose (GObject *object)
{
  HdHomeView         *self           = HD_HOME_VIEW (object);
  HdHomeViewPrivate  *priv	     = self->priv;
  GList              *l		     = priv->applets;
  GConfClient        *default_client = gconf_client_get_default ();

  /* Remove background image thread/source and delete processed image */
  hd_home_view_refresh_bg (self, NULL, priv->background_mode);
  
  /* Remove gconf notifications */
  if (priv->bg_color_notify)
    {
      gconf_client_notify_remove (default_client, priv->bg_color_notify);
      priv->bg_color_notify = 0;
    }
  if (priv->bg_image_notify)
    {
      gconf_client_notify_remove (default_client, priv->bg_image_notify);
      priv->bg_image_notify = 0;
    }
  if (priv->bg_style_notify)
    {
      gconf_client_notify_remove (default_client, priv->bg_style_notify);
      priv->bg_style_notify = 0;
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
  HdHomeViewPrivate *priv  = view->priv;
  
  basename = g_path_get_basename (filename);
  tmpname = g_strdup_printf ("%s/%d-%dx%d-%s.png",
			     g_get_tmp_dir (),
			     priv->background_mode,
                             clutter_actor_get_width (actor),
                             clutter_actor_get_height (actor),
			     basename);
  g_free (basename);
  
  return tmpname;
}

static gboolean
bg_image_set_idle_cb (gpointer data)
{
  ClutterActor      *new_bg;
  
  HdHomeView	    *self  = HD_HOME_VIEW (data);
  HdHomeViewPrivate *priv  = self->priv;
  ClutterActor      *actor = CLUTTER_ACTOR (self);
  GError            *error = NULL;
  
  new_bg = clutter_texture_new_from_file (priv->processed_bg_image_file,
                                          &error);
  
  if (!new_bg)
    {
      g_warning ("Error loading background: %s", error->message);
      g_error_free (error);
      return FALSE;
    }

  if (priv->background_mode == HDHV_BACKGROUND_TILED)
    {
      g_object_set (new_bg, "repeat-x", TRUE, "repeat-y", TRUE, NULL);
      clutter_actor_set_size (new_bg,
			      clutter_actor_get_width (actor),
			      clutter_actor_get_height (actor));
    }
  else
    {
      clutter_actor_set_position (new_bg,
				  (clutter_actor_get_width (actor) -
				   clutter_actor_get_width (new_bg))/2,
				  (clutter_actor_get_height (actor) -
				   clutter_actor_get_height (new_bg))/2);
    }
  clutter_container_add_actor (CLUTTER_CONTAINER (self), new_bg);
  
  if (priv->background_image)
    clutter_actor_destroy (priv->background_image);
  priv->background_image = new_bg;

  if (priv->background)
    clutter_actor_raise (priv->background_image, priv->background);
  
  if (!priv->bg_image_skip_gconf)
    {
      gchar *gconf_path =
	hd_home_view_get_gconf_path (self, HDHV_GCONF_BG_IMAGE);
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
  gint               width, height;
  GError	    *error  = NULL;
  GdkPixbuf         *pixbuf = NULL;
  HdHomeView	    *self   = HD_HOME_VIEW (data);
  HdHomeViewPrivate *priv   = self->priv;

  switch (priv->background_mode)
    {
    default :
    case HDHV_BACKGROUND_STRETCHED:
      pixbuf = gdk_pixbuf_new_from_file_at_scale (priv->background_image_file,
						  priv->bg_image_dest_width,
						  priv->bg_image_dest_height,
						  FALSE,
						  &error);
      if (!pixbuf)
	{
	  g_warning ("Error loading background: %s", error->message);
	  g_error_free (error);
	  error = NULL;
	}
      break;
    case HDHV_BACKGROUND_CENTERED:
      pixbuf = gdk_pixbuf_new_from_file (priv->background_image_file, &error);
      if (!pixbuf)
	{
	  g_warning ("Error loading background: %s", error->message);
	  g_error_free (error);
	  error = NULL;
	  break;
	}
      width = gdk_pixbuf_get_width (pixbuf);
      height = gdk_pixbuf_get_height (pixbuf);
      if ((width > priv->bg_image_dest_width) ||
	  (height > priv->bg_image_dest_height))
	{
	  gint x, y;
	  GdkPixbuf *new_pixbuf;
	  
	  /* Crop/centre */
	  x = MAX (0, (width - priv->bg_image_dest_width)/2);
	  y = MAX (0, (height - priv->bg_image_dest_height)/2);
	  width = MIN (width, priv->bg_image_dest_width);
	  height = MIN (height, priv->bg_image_dest_height);
	  
	  new_pixbuf = gdk_pixbuf_new_subpixbuf (pixbuf, x, y, width, height);
	  g_object_unref (pixbuf);
	  pixbuf = new_pixbuf;
	}
      break;
    case HDHV_BACKGROUND_SCALED:
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
      break;
    case HDHV_BACKGROUND_TILED:
      pixbuf = gdk_pixbuf_new_from_file (priv->background_image_file, &error);
      if (!pixbuf)
	{
	  g_warning ("Error loading background: %s", error->message);
	  g_error_free (error);
	  error = NULL;
	  break;
	}
      width = gdk_pixbuf_get_width (pixbuf);
      height = gdk_pixbuf_get_height (pixbuf);
      if ((width > priv->bg_image_dest_width) ||
	  (height > priv->bg_image_dest_height))
	{
	  GdkPixbuf *new_pixbuf;
	  
	  /* Crop */
	  width = MIN (width, priv->bg_image_dest_width);
	  height = MIN (height, priv->bg_image_dest_height);
	  
	  new_pixbuf = gdk_pixbuf_new_subpixbuf (pixbuf, 0, 0, width, height);
	  g_object_unref (pixbuf);
	  pixbuf = new_pixbuf;
	}
      break;
    case HDHV_BACKGROUND_CROPPED:
      pixbuf = gdk_pixbuf_new_from_file (priv->background_image_file, &error);
      if (!pixbuf)
	{
	  g_warning ("Error loading background: %s", error->message);
	  g_error_free (error);
	  error = NULL;
	  break;
	}
      width = gdk_pixbuf_get_width (pixbuf);
      height = gdk_pixbuf_get_height (pixbuf);
      if ((width > priv->bg_image_dest_width) ||
	  (height > priv->bg_image_dest_height))
	{
	  GdkPixbuf *new_pixbuf;
	  
	  if ((width - priv->bg_image_dest_width) >
	      (height - priv->bg_image_dest_height))
	    {
	      height = height * priv->bg_image_dest_width / (gdouble)width;
	      width = priv->bg_image_dest_width;
	    }
	  else
	    {
	      width = width * priv->bg_image_dest_height / (gdouble)height;
	      height = priv->bg_image_dest_height;
	    }
	  
	  new_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
						width,
						height,
						GDK_INTERP_HYPER);
	  g_object_unref (pixbuf);
	  pixbuf = new_pixbuf;
	}
      break;
    }
  
  if (pixbuf)
    {
      if (!gdk_pixbuf_save (pixbuf, priv->processed_bg_image_file, "png",
                            &error, NULL))
	{
	  g_warning ("Error saving background: %s", error->message);
	  g_error_free (error);
	}
      g_object_unref (pixbuf);
    }
  
  priv->bg_image_set_source = g_idle_add (bg_image_set_idle_cb, self);
  
  g_thread_exit (NULL);
  return NULL;
}

static void
hd_home_view_refresh_bg (HdHomeView		  *self,
			 gchar			  *image,
			 HdHomeViewBackgroundMode  mode)
{
  HdHomeViewPrivate *priv = self->priv;

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
  
  priv->background_image_file = image;
  priv->background_mode = mode;
  
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
    case PROP_BACKGROUND_COLOR:
      hd_home_view_set_background_color (HD_HOME_VIEW (object),
					 g_value_get_boxed (value));
      break;
    case PROP_BACKGROUND_IMAGE:
      hd_home_view_refresh_bg (self,
			       g_value_dup_string (value),
			       priv->background_mode);
      break;
    case PROP_BACKGROUND_MODE:
      {
	gchar *gconf_path;

	hd_home_view_refresh_bg (self,
				 priv->background_image_file,
				 g_value_get_enum (value));

	gconf_path = hd_home_view_get_gconf_path (self, HDHV_GCONF_BG_STYLE);
	gconf_client_set_int (gconf_client_get_default (),
			      gconf_path,
			      priv->background_mode,
			      NULL);
	g_free (gconf_path);
      }
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
    case PROP_BACKGROUND_COLOR:
      {
	ClutterColor color;
	
	clutter_rectangle_get_color (CLUTTER_RECTANGLE (priv->background),
				     &color);
	g_value_set_boxed (value, &color);
      }
      break;
    case PROP_BACKGROUND_IMAGE:
      g_value_set_string (value, priv->background_image_file);
      break;
    case PROP_BACKGROUND_MODE:
      g_value_set_enum (value, priv->background_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
hd_home_view_set_background_color (HdHomeView *view, ClutterColor *color)
{
  gchar             *gconf_path, *color_string;
  HdHomeViewPrivate *priv = view->priv;

  clutter_rectangle_set_color (CLUTTER_RECTANGLE (priv->background), color);

  gconf_path = hd_home_view_get_gconf_path (view, HDHV_GCONF_BG_COLOR);
  color_string = clutter_color_to_string (color);
  gconf_client_set_string (gconf_client_get_default (),
			   gconf_path,
			   color_string,
			   NULL);
  g_free (gconf_path);
  g_free (color_string);
}

void
hd_home_view_set_background_image (HdHomeView *view, const gchar * path)
{
  g_object_set (G_OBJECT (view), "background-image", path, NULL);
}

void
hd_home_view_set_background_mode (HdHomeView               *view,
				  HdHomeViewBackgroundMode  mode)
{
  g_object_set (G_OBJECT (view), "background-mode", mode, NULL);
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

      if (priv->capture_cb)
	{
	  g_signal_handler_disconnect (view, priv->capture_cb);
	  priv->capture_cb = 0;
	}
    }
  else if (!priv->thumbnail_mode && on)
    {
      priv->thumbnail_mode = TRUE;

      if (priv->capture_cb)
	{
	  g_warning ("Capture handler already connected.");

	  g_signal_handler_disconnect (view, priv->capture_cb);
	  priv->capture_cb = 0;
	}

      priv->capture_cb =
	g_signal_connect (view, "captured-event",
			  G_CALLBACK (hd_home_view_captured_event),
			  view);
    }
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

  g_debug ("Applet motion, %d,%d", event->x, event->y);

  /*
   * There is always a motion event just before a button release, so
   * we set a flag to indicated if the pointer actually moved.
   * NB: cannot just use the coords directly, as the final coords could
   * be the same as the start coords, with some values between.
   */
  if (priv->applet_motion_start_x != event->x ||
      priv->applet_motion_start_y != event->y)
    {
      priv->applet_motion_handled = TRUE;
    }

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
  if (event->x < HDH_SWITCH_WIDTH)
    hd_home_pan_and_move_applet (priv->home, TRUE, applet);
  else if (event->x > priv->xwidth - HDH_SWITCH_WIDTH)
    hd_home_pan_and_move_applet (priv->home, FALSE, applet);

  return FALSE;
}

static gboolean
hd_home_view_applet_press (ClutterActor       *applet,
			   ClutterButtonEvent *event,
			   HdHomeView         *view)
{
  HdHomeViewPrivate *priv = view->priv;

  guint id;

  id = g_signal_connect (applet, "motion-event",
			 G_CALLBACK (hd_home_view_applet_motion),
			 view);

  g_object_set_data (G_OBJECT (applet), "HD-VIEW-motion-cb",
		     GINT_TO_POINTER (id));

  priv->applet_motion_handled = FALSE;
  priv->applet_motion_start_x = event->x;
  priv->applet_motion_start_y = event->y;
  priv->applet_motion_last_x = event->x;
  priv->applet_motion_last_y = event->y;

  return FALSE;
}

static gboolean
hd_home_view_applet_release (ClutterActor       *applet,
			     ClutterButtonEvent *event,
			     HdHomeView         *view)
{
  HdHomeViewPrivate *priv = view->priv;
  guint id;

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
  if (!priv->applet_motion_handled)
    g_signal_emit (view, signals[SIGNAL_APPLET_CLICKED], 0, applet);
  else
    {
      /* Move the underlying window to match the actor's position */
      gint x, y;
      guint w, h;
      MBWindowManagerClient *client;
      MBWMCompMgrClient *cclient;
      MBGeometry geom;

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
    }

  return TRUE;
}

void
hd_home_view_add_applet (HdHomeView *view, ClutterActor *applet)
{
  HdHomeViewPrivate        *priv = view->priv;
  MBWMCompMgrClutterClient *cc;
  guint                     id;

  /*
   * Reparent the applet to ourselves; note that this automatically
   * gets us the correct position within the view.
   */
  clutter_actor_reparent (applet, CLUTTER_ACTOR (view));
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

  priv->applet_motion_handled = TRUE;
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

  if (priv->background_image)
    return priv->background_image;

  return priv->background;
}

GType
hd_home_view_background_mode_get_type (void)
{
  static GType etype = 0;
  if (G_UNLIKELY (!etype))
    {
      static const GEnumValue values[] = {
	{HDHV_BACKGROUND_STRETCHED, "HDHV_BACKGROUND_STRETCHED", "stretched"},
	{HDHV_BACKGROUND_CENTERED, "HDHV_BACKGROUND_CENTERED", "centered"},
	{HDHV_BACKGROUND_SCALED, "HDHV_BACKGROUND_SCALED", "scaled"},
	{HDHV_BACKGROUND_TILED, "HDHV_BACKGROUND_TILED", "tiled"},
	{HDHV_BACKGROUND_CROPPED, "HDHV_BACKGROUND_CROPPED", "cropped"},
	{0, NULL, NULL}
      };
      etype = g_enum_register_static (g_intern_static_string ("HdHomeViewBackgroundMode"), values);
    }
  return etype;
}
