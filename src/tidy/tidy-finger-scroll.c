/* tidy-finger-scroll.c: Finger scrolling container actor
 *
 * Copyright (C) 2008 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Written by: Chris Lord <chris@openedhand.com>
 */

#include "tidy-finger-scroll.h"
#include "tidy-enum-types.h"
#include "tidy-marshal.h"
#include "tidy-scroll-bar.h"
#include "tidy-scrollable.h"
#include "tidy-scroll-view.h"
#include <clutter/clutter.h>
#include <math.h>

#define TIDY_FINGER_SCROLL_INITIAL_SCROLLBAR_DELAY (2000)
#define TIDY_FINGER_SCROLL_FADE_SCROLLBAR_IN_TIME (250)
#define TIDY_FINGER_SCROLL_FADE_SCROLLBAR_OUT_TIME (500)
#define TIDY_FINGER_SCROLL_DRAG_TRASHOLD (25)

G_DEFINE_TYPE (TidyFingerScroll, tidy_finger_scroll, TIDY_TYPE_SCROLL_VIEW)

#define FINGER_SCROLL_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                  TIDY_TYPE_FINGER_SCROLL, \
                                  TidyFingerScrollPrivate))

typedef struct {
  /* Units to store the origin of a click when scrolling */
  ClutterUnit x;
  ClutterUnit y;
  GTimeVal    time;
} TidyFingerScrollMotion;

struct _TidyFingerScrollPrivate
{
  /* Scroll mode */
  TidyFingerScrollMode mode;

  /* @move tells whether TIDY_FINGER_SCROLL_DRAG_TRASHOLD has been exceeded.
   * @first_x and @first_y are the coordinates of the first touch. */
  gboolean               move;
  ClutterFixed           first_x, first_y;

  GArray                *motion_buffer;
  guint                  last_motion;

  /* Variables for storing acceleration information for kinetic mode */
  ClutterTimeline       *deceleration_timeline;
  int                    deceleration_timeline_lastframe;
  ClutterUnit            dx;
  ClutterUnit            dy;
  ClutterFixed           decel_rate;
  ClutterFixed           bouncing_decel_rate;
  ClutterFixed           bounce_back_speed_rate;

  /* Variables to fade in/out scroll-bars */
  ClutterEffectTemplate *template_in;
  ClutterEffectTemplate *template_out;
  ClutterTimeline       *hscroll_timeline;
  ClutterTimeline       *vscroll_timeline;

  /* Timeout for removing scrollbars after they first appear */
  guint                  scrollbar_timeout;

  /* We have to call clutter_set_capture_motion_events, so we need
   * this to be able to restore the state afterwards */
  gboolean               old_capture_motion_events;
};

enum {
  PROP_MODE = 1,
  PROP_BUFFER,
};

static gboolean captured_event_cb (ClutterActor *actor, ClutterEvent *event);
static void _tidy_finger_scroll_hide_scrollbars_later (TidyFingerScroll *scroll);

static void
tidy_finger_scroll_get_property (GObject *object, guint property_id,
                                 GValue *value, GParamSpec *pspec)
{
  TidyFingerScrollPrivate *priv = TIDY_FINGER_SCROLL (object)->priv;

  switch (property_id)
    {
    case PROP_MODE :
      g_value_set_enum (value, priv->mode);
      break;
    case PROP_BUFFER :
      g_value_set_uint (value, priv->motion_buffer->len);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
tidy_finger_scroll_set_property (GObject *object, guint property_id,
                                 const GValue *value, GParamSpec *pspec)
{
  TidyFingerScrollPrivate *priv = TIDY_FINGER_SCROLL (object)->priv;

  switch (property_id)
    {
    case PROP_MODE :
      priv->mode = g_value_get_enum (value);
      g_object_notify (object, "mode");
      break;
    case PROP_BUFFER :
      g_array_set_size (priv->motion_buffer, g_value_get_uint (value));
      g_object_notify (object, "motion-buffer");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
tidy_finger_scroll_dispose (GObject *object)
{
  TidyFingerScrollPrivate *priv = TIDY_FINGER_SCROLL (object)->priv;

  if (priv->scrollbar_timeout)
    {
      g_source_remove(priv->scrollbar_timeout);
      priv->scrollbar_timeout = 0;
    }

  if (priv->deceleration_timeline)
    {
      clutter_timeline_stop (priv->deceleration_timeline);
      g_object_unref (priv->deceleration_timeline);
      priv->deceleration_timeline = NULL;
    }

  if (priv->hscroll_timeline)
    {
      /* FIXME clutter_effect_ requires to get the ClutterTimeline::completed signal to not leak */
      g_signal_emit_by_name (priv->hscroll_timeline, "completed");
      priv->hscroll_timeline = NULL;
    }

  if (priv->vscroll_timeline)
    {
      /* FIXME clutter_effect_ requires to get the ClutterTimeline::completed signal to not leak */
      g_signal_emit_by_name (priv->vscroll_timeline, "completed");
      priv->vscroll_timeline = NULL;
    }

  if (priv->template_in)
    {
      g_object_unref (priv->template_in);
      priv->template_in = NULL;
    }
  if (priv->template_out)
    {
      g_object_unref (priv->template_out);
      priv->template_out = NULL;
    }

  G_OBJECT_CLASS (tidy_finger_scroll_parent_class)->dispose (object);
}

static void
tidy_finger_scroll_class_init (TidyFingerScrollClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TidyFingerScrollPrivate));

  object_class->get_property = tidy_finger_scroll_get_property;
  object_class->set_property = tidy_finger_scroll_set_property;
  object_class->dispose = tidy_finger_scroll_dispose;
  CLUTTER_ACTOR_CLASS (klass)->captured_event = captured_event_cb;

  g_object_class_install_property (object_class,
                                   PROP_MODE,
                                   g_param_spec_enum ("mode",
                                                      "TidyFingerScrollMode",
                                                      "Scrolling mode",
                                                      TIDY_TYPE_FINGER_SCROLL_MODE,
                                                      TIDY_FINGER_SCROLL_MODE_PUSH,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_BUFFER,
                                   g_param_spec_uint ("motion-buffer",
                                                      "Motion buffer",
                                                      "Amount of motion "
                                                      "events to buffer",
                                                      1, G_MAXUINT, 3,
                                                      G_PARAM_READWRITE));
}

static gboolean
motion_event_cb (ClutterActor *actor,
                 ClutterMotionEvent *event,
                 TidyFingerScroll *scroll)
{
  ClutterUnit x, y;

  TidyFingerScrollPrivate *priv = scroll->priv;

  if (clutter_actor_transform_stage_point (actor,
                                           CLUTTER_UNITS_FROM_DEVICE(event->x),
                                           CLUTTER_UNITS_FROM_DEVICE(event->y),
                                           &x, &y))
    {
      TidyFingerScrollMotion *motion;
      ClutterActor *child =
        tidy_scroll_view_get_child (TIDY_SCROLL_VIEW(scroll));

      if (child)
        {
          ClutterFixed dx, dy;
          TidyAdjustment *hadjust, *vadjust;

          tidy_scrollable_get_adjustments (TIDY_SCROLLABLE (child),
                                           &hadjust,
                                           &vadjust);

          motion = &g_array_index (priv->motion_buffer,
                                   TidyFingerScrollMotion, priv->last_motion);
          dx = CLUTTER_UNITS_TO_FIXED(motion->x - x) +
               tidy_adjustment_get_valuex (hadjust);
          dy = CLUTTER_UNITS_TO_FIXED(motion->y - y) +
               tidy_adjustment_get_valuex (vadjust);

          /* Has the drag treshold been reached? */
          if (!priv->move)
            {
                ClutterFixed d1 = x - priv->first_x;
                ClutterFixed d2 = y - priv->first_y;
                priv->move = clutter_qmulx (d1, d1) + clutter_qmulx (d2, d2)
                  >= CLUTTER_INT_TO_FIXED (TIDY_FINGER_SCROLL_DRAG_TRASHOLD
                                           * TIDY_FINGER_SCROLL_DRAG_TRASHOLD);
            }

          /* If not, do everything as if it had (we already did) except
           * for adjusting @child's position. */
          if (priv->move)
            {
              tidy_adjustment_set_valuex (hadjust, dx);
              tidy_adjustment_set_valuex (vadjust, dy);
              clutter_actor_queue_redraw( actor );
            }
        }

      priv->last_motion ++;
      if (priv->last_motion == priv->motion_buffer->len)
        {
          priv->motion_buffer = g_array_remove_index (priv->motion_buffer, 0);
          g_array_set_size (priv->motion_buffer, priv->last_motion);
          priv->last_motion --;
        }

      motion = &g_array_index (priv->motion_buffer,
                               TidyFingerScrollMotion, priv->last_motion);
      motion->x = x;
      motion->y = y;
      g_get_current_time (&motion->time);
    }

  return FALSE;
}

static void
hfade_complete_cb (ClutterActor *scrollbar, TidyFingerScroll *scroll)
{
  scroll->priv->hscroll_timeline = NULL;
}

static void
vfade_complete_cb (ClutterActor *scrollbar, TidyFingerScroll *scroll)
{
  scroll->priv->vscroll_timeline = NULL;
}

static void
stop_scrollbars (TidyFingerScroll *scroll)
{
  TidyFingerScrollPrivate *priv = scroll->priv;

  /* Stop current timelines */
  if (priv->scrollbar_timeout)
    {
      g_source_remove(priv->scrollbar_timeout);
      priv->scrollbar_timeout = 0;
    }
  if (priv->hscroll_timeline)
    {
      /* FIXME clutter_effect_ requires to get the ClutterTimeline::completed signal to not leak */
      g_signal_emit_by_name (priv->hscroll_timeline, "completed");
      priv->hscroll_timeline = NULL;
    }

  if (priv->vscroll_timeline)
    {
      /* FIXME clutter_effect_ requires to get the ClutterTimeline::completed signal to not leak */
      g_signal_emit_by_name (priv->vscroll_timeline, "completed");
      priv->vscroll_timeline = NULL;
    }
}

static void
show_scrollbars (TidyFingerScroll *scroll, gboolean show)
{
  ClutterActor *hscroll, *vscroll;
  TidyFingerScrollPrivate *priv = scroll->priv;

  stop_scrollbars (scroll);
  hscroll = tidy_scroll_view_get_hscroll_bar (TIDY_SCROLL_VIEW (scroll));
  vscroll = tidy_scroll_view_get_vscroll_bar (TIDY_SCROLL_VIEW (scroll));

  /* Create new ones */
  if (!CLUTTER_ACTOR_IS_REACTIVE (hscroll))
    priv->hscroll_timeline = clutter_effect_fade (
                               show ? priv->template_in : priv->template_out,
                               hscroll,
                               show ? 0xFF : 0x00,
                               (ClutterEffectCompleteFunc)hfade_complete_cb,
                               scroll);

  if (!CLUTTER_ACTOR_IS_REACTIVE (vscroll))
    priv->vscroll_timeline = clutter_effect_fade (
                               show ? priv->template_in : priv->template_out,
                               vscroll,
                               show ? 0xFF : 0x00,
                               (ClutterEffectCompleteFunc)vfade_complete_cb,
                               scroll);
}

static void
deceleration_completed_cb (ClutterTimeline *timeline,
                           TidyFingerScroll *scroll)
{
  _tidy_finger_scroll_hide_scrollbars_later (scroll);
  g_object_unref (timeline);
  scroll->priv->deceleration_timeline = NULL;
}

/*
 * Returns the next priv->dx or dy.  If we're scrolling normally
 * (lower <= value <= upper) it's decelerated by decel_rate.
 * If we're in the danger zone (below lower or above upper)
 * it's decelerated by bouncing_decel_rate.  If we're boucing back
 * (we hit lowest or highest or we stopped in the danger zone)
 * We return the initial bouncing speed, which is based on how
 * far we are in the danger zone (the distance from lower/upper).
 */
static ClutterFixed
get_next_delta (TidyFingerScroll *scroll,
                ClutterFixed lowest, ClutterFixed lower,
                ClutterFixed value, ClutterFixed prevdiff,
                ClutterFixed upper, ClutterFixed highest)
{
  TidyFingerScrollPrivate *priv = scroll->priv;
  ClutterFixed decel_rate, nextdiff;
  gboolean in_upper_danger_zone, in_lower_danger_zone;

  in_lower_danger_zone = value < lower;
  in_upper_danger_zone = value > upper;
  decel_rate = in_lower_danger_zone || in_upper_danger_zone
    ? priv->bouncing_decel_rate : priv->decel_rate;
  nextdiff = clutter_qmulx (prevdiff, decel_rate);
  if (prevdiff && decel_rate && prevdiff == nextdiff)
    /* Arithmetic underflow. */
    nextdiff = prevdiff < 0 ? -CFX_ONE : CFX_ONE;

  if (in_lower_danger_zone)
    {
      if (value+nextdiff <= lowest || (-CFX_ONE < nextdiff && nextdiff <= 0))
        {
          nextdiff = clutter_qmulx(lower-value, priv->bounce_back_speed_rate);
          if (!nextdiff)
            nextdiff = CFX_ONE;
        }
    }
  else if (in_upper_danger_zone)
    {
      if (value+nextdiff >= highest || (0 <= nextdiff && nextdiff < CFX_ONE))
        {
          nextdiff = clutter_qmulx(upper-value, priv->bounce_back_speed_rate);
          if (!nextdiff)
            nextdiff = -CFX_ONE;
        }
    }

  return nextdiff;
}

/* Advances *@valuep by @diff and returns how much it actually advanced. */
static ClutterFixed
advance_value (ClutterFixed lowest, ClutterFixed lower,
               ClutterFixed *valuep, ClutterFixed diff,
               ClutterFixed upper, ClutterFixed highest)
{
  static const gboolean likeable = FALSE;

  *valuep += diff;
  if (!likeable)
    {
      if (*valuep < lowest)
        { /* Bounce back from the top. */
          diff -= *valuep - lowest;
          *valuep = lowest;
        }
      else if (*valuep < lower && 0 < diff && diff < CFX_ONE)
        { /* Leaving the top danger zone, snap it to the boundary. */
          diff = 0;
          *valuep = lower;
        }
      else if (*valuep > upper && -CFX_ONE < diff && diff < 0)
        { /* Same for the bottom danger zone. */
          diff = 0;
          *valuep = upper;
        }
      else if (*valuep > highest)
        { /* Bounce back from the bottom. */
          diff -= *valuep - highest;
          *valuep = highest;
        }
    }
  return diff;
}

/*
 * Callback of an indefinite timeline which scrolls @child.
 * When we began scrolling in button_release_event_cb() its
 * length is calculated so that it finishes when our speed
 * drops under 1px/frame under normal scrolling conditions.
 * However, this doesn't account for bouncing, so we decide
 * when to stop eventually, and extend our lifetime if necessary.
 */
static void
deceleration_new_frame_cb (ClutterTimeline *timeline,
                           gint frame_num,
                           TidyFingerScroll *scroll)
{
  TidyFingerScrollPrivate *priv = scroll->priv;
  ClutterActor *child;
  TidyAdjustment *hadjust, *vadjust;
  ClutterFixed hvalue, hlowest, hlower, hpage, hupper, hhighest;
  ClutterFixed vvalue, vlowest, vlower, vpage, vupper, vhighest;

  if (!(child = tidy_scroll_view_get_child (TIDY_SCROLL_VIEW(scroll))))
    return;
  tidy_scrollable_get_adjustments (TIDY_SCROLLABLE (child),
                                   &hadjust, &vadjust);

  tidy_adjustment_get_skirtx (hadjust, &hlowest, &hhighest);
  tidy_adjustment_get_valuesx (hadjust, &hvalue, &hlower, &hupper,
                               NULL, NULL, &hpage);
  hupper -= hpage;

  tidy_adjustment_get_skirtx (vadjust, &vlowest, &vhighest);
  tidy_adjustment_get_valuesx (vadjust, &vvalue, &vlower, &vupper,
                               NULL, NULL, &vpage);
  vupper -= vpage;

  /* We need to keep track of how many frames were skipped so we can
   * make up for it... */
  for (; priv->deceleration_timeline_lastframe < frame_num;
       priv->deceleration_timeline_lastframe++)
    {
      priv->dx = advance_value (
                  hlowest, hlower, &hvalue, priv->dx, hupper, hhighest);
      priv->dx = get_next_delta (scroll,
                   hlowest, hlower, hvalue, priv->dx, hupper, hhighest);
      priv->dy = advance_value (
                  vlowest, vlower, &vvalue, priv->dy, vupper, vhighest);
      priv->dy = get_next_delta (scroll,
                   vlowest, vlower, vvalue, priv->dy, vupper, vhighest);
    }
  tidy_adjustment_set_valuex (hadjust, hvalue);
  tidy_adjustment_set_valuex (vadjust, vvalue);

  /* Stop the timeline if we don't move anymore,
   * or extend it if we're running out of frames. */
  if (   vlower <= vvalue && vvalue <= vupper
      && -CFX_ONE < priv->dy && priv->dy < CFX_ONE
      && hlower <= hvalue && hvalue <= hupper
      && -CFX_ONE < priv->dx && priv->dx < CFX_ONE)
    /* Not in danger zone and not moving. */
    deceleration_completed_cb (timeline, scroll);
  else if (clutter_timeline_get_n_frames (timeline) < frame_num+60)
    /* Extend our lifetime. */
    clutter_timeline_set_n_frames (timeline, frame_num+60);
}

/*
 * Figure out the initial speed when we start decelerating.  @diff is
 * the initial estimate which this function may override if the dhild
 * widget's position is overshot.  Specifically:
 *
 * ========================== top of view ==========================
 *
 *
 *                                 ^
 *   user dragged the widget below | and pushed with some initial
 *                        the view | momentum upwards
 * ------------------------- top of widget -------------------------
 *
 * This case if the initial momentum was not enough to bring the widget
 * to the top of the view, not in any number of frames we discard user's
 * momentum and return something with which the widget will reach the top
 * in half a second.  Similarly for the bottom of the widget.  Otherwise
 * the @diff is returned as is.
 */
static ClutterFixed
initial_speed (TidyFingerScroll *scroll, TidyAdjustment *adjust,
               ClutterFixed diff)
{
  TidyFingerScrollPrivate *priv = scroll->priv;
  ClutterFixed lower, value, upper, page;

  tidy_adjustment_get_valuesx (adjust, &value, &lower, &upper,
                               NULL, NULL, &page);
  upper -= page;

  if (diff > 0 && value < lower
      && value + clutter_qmulx (diff, 1-priv->bouncing_decel_rate) < lower)
    return clutter_qmulx(lower-value, priv->bounce_back_speed_rate);
  else if (diff < 0 && value > upper
      && value + clutter_qmulx (diff, 1-priv->bouncing_decel_rate) > upper)
    return clutter_qmulx(upper-value, priv->bounce_back_speed_rate);
  else
    return diff;
}

static gboolean
button_release_event_cb (ClutterActor *actor,
                         ClutterButtonEvent *event,
                         TidyFingerScroll *scroll)
{
  TidyFingerScrollPrivate *priv = scroll->priv;
  ClutterActor *child = tidy_scroll_view_get_child (TIDY_SCROLL_VIEW(scroll));
  gboolean decelerating = FALSE;

  if (event->button != 1)
    return FALSE;

  g_signal_handlers_disconnect_by_func (actor,
                                        motion_event_cb,
                                        scroll);
  g_signal_handlers_disconnect_by_func (actor,
                                        button_release_event_cb,
                                        scroll);

  clutter_ungrab_pointer ();
  clutter_set_motion_events_enabled(priv->old_capture_motion_events);

  if ((priv->mode == TIDY_FINGER_SCROLL_MODE_KINETIC) && (child))
    {
      ClutterUnit x, y;

      if (clutter_actor_transform_stage_point (actor,
                                               CLUTTER_UNITS_FROM_DEVICE(event->x),
                                               CLUTTER_UNITS_FROM_DEVICE(event->y),
                                               &x, &y))
        {
          ClutterUnit frac, x_origin, y_origin;
          GTimeVal release_time, motion_time;
          TidyAdjustment *hadjust, *vadjust;
          glong time_diff;
          gint i;

          /* Get time delta */
          g_get_current_time (&release_time);

          /* Get average position/time of last x mouse events */
          priv->last_motion ++;
          x_origin = y_origin = 0;
          motion_time = (GTimeVal){ 0, 0 };
          for (i = 0; i < priv->last_motion; i++)
            {
              TidyFingerScrollMotion *motion =
                &g_array_index (priv->motion_buffer, TidyFingerScrollMotion, i);

              /* FIXME: This doesn't guard against overflows - Should
               *        either fix that, or calculate the correct maximum
               *        value for the buffer size
               */
              x_origin += motion->x;
              y_origin += motion->y;
              motion_time.tv_sec += motion->time.tv_sec;
              motion_time.tv_usec += motion->time.tv_usec;
            }
          x_origin = CLUTTER_UNITS_FROM_FIXED (
            clutter_qdivx (CLUTTER_UNITS_TO_FIXED (x_origin),
                           CLUTTER_INT_TO_FIXED (priv->last_motion)));
          y_origin = CLUTTER_UNITS_FROM_FIXED (
            clutter_qdivx (CLUTTER_UNITS_TO_FIXED (y_origin),
                           CLUTTER_INT_TO_FIXED (priv->last_motion)));
          motion_time.tv_sec /= priv->last_motion;
          motion_time.tv_usec /= priv->last_motion;

          if (motion_time.tv_sec == release_time.tv_sec)
            time_diff = release_time.tv_usec - motion_time.tv_usec;
          else
            time_diff = release_time.tv_usec +
                        (G_USEC_PER_SEC - motion_time.tv_usec);

          /* Work out the fraction of 1/60th of a second that has elapsed */
          frac = clutter_qdivx (CLUTTER_FLOAT_TO_FIXED (time_diff/1000.0),
                                CLUTTER_FLOAT_TO_FIXED (1000.0/60.0));

          /* See how many units to move in 1/60th of a second */
          priv->dx = CLUTTER_UNITS_FROM_FIXED(clutter_qdivx (
                     CLUTTER_UNITS_TO_FIXED(x_origin - x), frac));
          priv->dy = CLUTTER_UNITS_FROM_FIXED(clutter_qdivx (
                     CLUTTER_UNITS_TO_FIXED(y_origin - y), frac));

          /* Get adjustments to do step-increment snapping */
          tidy_scrollable_get_adjustments (TIDY_SCROLLABLE (child),
                                           &hadjust,
                                           &vadjust);

          /* Possibly adjust the initial speed if we're overdragged. */
          priv->dx = initial_speed (scroll, hadjust, priv->dx);
          priv->dy = initial_speed (scroll, vadjust, priv->dy);

          if (ABS(CLUTTER_UNITS_TO_INT(priv->dx)) > 1 ||
              ABS(CLUTTER_UNITS_TO_INT(priv->dy)) > 1)
            {
              gdouble value, lower, step_increment, d, a, x, y, n;

              /* TODO: Convert this all to fixed point? */

              /* We want n, where x / y^n < z,
               * x = Distance to move per frame
               * y = Deceleration rate
               * z = maximum distance from target
               *
               * Rearrange to n = log (x / z) / log (y)
               * To simplify, z = 1, so n = log (x) / log (y)
               *
               * As z = 1, this will cause stops to be slightly abrupt -
               * add a constant 15 frames to compensate.
               */
              x = CLUTTER_FIXED_TO_FLOAT (MAX(ABS(priv->dx), ABS(priv->dy)));
              y = CLUTTER_FIXED_TO_FLOAT (priv->decel_rate);
              n = logf (x) * logf (y) + 15.0;

              /* Now we have n, adjust dx/dy so that we finish on a step
               * boundary.
               *
               * Distance moved, using the above variable names:
               *
               * d = x + x/y + x/y^2 + ... + x/y^n
               *
               * Using geometric series,
               *
               * d = (1 - 1/y^(n+1))/(1 - 1/y)*x
               *
               * Let a = (1 - 1/y^(n+1))/(1 - 1/y),
               *
               * d = a * x
               *
               * Find d and find its nearest page boundary, then solve for x
               *
               * x = d / a
               */

              /* Get adjustments, work out y^n */
              a = (1.0 - pow (y, n + 1)) / (1.0 - y);

              /* Solving for dx */
              d = a * CLUTTER_UNITS_TO_FLOAT (priv->dx);
              tidy_adjustment_get_values (hadjust, &value, &lower, NULL,
                                          &step_increment, NULL, NULL);
              d = ((rint (((value + d) - lower) / step_increment) *
                    step_increment) + lower) - value;
              priv->dx = CLUTTER_UNITS_FROM_FLOAT (d / a);

              /* Solving for dy */
              d = a * CLUTTER_UNITS_TO_FLOAT (priv->dy);
              tidy_adjustment_get_values (vadjust, &value, &lower, NULL,
                                          &step_increment, NULL, NULL);
              d = ((rint (((value + d) - lower) / step_increment) *
                    step_increment) + lower) - value;
              priv->dy = CLUTTER_UNITS_FROM_FLOAT (d / a);

              priv->deceleration_timeline = clutter_timeline_new ((gint)n, 60);
            }
          else
            {
              gdouble value, lower, step_increment, d, a, y;

              /* Start a short effects timeline to snap to the nearest step
               * boundary (see equations above)
               */
              y = CLUTTER_FIXED_TO_FLOAT (priv->decel_rate);
              a = (1.0 - pow (y, 4 + 1)) / (1.0 - y);

              tidy_adjustment_get_values (hadjust, &value, &lower, NULL,
                                          &step_increment, NULL, NULL);
              d = ((rint ((value - lower) / step_increment) *
                    step_increment) + lower) - value;
              priv->dx = CLUTTER_UNITS_FROM_FLOAT (d / a);

              tidy_adjustment_get_values (vadjust, &value, &lower, NULL,
                                          &step_increment, NULL, NULL);
              d = ((rint ((value - lower) / step_increment) *
                    step_increment) + lower) - value;
              priv->dy = CLUTTER_UNITS_FROM_FLOAT (d / a);

              priv->deceleration_timeline = clutter_timeline_new (4, 60);
            }

          g_signal_connect (priv->deceleration_timeline, "new_frame",
                            G_CALLBACK (deceleration_new_frame_cb), scroll);
          g_signal_connect (priv->deceleration_timeline, "completed",
                            G_CALLBACK (deceleration_completed_cb), scroll);
          clutter_timeline_start (priv->deceleration_timeline);
          /* force redraw of first frame */
          priv->deceleration_timeline_lastframe = -1;
          deceleration_new_frame_cb(priv->deceleration_timeline, 0, scroll);
          decelerating = TRUE;
        }
    }

  /* Reset motion event buffer */
  priv->last_motion = 0;

  if (!decelerating)
    _tidy_finger_scroll_hide_scrollbars_later (scroll);

  /* Pass through events to children.
   * FIXME: this probably breaks click-count.
   */
  clutter_event_put ((ClutterEvent *)event);

  return TRUE;
}

static gboolean
after_event_cb (TidyFingerScroll *scroll)
{
  /* Check the pointer grab - if something else has grabbed it - for example,
   * a scroll-bar or some such, don't do our funky stuff.
   */
  if (clutter_get_pointer_grab () != CLUTTER_ACTOR (scroll))
    {
      TidyFingerScrollPrivate *priv = scroll->priv;

      g_signal_handlers_disconnect_by_func (scroll,
                                            motion_event_cb,
                                            scroll);
      g_signal_handlers_disconnect_by_func (scroll,
                                            button_release_event_cb,
                                            scroll);

      clutter_set_motion_events_enabled(priv->old_capture_motion_events);
    }

  return FALSE;
}

static gboolean
captured_event_cb (ClutterActor     *actor,
                   ClutterEvent     *event)
{
  TidyFingerScroll *scroll = TIDY_FINGER_SCROLL (actor);
  TidyFingerScrollPrivate *priv = scroll->priv;

  if (event->type == CLUTTER_BUTTON_PRESS)
    {
      TidyFingerScrollMotion *motion;
      ClutterButtonEvent *bevent = (ClutterButtonEvent *)event;

      /* Reset motion buffer */
      priv->last_motion = 0;
      motion = &g_array_index (priv->motion_buffer, TidyFingerScrollMotion, 0);

      if ((bevent->button == 1) &&
          (clutter_actor_transform_stage_point (actor,
                                           CLUTTER_UNITS_FROM_DEVICE(bevent->x),
                                           CLUTTER_UNITS_FROM_DEVICE(bevent->y),
                                           &motion->x, &motion->y)))
        {
          g_get_current_time (&motion->time);

          /* Save the coordinates of the first touch to be able to determine
           * whether we've exceeded the drag treshold when processing motion
           * events.  Until then don't move @child. */
          priv->move = FALSE;
          priv->first_x = motion->x;
          priv->first_y = motion->y;

          if (priv->deceleration_timeline)
            {
              clutter_timeline_stop (priv->deceleration_timeline);
              g_object_unref (priv->deceleration_timeline);
              priv->deceleration_timeline = NULL;
            }

          /* Fade in scroll-bars */
          show_scrollbars (scroll, TRUE);

          clutter_grab_pointer (actor);
          /* We need the following so that this captures *all* motion
           * events, and doesn't do a pick per movement where we really
           * don't need it. */
          priv->old_capture_motion_events = clutter_get_motion_events_enabled();
          clutter_set_motion_events_enabled(FALSE);

          /* Add a high priority idle to check the grab after the event
           * emission is finished.
           */
          g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                           (GSourceFunc)after_event_cb,
                           scroll,
                           NULL);

          g_signal_connect (actor,
                            "motion-event",
                            G_CALLBACK (motion_event_cb),
                            scroll);
          g_signal_connect (actor,
                            "button-release-event",
                            G_CALLBACK (button_release_event_cb),
                            scroll);
        }
    }

  return FALSE;
}

static void
hscroll_notify_reactive_cb (ClutterActor     *bar,
                            GParamSpec       *pspec,
                            TidyFingerScroll *scroll)
{
  TidyFingerScrollPrivate *priv;

  priv = scroll->priv;
  if (CLUTTER_ACTOR_IS_REACTIVE (bar))
    {
      if (priv->hscroll_timeline)
        {
          clutter_timeline_stop (priv->hscroll_timeline);
          g_object_unref (priv->hscroll_timeline);
          priv->hscroll_timeline = NULL;
        }
      clutter_actor_set_opacity (bar, 0xFF);
    }
}

static void
vscroll_notify_reactive_cb (ClutterActor     *bar,
                            GParamSpec       *pspec,
                            TidyFingerScroll *scroll)
{
  TidyFingerScrollPrivate *priv;

  priv = scroll->priv;
  if (CLUTTER_ACTOR_IS_REACTIVE (bar))
    {
      if (priv->vscroll_timeline)
        {
          clutter_timeline_stop (priv->vscroll_timeline);
          g_object_unref (priv->vscroll_timeline);
          priv->vscroll_timeline = NULL;
        }
      clutter_actor_set_opacity (bar, 0xFF);
    }
}

static void
tidy_finger_scroll_init (TidyFingerScroll *self)
{
  ClutterActor *scrollbar;
  ClutterTimeline *effect_timeline_in;
  ClutterTimeline *effect_timeline_out;
  TidyFingerScrollPrivate *priv = self->priv = FINGER_SCROLL_PRIVATE (self);
  ClutterFixed qn;
  guint i;

  priv->motion_buffer = g_array_sized_new (FALSE, TRUE,
                                           sizeof (TidyFingerScrollMotion), 3);
  g_array_set_size (priv->motion_buffer, 3);
  priv->decel_rate = CLUTTER_FLOAT_TO_FIXED(0.90f);
  priv->bouncing_decel_rate = CLUTTER_FLOAT_TO_FIXED (0.7f);

  /*
   * @bounce_back_speed_rate :=
   *   (1-@boucing_decel_rate) / (1-@bouncing_decel_rate^nframes)
   *   == (1 + 1/@bouncing_decel_rate + 1/@bouncing_decel_rate^2
   *         + ... + 1/@bouncing_decel_rate^(@nframes-1))^-1
   *
   * Where @nframes == frames per half a second.
   *
   * When @bounce_back_speed_rate is multiplied with the distance
   * to be scrolled in @nframes it yields the initial speed.
   * (@distance == @initial_speed
   *    * (1-@bouncing_decel_rate^@nframes)/(1-@bouncing_decel_rate)
   *  solved for @initial_speed.)
   */
  qn = CFX_ONE;
  for (i = (clutter_get_default_frame_rate ()) / 2; i > 0; i--)
    qn = clutter_qmulx(qn, priv->bouncing_decel_rate);
  priv->bounce_back_speed_rate = clutter_qdivx (
                                    CFX_ONE - priv->bouncing_decel_rate,
                                    CFX_ONE - qn);

  clutter_actor_set_reactive (CLUTTER_ACTOR (self), TRUE);

  /* Make the scroll-bars unreactive and set their opacity - we'll fade them
   * in/out when we scroll.
   * Also, hook onto notify::reactive and don't fade in/out when the bars are
   * set reactive (which you might want to do if you want finger-scrolling
   * *and* a scroll bar.
   */
  scrollbar = tidy_scroll_view_get_hscroll_bar (TIDY_SCROLL_VIEW (self));
  clutter_actor_set_reactive (scrollbar, FALSE);
  clutter_actor_set_opacity (scrollbar, 0x00);
  g_signal_connect (scrollbar, "notify::reactive",
                    G_CALLBACK (hscroll_notify_reactive_cb), self);

  scrollbar = tidy_scroll_view_get_vscroll_bar (TIDY_SCROLL_VIEW (self));
  clutter_actor_set_reactive (scrollbar, FALSE);
  clutter_actor_set_opacity (scrollbar, 0x00);
  g_signal_connect (scrollbar, "notify::reactive",
                    G_CALLBACK (vscroll_notify_reactive_cb), self);

  effect_timeline_in = clutter_timeline_new_for_duration
                            (TIDY_FINGER_SCROLL_FADE_SCROLLBAR_IN_TIME);
  priv->template_in = clutter_effect_template_new (effect_timeline_in,
                                                   CLUTTER_ALPHA_RAMP_INC);
  effect_timeline_out = clutter_timeline_new_for_duration
                            (TIDY_FINGER_SCROLL_FADE_SCROLLBAR_OUT_TIME);
  priv->template_out = clutter_effect_template_new (effect_timeline_out,
                                                    CLUTTER_ALPHA_RAMP_INC);
}

ClutterActor *
tidy_finger_scroll_new (TidyFingerScrollMode mode)
{
  return CLUTTER_ACTOR (g_object_new (TIDY_TYPE_FINGER_SCROLL,
                                      "mode", mode, NULL));
}

void
tidy_finger_scroll_stop (TidyFingerScroll *scroll)
{
  TidyFingerScrollPrivate *priv;

  g_return_if_fail (TIDY_IS_FINGER_SCROLL (scroll));

  priv = scroll->priv;

  if (priv->deceleration_timeline)
    {
      clutter_timeline_stop (priv->deceleration_timeline);
      g_object_unref (priv->deceleration_timeline);
      priv->deceleration_timeline = NULL;
    }
}

/* callback for tidy_finger_scroll_show_scrollbars timeout -
 * to remove the scrollbars after some time period */
static gboolean
_tidy_finger_scroll_show_scrollbars_cb (ClutterActor *scroll)
{
  TidyFingerScrollPrivate *priv;

  if (!TIDY_IS_FINGER_SCROLL(scroll))
    return FALSE;

  priv = TIDY_FINGER_SCROLL(scroll)->priv;

  priv->scrollbar_timeout = 0;

  show_scrollbars(TIDY_FINGER_SCROLL(scroll), FALSE);

  return FALSE;
}

static void
_tidy_finger_scroll_hide_scrollbars_later (TidyFingerScroll *scroll)
{
  TidyFingerScrollPrivate *priv = scroll->priv;

  if (priv->scrollbar_timeout)
    g_source_remove (priv->scrollbar_timeout);
  priv->scrollbar_timeout = g_timeout_add(
          TIDY_FINGER_SCROLL_INITIAL_SCROLLBAR_DELAY,
          (GSourceFunc)_tidy_finger_scroll_show_scrollbars_cb, scroll);
}

/* Show the scrollbars and then fade them out */
void
tidy_finger_scroll_show_scrollbars (ClutterActor *scroll)
{
  TidyFingerScrollPrivate *priv;

  if (!TIDY_IS_FINGER_SCROLL(scroll))
    return;

  priv = TIDY_FINGER_SCROLL(scroll)->priv;

  /* Show scrollbars */
  show_scrollbars(TIDY_FINGER_SCROLL(scroll), TRUE);
  /* Add delay for removing scrollbars */
  priv->scrollbar_timeout = g_timeout_add(
          TIDY_FINGER_SCROLL_INITIAL_SCROLLBAR_DELAY,
          (GSourceFunc)_tidy_finger_scroll_show_scrollbars_cb, scroll);
}

/* Hide the scrollbars right now, without fading. */
void
tidy_finger_scroll_hide_scrollbars_now (ClutterActor *actor)
{
  stop_scrollbars (TIDY_FINGER_SCROLL (actor));
  clutter_actor_set_opacity (tidy_scroll_view_get_hscroll_bar (
                                         TIDY_SCROLL_VIEW (actor)), 0);
  clutter_actor_set_opacity (tidy_scroll_view_get_vscroll_bar (
                                         TIDY_SCROLL_VIEW (actor)), 0);
}
