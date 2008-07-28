#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cogl/cogl.h>
#include <clutter/clutter.h>

#include "tidy/tidy-adjustment.h"
#include "tidy/tidy-scrollable.h"

#include "hd-launcher-item.h"
#include "hd-task-launcher.h"

#define I_(str) (g_intern_static_string ((str)))

#define HD_TASK_LAUNCHER_GET_PRIVATE(obj)       (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HD_TYPE_TASK_LAUNCHER, HdTaskLauncherPrivate))

struct _HdTaskLauncherPrivate
{
  /* list of actors */
  GList *launchers;
  GList *top_level;

  HdLauncherPadding padding;

  ClutterUnit h_spacing;
  ClutterUnit v_spacing;

  TidyAdjustment *h_adjustment;
  TidyAdjustment *v_adjustment;
};

enum
{
  PROP_0,

  PROP_H_ADJUSTMENT,
  PROP_V_ADJUSTMENT
};

enum
{
  ITEM_CLICKED,

  LAST_SIGNAL
};

static void clutter_container_iface_init (ClutterContainerIface   *iface);
static void tidy_scrollable_iface_init   (TidyScrollableInterface *iface);

static guint task_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE_WITH_CODE (HdTaskLauncher,
                         hd_task_launcher,
                         CLUTTER_TYPE_ACTOR,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                                clutter_container_iface_init)
                         G_IMPLEMENT_INTERFACE (TIDY_TYPE_SCROLLABLE,
                                                tidy_scrollable_iface_init));

static void
hd_task_launcher_add (ClutterContainer *container,
                      ClutterActor     *actor)
{
  HdTaskLauncherPrivate *priv = HD_TASK_LAUNCHER (container)->priv;

  g_object_ref (actor);

  priv->launchers = g_list_append (priv->launchers, actor);
  clutter_actor_set_parent (actor, CLUTTER_ACTOR (container));

  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));

  g_signal_emit_by_name (container, "actor-added", actor);

  g_object_unref (actor);
}

static void
hd_task_launcher_remove (ClutterContainer *container,
                         ClutterActor     *actor)
{
  HdTaskLauncherPrivate *priv = HD_TASK_LAUNCHER (container)->priv;

  g_object_ref (actor);

  priv->launchers = g_list_remove (priv->launchers, actor);
  clutter_actor_unparent (actor);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (container));

  g_signal_emit_by_name (container, "actor-removed", actor);

  g_object_unref (actor);
}

static void
hd_task_launcher_foreach (ClutterContainer *container,
                          ClutterCallback   callback,
                          gpointer          callback_data)
{
  HdTaskLauncherPrivate *priv = HD_TASK_LAUNCHER (container)->priv;
  GList *l;

  for (l = priv->launchers; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;

      callback (child, callback_data);
    }
}

static void
clutter_container_iface_init (ClutterContainerIface *iface)
{
  iface->add = hd_task_launcher_add;
  iface->remove = hd_task_launcher_remove;
  iface->foreach = hd_task_launcher_foreach;
}

static void
hd_task_launcher_refresh_adjustment (HdTaskLauncher *launcher)
{
  HdTaskLauncherPrivate *priv = launcher->priv;
  ClutterFixed width, height, page_width, page_height;
  ClutterUnit clip_x, clip_y, clip_width, clip_height;

  clutter_actor_get_sizeu (CLUTTER_ACTOR (launcher), &width, &height);
  clutter_actor_get_clipu (CLUTTER_ACTOR (launcher),
                           &clip_x, &clip_y,
                           &clip_width, &clip_height);

  if (clip_width == 0)
    page_width = CLUTTER_UNITS_TO_FIXED (width);
  else
    page_width = MIN (CLUTTER_UNITS_TO_FIXED (width),
                      CLUTTER_UNITS_TO_FIXED (clip_width - clip_x));

  if (clip_height == 0)
    page_height = CLUTTER_UNITS_TO_FIXED (height);
  else
    page_height = MIN (CLUTTER_UNITS_TO_FIXED (height),
                       CLUTTER_UNITS_TO_FIXED (clip_height - clip_y));

  if (priv->h_adjustment)
    tidy_adjustment_set_valuesx (priv->h_adjustment,
                                 tidy_adjustment_get_valuex (priv->h_adjustment),
                                 0,
                                 width,
                                 CFX_ONE,
                                 CFX_ONE * 20,
                                 page_width);

  if (priv->v_adjustment)
    tidy_adjustment_set_valuesx (priv->v_adjustment,
                                 tidy_adjustment_get_valuex (priv->v_adjustment),
                                 0,
                                 height,
                                 CFX_ONE,
                                 CFX_ONE * 20,
                                 page_height);
}

static void
adjustment_value_notify (GObject    *gobject,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
  ClutterActor *launcher = user_data;

  clutter_actor_queue_redraw (launcher);
}

static void
hd_task_launcher_set_adjustments (TidyScrollable *scrollable,
                                  TidyAdjustment *h_adj,
                                  TidyAdjustment *v_adj)
{
  HdTaskLauncherPrivate *priv = HD_TASK_LAUNCHER (scrollable)->priv;

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
hd_task_launcher_get_adjustments (TidyScrollable  *scrollable,
                                  TidyAdjustment **h_adj,
                                  TidyAdjustment **v_adj)
{
  HdTaskLauncherPrivate *priv = HD_TASK_LAUNCHER (scrollable)->priv;

  if (h_adj)
    {
      if (priv->h_adjustment)
        *h_adj = priv->h_adjustment;
      else
        {
          TidyAdjustment *adjustment;

          adjustment = tidy_adjustment_newx (0, 0, 0, 0, 0, 0);
          hd_task_launcher_set_adjustments (scrollable,
                                            adjustment,
                                            priv->v_adjustment);
          hd_task_launcher_refresh_adjustment (HD_TASK_LAUNCHER (scrollable));

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
          hd_task_launcher_set_adjustments (scrollable,
                                            priv->h_adjustment,
                                            adjustment);
          hd_task_launcher_refresh_adjustment (HD_TASK_LAUNCHER (scrollable));

          *v_adj = adjustment;
        }
    }
}

static void
tidy_scrollable_iface_init (TidyScrollableInterface *iface)
{
  iface->set_adjustments = hd_task_launcher_set_adjustments;
  iface->get_adjustments = hd_task_launcher_get_adjustments;
}

static void
hd_task_launcher_get_preferred_height (ClutterActor *actor,
                                       ClutterUnit   for_width,
                                       ClutterUnit  *min_height_p,
                                       ClutterUnit  *natural_height_p)
{
  HdTaskLauncherPrivate *priv = HD_TASK_LAUNCHER (actor)->priv;
  ClutterUnit max_item_width, max_item_height;
  ClutterUnit max_row_width, max_row_height;
  ClutterUnit cur_width, cur_height;
  gint n_visible_launchers;
  GList *l;

  cur_width  = priv->padding.left;
  cur_height = priv->padding.top;

  /* the amount of available space on a row depends on the
   * number of visible launchers; in this first pass we check
   * how many visible launchers there are and what is the
   * maximum amount of width a launcher can get.
   */
  n_visible_launchers = 0;
  max_item_width = max_item_height = 0;
  for (l = priv->launchers; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;

      if (CLUTTER_ACTOR_IS_VISIBLE (child))
        {
          ClutterUnit natural_width, natural_height;

          n_visible_launchers += 1;

          natural_width = natural_height = 0;
          clutter_actor_get_preferred_size (child,
                                            NULL, NULL,
                                            &natural_width,
                                            &natural_height);

          max_item_width  = MAX (max_item_width, natural_width);
          max_item_height = MAX (max_item_height, natural_height);
        }
    }

  max_row_width = for_width
                - priv->padding.left
                - priv->padding.right;

  max_row_height = 0;

  /* in this second pass we allocate the launchers */
  for (l = priv->launchers; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      ClutterUnit natural_height;

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      /* we need to query the new height for the items smaller than
       * the maximum width, in order to get an updated value
       */
      natural_height = 0;
      clutter_actor_get_preferred_height (child, max_item_width,
                                          NULL,
                                          &natural_height);

      /* if it fits in the current row, keep it there; otherwise,
       * reflow into another row
       */
      if ((cur_width + max_item_width + priv->h_spacing) > max_row_width)
        {
          cur_height     += max_row_height + priv->v_spacing;
          cur_width       = priv->padding.left;
          max_row_height  = 0;
        }
      else
        {
          if (l != priv->launchers)
            cur_width += max_item_width + priv->h_spacing;
        }

      max_row_height = MAX (max_row_height, natural_height);
    }

  if (min_height_p)
    *min_height_p = 0;

  if (natural_height_p)
    *natural_height_p = cur_height + max_row_height;
}

static void
hd_task_launcher_allocate (ClutterActor          *actor,
                           const ClutterActorBox *box,
                           gboolean               origin_changed)
{
  HdTaskLauncherPrivate *priv = HD_TASK_LAUNCHER (actor)->priv;
  ClutterActorClass *parent_class;
  GList *l;
  ClutterUnit cur_width, cur_height;
  ClutterUnit max_row_width, max_row_height;
  ClutterUnit max_item_width, max_item_height;
  gint n_visible_launchers;

  /* chain up to save the allocation */
  parent_class = CLUTTER_ACTOR_CLASS (hd_task_launcher_parent_class);
  parent_class->allocate (actor, box, origin_changed);

  cur_width  = priv->padding.left;
  cur_height = priv->padding.top;

  /* the amount of available space on a row depends on the
   * number of visible launchers; in this first pass we check
   * how many visible launchers there are and what is the
   * maximum amount of width a launcher can get.
   */
  n_visible_launchers = 0;
  max_item_width = max_item_height = 0;
  for (l = priv->launchers; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;

      if (CLUTTER_ACTOR_IS_VISIBLE (child))
        {
          ClutterUnit natural_width, natural_height;

          n_visible_launchers += 1;

          natural_width = natural_height = 0;
          clutter_actor_get_preferred_size (child,
                                            NULL, NULL,
                                            &natural_width,
                                            &natural_height);

          max_item_width  = MAX (max_item_width, natural_width);
          max_item_height = MAX (max_item_height, natural_height);
        }
    }

  max_row_width = box->x2 - box->x1
                - priv->padding.left
                - priv->padding.right;

  max_row_height = 0;

  /* in this second pass we allocate the launchers */
  for (l = priv->launchers; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;
      ClutterUnit natural_height;
      ClutterActorBox child_box = { 0, };

      if (!CLUTTER_ACTOR_IS_VISIBLE (child))
        continue;

      /* we need to query the new height for the items smaller than
       * the maximum width, in order to get an updated value
       */
      natural_height = 0;
      clutter_actor_get_preferred_height (child, max_item_width,
                                          NULL,
                                          &natural_height);

      /* if it fits in the current row, keep it there; otherwise,
       * reflow into another row
       */
      if ((cur_width + max_item_width + priv->h_spacing) > max_row_width)
        {
          cur_height     += max_row_height + priv->v_spacing;
          cur_width       = priv->padding.left;
          max_row_height  = 0;
        }
      else
        {
          if (l != priv->launchers)
            cur_width += max_item_width + priv->h_spacing;
        }

      max_row_height = MAX (max_row_height, natural_height);

      child_box.x1 = cur_width;
      child_box.y1 = cur_height;
      child_box.x2 = child_box.x1 + max_item_width;
      child_box.y2 = child_box.y1 + natural_height;

      clutter_actor_allocate (child, &child_box, origin_changed);
    }
}

static void
hd_task_launcher_paint (ClutterActor *actor)
{
  HdTaskLauncherPrivate *priv = HD_TASK_LAUNCHER (actor)->priv;
  GList *l;

  cogl_push_matrix ();

  if (priv->v_adjustment)
    {
      ClutterFixed v_offset = tidy_adjustment_get_valuex (priv->v_adjustment);

      cogl_translatex (0, v_offset * -1, 0);
    }

  for (l = priv->launchers; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;

      if (CLUTTER_ACTOR_IS_VISIBLE (child))
        clutter_actor_paint (child);
    }

  cogl_pop_matrix ();
}

static void
hd_task_launcher_pick (ClutterActor       *actor,
                       const ClutterColor *pick_color)
{
  HdTaskLauncherPrivate *priv = HD_TASK_LAUNCHER (actor)->priv;
  GList *l;

  CLUTTER_ACTOR_CLASS (hd_task_launcher_parent_class)->pick (actor, pick_color);

  cogl_push_matrix ();

  for (l = priv->launchers; l != NULL; l = l->next)
    {
      ClutterActor *child = l->data;

      if (CLUTTER_ACTOR_IS_VISIBLE (child))
        clutter_actor_paint (child);
    }

  cogl_pop_matrix ();
}

static void
hd_task_launcher_realize (ClutterActor *actor)
{
  HdTaskLauncherPrivate *priv = HD_TASK_LAUNCHER (actor)->priv;

  g_list_foreach (priv->launchers,
                  (GFunc) clutter_actor_realize,
                  NULL);

  CLUTTER_ACTOR_SET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
}

static void
hd_task_launcher_unrealize (ClutterActor *actor)
{
  HdTaskLauncherPrivate *priv = HD_TASK_LAUNCHER (actor)->priv;

  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);

  g_list_foreach (priv->launchers,
                  (GFunc) clutter_actor_unrealize,
                  NULL);
}

static void
hd_task_launcher_dispose (GObject *gobject)
{
  HdTaskLauncherPrivate *priv = HD_TASK_LAUNCHER (gobject)->priv;

  g_list_foreach (priv->launchers,
                  (GFunc) clutter_actor_destroy,
                  NULL);
  g_list_free (priv->launchers);

  G_OBJECT_CLASS (hd_task_launcher_parent_class)->dispose (gobject);
}

static void
hd_task_launcher_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  HdTaskLauncherPrivate *priv = HD_TASK_LAUNCHER (gobject)->priv;

  switch (prop_id)
    {
    case PROP_H_ADJUSTMENT:
      hd_task_launcher_set_adjustments (TIDY_SCROLLABLE (gobject),
                                        g_value_get_object (value),
                                        priv->v_adjustment);
      break;

    case PROP_V_ADJUSTMENT:
      hd_task_launcher_set_adjustments (TIDY_SCROLLABLE (gobject),
                                        priv->h_adjustment,
                                        g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
hd_task_launcher_get_property (GObject    *gobject,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_H_ADJUSTMENT:
      {
        TidyAdjustment *adjustment = NULL;

        hd_task_launcher_get_adjustments (TIDY_SCROLLABLE (gobject),
                                          &adjustment,
                                          NULL);
        g_value_set_object (value, adjustment);
      }
      break;

    case PROP_V_ADJUSTMENT:
      {
        TidyAdjustment *adjustment = NULL;

        hd_task_launcher_get_adjustments (TIDY_SCROLLABLE (gobject),
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
hd_task_launcher_class_init (HdTaskLauncherClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdTaskLauncherPrivate));

  gobject_class->set_property = hd_task_launcher_set_property;
  gobject_class->get_property = hd_task_launcher_get_property;
  gobject_class->dispose = hd_task_launcher_dispose;

  actor_class->get_preferred_height = hd_task_launcher_get_preferred_height;
  actor_class->allocate = hd_task_launcher_allocate;
  actor_class->realize = hd_task_launcher_realize;
  actor_class->unrealize = hd_task_launcher_unrealize;
  actor_class->paint = hd_task_launcher_paint;
  actor_class->pick = hd_task_launcher_pick;

  g_object_class_override_property (gobject_class,
                                    PROP_H_ADJUSTMENT,
                                    "hadjustment");

  g_object_class_override_property (gobject_class,
                                    PROP_V_ADJUSTMENT,
                                    "vadjustment");

  task_signals[ITEM_CLICKED] =
    g_signal_new (I_("item-clicked"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (HdTaskLauncherClass, item_clicked),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  HD_TYPE_LAUNCHER_ITEM);
}

static void
hd_task_launcher_init (HdTaskLauncher *launcher)
{
  HdTaskLauncherPrivate *priv;

  launcher->priv = priv = HD_TASK_LAUNCHER_GET_PRIVATE (launcher);

  priv->padding.top = priv->padding.bottom = CLUTTER_UNITS_FROM_DEVICE (12);
  priv->padding.left = priv->padding.right = CLUTTER_UNITS_FROM_DEVICE (12);

  priv->h_spacing = priv->v_spacing = CLUTTER_UNITS_FROM_DEVICE (6);

  clutter_actor_set_reactive (CLUTTER_ACTOR (launcher), TRUE);
}

ClutterActor *
hd_task_launcher_new (void)
{
  return g_object_new (HD_TYPE_TASK_LAUNCHER, NULL);
}

static void
on_item_clicked (HdLauncherItem *item,
                 HdTaskLauncher *launcher)
{
  g_signal_emit (launcher, task_signals[ITEM_CLICKED], 0, item);
}

void
hd_task_launcher_add_item (HdTaskLauncher *launcher,
                           HdLauncherItem *item)
{
  g_return_if_fail (HD_IS_TASK_LAUNCHER (launcher));
  g_return_if_fail (HD_IS_LAUNCHER_ITEM (item));

  clutter_container_add_actor (CLUTTER_CONTAINER (launcher),
                               CLUTTER_ACTOR (item));
  g_signal_connect (item,
                    "clicked", G_CALLBACK (on_item_clicked),
                    launcher);
}

void
hd_task_launcher_clear (HdTaskLauncher *launcher)
{
  HdTaskLauncherPrivate *priv;
  GList *l;

  g_return_if_fail (HD_IS_TASK_LAUNCHER (launcher));

  priv = launcher->priv;

  l = priv->launchers;
  while (l)
    {
      ClutterActor *child = l->data;
      
      l = l->next;

      clutter_container_remove_actor (CLUTTER_CONTAINER (launcher), child);
    }
}
