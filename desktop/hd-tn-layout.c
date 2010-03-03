/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2010 Moises Martinez
 *
 * Author:  Moises Martinez <moimart@gmail.com>
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

#include "hd-tn-layout.h"
#include "hd-task-navigator.h"
#include "hd-comp-mgr.h"
#include "hd-scrollable-group.h"
#include "hd-plugin-module.h"
#include "hd-desktop-config.h"

enum 
{
  PROP_0,
  PROP_WIDTH,
  PROP_HEIGHT
};

G_DEFINE_ABSTRACT_TYPE (HdTnLayout, hd_tn_layout, G_TYPE_OBJECT);
#define HD_TN_LAYOUT_GET_PRIVATE(obj) \
                (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                HD_TYPE_TN_LAYOUT, HdTnLayoutPrivate))

struct _HdTnLayoutPrivate
{
  guint width;
  guint height;
};

static void 
hd_tn_layout_set_property (GObject *object,
		           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  HdTnLayoutPrivate *priv =
    HD_TN_LAYOUT_GET_PRIVATE (object);

  switch (prop_id)
    {
      case PROP_WIDTH:
        priv->width = g_value_get_int (value);
        break;

      case PROP_HEIGHT:
        priv->height = g_value_get_int (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void 
hd_tn_layout_get_property (GObject    *object,
			   guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  HdTnLayoutPrivate *priv = 
    HD_TN_LAYOUT_GET_PRIVATE (object);

  switch (prop_id)
    {
      case PROP_WIDTH:
        g_value_set_int (value, priv->width);
        break;

      case PROP_HEIGHT:
        g_value_set_int (value, priv->height);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
         break;
    }
}

static void
hd_tn_layout_class_init (HdTnLayoutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (HdTnLayoutPrivate));

  object_class->set_property = hd_tn_layout_set_property;
  object_class->get_property = hd_tn_layout_get_property;

  pspec = g_param_spec_int ("width",
			    "width",
			    "Thumbnail width",
			     0,
			     G_MAXINT,
			     0,
			     (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_WIDTH, pspec);

  pspec = g_param_spec_int ("height",
			    "height",
			    "Thumbnail height",
			     0,
			     G_MAXINT,
			     0,
			     (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_HEIGHT, pspec);

}

static void 
hd_tn_layout_init (HdTnLayout *layout)
{
  HdTnLayoutPrivate *priv =
    HD_TN_LAYOUT_GET_PRIVATE (layout);

  priv->width = priv->height = 0;
}

HdTnLayout *
hd_tn_layout_factory_get_layout (void)
{
  static HDPluginModule *plugin = NULL;
  const gchar *path;

  path = hd_desktop_config_get_tn_layout ();

  if (!path)
    return hd_default_layout_new ();
 
  if (plugin != NULL)
    g_object_unref (plugin);

  plugin = hd_plugin_module_new (path);

  if (g_type_module_use (G_TYPE_MODULE (plugin)) == FALSE)
    {
      g_warning ("Error loading module at %s", path);
      return hd_default_layout_new ();
    }  

  g_type_module_use (G_TYPE_MODULE (plugin));
  GObject *object = hd_plugin_module_get_object (plugin);

  g_debug ("NEW LAYOUT: %s OBJECT: %p", path, object);

  if (object != NULL && HD_IS_TN_LAYOUT (object))
    return HD_TN_LAYOUT (object);
  else
    return hd_default_layout_new ();
} 

void 
hd_tn_layout_calculate (HdTnLayout *layout, 
			GList *thumbnails, 
			ClutterActor *grid)
{
  if (HD_TN_LAYOUT_GET_CLASS (layout)->calculate != NULL)
    HD_TN_LAYOUT_GET_CLASS (layout)->calculate (layout,
						thumbnails, 
						grid);
}

void 
hd_tn_layout_get_thumbnail_size (HdTnLayout *layout, 
				 guint *width, 
				 guint *height)
{
  guint lw, lh;

  g_object_get (G_OBJECT (layout),
		"width", &lw,
		"height", &lh,
		NULL);

  if (width != NULL)
    *width = lw;

  if (height != NULL)
    *height = lh;
}

gboolean 
hd_tn_layout_within_grid (HdTnLayout *layout,
			  ClutterButtonEvent *event,
			  GList *thumbnails,
			  ClutterActor *grid)
{
  if (HD_TN_LAYOUT_GET_CLASS (layout)->within_grid != NULL)
    return HD_TN_LAYOUT_GET_CLASS (layout)->within_grid (layout, 
							 event, 
							 thumbnails, 
						         grid);

  return FALSE;
}

gboolean 
hd_tn_layout_animation_in_progress (HdTnLayout *layout)
{
  if (HD_TN_LAYOUT_GET_CLASS (layout)->animation_in_progress != NULL)
    return HD_TN_LAYOUT_GET_CLASS (layout)->animation_in_progress (layout);

  return FALSE;
}

void 
hd_tn_layout_stop_animation (HdTnLayout *layout)
{
  if (HD_TN_LAYOUT_GET_CLASS (layout)->stop_animation != NULL)
    HD_TN_LAYOUT_GET_CLASS (layout)->stop_animation (layout);
}

void 
hd_tn_layout_last_active_window (HdTnLayout *layout, ClutterActor *window)
{
  if (HD_TN_LAYOUT_GET_CLASS (layout)->last_active_window != NULL)
    HD_TN_LAYOUT_GET_CLASS (layout)->last_active_window (layout,
							 window);
}

/* Default layout object */

G_DEFINE_TYPE (HdDefaultLayout, hd_default_layout, HD_TYPE_TN_LAYOUT);
#define HD_DEFAULT_LAYOUT_GET_PRIVATE(obj) \
                (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                HD_TYPE_DEFAULT_LAYOUT, HdDefaultLayoutPrivate))

#define MARGIN_DEFAULT             8
/*
 * %GRID_TOP_MARGIN:              Space not considered at the top of the
 *                                switcher when layout out the thumbnails.
 * %GRID_HORIZONTAL_GAP,
 * %GRID_VERTICAL_GAP:            How much gap to leave between thumbnails.
 */
#define GRID_TOP_MARGIN           HD_COMP_MGR_TOP_MARGIN
#define GRID_HORIZONTAL_GAP       16
#define GRID_VERTICAL_GAP         16

/*
 * Application thumbnail dimensions, depending on the number of
 * currently running applications.  These dimension include everything
 * except gaps between thumbnails (naturally) and and enlarged close
 * button reaction area.  1-2 thumbnails are LARGE, 3-6 are MEDIUM
 * and the rest are SMALL.
 */
#define THUMB_LARGE_WIDTH         (int)(hd_comp_mgr_get_current_screen_width ()/2.32)
#define THUMB_LARGE_HEIGHT        (int)(THUMB_LARGE_WIDTH/HD_COMP_MGR_SCREEN_RATIO)
#define THUMB_MEDIUM_WIDTH        (int)(hd_comp_mgr_get_current_screen_width ()/3.2)
#define THUMB_MEDIUM_HEIGHT       (int)(THUMB_MEDIUM_WIDTH/HD_COMP_MGR_SCREEN_RATIO)
#define THUMB_SMALL_WIDTH         (int)(hd_comp_mgr_get_current_screen_width ()/5.5)
#define THUMB_SMALL_HEIGHT        (int)(THUMB_SMALL_WIDTH/HD_COMP_MGR_SCREEN_RATIO)

#define TH_CLOSE_ICON_SIZE           32
#define TH_TITLE_LEFT_MARGIN         8
#define TH_TITLE_RIGHT_MARGIN        4


struct _HdDefaultLayoutPrivate
{
  guint cells_per_row;
  guint xpos, last_row_xpos, ypos;
  guint hspace, vspace;
  guint nrows_per_page;

  guint n_thumbnails;
};

static inline gint __attribute__ ((const))
layout_fun (gint total, gint term1, gint term2, gint factor)
{
  /* Make sure all terms and factors are int:s because the result of
   * the outer subtraction can be negative and division is sensitive
   * to signedness. */
  return (total - (term1*factor + term2*(factor - 1))) / 2;
}

static void 
hd_default_layout_thumbnail_size (HdDefaultLayout *layout)
{
  HdDefaultLayoutPrivate *priv =
    HD_DEFAULT_LAYOUT_GET_PRIVATE (layout);
 
  if (priv->n_thumbnails <= 2)
    {
      g_object_set (G_OBJECT (layout), 
		    "width",
		    THUMB_LARGE_WIDTH,
		    NULL); 
      g_object_set (G_OBJECT (layout), 
		    "height",
		    THUMB_LARGE_HEIGHT,
		    NULL);
 
      priv->cells_per_row = priv->n_thumbnails;
      priv->nrows_per_page = 1;
    }
  else
  if (priv->n_thumbnails <= 6)
    {
      g_object_set (G_OBJECT (layout), 
		    "width",
		    THUMB_MEDIUM_WIDTH,
		    NULL); 
      g_object_set (G_OBJECT (layout), 
		    "height",
		    THUMB_MEDIUM_HEIGHT,
		    NULL);
      
      priv->cells_per_row = 3;
      priv->nrows_per_page = 2;
    }
  else
    {
      g_object_set (G_OBJECT (layout), 
		    "width",
		    THUMB_SMALL_WIDTH,
		    NULL); 

      g_object_set (G_OBJECT (layout), 
		    "height",
		    THUMB_SMALL_HEIGHT,
		    NULL);
    
      priv->cells_per_row = 4;
      priv->nrows_per_page = (priv->n_thumbnails <= 8) ? 2 : 3;
    }
}

static void
hd_default_layout_set_layout (HdDefaultLayout *layout)
{
  HdDefaultLayoutPrivate *priv =
    HD_DEFAULT_LAYOUT_GET_PRIVATE (layout);
  gint current_width, current_height;

  hd_default_layout_thumbnail_size (layout);

  g_object_get (G_OBJECT (layout),
		"width", &current_width,
		"height", &current_height,
		NULL);

  guint width  = hd_comp_mgr_get_current_screen_width ();
  guint height = hd_comp_mgr_get_current_screen_height (); 

  priv->xpos = layout_fun (width,
                           current_width,
                           GRID_HORIZONTAL_GAP,
                           priv->cells_per_row);

  priv->last_row_xpos = priv->xpos;

  if (priv->n_thumbnails <= 12)
    priv->ypos = layout_fun (height + GRID_TOP_MARGIN,
                             current_height,
                             GRID_VERTICAL_GAP,
                             priv->nrows_per_page);
  else
    priv->ypos = GRID_TOP_MARGIN + MARGIN_DEFAULT;

  priv->hspace = current_width  + GRID_HORIZONTAL_GAP;
  priv->vspace = current_height + GRID_VERTICAL_GAP;
}

static void
hd_default_layout_calculate (HdTnLayout *layout, 
			     GList *thumbnails,
			     ClutterActor *grid)
{
  HdDefaultLayoutPrivate *priv =
    HD_DEFAULT_LAYOUT_GET_PRIVATE (layout);
  gint maxwtitle,xthumb,ythumb, i;
  GList *l;
  gint width, height;

  priv->n_thumbnails = 0;

  for (l = thumbnails; l != NULL; l = l->next)
    priv->n_thumbnails++;

  hd_default_layout_set_layout (HD_DEFAULT_LAYOUT (layout));

  g_object_get (G_OBJECT (layout),
		"width", &width,
		"height", &height,
		NULL);

   /* Clip titles longer than this. */
  maxwtitle = width
    - (TH_TITLE_LEFT_MARGIN + TH_TITLE_RIGHT_MARGIN + TH_CLOSE_ICON_SIZE);

  /* Place and scale each thumbnail row by row. */
  xthumb = ythumb = 0xB002E;
  for (l = thumbnails, i = 0; l != NULL && (l->data != NULL); l = l->next, i++)
    {

      /* If it's a new row re/set @ythumb and @xthumb. */
      g_assert (priv->cells_per_row > 0);

      if (!(i % priv->cells_per_row))
        {
          if (i == 0)
            /* This is the very first row. */
            ythumb = priv->ypos;
          else
            ythumb += priv->vspace;

          /* Use @last_row_xpos if it's the last row. */
          xthumb = i + (priv->cells_per_row <= priv->n_thumbnails)
            ? priv->xpos : priv->last_row_xpos;
        }

      /* If @thwin's been there, animate as it's moving.  Otherwise if it's
       * a new one to enter the navigator, don't, it's hidden anyway. */
      //ops = thumb->thwin == newborn ? &Fly_at_once : &Fly_smoothly;

      /* Place @thwin in any case. */
      clutter_actor_set_position (CLUTTER_ACTOR (l->data), xthumb, ythumb);

      /* If @Thumbnails are not changing size and this is not a newborn
       * the inners of @thumb are already setup. */
      /*if (oldthsize == Thumbsize && thumb->thwin != newborn)
        goto skip_the_circus;*/

      /* Set thumbnail's reaction area. */
      clutter_actor_set_size (CLUTTER_ACTOR (l->data), 
			      width, 
			      height);

      /* @thumb->close */
      hd_tn_thumbnail_update_inners (HD_TN_THUMBNAIL (l->data), maxwtitle);

    xthumb += priv->hspace;
  }
 
  hd_scrollable_group_set_real_estate (HD_SCROLLABLE_GROUP (grid),
                                       HD_SCROLLABLE_GROUP_VERTICAL,
                                       ythumb + height);
}

static gboolean 
hd_default_layout_within_grid (HdTnLayout *layout, 
			       ClutterButtonEvent *event,
			       GList *thumbnails,
			       ClutterActor *grid)
{
   HdDefaultLayoutPrivate *priv =
    HD_DEFAULT_LAYOUT_GET_PRIVATE (layout);
 
  HdScrollableGroup *tgrid = HD_SCROLLABLE_GROUP (grid);
 
  gint x, y, n, m;
  gint current_width, current_height;

  if (thumbnails == NULL)
    return FALSE;

  priv->n_thumbnails = 0;

  GList *l = NULL;

  g_object_get (G_OBJECT (layout),
		"width",  &current_width,
		"height", &current_height,
		NULL);

  for (l = thumbnails; l != NULL; l = l->next)
    priv->n_thumbnails++;

  //calc_layout (&lout);

  /* y := top of the first row */
  y = priv->ypos - hd_scrollable_group_get_viewport_y (tgrid);

  if (event->y < y)
    /* Clicked above the first row. */
    return FALSE;

  /* y := the bottom of the last complete row */
  n  = priv->n_thumbnails / priv->cells_per_row;
  m  = priv->n_thumbnails % priv->cells_per_row;
  y += priv->vspace*(n-1) + current_height;

    if (event->y <= y)
    { /* Clicked somewhere in the complete rows. */
      x = priv->xpos;
      n = priv->cells_per_row;
    }
  else if (m && event->y <= y + priv->vspace)
    { /* Clicked somewhere in the incomplete row. */
      x = priv->last_row_xpos;
      n = m;
    }
  else /* Clicked below the last row. */
    return FALSE;

  /* Clicked somehere in the last (either complete or incomplete) row. */
  g_assert (n > 0);

  if (event->x < x)
    return FALSE;

  if (event->x > x + priv->hspace*(n-1) + current_width)
    return FALSE;

  /* Clicked between the thumbnails. */
  return TRUE;
}

static void 
hd_default_layout_class_init (HdDefaultLayoutClass *klass)
{
  HdTnLayoutClass *layout_class = HD_TN_LAYOUT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdDefaultLayoutPrivate));

  layout_class->calculate   = hd_default_layout_calculate;
  layout_class->within_grid = hd_default_layout_within_grid; 
}

static void
hd_default_layout_init (HdDefaultLayout *layout)
{

}

HdTnLayout *
hd_default_layout_new (void)
{
  return g_object_new (HD_TYPE_DEFAULT_LAYOUT, NULL);
}
