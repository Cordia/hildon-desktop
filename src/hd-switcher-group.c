/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Johan Bilien <johan.bilien@nokia.com>
 *          Tomas Frydrych <tf@o-hand.com>
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

#include "hd-switcher-group.h"

#include <matchbox/core/mb-wm.h>
#include <matchbox/core/mb-wm-object.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr-clutter.h>

#include <clutter/clutter.h>

#define CLOSE_BUTTON "close-button.png"

/* FIXME */
#define ITEM_WIDTH      800
#define ITEM_HEIGHT     480
#define PADDING         20
#define ZOOM_PADDING    50
#define HDWG_SCALE_DURATION 1000

/*
 * HDSwitcherGroup is a special ClutterGroup subclass, that implements the
 * switcher grid. It is atypical in that it's children are managed by the
 * CM and already have a parent (the CM desktop group), so effectively the
 * children are shared by two containers.
 *
 * The way we handle this is as follow:
 *
 * 1. We have a specialized _add_actor () and _remove_actor () methods; these
 *    do not insert children directly into the container, but instead store
 *    them in a list.
 *
 * 2. We implement the ClutterActor::show_all() and ::hide_all() virtuals;
 *    these methods take care of reparenting the children to our group on
 *    show_all and back to the original parent on hide_all.
 */

enum
{
  SIGNAL_ITEM_SELECTED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

enum
{
  PROP_COMP_MGR = 1,
};

typedef struct
{
  ClutterActor *actor;
  ClutterActor *close_button;
  ClutterActor *group;

  guint         click_handler;
} ChildData;

/*
 * This table contains locations for each child as function of the number
 * of children.
 *
 * The x coordinate is in *halfs* of ITEM_WIDTH, the y coordinate is in
 * *whole* ITEM_HEIGHT.
 *
 * TODO -- might need to move the coords about a bit to match the colour tiles
 *         in the UI spec.
 */
static const gint positions[11][11][2] =
/* 1 */ { { {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0} },
/* 2 */   { {  0,  0 } , { 2,  0}, { 0,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0} },
/* 3 */   { {  0,  0 } , { 2,  0}, { 4,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0} },
/* 4 */   { {  0,  0 } , { 2,  0}, { 0,  1},
            {  2,  1 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0} },
/* 5 */   { {  0,  0 } , { 2,  0}, {4,   0},
            {  1,  1 } , { 3,  1}, { 0,  0},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0} },
/* 6 */   { {  0,  0 } , { 2,  0}, { 4,  0},
            {  0,  1 } , { 2,  1}, { 4,  1},
            {  0,  0 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0} },
/* 7 */   { {  0,  0 } , { 2,  0}, { 4,  0},
            {  0,  1 } , { 2,  1}, { 4,  1},
            {  2,  2 } , { 0,  0}, { 0,  0},
            {  0,  0 } , { 0,  0} },
/* 8 */   { {  0,  0 } , { 2,  0}, { 4,  0},
            {  0,  1 } , { 2,  1}, { 4,  1},
            {  1,  2 } , { 3,  2}, { 0,  0},
            {  0,  0 } , { 0,  0} },
/* 9 */   { {  0,  0 } , { 2,  0}, { 4,  0},
            {  0,  1 } , { 2,  1}, { 4,  1},
            {  0,  2 } , { 2,  2}, { 4,  2},
            {  0,  0 } , { 0,  0} },
/* 10 */  { {  0,  0 } , { 2,  0}, { 4,  0},
            {  0,  1 } , { 2,  1}, { 4,  1},
            {  0,  2 } , { 2,  2}, { 4,  2},
            {  2,  3 } , { 0,  0} },
/* 11 */  { {  0,  0 } , { 2,  0}, { 4,  0},
            {  0,  1 } , { 2,  1}, { 4,  1},
            {  0,  2 } , { 2,  2}, { 4,  2},
            {  1,  2 } , { 5,  2} } };

struct _HdSwitcherGroupPrivate
{
  GList                        *children;

  ClutterEffectTemplate        *move_template;
  ClutterEffectTemplate        *zoom_template;

  ClutterActor                 *close_button;

  MBWMCompMgrClutter           *comp_mgr;

  /* Used in the item placement loop */
  guint                         n_items;
  guint                         i_item;
  gint                          min_x, min_y, max_x, max_y;
};

static void hd_switcher_group_class_init (HdSwitcherGroupClass *klass);
static void hd_switcher_group_init       (HdSwitcherGroup *self);
static void hd_switcher_group_dispose    (GObject *object);
static void hd_switcher_group_finalize   (GObject *object);
static void hd_switcher_group_set_property (GObject       *object,
					    guint         prop_id,
					    const GValue *value,
					    GParamSpec   *pspec);

static void hd_switcher_group_get_property (GObject      *object,
					    guint         prop_id,
					    GValue       *value,
					    GParamSpec   *pspec);

static void hd_switcher_group_place (HdSwitcherGroup *group);

static void hd_switcher_group_show_all (ClutterActor *actor);
static void hd_switcher_group_hide_all (ClutterActor *self);

static void hd_switcher_group_zoom (HdSwitcherGroup *group,
				    ClutterActor    *actor,
				    gboolean         with_effect);

static gint
find_by_actor (ChildData *data, ClutterActor *actor)
{
  return (data->actor != actor);
}

G_DEFINE_TYPE (HdSwitcherGroup, hd_switcher_group, CLUTTER_TYPE_GROUP);

static void
hd_switcher_group_class_init (HdSwitcherGroupClass *klass)
{
  GObjectClass         *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass    *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec           *pspec;

  g_type_class_add_private (klass, sizeof (HdSwitcherGroupPrivate));

  object_class->dispose      = hd_switcher_group_dispose;
  object_class->finalize     = hd_switcher_group_finalize;
  object_class->set_property = hd_switcher_group_set_property;
  object_class->get_property = hd_switcher_group_get_property;

  actor_class->show_all      = hd_switcher_group_show_all;
  actor_class->hide_all      = hd_switcher_group_hide_all;

  pspec = g_param_spec_pointer ("comp-mgr",
				"Composite Manager",
				"MBWMCompMgrClutter Object",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class,
                                   PROP_COMP_MGR,
                                   pspec);

  signals[SIGNAL_ITEM_SELECTED] =
      g_signal_new ("item-selected",
                    G_OBJECT_CLASS_TYPE (object_class),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (HdSwitcherGroupClass, item_selected),
                    NULL,
                    NULL,
                    g_cclosure_marshal_VOID__OBJECT,
                    G_TYPE_NONE,
                    1,
                    CLUTTER_TYPE_ACTOR);
}

static void
hd_switcher_group_init (HdSwitcherGroup *self)
{
  HdSwitcherGroupPrivate *priv;
  GError                 *error = NULL;

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
						   HD_TYPE_SWITCHER_GROUP,
						   HdSwitcherGroupPrivate);

  priv->move_template =
    clutter_effect_template_new_for_duration (HDWG_SCALE_DURATION,
					      CLUTTER_ALPHA_RAMP_INC);

  priv->zoom_template =
    clutter_effect_template_new_for_duration (HDWG_SCALE_DURATION,
					      CLUTTER_ALPHA_RAMP_INC);

  priv->close_button =
    clutter_texture_new_from_file (CLOSE_BUTTON, &error);

  clutter_actor_set_reactive (CLUTTER_ACTOR (self), TRUE);
}

static void
hd_switcher_group_dispose (GObject *object)
{
  G_OBJECT_CLASS (hd_switcher_group_parent_class)->dispose (object);
}

static void
hd_switcher_group_finalize (GObject *object)
{
  HdSwitcherGroupPrivate *priv = HD_SWITCHER_GROUP (object)->priv;

  if (priv->children)
    {
      g_list_free (priv->children);
      priv->children = NULL;
    }

  G_OBJECT_CLASS (hd_switcher_group_parent_class)->finalize (object);
}

static void
hd_switcher_group_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  HdSwitcherGroupPrivate *priv = HD_SWITCHER_GROUP (object)->priv;

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
hd_switcher_group_get_property (GObject      *object,
			      guint         prop_id,
			      GValue       *value,
			      GParamSpec   *pspec)
{
  HdSwitcherGroupPrivate *priv = HD_SWITCHER_GROUP (object)->priv;

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

static ChildData *
hd_switcher_group_get_child_data (HdSwitcherGroup *group, ClutterActor *actor)
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
hd_switcher_group_child_button_release (HdSwitcherGroup    *group,
					ClutterEvent     *event,
					ClutterActor     *actor)
{
  hd_switcher_group_zoom (group, actor, TRUE);

  return FALSE;
}

void
hd_switcher_group_add_actor (HdSwitcherGroup *group, ClutterActor *actor)
{
  HdSwitcherGroupPrivate *priv = HD_SWITCHER_GROUP (group)->priv;
  ChildData              *data;
  CoglHandle              handle;

  data = hd_switcher_group_get_child_data (HD_SWITCHER_GROUP (group), actor);

  if (data)
    return;

  clutter_actor_set_reactive (actor, TRUE);

  data = g_new0 (ChildData, 1);
  data->actor = actor;

  data->click_handler =
      g_signal_connect_swapped (actor, "button-release-event",
                          G_CALLBACK (hd_switcher_group_child_button_release),
			  group);

  priv->children = g_list_append (priv->children, data);

  handle =
    clutter_texture_get_cogl_texture (CLUTTER_TEXTURE (priv->close_button));

  data->close_button = clutter_texture_new ();

  clutter_texture_set_cogl_texture (CLUTTER_TEXTURE (data->close_button),
				    handle);

  clutter_actor_show (data->close_button);

  data->group = clutter_group_new ();

  clutter_container_add_actor (CLUTTER_CONTAINER (data->group),
			       data->close_button);

  clutter_container_add_actor (CLUTTER_CONTAINER (group), data->group);

  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (group)))
    {
      hd_switcher_group_place (HD_SWITCHER_GROUP (group));
      hd_switcher_group_zoom (HD_SWITCHER_GROUP (group), NULL, FALSE);
    }
}

static gboolean
hd_switcher_group_close_button_clicked (ClutterActor     *client_actor,
					ClutterEvent     *event,
					ClutterActor     *clicked_actor)
{
  MBWindowManagerClient * c;

  c = g_object_get_data (G_OBJECT (client_actor), "HD-MBWindowManagerClient");

  mb_wm_client_deliver_delete (c);

  return FALSE;
}

static void
hd_switcher_group_show_all (ClutterActor *self)
{
  HdSwitcherGroupPrivate *priv = HD_SWITCHER_GROUP (self)->priv;
  GList                  *l;
  ClutterActor           *arena;

  l = priv->children;

  while (l)
    {
      ChildData    * data = l->data;
      ClutterActor * a = data->actor;
      ClutterActor * close_button = data->close_button;
      ClutterActor * group = data->group;
      ClutterActor * parent = clutter_actor_get_parent (a);
      gint           x, y, anchor_x, anchor_y;
      guint          width, height, close_width, close_height;

      /*
       * Store the original position and parent of the actor, so we
       * we can restore it when the group hides.
       */
      clutter_actor_get_position (a, &x, &y);
      clutter_actor_get_anchor_point (a, &anchor_x, &anchor_y);

      g_object_set_data (G_OBJECT (a), "HD-original-parent", parent);
      g_object_set_data (G_OBJECT (a), "HD-original-x", GINT_TO_POINTER (x));
      g_object_set_data (G_OBJECT (a), "HD-original-y", GINT_TO_POINTER (y));
      g_object_set_data (G_OBJECT (a), "HD-original-anchor-x",
			 GINT_TO_POINTER (anchor_x));
      g_object_set_data (G_OBJECT (a), "HD-original-anchor-y",
			 GINT_TO_POINTER (anchor_y));

      clutter_actor_reparent (a, group);
      clutter_actor_set_reactive (a, TRUE);
      clutter_actor_show_all (a);

      clutter_actor_get_size (a, &width, &height);
      clutter_actor_get_size (close_button, &close_width, &close_height);

      clutter_actor_set_position (close_button, width - close_width, 0);
      clutter_actor_show (close_button);
      clutter_actor_raise_top (close_button);
      clutter_actor_set_reactive (close_button, TRUE);

      g_signal_connect_swapped (close_button, "button-release-event",
                          G_CALLBACK (hd_switcher_group_close_button_clicked),
			  a);

      clutter_actor_show_all (group);

      l = l->next;
    }

  /*
   * We have to hide the CM arena group, so that any clients we did not
   * reparent to ourselves become invisible.
   *
   * TODO -- this is temporary fix; once we know how the Home views are
   * implemented we just bring the View actor immediately below ourselves.
   */
  arena = mb_wm_comp_mgr_clutter_get_arena (priv->comp_mgr);
  clutter_actor_hide (arena);

  hd_switcher_group_place (HD_SWITCHER_GROUP (self));

  clutter_actor_set_position (self, 0, 0);

  CLUTTER_ACTOR_CLASS (hd_switcher_group_parent_class)->show_all (self);

  clutter_actor_set_scale (self, 1.0, 1.0);

  hd_switcher_group_zoom (HD_SWITCHER_GROUP (self), NULL, TRUE);
}

static void
hd_switcher_group_hide_all (ClutterActor *self)
{
  HdSwitcherGroupPrivate *priv = HD_SWITCHER_GROUP (self)->priv;
  GList                  *l;
  MBWMCompMgrClutter     *cmgr;
  ClutterActor           *arena;

  g_object_get (G_OBJECT (self), "comp-mgr", &cmgr, NULL);

  l = priv->children;

  while (l)
    {
      ChildData    * data = l->data;
      ClutterActor * a = data->actor;
      ClutterActor * orig_parent = g_object_get_data (G_OBJECT (a),
						      "HD-original-parent");
      gint x =
	GPOINTER_TO_INT (g_object_get_data (G_OBJECT (a), "HD-original-x"));
      gint y =
	GPOINTER_TO_INT (g_object_get_data (G_OBJECT (a), "HD-original-y"));
      gint anchor_x =
	GPOINTER_TO_INT (g_object_get_data (G_OBJECT (a),
					    "HD-original-anchor-x"));
      gint anchor_y =
	GPOINTER_TO_INT (g_object_get_data (G_OBJECT (a),
					    "HD-original-anchor-y"));

      clutter_actor_reparent (a, orig_parent);
      clutter_actor_set_anchor_point (a, anchor_x, anchor_y);
      clutter_actor_set_position (a, x, y);

      l = l->next;
    }

  /*
   * Make the arena visible again.
   *
   * TODO -- temporary fix; see comments in _show_all().
   */
  arena = mb_wm_comp_mgr_clutter_get_arena (priv->comp_mgr);
  clutter_actor_show (arena);

  clutter_actor_hide (self);
}

void
hd_switcher_group_remove_actor (HdSwitcherGroup *group, ClutterActor *actor)
{
  HdSwitcherGroupPrivate *priv = HD_SWITCHER_GROUP (group)->priv;
  ChildData              *data;

  data = hd_switcher_group_get_child_data (HD_SWITCHER_GROUP (group), actor);

  if (!data)
    return;

  g_signal_handler_disconnect (actor, data->click_handler);
  priv->children = g_list_remove (priv->children, data);

  /*
   * TODO -- should we reparent the child actor first ? Probably superfluous
   *         as the actor is about to be destroyed anyway.
   */
  clutter_actor_destroy (data->group);

  g_free (data);

  /*
   * If the group is visible, we need to reorganise it
   */
  if (CLUTTER_ACTOR_IS_VISIBLE (CLUTTER_ACTOR (group)))
    {
      hd_switcher_group_place (HD_SWITCHER_GROUP (group));
      hd_switcher_group_zoom (HD_SWITCHER_GROUP (group), NULL, FALSE);
    }
}

static void
place_child (ChildData *data, HdSwitcherGroup *group)
{
  HdSwitcherGroupPrivate *priv = group->priv;
  gint x, y;
  gint x_new, y_new, x_old, y_old;

  if (priv->n_items <= 11)
    {
      x = positions [priv->n_items - 1][priv->i_item][0];
      y = positions [priv->n_items - 1][priv->i_item][1];
    }
  else
    {
      x = 2 * (priv->i_item % 3) - 3;
      y = (priv->i_item / 3) - 3;
    }

  /* x is in halfs of ITEM_WIDTH, y in whole units of ITEM_HEIGHT */
  x_new = x * (ITEM_WIDTH+PADDING) / 2;
  y_new = y * (ITEM_HEIGHT+PADDING);

  clutter_actor_get_position (data->group, &x_old, &y_old);

  if (x_new != x_old || y_new != y_old)
    {
      clutter_actor_set_position (data->group, x_new, y_new);
    }

  /*
   * Make sure close button is visible and above the window actor
   */
  clutter_actor_show (data->close_button);
  clutter_actor_raise_top (data->close_button);

  priv->i_item ++;

  if (x < priv->min_x) priv->min_x = x;
  if (y < priv->min_y) priv->min_y = y;
  if (x > priv->max_x) priv->max_x = x;
  if (y > priv->max_y) priv->max_y = y;
}

static void
hd_switcher_group_place (HdSwitcherGroup *group)
{
  HdSwitcherGroupPrivate *priv = group->priv;

  priv->n_items = g_list_length (priv->children);
  priv->i_item = 0;
  priv->min_x = priv->min_y = G_MAXINT;
  priv->max_x = priv->max_y = G_MININT;

  g_list_foreach (priv->children, (GFunc)place_child, group);
}

static void
hd_switcher_group_zoom_completed (HdSwitcherGroup *group, ClutterActor *actor)
{
  g_signal_emit (group, signals[SIGNAL_ITEM_SELECTED], 0, actor);
}

static void
hd_switcher_group_zoom (HdSwitcherGroup *group,
			ClutterActor    *actor,
			gboolean         with_effect)
{
  HdSwitcherGroupPrivate *priv = group->priv;
  ClutterTimeline        *timeline;
  gdouble                 scale_x, scale_y;
  gint                    x, y;

  if (actor)
    {
      /*
       * Move the entire group so that when the effect finishes, the
       * position of the child will match that of the underlying window.
       *
       * Scale the group back to 1:1 scale.
       */
      clutter_actor_get_position (actor, &x, &y);
      x = -x;
      y = -y;

      scale_x = scale_y = 1.0;
    }
  else
    {
      /*
       * We are zooming out so that we can see all the children; the group is
       * initially completely off screen touching the top-left corner and is
       * being moved so that it's top-left corner ends up at the top-left
       * corner of the screen.
       *
       * TODO -- perhaps we should adjust the group position in the y axis,
       * rather than scale with a different factor.
       */
      guint width, height;
      gint  group_x, group_y;

      clutter_actor_get_size (CLUTTER_ACTOR (group),
			      (guint *) &group_x, (guint *) &group_y);

      clutter_actor_set_position (CLUTTER_ACTOR (group), -group_x, -group_y);

      width = ZOOM_PADDING +
	(priv->max_x - priv->min_x + 2) * (ITEM_WIDTH + PADDING) / 2;

      scale_x = (gdouble) ITEM_WIDTH  / width;

      x = (gint) ((double)(ZOOM_PADDING + PADDING) * scale_x) / 2;

      height = ZOOM_PADDING +
	(priv->max_y - priv->min_y + 1) * (ITEM_HEIGHT + PADDING);

      scale_y = (gdouble) ITEM_HEIGHT  / height;

      y = (gint) ((double)ZOOM_PADDING * scale_y) / 2;
    }

  if (with_effect)
    {
      timeline = clutter_effect_scale (priv->zoom_template,
				       CLUTTER_ACTOR (group),
				       scale_x, scale_y,
				       (ClutterEffectCompleteFunc)
				       hd_switcher_group_zoom_completed,
				       actor);

      clutter_timeline_start (timeline);


      timeline = clutter_effect_move (priv->move_template,
				      CLUTTER_ACTOR (group),
				      x, y,
				      NULL, NULL);

      clutter_timeline_start (timeline);
    }
  else
    {
      clutter_actor_set_scale (CLUTTER_ACTOR (group), scale_x, scale_y);
      clutter_actor_set_position (CLUTTER_ACTOR (group), x, y);
    }
}

gboolean
hd_switcher_group_have_children (HdSwitcherGroup * group)
{
  HdSwitcherGroupPrivate *priv = group->priv;

  if (priv->children)
    return TRUE;

  return FALSE;
}

