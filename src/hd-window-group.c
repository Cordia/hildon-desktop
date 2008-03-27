/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Johan Bilien <johan.bilien@nokia.com>
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

#include "hd-window-group.h"

#include <clutter/clutter-effect.h>

/* FIXME */
#define ITEM_WIDTH      800
#define ITEM_HEIGHT     480
#define PADDING         20
#define ZOOM_PADDING    50

enum
{
  SIGNAL_ITEM_SELECTED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

typedef struct
{
  ClutterActor *actor;
  gint          x, y;
  guint         click_handler;
} ChildData;

static const gint positions[11][11][2] =
/* 1 */ { { { -1, -1 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0} },
/* 2 */   { { -2, -1 } , { 0,  -1}, { 0,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0} },
/* 3 */   { { -2, -2 } , { 0, -2}, {-1,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0} },
/* 4 */   { { -2, -2 } , { 0, -2}, {-2,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0} },
/* 5 */   { { -3, -2 } , {-1, -2}, {-3,  0},
            { -1,  0 } , { 1, -1}, { 0,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0} },
/* 6 */   { { -3, -2 } , {-1, -2}, {-3,  0},
            { -1,  0 } , { 1, -2}, { 1,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0} },
/* 7 */   { { -3, -3 } , {-1, -3}, {-3, -1},
            { -1, -1 } , { 1, -3}, { 1, -1},
            { -1,  1 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0} },
/* 8 */   { { -3, -3 } , {-1, -3}, {-3, -1},
            { -1, -1 } , { 1, -3}, { 1, -1},
            { -2,  1 } , { 0,  1}, { 0,  0},
            {  0,  0 } , { 0,  0} },
/* 9 */   { { -3, -3 } , {-1, -3}, {-3, -1},
            { -1, -1 } , { 1, -3}, { 1, -1},
            { -3,  1 } , {-1,  1}, { 1,  1},
            {  0,  0 } , { 0,  0} },
/* 10 */  { { -3, -3 } , {-1, -3}, {-3, -1},
            { -1, -1 } , { 1, -3}, { 1, -1},
            { -3,  1 } , {-1,  1}, { 1,  1},
            { -1,  3 } , { 0,  0} },
/* 11 */  { { -3, -3 } , {-1, -3}, {-3, -1},
            { -1, -1 } , { 1, -3}, { 1, -1},
            { -3,  1 } , {-1,  1}, { 1,  1},
            { -2,  3 } , { 0,  3} } };


struct _HdWindowGroupPrivate
{
  GList                        *children;

  ClutterEffectTemplate        *move_template;
  ClutterEffectTemplate        *zoom_template;

  /* Used in the item placement loop only */
  guint                         n_items;
  guint                         i_item;
  gint                          min_x, min_y, max_x;
};

static void hd_window_group_class_init (HdWindowGroupClass *klass);
static void hd_window_group_init       (HdWindowGroup *self);
static void hd_window_group_dispose    (GObject *object);
static void hd_window_group_finalize   (GObject *object);

static void hd_window_group_add (ClutterGroup *group, ClutterActor *actor);
static void hd_window_group_remove (ClutterGroup *group, ClutterActor *actor);

static void hd_window_group_place (HdWindowGroup *group);

static gint
find_by_actor (ChildData *data, ClutterActor *actor)
{
  return (data->actor != actor);
}

G_DEFINE_TYPE (HdWindowGroup, hd_window_group, CLUTTER_TYPE_GROUP);

static void
hd_window_group_class_init (HdWindowGroupClass *klass)
{
  GObjectClass         *object_class = G_OBJECT_CLASS (klass);
  ClutterGroupClass    *group_class = CLUTTER_GROUP_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdWindowGroupPrivate));

  object_class->dispose = hd_window_group_dispose;
  object_class->finalize = hd_window_group_finalize;

  group_class->add = hd_window_group_add;
  group_class->remove = hd_window_group_remove;

  signals[SIGNAL_ITEM_SELECTED] =
      g_signal_new ("item-selected",
                    G_OBJECT_CLASS_TYPE (object_class),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (HdWindowGroupClass, item_selected),
                    NULL,
                    NULL,
                    g_cclosure_marshal_VOID__OBJECT,
                    G_TYPE_NONE,
                    1,
                    CLUTTER_TYPE_ACTOR);
}

static void
hd_window_group_init (HdWindowGroup *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            HD_TYPE_WINDOW_GROUP,
                                            HdWindowGroupPrivate);

  self->priv->move_template =
      clutter_effect_template_new_for_duration (300, CLUTTER_ALPHA_RAMP_INC);

  self->priv->zoom_template =
      clutter_effect_template_new_for_duration (300, CLUTTER_ALPHA_RAMP_INC);

  clutter_actor_set_reactive (CLUTTER_ACTOR (self), TRUE);

}

static void
hd_window_group_dispose (GObject *object)
{
  G_OBJECT_CLASS (hd_window_group_parent_class)->dispose (object);
}

static void
hd_window_group_finalize (GObject *object)
{
  HdWindowGroupPrivate *priv = HD_WINDOW_GROUP (object)->priv;

  if (priv->children)
    {
      g_list_free (priv->children);
      priv->children = NULL;
    }

  if (priv->move_template)
    {
      g_object_unref (priv->move_template);
      priv->move_template = NULL;
    }

  G_OBJECT_CLASS (hd_window_group_parent_class)->finalize (object);
}

static ChildData *
hd_window_group_get_child_data (HdWindowGroup *group, ClutterActor *actor)
{
  GList *l;

  l = g_list_find_custom (group->priv->children,
                          actor,
                          (GCompareFunc)find_by_actor);

  if (l)
    return l->data;

  else
    return NULL;
}

static gboolean
hd_window_group_child_button_release (HdWindowGroup    *group,
                                      ClutterEvent     *event,
                                      ClutterActor     *actor)
{
  hd_window_group_zoom_item (group, actor);

  return FALSE;
}

static void
hd_window_group_add (ClutterGroup *group, ClutterActor *actor)
{
  HdWindowGroupPrivate *priv = HD_WINDOW_GROUP (group)->priv;
  ChildData            *data;

  data = hd_window_group_get_child_data (HD_WINDOW_GROUP (group), actor);
  if (data)
    return;

  clutter_actor_set_reactive (actor, TRUE);

  data = g_new0 (ChildData, 1);
  data->actor = actor;
  data->click_handler =
      g_signal_connect_swapped (actor, "button-release-event",
                                G_CALLBACK (hd_window_group_child_button_release),
                                group);
  priv->children = g_list_append (priv->children, data);

  hd_window_group_place (HD_WINDOW_GROUP (group));
  hd_window_group_zoom_item (HD_WINDOW_GROUP (group), NULL);

}

static void
hd_window_group_remove (ClutterGroup *group, ClutterActor *actor)
{
  HdWindowGroupPrivate *priv = HD_WINDOW_GROUP (group)->priv;
  ChildData            *data;

  data = hd_window_group_get_child_data (HD_WINDOW_GROUP (group), actor);
  if (!data) return;

  g_signal_handler_disconnect (actor, data->click_handler);
  priv->children = g_list_remove (priv->children, data);
  g_free (data);

  hd_window_group_place (HD_WINDOW_GROUP (group));
  hd_window_group_zoom_item (HD_WINDOW_GROUP (group), NULL);
}

static void
hd_window_group_move_item (HdWindowGroup       *group,
                           ClutterActor        *actor,
                           gint x, gint y)
{
  HdWindowGroupPrivate *priv            = group->priv;
  gint                  xcoord, ycoord;

  xcoord = x * (ITEM_WIDTH+PADDING)  / 2;
  ycoord = y * (ITEM_HEIGHT+PADDING) / 2;

  clutter_timeline_start (clutter_effect_move (priv->move_template,
                                               actor,
                                               xcoord, ycoord,
                                               NULL,
                                               NULL));

}

static void
place_child (ChildData *data, HdWindowGroup *group)
{
  HdWindowGroupPrivate *priv = group->priv;
  gint x, y;

  if (priv->n_items <= 11)
    {
      x = positions [priv->n_items - 1][priv->i_item][0];
      y = positions [priv->n_items - 1][priv->i_item][1];
    }
  else
    {
      x = 2 * (priv->i_item % 3) - 3;
      y = 2 * (priv->i_item / 3) - 3;
    }

  if (data->x != x || data->y != y)
    {
      data->x = x;
      data->y = y;
      hd_window_group_move_item (group, data->actor, x, y);
    }

  priv->i_item ++;

  if (x < priv->min_x) priv->min_x = x;
  if (y < priv->min_y) priv->min_y = y;
  if (x > priv->max_x) priv->max_x = x;
}

static void
hd_window_group_place (HdWindowGroup *group)
{
  HdWindowGroupPrivate *priv = group->priv;

  priv->n_items = g_list_length (priv->children);
  priv->i_item = 0;
  priv->min_x = priv->min_y = G_MAXINT;
  priv->max_x = G_MININT;

  g_list_foreach (priv->children, (GFunc)place_child, group);
}

static void
hd_window_group_zoom_completed (HdWindowGroup *group, ClutterActor *actor)
{
  g_signal_emit (group, signals[SIGNAL_ITEM_SELECTED], 0, actor);
}

void
hd_window_group_zoom_item (HdWindowGroup *group, ClutterActor *actor)
{
  HdWindowGroupPrivate *priv = group->priv;
  ClutterTimeline      *timeline;
  gdouble               scale;
  gint                  x, y;

  if (actor)
    {
      x = 0 - clutter_actor_get_x (actor);
      y = 0 - clutter_actor_get_y (actor);
      scale = 1.0;
    }
  else
    {
      /* Zooming out, expose style. We want to position ourselves at
       * (min_x, min_y) and zoom out enough so [min_x , max_x] is visible */
      guint width;

      width =ZOOM_PADDING +
                  (priv->max_x - priv->min_x + 2) * (ITEM_WIDTH + PADDING) / 2;

      scale = (gdouble) ITEM_WIDTH  / width;
      x = ZOOM_PADDING / 2 +
          (gint)(scale * (0 - priv->min_x * (ITEM_WIDTH + PADDING) / 2));
      y = ZOOM_PADDING / 2 +
          (gint)(scale * (0 - priv->min_y * (ITEM_HEIGHT + PADDING) / 2));

    }

  timeline = clutter_effect_scale (priv->zoom_template,
                                   CLUTTER_ACTOR (group),
                                   scale,
                                   scale,
                                   (ClutterEffectCompleteFunc)
                                   hd_window_group_zoom_completed,
                                   actor);
  clutter_timeline_start (timeline);

  timeline = clutter_effect_move (priv->zoom_template,
                                  CLUTTER_ACTOR (group),
                                  x, y,
                                  NULL,
                                  NULL);
  clutter_timeline_start (timeline);

}

