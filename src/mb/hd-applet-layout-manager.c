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

#include "hd-applet-layout-manager.h"

#define HDALM_LAYER_X 0
#define HDALM_LAYER_Y 64
#define HDALM_LAYER_WIDTH 800
#define HDALM_LAYER_HEIGHT (480-HDALM_LAYER_Y)

#define HDALM_APPLET_PADDING 20

typedef struct _HdAlLayer   HdAlLayer;

struct _HdAlLayer
{
  GList * rects;
};

struct _HdAppletLayoutManager
{
  GList * layers;
};

static gint
hd_applet_layout_layer_sort_func (gconstpointer a, gconstpointer b)
{
  const MBGeometry *r1 = a;
  const MBGeometry *r2 = b;

  /*
   * If we find two rectangles with the same coords in the layer,
   * then our algorithm is broken.
   */
  if (r1->x == r2->x && r1->y == r2->y)
    g_warning ("Error in applet layout algorithm detected.");

  if (r1->x < r2->x || r1->y < r2->y)
    return -1;

  return 1;
}

static void
hd_applet_layout_layer_sort (HdAlLayer *layer)
{
  layer->rects = g_list_sort (layer->rects, hd_applet_layout_layer_sort_func);
}

static HdAlLayer *
hd_applet_layout_manager_add_layer (HdAppletLayoutManager *mgr)
{
  HdAlLayer  *layer = g_new0 (HdAlLayer, 1);
  MBGeometry *rect  = g_new0 (MBGeometry, 1);

  rect->x      = HDALM_LAYER_X;
  rect->y      = HDALM_LAYER_Y;
  rect->width  = HDALM_LAYER_WIDTH;
  rect->height = HDALM_LAYER_HEIGHT;

  layer->rects = g_list_prepend (layer->rects, rect);

  mgr->layers = g_list_append (mgr->layers, layer);

  return layer;
}

/*
 * Attempts to split outer rectangle so as to exclued area
 * [0,0; rect->width x rect->height] from it. If successful, it returns TRUE
 * and new_rects containins the new rectangles representing the remaining
 * space (or NULL, if the whole rectangle was used up).
 *
 * x,y coords of rect are adjusted to match the allocated space.
 */
static gboolean
hd_applet_layout_split_rectangle (MBGeometry  *outer,
				  MBGeometry  *rect,
				  GList      **new_rects)
{
  gint        o_x, o_y, o_w, o_h;
  gint        width, height;
  GList      *l = NULL;
  MBGeometry *r;

  o_x = outer->x;
  o_y = outer->y;
  o_w = outer->width;
  o_h = outer->height;

  width  = rect->width;
  height = rect->height;

  if (o_w < width || o_h < height)
    {
      /*
       * The inner rectangle does not fit.
       */
      return FALSE;
    }

  if (o_w == width && o_h == height)
    {
      /*
       * Exact match.
       */
      *new_rects = NULL;
      rect->x = o_x;
      rect->y = o_y;

      return TRUE;
    }

  /* 0,1 */
  if (o_h > height)
    {
      r = g_new0 (MBGeometry, 1);
      r->x      = o_x;
      r->y      = o_y + height;
      r->width  = width;
      r->height = o_h - height;

      l = g_list_append (l, r);
    }

  /* The right band, 1,0; 1,1 */
  if (o_w > width)
    {
      r = g_new0 (MBGeometry, 1);
      r->x      = o_x + width;
      r->y      = o_y;
      r->width  = o_w - width;
      r->height = height;

      l = g_list_append (l, r);

      if (o_h > height)
	{
	  r = g_new0 (MBGeometry, 1);
	  r->x      = o_x + width;
	  r->y      = o_y + height;
	  r->width  = o_w - width;
	  r->height = o_h  - height;

	  l = g_list_append (l, r);
	}
    }

  rect->x = o_x;
  rect->y = o_y;

  *new_rects = l;

  return TRUE;
}

/*
 * Returns TRUE if the rectangle was successfully inserted into the
 * given layer.
 */
static gboolean
hd_applet_layout_layer_insert_rect (HdAlLayer *layer, MBGeometry *rect)
{
  GList    *rects = layer->rects;
  GList    *start = rects;

  while (rects)
    {
      MBGeometry *r = rects->data;
      GList      *new_rects;
      gboolean    success;

      success = hd_applet_layout_split_rectangle (r, rect, &new_rects);

      if (success && new_rects)
	{
	  /*
	   * We managed to split this rectangle -- replace the original
	   * with the new list.
	   */
	  GList *prev    = rects->prev;
	  GList *next    = rects->next;

	  if (prev)
	    prev->next = new_rects;
	  else
	    start = new_rects;

	  if (next)
	    {
	      GList *new_end = g_list_last (new_rects);

	      new_end->next = next;
	    }

	  g_free (r);
	  g_list_free_1 (rects);

	  /* Make sure the layer pointer is correct */
	  layer->rects = start;

	  hd_applet_layout_layer_sort (layer);

	  return TRUE;
	}
      else if (success)
	{
	  /*
	   * The whole rectangle was used up; remove it from the list.
	   */
	  layer->rects = g_list_remove_link (layer->rects, rects);
	  return TRUE;
	}

      rects = rects->next;
    }

  return FALSE;
}

static gboolean
hd_applet_layout_layer_reclaim_rect (HdAlLayer *layer, MBGeometry *rect)
{
  MBGeometry *r;

  r = g_new0 (MBGeometry, 1);
  r->x      = rect->x;
  r->y      = rect->y;
  r->width  = rect->width;
  r->height = rect->height;

  layer->rects = g_list_prepend (layer->rects, r);

  hd_applet_layout_layer_sort (layer);

  return TRUE;
}

/*
 * Adjusts geometry for the applet, returning the number of the layer
 * allocated for the applet.
 */
gint
hd_applet_layout_manager_request_geometry (HdAppletLayoutManager *mgr,
					   MBGeometry            *geom)
{
  GList    *layers  = mgr->layers;
  gboolean  success = FALSE;
  gint      i       = 0;

  /*
   * Adjust the inital geometry by required padding between applets.
   */
  geom->width  += 2 * HDALM_APPLET_PADDING;
  geom->height += 2 * HDALM_APPLET_PADDING;

  while (layers)
    {
      HdAlLayer *layer = layers->data;

      if (hd_applet_layout_layer_insert_rect (layer, geom))
	{
	  success = TRUE;
	  break;
	}

      ++i;
      layers = layers->next;
    }

  if (!success)
    {
      /*
       * Add new layer, and insert the rectangle into it
       */
      HdAlLayer * layer;

      layer = hd_applet_layout_manager_add_layer (mgr);

      if (!hd_applet_layout_layer_insert_rect (layer, geom))
	{
	  /* This should not happen */
	  g_warning ("Failed to insert applet into empty layer!");
	}
    }

  /*
   * Adjust the new geometry by padding between applets.
   */
  geom->x      += HDALM_APPLET_PADDING;
  geom->y      += HDALM_APPLET_PADDING;
  geom->width  -= 2*HDALM_APPLET_PADDING;
  geom->height -= 2*HDALM_APPLET_PADDING;

  return i;
}

HdAppletLayoutManager *
hd_applet_layout_manager_new (void)
{
  HdAppletLayoutManager * mgr = g_new0 (HdAppletLayoutManager, 1);

  /* Add initial layer */
  hd_applet_layout_manager_add_layer (mgr);

  return mgr;
}

gint
hd_applet_layout_manager_get_layer_count (HdAppletLayoutManager *mgr)
{
  return g_list_length (mgr->layers);
}

void
hd_applet_layout_manager_reclaim_geometry (HdAppletLayoutManager *mgr,
					   gint                   layer_id,
					   MBGeometry            *geom)
{
  HdAlLayer *layer;

  /*
   * Adjust the inital geometry by required padding between applets.
   */
  geom->x      -= HDALM_APPLET_PADDING;
  geom->y      -= HDALM_APPLET_PADDING;
  geom->width  += 2 * HDALM_APPLET_PADDING;
  geom->height += 2 * HDALM_APPLET_PADDING;

  layer = g_list_nth_data (mgr->layers, layer_id);

  if (layer)
    hd_applet_layout_layer_reclaim_rect (layer, geom);
  else
    g_warning ("Layer %d does not exist", layer_id);
}
