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

#include <cogl/cogl.h>
#include <clutter/clutter.h>
#include <hildon/hildon-defines.h>

#include <tidy/tidy-adjustment.h>
#include <tidy/tidy-interval.h>
#include <tidy/tidy-scrollable.h>

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

  TidyAdjustment *h_adjustment;
  TidyAdjustment *v_adjustment;

  /* How far the transition will move the icons */
  gint transition_depth;
  /* Do we move the icons all together or in sequence? for launcher_in transitions */
  gboolean transition_sequenced;
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

static void clutter_container_iface_init (ClutterContainerIface   *iface);
static void tidy_scrollable_iface_init   (TidyScrollableInterface *iface);

static gboolean _hd_launcher_grid_blocker_release_cb (ClutterActor *actor,
                                        ClutterButtonEvent *event,
                                        gpointer *data);

#define HD_LAUNCHER_GRID_MAX_COLUMNS 5

G_DEFINE_TYPE_WITH_CODE (HdLauncherGrid,
                         hd_launcher_grid,
                         CLUTTER_TYPE_ACTOR,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                                clutter_container_iface_init)
                         G_IMPLEMENT_INTERFACE (TIDY_TYPE_SCROLLABLE,
                                                tidy_scrollable_iface_init));

static inline void
hd_launcher_grid_refresh_h_adjustment (HdLauncherGrid *grid)
{
  HdLauncherGridPrivate *priv = grid->priv;
  ClutterFixed width;
  ClutterUnit clip_x, clip_width;
  ClutterUnit page_width;
  width = 0;
  clip_x = 0;
  clip_width = 0;
  page_width = 0;

  if (!priv->h_adjustment)
    return;

  clutter_actor_get_sizeu (CLUTTER_ACTOR (grid), &width, NULL);
  clutter_actor_get_clipu (CLUTTER_ACTOR (grid),
                           &clip_x, NULL,
                           &clip_width, NULL);

  if (clip_width == 0)
    page_width = CLUTTER_UNITS_TO_FIXED (width);
  else
    page_width = MIN (CLUTTER_UNITS_TO_FIXED (width),
                      CLUTTER_UNITS_TO_FIXED (clip_width - clip_x));

  tidy_adjustment_set_valuesx (priv->h_adjustment,
                               tidy_adjustment_get_valuex (priv->h_adjustment),
                               0,
                               width,
                               CFX_ONE,
                               CFX_ONE * 20,
                               page_width);
}

static inline void
hd_launcher_grid_refresh_v_adjustment (HdLauncherGrid *grid)
{
  HdLauncherGridPrivate *priv = grid->priv;
  ClutterFixed height;
  ClutterUnit clip_y, clip_height;
  ClutterUnit page_height;
  height = 0;
  clip_y = 0;
  clip_height = 0;
  page_height = 0;

  if (!priv->v_adjustment)
    return;

  clutter_actor_get_sizeu (CLUTTER_ACTOR (grid), NULL, &height);
  clutter_actor_get_clipu (CLUTTER_ACTOR (grid),
                           NULL, &clip_y,
                           NULL, &clip_height);

  if (clip_height == 0)
    page_height = CLUTTER_UNITS_TO_FIXED (height);
  else
    page_height = MIN (CLUTTER_UNITS_TO_FIXED (height),
                       CLUTTER_UNITS_TO_FIXED (clip_height - clip_y));

  tidy_adjustment_set_valuesx (priv->v_adjustment,
                               tidy_adjustment_get_valuex (priv->v_adjustment),
                               0,
                               height,
                               CFX_ONE,
                               CFX_ONE * 20,
                               page_height);
}

void
hd_launcher_grid_reset_v_adjustment (HdLauncherGrid *grid)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID_GET_PRIVATE (grid);

  tidy_adjustment_set_valuex (priv->v_adjustment, 0);
}

static void
adjustment_value_notify (GObject    *gobject,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
  ClutterActor *grid = user_data;

  if (CLUTTER_ACTOR_IS_VISIBLE (grid))
    clutter_actor_queue_redraw (grid);
}

static void
hd_launcher_grid_set_adjustments (TidyScrollable *scrollable,
                                  TidyAdjustment *h_adj,
                                  TidyAdjustment *v_adj)
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
hd_launcher_grid_get_adjustments (TidyScrollable  *scrollable,
                                  TidyAdjustment **h_adj,
                                  TidyAdjustment **v_adj)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID (scrollable)->priv;

  if (h_adj)
    {
      if (priv->h_adjustment)
        *h_adj = priv->h_adjustment;
      else
        {
          TidyAdjustment *adjustment;

          adjustment = tidy_adjustment_newx (0, 0, 0, 0, 0, 0);
          hd_launcher_grid_set_adjustments (scrollable,
                                            adjustment,
                                            priv->v_adjustment);
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
          TidyAdjustment *adjustment;

          adjustment = tidy_adjustment_newx (0, 0, 0, 0, 0, 0);
          hd_launcher_grid_set_adjustments (scrollable,
                                            priv->h_adjustment,
                                            adjustment);
          hd_launcher_grid_refresh_v_adjustment (HD_LAUNCHER_GRID (scrollable));

          *v_adj = adjustment;
        }
    }
}

static void
tidy_scrollable_iface_init (TidyScrollableInterface *iface)
{
  iface->set_adjustments = hd_launcher_grid_set_adjustments;
  iface->get_adjustments = hd_launcher_grid_get_adjustments;
}

static void
hd_launcher_grid_add (ClutterContainer *container,
                      ClutterActor     *actor)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID (container)->priv;

  g_object_ref (actor);

  priv->tiles = g_list_append (priv->tiles, actor);
  clutter_actor_set_parent (actor, CLUTTER_ACTOR (container));

  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));

  if (priv->h_adjustment)
    hd_launcher_grid_refresh_h_adjustment (HD_LAUNCHER_GRID (container));

  if (priv->v_adjustment)
    hd_launcher_grid_refresh_v_adjustment (HD_LAUNCHER_GRID (container));

  g_signal_emit_by_name (container, "actor-added", actor);

  g_object_unref (actor);
}

static void
hd_launcher_grid_remove (ClutterContainer *container,
                         ClutterActor     *actor)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID (container)->priv;

  g_object_ref (actor);

  priv->tiles = g_list_remove (priv->tiles, actor);
  clutter_actor_unparent (actor);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));

  if (priv->h_adjustment)
    hd_launcher_grid_refresh_h_adjustment (HD_LAUNCHER_GRID (container));

  if (priv->v_adjustment)
    hd_launcher_grid_refresh_v_adjustment (HD_LAUNCHER_GRID (container));

  g_signal_emit_by_name (container, "actor-removed", actor);

  g_object_unref (actor);
}

static void
hd_launcher_grid_foreach (ClutterContainer *container,
                          ClutterCallback   callback,
                          gpointer          callback_data)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID (container)->priv;
  GList *l;

  for (l = priv->tiles; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;

      callback (child, callback_data);
    }
}

static void
clutter_container_iface_init (ClutterContainerIface *iface)
{
  iface->add = hd_launcher_grid_add;
  iface->remove = hd_launcher_grid_remove;
  iface->foreach = hd_launcher_grid_foreach;
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

static void
hd_launcher_grid_get_preferred_height (ClutterActor *actor,
                                       ClutterUnit   for_width,
                                       ClutterUnit  *min_height_p,
                                       ClutterUnit  *natural_height_p)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID_GET_PRIVATE (actor);
  guint n_visible_launchers, n_rows, natural_height;

  if (min_height_p)
    *min_height_p = CLUTTER_UNITS_FROM_DEVICE (HD_LAUNCHER_GRID_MIN_HEIGHT);

  if (natural_height_p)
    {
      _hd_launcher_grid_count_children_and_rows (HD_LAUNCHER_GRID (actor),
          &n_visible_launchers, &n_rows);

        natural_height = HD_LAUNCHER_PAGE_YMARGIN +
                         HD_LAUNCHER_TILE_HEIGHT * n_rows +
                         (n_rows > 0 ? priv->v_spacing * (n_rows - 1) : 0);
        *natural_height_p = CLUTTER_UNITS_FROM_DEVICE (MAX (natural_height,
                                HD_LAUNCHER_GRID_MIN_HEIGHT));
    }
}

/**
 * Allocates a number of tiles in a row, starting at cur_y.
 * Returns the number of children allocated.
 */
static GList *
_hd_launcher_grid_allocate_row (GList *l,
                                guint *remaining,
                                guint cur_y,
                                guint h_spacing,
                                gboolean origin_changed)
{
  ClutterActor *child;
  ClutterActorBox box;
  guint allocated = MIN (HD_LAUNCHER_GRID_MAX_COLUMNS, *remaining);
  /* Figure out the starting X position needed to centre the icons */
  guint icons_width = HD_LAUNCHER_TILE_WIDTH * HD_LAUNCHER_GRID_MAX_COLUMNS +
                      h_spacing * (HD_LAUNCHER_GRID_MAX_COLUMNS-1);
  guint cur_x = (HD_LAUNCHER_PAGE_WIDTH - icons_width) / 2;
  /* for each icon in the row... */
  for (int i = 0; i < allocated; i++)
    {
      child = l->data;

      box.x1 = CLUTTER_UNITS_FROM_DEVICE (cur_x);
      box.y1 = CLUTTER_UNITS_FROM_DEVICE (cur_y);
      cur_x += HD_LAUNCHER_TILE_WIDTH;
      box.x2 = CLUTTER_UNITS_FROM_DEVICE (cur_x);
      box.y2 = CLUTTER_UNITS_FROM_DEVICE (cur_y + HD_LAUNCHER_TILE_HEIGHT);
      clutter_actor_allocate (child, &box, origin_changed);

      cur_x += h_spacing;
      l = l->next;
    }
  *remaining -= allocated;
  return l;
}

static void
hd_launcher_grid_allocate (ClutterActor          *actor,
                           const ClutterActorBox *box,
                           gboolean               origin_changed)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID (actor)->priv;
  ClutterActorClass *parent_class;
  GList *l;
  guint cur_height, n_visible_launchers, n_rows;

  /* Free our list of 'blocker' actors that we use to block mouse clicks */
  g_list_foreach(priv->blockers,
                 (GFunc) clutter_actor_destroy,
                 NULL);
  g_list_free(priv->blockers);
  priv->blockers = NULL;

  /* chain up to save the allocation */
  parent_class = CLUTTER_ACTOR_CLASS (hd_launcher_grid_parent_class);
  parent_class->allocate (actor, box, origin_changed);

  _hd_launcher_grid_count_children_and_rows (HD_LAUNCHER_GRID (actor),
      &n_visible_launchers, &n_rows);

  cur_height = HD_LAUNCHER_PAGE_YMARGIN;
  l = priv->tiles;
  while (l) {
    /* Allocate all icons on this row */
    l = _hd_launcher_grid_allocate_row(l, &n_visible_launchers,
                                       cur_height, priv->h_spacing,
                                       origin_changed);
    if (l)
      {
        /* If there is another row, we must create an actor that
         * goes between the two rows that will grab the clicks that
         * would have gone between them and dismissed the launcher  */
        ClutterActorBox box;
        ClutterActor *blocker = clutter_group_new();
        clutter_actor_set_name(blocker, "HdLauncherGrid::blocker");
        clutter_actor_show(blocker);
        clutter_actor_set_parent (blocker, actor);
        clutter_actor_set_reactive(blocker, TRUE);
        g_signal_connect (blocker, "button-release-event",
                          G_CALLBACK (_hd_launcher_grid_blocker_release_cb),
                          NULL);
        box.x1 = CLUTTER_UNITS_FROM_INT(HD_LAUNCHER_LEFT_MARGIN);
        box.y1 = CLUTTER_UNITS_FROM_INT(cur_height + HD_LAUNCHER_TILE_HEIGHT);
        box.x2 = CLUTTER_UNITS_FROM_INT(
            HD_LAUNCHER_GRID_WIDTH - HD_LAUNCHER_RIGHT_MARGIN);
        box.y2 = CLUTTER_UNITS_FROM_INT(cur_height +
            HD_LAUNCHER_TILE_HEIGHT + priv->v_spacing);
        clutter_actor_allocate(blocker, &box, origin_changed);

        priv->blockers = g_list_prepend(priv->blockers, blocker);
      }
    cur_height += HD_LAUNCHER_TILE_HEIGHT + priv->v_spacing;
  }
}

static void
hd_launcher_grid_paint (ClutterActor *actor)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID (actor)->priv;
  GList *l;

  if (!CLUTTER_ACTOR_IS_VISIBLE (actor))
    return;

  cogl_push_matrix ();

  /* offset by the adjustment value */
  if (priv->v_adjustment)
    {
      ClutterFixed v_offset = tidy_adjustment_get_valuex (priv->v_adjustment);

      cogl_translatex (0, v_offset * -1, 0);
    }

  for (l = priv->tiles; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;

      if (CLUTTER_ACTOR_IS_VISIBLE (child))
        clutter_actor_paint (child);
    }

  cogl_pop_matrix ();
}

static void
hd_launcher_grid_pick (ClutterActor       *actor,
                       const ClutterColor *pick_color)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID (actor)->priv;
  GList *l;

  CLUTTER_ACTOR_CLASS (hd_launcher_grid_parent_class)->pick (actor, pick_color);

  cogl_push_matrix ();

  /* offset by the adjustment value */
  if (priv->v_adjustment)
    {
      ClutterFixed v_offset = tidy_adjustment_get_valuex (priv->v_adjustment);

      cogl_translatex (0, v_offset * -1, 0);
    }

  for (l = priv->tiles; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;

      if (CLUTTER_ACTOR_IS_VISIBLE (child))
        clutter_actor_paint (child);
    }
  /* render blocking areas to stop clicks through to the background */
  for (l = priv->blockers; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;

      if (CLUTTER_ACTOR_IS_VISIBLE (child))
        clutter_actor_paint (child);
    }

  cogl_pop_matrix ();
}

static void
hd_launcher_grid_realize (ClutterActor *actor)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID (actor)->priv;

  g_list_foreach (priv->tiles,
                  (GFunc) clutter_actor_realize,
                  NULL);

  CLUTTER_ACTOR_SET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
}

static void
hd_launcher_grid_unrealize (ClutterActor *actor)
{
  HdLauncherGridPrivate *priv = HD_LAUNCHER_GRID (actor)->priv;

  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);

  g_list_foreach (priv->tiles,
                  (GFunc) clutter_actor_unrealize,
                  NULL);
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

  g_list_foreach(priv->blockers,
                 (GFunc) clutter_actor_destroy,
                 NULL);
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
      hd_launcher_grid_set_adjustments (TIDY_SCROLLABLE (gobject),
                                        g_value_get_object (value),
                                        priv->v_adjustment);
      break;

    case PROP_V_ADJUSTMENT:
      hd_launcher_grid_set_adjustments (TIDY_SCROLLABLE (gobject),
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
        TidyAdjustment *adjustment = NULL;

        hd_launcher_grid_get_adjustments (TIDY_SCROLLABLE (gobject),
                                          &adjustment,
                                          NULL);
        g_value_set_object (value, adjustment);
      }
      break;

    case PROP_V_ADJUSTMENT:
      {
        TidyAdjustment *adjustment = NULL;

        hd_launcher_grid_get_adjustments (TIDY_SCROLLABLE (gobject),
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
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdLauncherGridPrivate));

  gobject_class->set_property = hd_launcher_grid_set_property;
  gobject_class->get_property = hd_launcher_grid_get_property;
  gobject_class->dispose = hd_launcher_grid_dispose;

  actor_class->get_preferred_height = hd_launcher_grid_get_preferred_height;
  actor_class->allocate = hd_launcher_grid_allocate;
  actor_class->realize = hd_launcher_grid_realize;
  actor_class->unrealize = hd_launcher_grid_unrealize;
  actor_class->paint = hd_launcher_grid_paint;
  actor_class->pick = hd_launcher_grid_pick;

  g_object_class_override_property (gobject_class,
                                    PROP_H_ADJUSTMENT,
                                    "hadjustment");

  g_object_class_override_property (gobject_class,
                                    PROP_V_ADJUSTMENT,
                                    "vadjustment");
}

static void
hd_launcher_grid_init (HdLauncherGrid *launcher)
{
  HdLauncherGridPrivate *priv;

  launcher->priv = priv = HD_LAUNCHER_GRID_GET_PRIVATE (launcher);

  priv->h_spacing = HILDON_MARGIN_DEFAULT;
  priv->v_spacing = HD_LAUNCHER_GRID_ROW_SPACING;

  clutter_actor_set_reactive (CLUTTER_ACTOR (launcher), FALSE);
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

static float sexy_overshoot(float x)
{
  float smooth_ramp, converge;
  smooth_ramp = 1.0f - cos(x*3.141592);
  converge = sin(0.5*3.141592*(1-x));
  return (smooth_ramp*1.1)*converge + (1-converge);
}

/* Reset the grid before it is shown */
void
hd_launcher_grid_reset(HdLauncherGrid *grid)
{
  HdLauncherGridPrivate *priv;
  GList *l;

  g_return_if_fail (HD_IS_LAUNCHER_GRID (grid));

  priv = grid->priv;
  l = priv->tiles;

  while (l)
    {
      HdLauncherTile *tile = l->data;

      hd_launcher_tile_reset(tile);
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
    }
}

void
hd_launcher_grid_transition_end(HdLauncherGrid *grid)
{
  /* Free anything we may have allocated for the transition here */
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
       clutter_actor_get_sizeu(CLUTTER_ACTOR(page),
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
        ClutterUnit depth;

        tile_icon = hd_launcher_tile_get_icon(tile);
        tile_label = hd_launcher_tile_get_label(tile);

        clutter_actor_get_positionu(CLUTTER_ACTOR(tile), &pos.x, &pos.y);
        dx = CLUTTER_UNITS_TO_FLOAT(pos.x - movement_centre.x);
        dy = CLUTTER_UNITS_TO_FLOAT(pos.y - movement_centre.y);
        /* We always want d to be 0 <=d <= 1 */
        d = sqrt(dx*dx + dy*dy) / 1000.0f;
        if (d>1) d=1;

        order_diff = (CLUTTER_UNITS_TO_FLOAT(pos.x) +
                     (CLUTTER_UNITS_TO_FLOAT(pos.y))) /
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
                float label_amt, tile_amt;

                if (priv->transition_sequenced)
                  {
                    /* Do tile movement and icon fading diagonally */
                    label_amt = (((order_amt*26) - 16) / 10);
                    tile_amt = ((order_amt*26) / 16);

                    if (tile_amt<0) tile_amt = 0;
                    if (tile_amt>1) tile_amt = 1;
                    if (label_amt<0) label_amt=0;
                    if (label_amt>1) label_amt=1;
                    depth = CLUTTER_UNITS_FROM_FLOAT(
                      priv->transition_depth * (1 - sexy_overshoot(tile_amt)));
                  }
                else
                  {
                    depth = CLUTTER_UNITS_FROM_FLOAT(
                                        priv->transition_depth * (1 - amount));
                    label_amt = amount;
                    tile_amt = amount;
                  }

                clutter_actor_set_depthu(CLUTTER_ACTOR(tile), depth);
                clutter_actor_set_opacity(CLUTTER_ACTOR(tile), 255);
                if (tile_icon)
                  clutter_actor_set_opacity(tile_icon, (int)(tile_amt*255));
                if (tile_label)
                  clutter_actor_set_opacity(tile_label, (int)(label_amt*255));
                break;
              }
            case HD_LAUNCHER_PAGE_TRANSITION_OUT:
            case HD_LAUNCHER_PAGE_TRANSITION_OUT_SUB:
              {
                depth = CLUTTER_UNITS_FROM_FLOAT(priv->transition_depth*amount);
                clutter_actor_set_depthu(CLUTTER_ACTOR(tile), depth);
                clutter_actor_set_opacity(CLUTTER_ACTOR(tile), 255 - (int)(amount*255));
                break;
              }
            case HD_LAUNCHER_PAGE_TRANSITION_LAUNCH:
              {
                float tile_amt = amount*2 - d;
                if (tile_amt<0) tile_amt = 0;
                if (tile_amt>1) tile_amt = 1;
                depth = CLUTTER_UNITS_FROM_FLOAT(-priv->transition_depth*tile_amt);
                clutter_actor_set_depthu(CLUTTER_ACTOR(tile), depth);
                clutter_actor_set_opacity(CLUTTER_ACTOR(tile), 255 - (int)(amount*255));
                break;
              }
            /* We do't do anything for these now because we just use blur on
             * the whole group */
            case HD_LAUNCHER_PAGE_TRANSITION_BACK:
                depth = CLUTTER_UNITS_FROM_FLOAT(-priv->transition_depth*amount);
                clutter_actor_set_depthu(CLUTTER_ACTOR(tile), depth);
                clutter_actor_set_opacity(CLUTTER_ACTOR(tile), 255 - (int)(amount*255));
                break;
            case HD_LAUNCHER_PAGE_TRANSITION_FORWARD:
                depth = CLUTTER_UNITS_FROM_FLOAT(-priv->transition_depth*(1-amount));
                clutter_actor_set_depthu(CLUTTER_ACTOR(tile), depth);
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
