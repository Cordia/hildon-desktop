/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Unknown
 *          Marc Ordinas i Llopis <marc.ordinasillopis@collabora.co.uk>
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

#include "hd-launcher-grid.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <hildon/hildon-defines.h>

#include <cogl/cogl.h>
#include <clutter/clutter.h>

#include <mx/mx.h>

#include <math.h>

#include "hd-launcher.h"
#include "hd-launcher-item.h"
#include "hd-comp-mgr.h"
#include "hd-util.h"
#include "hd-transition.h"

#define I_(str) (g_intern_static_string ((str)))

#define HD_PARAM_READWRITE      (G_PARAM_READWRITE | \
                                 G_PARAM_STATIC_NICK | \
                                 G_PARAM_STATIC_NAME | \
                                 G_PARAM_STATIC_BLURB)
#define HD_PARAM_READABLE       (G_PARAM_READABLE | \
                                 G_PARAM_STATIC_NICK | \
                                 G_PARAM_STATIC_NAME | \
                                 G_PARAM_STATIC_BLURB)
#define HD_PARAM_WRITABLE       (G_PARAM_WRITABLE | \
                                 G_PARAM_STATIC_NICK | \
                                 G_PARAM_STATIC_NAME | \
                                 G_PARAM_STATIC_BLURB)

#define HD_LAUNCHER_GRID_GET_PRIVATE(obj)       (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HD_TYPE_LAUNCHER_GRID, HdLauncherGridPrivate))

struct _HdLauncherGridPrivate
{
  /* list of actors */
  GList *tiles;
  /* list of 'blocker' actors that block presses
   * on the empty rows of pixels between the icons */
  GList *blockers;

  guint h_spacing;
  guint v_spacing;

  MxAdjustment *h_adjustment;
  MxAdjustment *v_adjustment;

  /* How far the transition will move the icons */
  gint transition_depth;
  /* Do we move the icons all together or in sequence? for launcher_in transitions */
  gboolean transition_sequenced;
  /* List of keyframes used on transitions like _IN and _IN_SUB */
  HdKeyFrameList *transition_keyframes; // ramp for tile movement
  HdKeyFrameList *transition_keyframes_label; // ramp for label alpha values
  HdKeyFrameList *transition_keyframes_icon; // ramp for icon alpha values
};

enum
{
  PROP_0,

  PROP_H_ADJUSTMENT,
  PROP_V_ADJUSTMENT
};

/* FIXME: Do we need signals here?
enum
{
  LAST_SIGNAL
};

static guint task_signals[LAST_SIGNAL] = {};
*/

static void mx_scrollable_iface_init   (MxScrollableIface *iface);

static gboolean _hd_launcher_grid_blocker_release_cb (ClutterActor *actor,
                                        ClutterButtonEvent *event,
                                        gpointer *data);

#define HD_LAUNCHER_GRID_MAX_COLUMNS (int)(HD_COMP_MGR_LANDSCAPE_WIDTH/160)

G_DEFINE_TYPE_WITH_CODE (HdLauncherGrid,
                         hd_launcher_grid,
                         CLUTTER_TYPE_GROUP,
                         G_IMPLEMENT_INTERFACE (MX_TYPE_SCROLLABLE,
                                                mx_scrollable_iface_init));

static inline void
hd_launcher_grid_refresh_h_adjustment (HdLauncherGrid *grid)
{
  HdLauncherGridPrivate *priv = grid->priv;
  gfloat width;
  gfloat clip_x, clip_width;
  gfloat page_width;
  width = 0;
  clip_x = 0;
  clip_width = 0;
  page_width = 0;

  if (!priv->h_adjustment)
    return;

  clutter_actor_get_size (CLUTTER_ACTOR (grid), &width, NULL);
  clutter_actor_get_clip (CLUTTER_ACTOR (grid),
                          &clip_x, NULL,
                          &clip_width, NULL);

  if (clip_width == 0)
    page_width = width;
  else
    page_width = MIN (width, clip_width - clip_x);

  mx_adjustment_set_values (priv->h_adjustment,
                              mx_adjustment_get_value (priv->h_adjustment),
                              0,
                              width,
                              1.0f,
                              20.0f,
                              page_width);
}

static inline void
hd_launcher_grid_refresh_v_adjustment (HdLauncherGrid *grid)
{
  HdLauncherGridPrivate *priv = grid->priv;
  gfloat height;
  gfloat clip_y, clip_height;
  gfloat page_height;
  height = 0;
  clip_y = 0;
  clip_height = 0;
  page_height = 0;

  if (!priv->v_adjustment)
    return;

  clutter_actor_get_size (CLUTTER_ACTOR (grid), NULL, &height);
  clutter_actor_get_clip (CLUTTER_ACTOR (grid),
                          NULL, &clip_y,
                          NULL, &clip_height);
  if (height >= HD_COMP_MGR_LANDSCAPE_HEIGHT)
    {
      /* Padding at the bottom. */
      height += HD_LAUNCHER_BOTTOM_MARGIN - HD_LAUNCHER_GRID_ROW_SPACING;
      mx_adjustment_set_elastic (priv->v_adjustment, TRUE);
    }
  else
    mx_adjustment_set_elastic (priv->v_adjustment, FALSE);

  if (clip_height == 0)
    page_height = MIN (height, HD_COMP_MGR_LANDSCAPE_HEIGHT);
  else
    page_height = MIN (height, clip_height - clip_y);

  mx_adjustment_set_values (priv->v_adjustment,
                               mx_adjustment_get_value (priv->v_adjustment),
                               0,
                               height,
                               1.0f,
                               1.0f * 20,
                               page_height);
}

void
hd_launcher_grid_reset_v_adjustment (HdLauncherGrid *grid)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID_GET_PRIVATE (grid);

  mx_adjustment_set_value (priv->v_adjustment, 0);
}

static void
adjustment_value_notify (GObject    *gobject,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
  ClutterActor *grid = user_data;
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID (grid)->priv;

  clutter_actor_set_anchor_point(grid,
                             0,
                             mx_adjustment_get_value(priv->v_adjustment));
}

static void
hd_launcher_grid_set_adjustments (MxScrollable *scrollable,
                                  MxAdjustment *h_adj,
                                  MxAdjustment *v_adj)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID (scrollable)->priv;

  if (h_adj != priv->h_adjustment)
    {
      if (priv->h_adjustment)
        {
          g_signal_handlers_disconnect_by_func (priv->h_adjustment,
                                                adjustment_value_notify,
                                                scrollable);
          g_object_unref (priv->h_adjustment);
          priv->h_adjustment = NULL;
        }

      if (h_adj)
        {
          priv->h_adjustment = g_object_ref (h_adj);
          g_signal_connect (priv->h_adjustment, "notify::value",
                            G_CALLBACK (adjustment_value_notify),
                            scrollable);
        }
    }

  if (v_adj != priv->v_adjustment)
    {
      if (priv->v_adjustment)
        {
          g_signal_handlers_disconnect_by_func (priv->v_adjustment,
                                                adjustment_value_notify,
                                                scrollable);
          g_object_unref (priv->v_adjustment);
          priv->v_adjustment = NULL;
        }

      if (v_adj)
        {
          priv->v_adjustment = g_object_ref (v_adj);
          g_signal_connect (priv->v_adjustment, "notify::value",
                            G_CALLBACK (adjustment_value_notify),
                            scrollable);
        }
    }
}

static void
hd_launcher_grid_get_adjustments (MxScrollable  *scrollable,
                                  MxAdjustment **h_adj,
                                  MxAdjustment **v_adj)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID (scrollable)->priv;

  if (h_adj)
    {
      if (priv->h_adjustment)
        *h_adj = priv->h_adjustment;
      else
        {
          MxAdjustment *adjustment;

          adjustment = mx_adjustment_new_with_values (0, 0, 0, 0, 0, 0);
          hd_launcher_grid_set_adjustments (scrollable,
                                            adjustment,
                                            priv->v_adjustment);
          g_object_unref (adjustment);
          hd_launcher_grid_refresh_h_adjustment (HD_LAUNCHER_GRID (scrollable));

          *h_adj = adjustment;
        }
    }

  if (v_adj)
    {
      if (priv->v_adjustment)
        *v_adj = priv->v_adjustment;
      else
        {
          MxAdjustment *adjustment;

          adjustment = mx_adjustment_new_with_values (0, 0, 0, 0, 0, 0);
          hd_launcher_grid_set_adjustments (scrollable,
                                            priv->h_adjustment,
                                            adjustment);
          g_object_unref (adjustment);
          hd_launcher_grid_refresh_v_adjustment (HD_LAUNCHER_GRID (scrollable));

          *v_adj = adjustment;
        }
    }
}

static void
mx_scrollable_iface_init (MxScrollableIface *iface)
{
  iface->set_adjustments = hd_launcher_grid_set_adjustments;
  iface->get_adjustments = hd_launcher_grid_get_adjustments;
}

static void
hd_launcher_grid_actor_added (ClutterContainer *container,
                              ClutterActor     *actor)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID (container)->priv;

  g_object_ref (actor);

  if (HD_IS_LAUNCHER_TILE(actor))
    {
      priv->tiles = g_list_append (priv->tiles, g_object_ref(actor));

      /* relayout moved to the traversal code */
    }

  g_object_unref (actor);
}

static void
hd_launcher_grid_actor_removed (ClutterContainer *container,
                                ClutterActor     *actor)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID (container)->priv;

  g_object_ref (actor);

  if (HD_IS_LAUNCHER_TILE(actor))
    {
      priv->tiles = g_list_remove (priv->tiles, actor);
      g_object_unref(actor);

      /* relayout moved to the traversal code */
    }

  g_object_unref (actor);
}

static void
_hd_launcher_grid_count_children_and_rows (HdLauncherGrid *grid,
                                           guint *children,
                                           guint *rows)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID_GET_PRIVATE (grid);
  GList *l;

  *children = 0;
  for (l = priv->tiles; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;

      if (CLUTTER_ACTOR_IS_VISIBLE (child))
        {
          ++(*children);
        }
    }

  if (*children > 0)
    *rows = (*children / HD_LAUNCHER_GRID_MAX_COLUMNS) +
            (*children % HD_LAUNCHER_GRID_MAX_COLUMNS? 1 : 0);
  else
    *rows = 0;
}


/**
 * Allocates a number of tiles in a row, starting at cur_y.
 * Returns the number of children allocated.
 */
static GList *
_hd_launcher_grid_layout_row   (GList *l,
                                guint *remaining,
                                guint cur_y,
                                guint h_spacing)
{
  ClutterActor *child;
  guint allocated = MIN (HD_LAUNCHER_GRID_MAX_COLUMNS, *remaining);
  /* Figure out the starting X position needed to centre the icons */
  guint icons_width = HD_LAUNCHER_TILE_WIDTH * HD_LAUNCHER_GRID_MAX_COLUMNS +
                      h_spacing * (HD_LAUNCHER_GRID_MAX_COLUMNS-1);
  guint cur_x = (HD_LAUNCHER_PAGE_WIDTH - icons_width) / 2;
  /* for each icon in the row... */
  for (int i = 0; i < allocated; i++)
    {
      child = l->data;

      clutter_actor_set_position(child, cur_x, cur_y);
      cur_x += HD_LAUNCHER_TILE_WIDTH + h_spacing;

      l = l->next;
    }
  *remaining -= allocated;
  return l;
}

void hd_launcher_grid_layout (HdLauncherGrid *grid)
{
  HdLauncherGridPrivate *priv = grid->priv;
  GList *l;
  guint cur_height, n_visible_launchers, n_rows;

  /* Free our list of 'blocker' actors that we use to block mouse clicks.
   * TODO: just check we have 'nrows' worth */
  g_list_foreach(priv->blockers,
                 (GFunc)clutter_actor_destroy,
                 NULL);
  g_list_free(priv->blockers);
  priv->blockers = NULL;

  _hd_launcher_grid_count_children_and_rows (grid,
      &n_visible_launchers, &n_rows);

  cur_height = HD_LAUNCHER_PAGE_YMARGIN;

  l = priv->tiles;
  while (l) {
    /* Allocate all icons on this row */
    l = _hd_launcher_grid_layout_row(l, &n_visible_launchers,
                                       cur_height, priv->h_spacing);
    if (l)
      {
        /* If there is another row, we must create an actor that
         * goes between the two rows that will grab the clicks that
         * would have gone between them and dismissed the launcher  */
        ClutterActor *blocker = clutter_group_new();
        clutter_actor_set_name(blocker, "HdLauncherGrid::blocker");
        clutter_actor_show(blocker);
        clutter_container_add_actor(CLUTTER_CONTAINER(grid), blocker);
        clutter_actor_set_reactive(blocker, TRUE);
        g_signal_connect (blocker, "button-release-event",
                          G_CALLBACK (_hd_launcher_grid_blocker_release_cb),
                          NULL);
        clutter_actor_set_position(blocker,
                                   HD_LAUNCHER_LEFT_MARGIN,
                                   cur_height + HD_LAUNCHER_TILE_HEIGHT);
        clutter_actor_set_size(blocker,
            HD_LAUNCHER_GRID_WIDTH - (HD_LAUNCHER_LEFT_MARGIN+HD_LAUNCHER_RIGHT_MARGIN),
            priv->v_spacing);

        priv->blockers = g_list_prepend(priv->blockers, blocker);
      }
    cur_height += HD_LAUNCHER_TILE_HEIGHT + priv->v_spacing;
  }

  clutter_actor_set_size(CLUTTER_ACTOR(grid),
                         HD_LAUNCHER_PAGE_WIDTH,
                         cur_height);

  if (priv->h_adjustment)
    hd_launcher_grid_refresh_h_adjustment (grid);

  if (priv->v_adjustment)
    hd_launcher_grid_refresh_v_adjustment (grid);
}

static void
hd_launcher_grid_dispose (GObject *gobject)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID (gobject)->priv;

  g_list_foreach (priv->tiles,
                  (GFunc) clutter_actor_destroy,
                  NULL);
  g_list_free (priv->tiles);
  priv->tiles = NULL;

  g_list_free(priv->blockers);
  priv->blockers = NULL;

  G_OBJECT_CLASS (hd_launcher_grid_parent_class)->dispose (gobject);
}

static void
hd_launcher_grid_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID (gobject)->priv;

  switch (prop_id)
    {
    case PROP_H_ADJUSTMENT:
      hd_launcher_grid_set_adjustments (MX_SCROLLABLE (gobject),
                                        g_value_get_object (value),
                                        priv->v_adjustment);
      break;

    case PROP_V_ADJUSTMENT:
      hd_launcher_grid_set_adjustments (MX_SCROLLABLE (gobject),
                                        priv->h_adjustment,
                                        g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
hd_launcher_grid_get_property (GObject    *gobject,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_H_ADJUSTMENT:
      {
        MxAdjustment *adjustment = NULL;

        hd_launcher_grid_get_adjustments (MX_SCROLLABLE (gobject),
                                          &adjustment,
                                          NULL);
        g_value_set_object (value, adjustment);
      }
      break;

    case PROP_V_ADJUSTMENT:
      {
        MxAdjustment *adjustment = NULL;

        hd_launcher_grid_get_adjustments (MX_SCROLLABLE (gobject),
                                          NULL,
                                          &adjustment);
        g_value_set_object (value, adjustment);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
hd_launcher_grid_class_init (HdLauncherGridClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdLauncherGridPrivate));

  gobject_class->set_property = hd_launcher_grid_set_property;
  gobject_class->get_property = hd_launcher_grid_get_property;
  gobject_class->dispose = hd_launcher_grid_dispose;
}

static void
hd_launcher_grid_init (HdLauncherGrid *launcher)
{
  HdLauncherGridPrivate *priv;

  launcher->priv = priv = HD_LAUNCHER_GRID_GET_PRIVATE (launcher);

  priv->h_spacing = HILDON_MARGIN_DEFAULT;
  priv->v_spacing = HD_LAUNCHER_GRID_ROW_SPACING;

  clutter_actor_set_reactive (CLUTTER_ACTOR (launcher), FALSE);

  g_signal_connect(
      launcher, "actor-added", G_CALLBACK(hd_launcher_grid_actor_added), 0);
  g_signal_connect(
      launcher, "actor-removed", G_CALLBACK(hd_launcher_grid_actor_removed), 0);
}

ClutterActor *
hd_launcher_grid_new (void)
{
  return g_object_new (HD_TYPE_LAUNCHER_GRID, NULL);
}

void
hd_launcher_grid_clear (HdLauncherGrid *grid)
{
  HdLauncherGridPrivate *priv;
  GList *l;

  g_return_if_fail (HD_IS_LAUNCHER_GRID (grid));

  priv = grid->priv;

  l = priv->tiles;
  while (l)
    {
      ClutterActor *child = l->data;

      l = l->next;

      clutter_container_remove_actor (CLUTTER_CONTAINER (grid), child);
    }
}

/* Reset the grid before it is shown */
void
hd_launcher_grid_reset(HdLauncherGrid *grid, gboolean hard)
{
  HdLauncherGridPrivate *priv;
  GList *l;

  g_return_if_fail (HD_IS_LAUNCHER_GRID (grid));

  priv = grid->priv;
  l = priv->tiles;

  while (l)
    {
      HdLauncherTile *tile = l->data;

      hd_launcher_tile_reset(tile, hard);
      l = l->next;
    }
}


void
hd_launcher_grid_transition_begin(HdLauncherGrid *grid,
                                  HdLauncherPageTransition trans_type)
{
  HdLauncherGridPrivate *priv = grid->priv;

  priv->transition_depth =
      hd_transition_get_int(
                hd_launcher_page_get_transition_string(trans_type),
                "depth",
                100 /* default value */);
  priv->transition_sequenced = 0;
  if (trans_type == HD_LAUNCHER_PAGE_TRANSITION_IN ||
      trans_type == HD_LAUNCHER_PAGE_TRANSITION_IN_SUB)
    {
      priv->transition_sequenced =
            hd_transition_get_int(
                      hd_launcher_page_get_transition_string(trans_type),
                      "sequenced",
                      0);
      if (priv->transition_sequenced)
        {
          grid->priv->transition_keyframes =
            hd_transition_get_keyframes(
                      hd_launcher_page_get_transition_string(trans_type),
                      "keyframes", "0,1");
          grid->priv->transition_keyframes_label =
            hd_transition_get_keyframes(
                      hd_launcher_page_get_transition_string(trans_type),
                      "keyframes_label", "0,1");
          grid->priv->transition_keyframes_icon =
                 hd_transition_get_keyframes(
                      hd_launcher_page_get_transition_string(trans_type),
                      "keyframes_icon", "0,1");
        }

      /* Reset adjustments so the view is always back to 0,0 */
      if (priv->h_adjustment)
        mx_adjustment_set_value (priv->h_adjustment, 0);

      if (priv->v_adjustment)
        mx_adjustment_set_value (priv->v_adjustment, 0);
    }
}

void
hd_launcher_grid_transition_end(HdLauncherGrid *grid)
{
  /* Free anything we may have allocated for the transition here */
  if (grid->priv->transition_keyframes)
    {
      hd_key_frame_list_free(grid->priv->transition_keyframes);
      grid->priv->transition_keyframes = 0;
    }
  if (grid->priv->transition_keyframes_label)
    {
      hd_key_frame_list_free(grid->priv->transition_keyframes_label);
      grid->priv->transition_keyframes_label = 0;
    }
  if (grid->priv->transition_keyframes_icon)
    {
      hd_key_frame_list_free(grid->priv->transition_keyframes_icon);
      grid->priv->transition_keyframes_icon = 0;
    }
}


void
hd_launcher_grid_transition(HdLauncherGrid *grid,
                            HdLauncherPage *page,
                            HdLauncherPageTransition trans_type,
                            float amount)
{
  HdLauncherGridPrivate *priv;
  GList *l;
  ClutterVertex movement_centre = {0,0,0};

  switch (trans_type)
   {
     case HD_LAUNCHER_PAGE_TRANSITION_IN:
       movement_centre.x = 0;
       movement_centre.y = 0;
       break;
     case HD_LAUNCHER_PAGE_TRANSITION_LAUNCH:
       clutter_actor_get_size(CLUTTER_ACTOR(page),
                               &movement_centre.x, &movement_centre.y);
       break;
     default:
       break;
   }

  g_return_if_fail (HD_IS_LAUNCHER_GRID (grid));

  priv = grid->priv;

  l = priv->tiles;
  while (l)
    {
      ClutterActor *child = l->data;
      l = l->next;
      if (HD_IS_LAUNCHER_TILE(child))
      {
        HdLauncherTile *tile = HD_LAUNCHER_TILE(child);
        ClutterActor *tile_icon = 0;
        ClutterActor *tile_label = 0;
        ClutterVertex pos = {0,0,0};
        float d, dx, dy;
        float order_diff;
        float order_amt; /* amount as if ordered */
        gfloat depth;

        tile_icon = hd_launcher_tile_get_icon(tile);
        tile_label = hd_launcher_tile_get_label(tile);

        clutter_actor_get_position(CLUTTER_ACTOR(tile), &pos.x, &pos.y);
        dx = pos.x - movement_centre.x;
        dy = pos.y - movement_centre.y;
        /* We always want d to be 0 <=d <= 1 */
        d = sqrt(dx*dx + dy*dy) / 1000.0f;
        if (d>1) d=1;

        order_diff = (pos.x + pos.y) /
                      (HD_COMP_MGR_LANDSCAPE_WIDTH +
                       HD_COMP_MGR_LANDSCAPE_HEIGHT);
        if (order_diff>1) order_diff = 1;
        order_amt = amount*2 - order_diff;
        if (order_amt<0) order_amt = 0;
        if (order_amt>1) order_amt = 1;

        switch (trans_type)
          {
            case HD_LAUNCHER_PAGE_TRANSITION_IN:
            case HD_LAUNCHER_PAGE_TRANSITION_IN_SUB:
              {
                float label_amt, icon_amt;

                if (priv->transition_sequenced)
                  {
                    label_amt = hd_key_frame_interpolate(
                          priv->transition_keyframes_label, order_amt);
                    icon_amt = hd_key_frame_interpolate(
                          priv->transition_keyframes_icon, order_amt);

                    if (label_amt<0) label_amt=0;
                    if (label_amt>1) label_amt=1;
                    if (icon_amt<0) icon_amt = 0;
                    if (icon_amt>1) icon_amt = 1;
                    depth = priv->transition_depth *
                             (1 - hd_key_frame_interpolate(priv->transition_keyframes,
                                                           order_amt));
                  }
                else
                  {
                    depth = priv->transition_depth * (1 - amount);
                    label_amt = amount;
                    icon_amt = amount;
                  }

                clutter_actor_set_depth(CLUTTER_ACTOR(tile), depth);
                clutter_actor_set_opacity(CLUTTER_ACTOR(tile), 255);
                if (tile_icon)
                  clutter_actor_set_opacity(tile_icon, (int)(icon_amt*255));
                if (tile_label)
                  clutter_actor_set_opacity(tile_label, (int)(label_amt*255));
                break;
              }
            case HD_LAUNCHER_PAGE_TRANSITION_OUT:
            case HD_LAUNCHER_PAGE_TRANSITION_OUT_SUB:
              {
                depth = priv->transition_depth * amount;
                clutter_actor_set_depth(CLUTTER_ACTOR(tile), depth);
                clutter_actor_set_opacity(CLUTTER_ACTOR(tile), 255 - (int)(amount*255));
                break;
              }
            case HD_LAUNCHER_PAGE_TRANSITION_LAUNCH:
              {
                float tile_amt = amount*2 - d;
                if (tile_amt<0) tile_amt = 0;
                if (tile_amt>1) tile_amt = 1;
                depth = -priv->transition_depth * tile_amt;
                clutter_actor_set_depth(CLUTTER_ACTOR(tile), depth);
                clutter_actor_set_opacity(CLUTTER_ACTOR(tile), 255 - (int)(amount*255));
                break;
              }
            /* We do't do anything for these now because we just use blur on
             * the whole group */
            case HD_LAUNCHER_PAGE_TRANSITION_BACK:
                depth = -priv->transition_depth * amount;
                clutter_actor_set_depth(CLUTTER_ACTOR(tile), depth);
                clutter_actor_set_opacity(CLUTTER_ACTOR(tile), 255 - (int)(amount*255));
                break;
            case HD_LAUNCHER_PAGE_TRANSITION_FORWARD:
                depth = -priv->transition_depth * (1.0f-amount);
                clutter_actor_set_depth(CLUTTER_ACTOR(tile), depth);
                clutter_actor_set_opacity(CLUTTER_ACTOR(tile), (int)(amount*255));
                break;
            case HD_LAUNCHER_PAGE_TRANSITION_OUT_BACK:
                break;
          }
      }
    }
}

static gboolean
_hd_launcher_grid_blocker_release_cb (ClutterActor *actor,
                                      ClutterButtonEvent *event,
                                      gpointer *data)
{
  return TRUE;
}
