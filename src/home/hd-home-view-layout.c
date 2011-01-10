/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2009 Nokia Corporation.
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

#include "hd-home-view-layout.h"
#include "hd-comp-mgr.h"

/* Padding between applets - Just enough to get 5 contacts onto the screen.
 * See bug 137601
 */
#define PADDING 13
#define MIN_SIZE (2 * PADDING + 1)

typedef struct rect_t rect_t;
typedef struct layer_t layer_t;

struct rect_t
{
  int x1;
  int y1;
  int x2;
  int y2;
};

struct layer_t
{
  layer_t *child;
  GList *rectangles;
};

struct _HdHomeViewLayoutPrivate
{
  layer_t *layer;
};

G_DEFINE_TYPE (HdHomeViewLayout, hd_home_view_layout, G_TYPE_OBJECT);

static rect_t *
rect_new (int x1, int y1,
          int x2, int y2)
{
  rect_t *r = g_slice_new (rect_t);

  r->x1 = x1;
  r->y1 = y1;
  r->x2 = x2;
  r->y2 = y2;

  return r;
}

static void
rect_free (rect_t *r)
{
  if (r)
    g_slice_free (rect_t, r);
}

static gint
rect_cmp (gconstpointer a,
          gconstpointer b)
{
  const rect_t *r1, *r2;

  r1 = a;
  r2 = b;

  return r1->y1 != r2->y1 ? r1->y1 - r2->y1 : r1->x1 - r2->x1;
}

static gboolean
rect_subtract (rect_t *r1, rect_t *r2, GList **l)
{
  if (r1->x1 < r2->x2 &&
      r2->x1 < r1->x2 &&
      r1->y1 < r2->y2 &&
      r2->y1 < r1->y2)
    {
      /* intersect */

      /* new north rectangle */
      if ((r2->y1 - r1->y1) >= MIN_SIZE)
        *l = g_list_insert_sorted (*l,
                                   rect_new (r1->x1, r1->y1,
                                             r1->x2, r2->y1),
                                   rect_cmp);
      /* new south rectangle */
      if ((r1->y2 - r2->y2) >= MIN_SIZE)
        *l = g_list_insert_sorted (*l,
                                   rect_new (r1->x1, r2->y2,
                                             r1->x2, r1->y2),
                                   rect_cmp);
      /* new west rectangle */
      if ((r2->x1 - r1->x1) >= MIN_SIZE)
        *l = g_list_insert_sorted (*l,
                                   rect_new (r1->x1, r1->y1,
                                             r2->x1, r1->y2),
                                   rect_cmp);
      /* new east rectangle */
      if ((r1->x2 - r2->x2) >= MIN_SIZE)
        *l = g_list_insert_sorted (*l,
                                   rect_new (r2->x2, r1->y1,
                                             r1->x2, r1->y2),
                                   rect_cmp);

      return TRUE;
    }

  /* no intersection */
  return FALSE;
}

static GList *
list_subtract (GList *list, rect_t *r)
{
  GList *new_list = NULL;

  while (list)
    {
      rect_t *old = list->data;

      list = g_list_delete_link (list, list);

      if (rect_subtract (old, r, &new_list))
        rect_free (old);
      else
        new_list = g_list_insert_sorted (new_list,
                                         old,
                                         rect_cmp);
    }

  return new_list;
}

static rect_t *
list_find (GList *list, rect_t *r)
{
  GList *l;

  for (l = list; l; l = l->next)
    {
      rect_t *d = l->data;

      if ((d->x2 - d->x1) >= (r->x2 - r->x1) &&
          (d->y2 - d->y1) >= (r->y2 - r->y1))
        return d;
    }

  return NULL;
}

static layer_t *
layer_new (GSList *applets)
{
  layer_t *layer = g_slice_new0 (layer_t);
  GSList *a;

  layer->rectangles = g_list_prepend (NULL, rect_new (0,
                                                      HD_COMP_MGR_TOP_MARGIN,
                                                      HD_COMP_MGR_LANDSCAPE_WIDTH,
                                                      HD_COMP_MGR_LANDSCAPE_HEIGHT));

  for (a = applets; a; a = a->next)
    {
      gfloat x, y;
      gfloat width, height;
      rect_t r;

      clutter_actor_get_position (CLUTTER_ACTOR (a->data), &x, &y);
      clutter_actor_get_size (CLUTTER_ACTOR (a->data), &width, &height);

      r.x1 = x;
      r.y1 = y;
      r.x2 = r.x1 + width;
      r.y2 = r.y1 + height;

      layer->rectangles = list_subtract (layer->rectangles, &r);
    }

  return layer;
}

static void
layer_free (layer_t *layer)
{
  if (!layer)
    return;

  layer_free (layer->child);

  g_list_foreach (layer->rectangles, (GFunc) rect_free, NULL);
  g_list_free (layer->rectangles);
  g_slice_free (layer_t, layer);
}

static void
hd_home_view_layout_init (HdHomeViewLayout *layout)
{
  layout->priv = G_TYPE_INSTANCE_GET_PRIVATE (layout, HD_TYPE_HOME_VIEW_LAYOUT, HdHomeViewLayoutPrivate);
}

static void
hd_home_view_layout_dispose (GObject *object)
{
  HdHomeViewLayoutPrivate *priv = HD_HOME_VIEW_LAYOUT (object)->priv;

  if (priv->layer)
    priv->layer = (layer_free (priv->layer), NULL);

  G_OBJECT_CLASS (hd_home_view_layout_parent_class)->dispose (object);
}

static void
hd_home_view_layout_class_init (HdHomeViewLayoutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = hd_home_view_layout_dispose;

  g_type_class_add_private (klass, sizeof (HdHomeViewLayoutPrivate));
}

HdHomeViewLayout *
hd_home_view_layout_new (void)
{
  return g_object_new (HD_TYPE_HOME_VIEW_LAYOUT, NULL);
}

void
hd_home_view_layout_reset (HdHomeViewLayout *layout)
{
  HdHomeViewLayoutPrivate *priv = layout->priv;

  if (priv->layer)
    priv->layer = (layer_free (priv->layer), NULL);
}

void
hd_home_view_layout_arrange_applet (HdHomeViewLayout *layout,
                                    GSList           *applets,
                                    ClutterActor     *new_applet)
{
  HdHomeViewLayoutPrivate *priv = layout->priv;
  gfloat width, height;
  rect_t r;
  layer_t *l;

  if (!priv->layer)
    priv->layer = layer_new (applets);

  clutter_actor_get_size (new_applet, &width, &height);

  r.x1 = 0;
  r.y1 = 0;
  r.x2 = width + 2 * PADDING;
  r.y2 = height + 2 * PADDING;

  for (l = priv->layer; l; l = l->child)
    {
      rect_t *f = list_find (l->rectangles, &r);

      if (f)
        {
          layer_t *k;

          clutter_actor_set_position (new_applet, f->x1 + PADDING, f->y1 + PADDING);

          r.x1 = f->x1 + PADDING;
          r.y1 = f->y1 + PADDING;
          r.x2 = r.x1 + width;
          r.y2 = r.y1 + height;

          for (k = priv->layer; k; k = k->child)
            {
              k->rectangles = list_subtract (k->rectangles,
                                             &r);

              if (k == l)
                return;
            }

          return;
        }
    }

  clutter_actor_set_position (new_applet, PADDING, HD_COMP_MGR_TOP_MARGIN + PADDING);

  r.x1 = PADDING;
  r.y1 = HD_COMP_MGR_TOP_MARGIN + PADDING;
  r.x2 = r.x1 + width;
  r.y2 = r.y1 + height;

  for (l = priv->layer; l; l = l->child)
    {
      l->rectangles = list_subtract (l->rectangles, &r);

      if (!l->child)
        {
          l->child = layer_new (NULL);
          l->child->rectangles = list_subtract (l->child->rectangles, &r);
          return;
        }
    }
}
