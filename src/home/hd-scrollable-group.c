/*
 * hd-scrollable-group.c -- #ClutterGroup + #TidyScrollable
 *
 * Synopsis: {{{
 *
 * parent = tidy_finger_scroll_new();
 * clutter_actor_set_size(parent, width, height);
 *
 * // The parent must have been sized and the area canned be reparented.
 * scrable = hd_scrollable_group_new();
 * clutter_container_add_actor(parent, scrable);
 *
 * clutter_container_add_actor(scrable, some_actor);
 * ...
 *
 * // Set the eventual size of the scrollable area.
 * // Can be changed later as children added/removed.
 * hd_scrollable_group_set_real_estate(scrable,
 *  HD_SCROLLABLE_GROUP_HORIZONTAL, total_width);
 * hd_scrollable_group_set_real_estate(scrable,
 *  HD_SCROLLABLE_GROUP_VERTICAL,   total_height);
 *
 * // @scrable will scroll automatically when the user fingers it.
 * // You can scroll it programmatically too:
 * hd_scrollable_group_scroll(scrable, HD_SCROLLABLE_GROUP_VERTICAL,
 *                            100, TRUE);
 *
 * +----------/-----------------------------------------+ \
 * |          |                              REAL ESTATE| |
 * |          |                                         | |
 * |          |                                         | |
 * |          |                                         | |
 * |   value -+                                         | |
 * |          |                                         | |
 * |          |                                         | |
 * |          |                                         | +- upper
 * |          |                                         | |
 * |          \+--------------------+  \                | |
 * |           |            VIEWPORT|  |                | |
 * |           |                    |  +- page_size     | |
 * |           |                    |  |                | |
 * |           +--------------------+  /                | |
 * |                                                    | |
 * +----------------------------------------------------+ /
 *
 * Invariants:
 * 1.         page_size <= upper
 * 2. value + page_size <= upper
 *
 * Note that if #2 is asserted then #1 is asserted too,
 * because all variables are non-negative.
 * }}}
 */

#include <math.h>
#include <string.h>

#include <clutter/clutter.h>
#include <tidy/tidy-scrollable.h>
#include <tidy/tidy-adjustment.h>

#include "hd-scrollable-group.h"

/* How many pixels may the user drag the pointer before we consider it
 * a purely scrolling motion.  Tunable. */
#define MAX_CLICK_DRIFT                     10

/* The average speed of hd_scrollable_group_scroll() in pixels per second.
 * Tunable. */
#define MANUAL_SCROLL_PPS                   200

#define HD_SCROLLABLE_GROUP_GET_PRIVATE(obj)            \
  G_TYPE_INSTANCE_GET_PRIVATE((obj),                    \
                              HD_TYPE_SCROLLABLE_GROUP, \
                              HdScrollableGroupPrivate)

/* Type definitions {{{ */
typedef struct
{
  /*
   * All of these variables are specific to a direction,
   * either horizontal or vertical.
   *
   * @self:               This is ourselves; used to find out the
   *                      object when only this context is available.
   *
   * @last_press, @last_release:
   *                      Coordinate of the last "button-press-event"
   *                      "and "button-release-event".
   * @adjustment:         The #TinyAdjustments we manage and listen to
   *                      and driven by our parent #TidyFingerScroll.
   * @can_scroll:         Are we scrollable in this direction?  ie. is
   *                      our real estate greater than our viewport?
   *
   * @manual_scroll_timeline:
   *                      The timeline driving manual scrolling
   *                      initiated by hd_scrollable_group_scroll().
   *
   * These variables are used as a means of communication between
   * hsg_scroll_viewport() and hsg_tick():
   * @manual_scroll_from: #ClutterFixed pixel number of the starting
   *                      point of manual scrolling.
   * @manual_scroll_to:   Likewise for the destination pixel.
   * @on_manual_scroll_complete, @on_manual_scroll_complete_param:
   *                      What to do when the scolling effect
   *                      completes.
   *
   * Communication area between hsg_set_real_estate() and hsg_set_new_upper():
   * @new_upper:          The new "upper" bound for @adjustment
   *                      after scrolling if that was triggered
   *                      by hd_scrollable_greoup_set_real_estate().
   */
  HdScrollableGroup         *self;
  gint                      last_press, last_release;
  gboolean                  can_scroll;
  TidyAdjustment           *adjustment;
  ClutterTimeline          *manual_scroll_timeline;
  ClutterFixed              manual_scroll_from, manual_scroll_to;
  ClutterEffectCompleteFunc on_manual_scroll_complete;
  gpointer                  on_manual_scroll_complete_param;
  ClutterFixed              new_upper;
} HdScrollableGroupDirectionInfo;

typedef struct
{
  HdScrollableGroupDirectionInfo horizontal, vertical;
} HdScrollableGroupPrivate;
/* Type definitions }}} */

/* Declare @hd_scrollable_group_parent_class here because
 * we #G_DEFINE_TYPE() late in the source code. */
static gpointer hd_scrollable_group_parent_class;

/* #GObject overrides {{{ */
static void
hd_scrollable_group_get_property (GObject * obj,
                                  guint prop_id,
                                  GValue * value,
                                  GParamSpec * pspec)
{
  HdScrollableGroupPrivate *priv = HD_SCROLLABLE_GROUP_GET_PRIVATE (obj);

  switch (prop_id)
    {
    case HD_SCROLLABLE_GROUP_HORIZONTAL:
      g_value_set_object (value, priv->horizontal.adjustment);
      break;
    case HD_SCROLLABLE_GROUP_VERTICAL:
      g_value_set_object (value, priv->vertical.adjustment);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
hd_scrollable_group_set_property (GObject * obj, guint prop_id,
                                  const GValue * value, GParamSpec * pspec)
{
  /* You don't set my properties. */
  G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
}
/* #GObject overrides }}} */

/* #ClutterActor overrides {{{ */
static void
hd_scrollable_group_paint (ClutterActor * actor)
{
  HdScrollableGroupPrivate *priv = HD_SCROLLABLE_GROUP_GET_PRIVATE (actor);
  ClutterFixed x, y;

  /*
   * Translate the position of our children behind clutter's back.
   * (#TidyFingerScroll somehow disables clutter_actor_set_position())
   * If the user didn't scroll in one of the direction, its value
   * will be zero, and the translation will be a NOP.
   */
  x = tidy_adjustment_get_valuex (priv->horizontal.adjustment);
  y = tidy_adjustment_get_valuex (priv->vertical.adjustment);

  cogl_push_matrix ();
  cogl_translatex (-x, -y, 0);
  CLUTTER_ACTOR_CLASS (hd_scrollable_group_parent_class)->paint (actor);
  cogl_pop_matrix ();
}

static void
hd_scrollable_group_pick (ClutterActor * actor, const ClutterColor * color)
{
  HdScrollableGroupPrivate *priv = HD_SCROLLABLE_GROUP_GET_PRIVATE (actor);
  ClutterFixed x, y, w, h;

  tidy_adjustment_get_valuesx (priv->horizontal.adjustment,
                               &x, NULL, &w, NULL, NULL, NULL);
  tidy_adjustment_get_valuesx (priv->vertical.adjustment,
                               &y, NULL, &h, NULL, NULL, NULL);

  /* Just like with paint(). */
  cogl_push_matrix ();
  cogl_translatex (-x, -y, 0);

  /*
   * This is almost like #ClutterActor's pick implementation
   * (which #ClutterGroup calls to pick itself), but draws a
   * rectangle as large as our #TidyAdjustment:s require.
   * This is necessary to get events when we're scrolled,
   * because someone (possible #TidyScrolledView) forces
   * its own size upon us.
   */
  if (clutter_actor_should_pick_paint (actor))
    {
      cogl_color (color);
      cogl_rectanglex (0, 0, w, h);
    }

  /* The children's paint() method will know they should pick
   * the child instead of a regular paint.  This is reasonable
   * because we don't know the right @color anyway. */
  if (CLUTTER_ACTOR_IS_VISIBLE (actor))
    CLUTTER_ACTOR_CLASS (hd_scrollable_group_parent_class)->paint (actor);

  cogl_pop_matrix ();
}
/* #ClutterActor overrides }}} */

/* #TidyScrollable implementation {{{ */
static void
hd_scrollable_group_get_adjustments (TidyScrollable * scrable,
                                     TidyAdjustment ** hadjp,
                                     TidyAdjustment ** vadjp)
{
  HdScrollableGroupPrivate *priv = HD_SCROLLABLE_GROUP_GET_PRIVATE (scrable);

  if (hadjp)
    *hadjp = priv->horizontal.adjustment;
  if (vadjp)
    *vadjp = priv->vertical.adjustment;
}

/* We don't care about set_adjustments(), you can segfault if you wish. */
/* }}} */

/* Callbacks {{{ */
/* Either @hadj's or @vadj's value has changed. */
static void
hd_scrollable_group_adjval_changed (HdScrollableGroup * self,
                                    GParamSpec * pspec,
                                    TidyAdjustment * adj)
{
  /*
   * We're not supposed to call clutter_actor_paint() directly,
   * but we can request a repaint this way.  Since inactive
   * %TidyAdjustment:s (whose "upper" value is less than their
   * "page-size") don't change their "value" they won't cause
   * redrawal.
   */
  clutter_actor_queue_redraw (CLUTTER_ACTOR (self));
}

/* We're adopted by a #TidyScrollView. */
static void
hd_scrollable_group_parent_changed (ClutterActor * actor,
                                    ClutterActor * unused)
{
  HdScrollableGroupPrivate *priv = HD_SCROLLABLE_GROUP_GET_PRIVATE (actor);
  ClutterFixed width, height;

  /*
   * Set up our #TidyAdjustment:s according to our parent's size.
   * Since we're scrolling in the parent (a kind of #TidyScrollView)
   * its size is a page for the #TidyAdjustment:s.  Also set the
   * "upper" bounds for pick() to work.  If we didn't and the user
   * doesn't set our real estate either we would not receive any
   * pointer events.
   */
  width = height = 0; /* Make coverity and kuzak happy. */
  clutter_actor_get_size (clutter_actor_get_parent (actor),
                          (guint *)&width, (guint *)&height);
  width  = CLUTTER_INT_TO_FIXED (width);
  height = CLUTTER_INT_TO_FIXED (height);
  tidy_adjustment_set_valuesx (priv->horizontal.adjustment,
                               0, 0, width, 1, 1, width);
  tidy_adjustment_set_valuesx (priv->vertical.adjustment,
                               0, 0, height, 1, 1, height);
}

/* #HdScrollableGroup's "captured-event" handler. */
static gboolean
hd_scrollable_group_clicked (ClutterActor * actor, ClutterEvent * event)
{
  HdScrollableGroupPrivate *priv = HD_SCROLLABLE_GROUP_GET_PRIVATE (actor);

  /* Remember the coordinates for hd_scrollable_group_is_clicked(). */
  if (event->type == CLUTTER_BUTTON_PRESS)
    {
      priv->horizontal.last_press = ((ClutterButtonEvent *)event)->x;
      priv->vertical.last_press   = ((ClutterButtonEvent *)event)->y;
    }
  else if (event->type == CLUTTER_BUTTON_RELEASE)
    {
      priv->horizontal.last_release = ((ClutterButtonEvent *)event)->x;
      priv->vertical.last_release   = ((ClutterButtonEvent *)event)->y;
    }

  return FALSE;
}

/* #ClutterTimeline::next-frame callback of the @manual_scroll_timeline:s. */
static gboolean
hd_scrollable_group_tick (ClutterTimeline * timeline, guint current,
                          const HdScrollableGroupDirectionInfo * dir)
{
  guint max;

  max = clutter_timeline_get_n_frames (timeline);
  if (current < max)
    {
      gdouble t, diff;

      /*
       * relative_pos(t) := path_length/2 * ((-cos(t*PI)) - (-cos(0)))
       *
       * We want our scrolling velocity to follow a sine curve,
       * for which v(0) = 0, v(half_the_path) = half_the_path
       * and v(path_length) = path_length.  To get these properties
       * we need to transform the sin() function like this:
       *
       * v(t) = path_length / 2 * sin(t * PI / path_length)
       *
       * where t is a number between [0..1] indicating how much
       * we are near completition.  To get x(t) == relative_pos(t)
       * we need to compute the definite integral of v(t) for
       * 0..t_now.
       */
      t = (gdouble) current / max;
      diff = CLUTTER_FIXED_TO_INT (
          dir->manual_scroll_to - dir->manual_scroll_from);
      diff *= (1 - cos (t * M_PI)) / 2.0;
      tidy_adjustment_set_valuex (dir->adjustment,
                                  dir->manual_scroll_from +
                                    CLUTTER_FLOAT_TO_FIXED (diff));
    }
  else
    { /* Make sure we land at @manual_scroll_to at the end. */
      tidy_adjustment_set_valuex (dir->adjustment, dir->manual_scroll_to);
      if (dir->on_manual_scroll_complete)
        dir->on_manual_scroll_complete (CLUTTER_ACTOR (dir->self),
                                        dir->on_manual_scroll_complete_param);
    }

  return TRUE;
}
/* Callbacks }}} */

/* Interface functions {{{ */
/*
 * Call it from button-release-event handlers to decide if the user
 * wanted to click on @self or just wanted to scroll it.  @self needs
 * to be reactive.
 * NOTE The other alternative is not leaving button-release-event
 * propagate if it was not a real click according to this function.
 */
gboolean
hd_scrollable_group_is_clicked (HdScrollableGroup * self)
{
  gint dx, dy;
  HdScrollableGroupPrivate *priv = HD_SCROLLABLE_GROUP_GET_PRIVATE (self);

  if (!priv->horizontal.can_scroll && !priv->vertical.can_scroll)
    return TRUE;

  /* distance(a, b) = sqrt((x_a - x_b)^2 + (y_a - y_b)^2) */
  dx = priv->horizontal.last_release - priv->horizontal.last_press;
  dy = priv->vertical.last_release - priv->vertical.last_press;
  return dx*dx + dy*dy <= MAX_CLICK_DRIFT*MAX_CLICK_DRIFT;
}

/* Returns in pixels how far the left side of the estate is from the left side
 * of the viewport.  IOW How much to scroll leftwards to see the left side. */
guint
hd_scrollable_group_get_viewport_x (HdScrollableGroup * self)
{
  HdScrollableGroupPrivate *priv = HD_SCROLLABLE_GROUP_GET_PRIVATE (self);
  return CLUTTER_FIXED_TO_INT (
      tidy_adjustment_get_valuex (priv->horizontal.adjustment));
}

/* Returns in pixels how far the top of the estate is from the top of
 * the viewport.  IOW how much to scroll upwards to see the top. */
guint
hd_scrollable_group_get_viewport_y (HdScrollableGroup * self)
{
  HdScrollableGroupPrivate *priv = HD_SCROLLABLE_GROUP_GET_PRIVATE (self);
  return CLUTTER_FIXED_TO_INT (
      tidy_adjustment_get_valuex (priv->vertical.adjustment));
}

/* Move the viewport horizontally. */
void
hd_scrollable_group_set_viewport_x (HdScrollableGroup * self, guint x)
{ /* tidy_adjustment_set_valuex() takes care of proper clamping. */
  HdScrollableGroupPrivate *priv = HD_SCROLLABLE_GROUP_GET_PRIVATE (self);
  tidy_adjustment_set_valuex (priv->horizontal.adjustment,
                              CLUTTER_INT_TO_FIXED (x));
}

/* Move the viewport vertically. */
void
hd_scrollable_group_set_viewport_y (HdScrollableGroup * self, guint y)
{
  HdScrollableGroupPrivate *priv = HD_SCROLLABLE_GROUP_GET_PRIVATE (self);
  tidy_adjustment_set_valuex (priv->vertical.adjustment,
                              CLUTTER_INT_TO_FIXED (y));
}

/*
 * If @is_relative scroll the #HdScrollableGroup in the indicated direction
 * by the given number of pixels.  Otherwise scroll to @diff.  It is possible
 * to scroll in both directions at the same time.
 *
 * TODO Once scrolling is in progress for a direction you should not attempt
 *      to scroll in the same direction until the first one completes.
 */
void
hd_scrollable_group_scroll_viewport (HdScrollableGroup * self,
                                     HdScrollableGroupDirection which,
                                     gboolean is_relative, gint diff,
                                     ClutterEffectCompleteFunc fun,
                                     gpointer funparam)
{
  HdScrollableGroupPrivate *priv = HD_SCROLLABLE_GROUP_GET_PRIVATE (self);
  HdScrollableGroupDirectionInfo *dir;
  guint fps, pps;
  ClutterFixed upper, page;

  /* We could use #TidyAdjustment's interpolation function
   * but we don't because that one does a simple linear
   * interpolation while we want something fancier. */
  dir = which == HD_SCROLLABLE_GROUP_HORIZONTAL
    ? &priv->horizontal : &priv->vertical;

  /* Can we scroll at all? */
  if (!dir->can_scroll)
    goto shortcut;

  /* Get the starting and ending point. */
  tidy_adjustment_get_valuesx (dir->adjustment, &dir->manual_scroll_from,
                               NULL, &upper, NULL, NULL, &page);
  dir->manual_scroll_to = is_relative
      ? dir->manual_scroll_from + CLUTTER_INT_TO_FIXED (diff)
      :                           CLUTTER_INT_TO_FIXED (diff);
  if (dir->manual_scroll_to > upper - page)
    /* Confine the destination within the bounds. */
    dir->manual_scroll_to = upper - page;
  if (dir->manual_scroll_from == dir->manual_scroll_to)
    goto shortcut;

  /* Don't animate if we're not visible in the first place. */
  if (!CLUTTER_ACTOR_IS_VISIBLE (self))
    {
      tidy_adjustment_set_valuex (dir->adjustment, dir->manual_scroll_to);
      goto shortcut;
    }

  /* Calculate the number of frames of the scolling effect.
   * This is a linear function of the pixels we need to move by. */
  if (!is_relative)
    diff = CLUTTER_FIXED_TO_INT (
        dir->manual_scroll_to - dir->manual_scroll_from);
  if (diff < 0)
    diff = -diff;
  fps = clutter_timeline_get_speed (dir->manual_scroll_timeline);
  pps = MANUAL_SCROLL_PPS;
  clutter_timeline_set_n_frames (dir->manual_scroll_timeline,
                                 /* Needs to last for one frame at least. */
                                 diff * fps > pps ? diff * fps / pps : 1);

  dir->on_manual_scroll_complete = fun;
  dir->on_manual_scroll_complete_param = funparam;

  /* The "new-frame" callback was set in construction time. */
  clutter_timeline_start (dir->manual_scroll_timeline);
  return;

shortcut:
  if (fun)
    fun (CLUTTER_ACTOR (self), funparam);
}

/* #ClutterEffectCompleteFunc of hd_scrollable_group_set_real_estate()
 * for hd_scrollable_group_scroll_viewport(). */
static void
hd_scrollable_group_set_new_upper (HdScrollableGroup * self,
                                   HdScrollableGroupDirectionInfo * dir)
{
  ClutterFixed current, lower, stepinc, pageinc, pagesize;

  tidy_adjustment_get_valuesx (dir->adjustment,
                               &current, &lower, NULL, &stepinc, &pageinc,
                               &pagesize);
  tidy_adjustment_set_valuesx (dir->adjustment, current, lower,
                               dir->new_upper, stepinc, pageinc, pagesize);
  dir->can_scroll = dir->new_upper > pagesize;
}

/* Tells us just how large the scrollable area is in a direction. */
void
hd_scrollable_group_set_real_estate (HdScrollableGroup * self,
                                     HdScrollableGroupDirection which,
                                     guint upper)
{
  HdScrollableGroupPrivate *priv = HD_SCROLLABLE_GROUP_GET_PRIVATE (self);
  HdScrollableGroupDirectionInfo *dir;
  ClutterFixed current, lower, stepinc, pageinc, pagesize;

  dir = which == HD_SCROLLABLE_GROUP_HORIZONTAL
    ? &priv->horizontal : &priv->vertical;
  tidy_adjustment_get_valuesx (dir->adjustment, &current, &lower, NULL,
                               &stepinc, &pageinc, &pagesize);
  upper = CLUTTER_INT_TO_FIXED (upper);

  if (upper < pagesize)
    { /* Invariant #1 violated, align with the top. */
      dir->new_upper = pagesize;
      hd_scrollable_group_scroll_viewport (self, which, FALSE, 0,
                                           (ClutterEffectCompleteFunc)
                                           hd_scrollable_group_set_new_upper,
                                           dir);
      return;
    }
  else if (upper < current + pagesize)
    { /* Invariant #2 violated, align with the bottom. */
      dir->new_upper = upper;
      hd_scrollable_group_scroll_viewport (self, which, FALSE,
                                           CLUTTER_FIXED_TO_INT (upper - pagesize),
                                           (ClutterEffectCompleteFunc)
                                           hd_scrollable_group_set_new_upper,
                                           dir);
      return;
    }

  tidy_adjustment_set_valuesx (dir->adjustment, current, lower, upper,
                               stepinc, pageinc, pagesize);
  dir->can_scroll = upper > pagesize;
}

/* ClutterActor::hide signal of one of the scrollbars
 * for tidy_scroll_view_show_scrollbar(). */
static gboolean
scrollbar_shown (ClutterActor * sbar, gpointer unused)
{ /* Simply revert the show request. */
  clutter_actor_hide (sbar);
  return FALSE;
}

/*
 * Unless @enable:d, prevents #TidyScrollView from showing one of
 * the scrollbars even if it's scrolling.  @enable:ing it reverts
 * the request.
 *
 * TODO It's not clear where this funcion belongs to.
 * TODO We could be more flexible and control from here to
 *      always show the scrollbar by setting it reactive.
 */
void
tidy_scroll_view_show_scrollbar (TidyScrollView * self,
                                 HdScrollableGroupDirection which,
                                 gboolean enable)
{
  ClutterActor *sbar;

  sbar = which == HD_SCROLLABLE_GROUP_HORIZONTAL
    ? tidy_scroll_view_get_hscroll_bar (self)
    : tidy_scroll_view_get_vscroll_bar (self);

  /* Make sure not to be g_signal_connect()ed more than once. */
  g_signal_handlers_disconnect_by_func (sbar, scrollbar_shown, NULL);
  if (!enable)
    { /* Override clutter_actor_show() requests from the signal handler. */
      clutter_actor_hide (sbar);
      g_signal_connect (sbar, "show", G_CALLBACK (scrollbar_shown), NULL);
    }
  else
    clutter_actor_show (sbar);
}
/* Interface functions }}} */

/* Constructors {{{ */
ClutterActor *
hd_scrollable_group_new (void)
{
  return g_object_new (hd_scrollable_group_get_type (), NULL);
}

static void
hd_scrollable_group_class_init (HdScrollableGroupClass * klass)
{
  GObjectClass *gobject_class;
  ClutterActorClass *actor_class;

  gobject_class = G_OBJECT_CLASS (klass);
  actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->get_property       = hd_scrollable_group_get_property;
  gobject_class->set_property       = hd_scrollable_group_set_property;
  actor_class->paint                = hd_scrollable_group_paint;
  actor_class->pick                 = hd_scrollable_group_pick;
  actor_class->parent_set           = hd_scrollable_group_parent_changed;
  actor_class->captured_event       = hd_scrollable_group_clicked;

  /* Provided for the sake of #TidyScrollable. */
  g_object_class_override_property (G_OBJECT_CLASS (klass),
                                    HD_SCROLLABLE_GROUP_HORIZONTAL,
                                    "hadjustment");
  g_object_class_override_property (G_OBJECT_CLASS (klass),
                                    HD_SCROLLABLE_GROUP_VERTICAL,
                                    "vadjustment");

  g_type_class_add_private (klass, sizeof (HdScrollableGroupPrivate));
}

/* #TidyScrollable interface initialization. */
static void
hd_scrollable_group_iface_init (TidyScrollableInterface * iface)
{
  iface->get_adjustments = hd_scrollable_group_get_adjustments;
}

static void
setup_direction (HdScrollableGroup * self,
                 HdScrollableGroupDirectionInfo * dir)
{
  dir->self = self;

  /* All numeric values are dummy, they are set up properly later.
   * For now we only allocate the resources. */
  dir->adjustment = tidy_adjustment_new (0, 0, 0, 1, 1, 0);
  g_signal_connect_swapped (dir->adjustment, "notify::value",
                            G_CALLBACK (hd_scrollable_group_adjval_changed),
                            self);

  dir->manual_scroll_timeline = clutter_timeline_new (1,
                                     clutter_get_default_frame_rate ());
  g_signal_connect (dir->manual_scroll_timeline, "new-frame",
                    G_CALLBACK (hd_scrollable_group_tick), dir);
}

static void
hd_scrollable_group_init (HdScrollableGroup * self)
{
  HdScrollableGroupPrivate *priv = HD_SCROLLABLE_GROUP_GET_PRIVATE (self);

  /* Our directions are set up equivalently. */
  setup_direction (self, &priv->horizontal);
  setup_direction (self, &priv->vertical);
}

G_DEFINE_TYPE_WITH_CODE (HdScrollableGroup, hd_scrollable_group,
                         CLUTTER_TYPE_GROUP,
                         G_IMPLEMENT_INTERFACE (TIDY_TYPE_SCROLLABLE,
                                       hd_scrollable_group_iface_init));
/* Constructors }}} */

/* vim: set foldmethod=marker: */
