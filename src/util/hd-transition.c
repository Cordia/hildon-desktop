/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Gordon Williams <gordon.williams@collabora.co.uk>
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

#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include <sys/inotify.h>

#include <clutter/clutter.h>
#include <canberra.h>

#include "hd-transition.h"
#include "hd-comp-mgr.h"
#include "hd-gtk-style.h"
#include "hd-render-manager.h"
#include "hildon-desktop.h"
#include "hd-theme.h"
#include "hd-title-bar.h"
#include "hd-clutter-cache.h"
#include "tidy/tidy-sub-texture.h"
#include "tidy/tidy-blur-group.h"

#include "hd-app.h"
#include "hd-volume-profile.h"
#include "hd-util.h"

/* The master of puppets */
#define TRANSITIONS_INI             "/usr/share/hildon-desktop/transitions.ini"
#define TRANSITIONS_INI_FROM_THEME  "/etc/hildon/theme/transitions.ini"

typedef struct _HDEffectData
{
  MBWMCompMgrClientEvent   event;
  ClutterTimeline          *timeline;
  MBWMCompMgrClutterClient *cclient;
  ClutterActor             *cclient_actor;
  /* In subview transitions, this is the ORIGINAL (non-subview) view */
  MBWMCompMgrClutterClient *cclient2;
  ClutterActor             *cclient2_actor;
  HdCompMgr                *hmgr;
  /* hd_render_manager_set_visibilities() when all transitions are done */
  gboolean                  fixup_visibilities;
  /* original/expected position of application/menu */
  ClutterGeometry           geo;
  /* used in rotate_screen to set the direction (and amount) of movement */
  float                     angle;
  /* Any extra particles if they are used for this effect */
  ClutterActor             *particles[HDCM_UNMAP_PARTICLES];
  /* In Fade effects, final_alpha specifies the alpha value when the
   * window/note if fully faded in. */
  float                     final_alpha;
} HDEffectData;

/* %HPTimer %GSource state. */
typedef struct
{
  /*
   * @id:         the %GSource id of the source
   * @last:       the last time the source was evaluated
   * @remaining:  at the last evaluation of the source how many
   *              milisecons were until @expiry
   * @expiry:     the expiration time in miliseconds this %HPTimer
   *              was created with
   */
  GSource parent;
  guint id;
  GTimeVal last;
  unsigned remaining, expiry;
} HPTimer;

/* Describes the state of hd_transition_rotating_fsm(). */
static struct
{
  MBWindowManager *wm;

  /*
   * @direction:      Where we're going now.
   * @new_direction:  Reaching the next @phase where to go.
   *                  Used to override half-finished transitions.
   *                  *_fsm() needs to check it at the end of each @phase.
   */
  enum
  {
    GOTO_LANDSCAPE,
    GOTO_PORTRAIT,
  } direction, new_direction;

  /*
   * What is *_fsm() currently doing:
   * -- #IDLE:
   *    Nothing, we're sitting in landscape or portrait.
   *    Freeze the display and start reconfiguring the windows.
   * -- #TRANS_START:
   *    Windows reconfigured, start fading.
   * -- #FADE_OUT:
   *    Faded out to blankness, reconfigure the screen and wait until
   *    X is done.
   * -- #WAIT_FOR_ROOT_CONFIG:
   *    The root window is reconfigured, the screen is now officially
   *    in portrait.  Wait for a while for the initial application
   *    updates until thawing the display.
   * -- #WAIT_FOR_DAMAGES:
   *    Damage done, start fading in.
   * -- #FADE_IN:
   *    Mission complete.
   * -- #RECOVER:
   *    This is a special phase, not reached normally.  We only get here
   *    if the direction changed during the transition to IDLE->TRANS_START.
   *    Then we proceed to #FADE_IN.
   *
   * The FSM tries to do the right thing when it's instructed to change
   * direction in the middle of the transition, so the phases may jump
   * from here to there.
   */
  enum
  {
    IDLE,
    TRANS_START,
    FADE_OUT,
    WAIT_FOR_ROOT_CONFIG,
    WAIT_FOR_DAMAGES,
    RECOVER,
    FADE_IN,
  } phase;

  /*
   * @goto_state when we've %FADE_OUT:d.  Set by
   * hd_transition_rotate_screen_and_change_state()
   * Its initial value is %HDRM_STATE_UNDEFINED,
   * which means don't change the state.
   */
  HDRMStateEnum goto_state;

  gulong root_config_signal_id;

  /* In the WAITING state we have a timer that calls us back a few ms
   * after the last damage event. This is the id, as we need to restart
   * it whenever we get another damage event. */
  HPTimer *timeout_id;

  /* This timer counts from when we first entered the WAITING state,
   * so if we are continually getting damage we don't just hang there. */
  GTimer *timer;

  /*
   * The number of %_MAEMO_ROTATION_PATIENCE requests we received since
   * we notified our clients about the screen size change.  We we get one
   * we extend our patience with regards to client redraws to our maximum.
   * If we were asked for patience and then notified that it's not longer
   * necessary we stop waiting for damages immedeately.
   */
  guint patience_requests;
} Orientation_change;

/* The number of transitions in progress requesting for @fixup_visibilities.
 * At the moment only the popup (menus and dialogs), the fade (notes, banners)
 * and subview transitions are involved. */
static guint Transitions_running;

/* If %TRUE keep reloading transitions.ini until we can
 * and we can watch it. */
static gboolean transitions_ini_is_dirty;

/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

/* Seeing the time elapsed since @hptimer's @last evaluation update
 * the @remaining number of miliseconds until expiry. */
static void
hptimer_calc_remaining(HPTimer *hptimer)
{
  guint diff;
  GTimeVal now;

  /* Don't use g_source_get_current_time()s cached clock,
   * it may be inaccurate by now. */
  g_get_current_time (&now);

  /* Yes we don't handle time going back. */
  diff = (now.tv_sec - hptimer->last.tv_sec) * 1000;
  if (now.tv_usec < hptimer->last.tv_usec)
    {
      diff -= 1000;
      diff += ((1000000 - hptimer->last.tv_usec) + now.tv_usec) / 1000;
    } else
      diff += (now.tv_usec - hptimer->last.tv_usec) / 1000;

  if (hptimer->remaining > diff)
    hptimer->remaining -= diff;
  else
    hptimer->remaining  = 0;

  hptimer->last = now;
}

static gboolean
hptimer_source_prepare(GSource *src, gint *timeout)
{
  HPTimer *hptimer = (HPTimer *)src;

  hptimer_calc_remaining (hptimer);
  return (*timeout = hptimer->remaining) == 0;
}

static gboolean
hptimer_source_check(GSource *src)
{
  HPTimer *hptimer = (HPTimer *)src;

  hptimer_calc_remaining (hptimer);
  return hptimer->remaining == 0;
}

static gboolean
hptimer_source_dispatch(GSource *src, GSourceFunc cb, gpointer cbarg)
{
  HPTimer *hptimer = (HPTimer *)src;

  hptimer->remaining = hptimer->expiry;
  return cb (cbarg);
}

/*
 * Returns a "high performance" timer, which is similar to an ordinary
 * %GTimeoutSource but may be better suited for short @expiry:s in
 * the sub-100th-second range.  You're free to change the %HPTimer's
 * @remaining and @expiry states.
 */
static HPTimer *
hptimer_new(unsigned expiry,
            GSourceFunc cb, gpointer cbarg, GDestroyNotify dtor)
{
  static GSourceFuncs hptimer_funcs =
  {
    hptimer_source_prepare,
    hptimer_source_check,
    hptimer_source_dispatch
  };

  HPTimer *hptimer;
  GSource *src;
  guint id;

  src = g_source_new (&hptimer_funcs, sizeof (*hptimer));
  g_source_set_callback (src, cb, cbarg, dtor);
  g_source_set_priority (src, G_PRIORITY_HIGH);
  id = g_source_attach (src, NULL);
  g_source_unref (src);

  hptimer = (HPTimer *)src;
  hptimer->id = id;
  g_get_current_time (&hptimer->last);
  hptimer->remaining = hptimer->expiry = expiry;

  return hptimer;
}

/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

/* amt goes from 0->1, and the result goes mostly from 0->1 with a bit of
 * overshoot at the end */
float
hd_transition_overshoot(float x)
{
  float smooth_ramp, converge;
  float amt;
  int offset;
  offset = (int)x;
  amt = x-offset;
  smooth_ramp = 1.0f - cos(amt*3.141592); // 0 <= smooth_ramp <= 2
  converge = sin(0.5*3.141592*(1-amt)); // 0 <= converve <= 1
  return offset + (smooth_ramp*0.675)*converge + (1-converge);
}

/* amt goes from 0->1, and the result goes from 0->1 smoothly */
float
hd_transition_smooth_ramp(float amt)
{
  if (amt>0 && amt<1)
    return (1.0f - cos(amt*3.141592)) * 0.5f;
  return amt;
}

float
hd_transition_ease_in(float amt)
{
  if (amt>0 && amt<1)
    return (1.0f - cos(amt*3.141592*0.5));
  return amt;
}

float
hd_transition_ease_out(float amt)
{
  if (amt>0 && amt<1)
    return cos((1-amt)*3.141592*0.5);
  return amt;
}

/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

static ClutterTimeline *
hd_transition_timeline_new(const gchar *transition,
                           MBWMCompMgrClientEvent event,
                           gint default_length)
{
  const char *key =
    event==MBWMCompMgrClientEventMap ?"duration_in":"duration_out";
  return clutter_timeline_new_for_duration (
      hd_transition_get_int(transition, key, default_length) );
}

/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

extern gboolean hd_dbus_display_is_off;

/* For the animated progress indicator in the title bar */
void
on_decor_progress_timeline_new_frame(ClutterTimeline *timeline,
                                     gint frame_num,
                                     ClutterActor *progress_texture)
{
  if (!hd_dbus_display_is_off && TIDY_IS_SUB_TEXTURE(progress_texture) &&
      hd_render_manager_actor_is_visible(progress_texture))
    {
      /* The progress animation is a series of frames packed
       * into a texture - like a film strip
       */
      ClutterGeometry progress_region =
         {HD_THEME_IMG_PROGRESS_SIZE*frame_num, 0,
          HD_THEME_IMG_PROGRESS_SIZE, HD_THEME_IMG_PROGRESS_SIZE };

      tidy_sub_texture_set_region(
          TIDY_SUB_TEXTURE(progress_texture),
          &progress_region);

      /* We just queue damage with an area like we do for windows.
       * Otherwise we end up updating the whole screen for this.
       * This also takes account of visibility. */
      hd_util_partial_redraw_if_possible(progress_texture, 0);
    }
}

static void
on_popup_timeline_new_frame(ClutterTimeline *timeline,
                            gint frame_num, HDEffectData *data)
{
  float amt;
  ClutterActor *actor, *filler;
  int status_low, status_high;
  float status_pos;
  gboolean pop_top, pop_bottom; /* pop in from the top, or the bottom */

  float overshoot;
  ClutterGeometry geo;

  actor = data->cclient_actor;
  if (!CLUTTER_IS_ACTOR(actor))
    return;
  filler = data->particles[0];

  /* We need to get geometry each frame as often windows have
   * a habit of changing size while they move. */
  clutter_actor_get_geometry(actor, &geo);

  pop_top = geo.y<=0;
  pop_bottom = geo.y+geo.height==hd_comp_mgr_get_current_screen_height();
  if (pop_top && pop_bottom)
    pop_top = FALSE;
  amt =  (float)clutter_timeline_get_progress(timeline);
  /* reverse if we're removing this */
  if (data->event == MBWMCompMgrClientEventUnmap)
    amt = 1-amt;

  overshoot = hd_transition_overshoot(amt);

  if (pop_top)
    {
      status_low = geo.y-geo.height;
      status_high = geo.y;
    }
  else if (pop_bottom)
    {
      status_low = geo.y+geo.height;
      status_high = geo.y;
    }
  else
    {
      status_low = geo.y;
      status_high = geo.y;
    }
  status_pos = status_low*(1-overshoot) + status_high*overshoot;

  clutter_actor_set_anchor_pointu(actor, 0,
      CLUTTER_INT_TO_FIXED(geo.y) - CLUTTER_FLOAT_TO_FIXED(status_pos));
  clutter_actor_set_opacity(actor, (int)(255*amt));

  /* use a slither of filler to fill in the gap where the menu
   * has jumped a bit too far up */
  if ((status_pos>status_high && pop_top) ||
       (status_pos<status_high && pop_bottom))
    {
      clutter_actor_show(filler);
      if (pop_top)
        {
          clutter_actor_set_positionu(filler,
                    CLUTTER_INT_TO_FIXED(0),
                    CLUTTER_FLOAT_TO_FIXED(status_high-status_pos));
          clutter_actor_set_sizeu(filler,
                    CLUTTER_INT_TO_FIXED(geo.width),
                    CLUTTER_FLOAT_TO_FIXED(status_pos-status_high));
        }
      else if (pop_bottom)
        {
          clutter_actor_set_positionu(filler,
                    CLUTTER_INT_TO_FIXED(0),
                    CLUTTER_INT_TO_FIXED(geo.height));
          clutter_actor_set_sizeu(filler,
                    CLUTTER_INT_TO_FIXED(geo.width),
                    CLUTTER_FLOAT_TO_FIXED(status_high-status_pos));
        }
    }
  else
    clutter_actor_hide(filler);
}

static void
on_fade_timeline_new_frame(ClutterTimeline *timeline,
                            gint frame_num, HDEffectData *data)
{
  float amt, ramt;
  gint alpha;
  ClutterActor *actor;

  actor = data->cclient_actor;
  if (!CLUTTER_IS_ACTOR(actor))
    return;

  if (hd_dbus_display_is_off)
    {
      /* skip the animation */
      if (data->event == MBWMCompMgrClientEventUnmap)
        clutter_actor_set_opacity(actor, 0);
      else
        clutter_actor_set_opacity(actor, 255);
      return;
    }

  amt =  (float)clutter_timeline_get_progress(timeline);
  /* reverse if we're removing this */
  if (data->event == MBWMCompMgrClientEventUnmap)
    amt = 1-amt;
  ramt = hd_transition_smooth_ramp(1-amt);
  amt = hd_transition_smooth_ramp(amt);

  alpha = (gint)(255*amt*data->final_alpha);
  if (alpha>255) alpha=255;
  clutter_actor_set_opacity(actor, alpha);

  /* Slide information notes and banners from top to their final position. */
  if (data->geo.width)
    clutter_actor_set_anchor_point(actor, 0, ramt*data->geo.y);
}

static void
on_close_timeline_new_frame(ClutterTimeline *timeline,
                            gint frame_num, HDEffectData *data)
{
  float amt;
  ClutterActor *actor;
  float amtx, amty, amtp;
  int centrex, centrey;
  float particle_opacity, particle_radius, particle_scale;
  gint i;

  actor = data->cclient_actor;
  if (!CLUTTER_IS_ACTOR(actor))
    return;

  if (hd_dbus_display_is_off)
    {
      for (i = 0; i < HDCM_UNMAP_PARTICLES; ++i)
        if (data->particles[i])
	  clutter_actor_hide (data->particles[i]);
      clutter_actor_hide (actor);
      return;
    }

  amt = (float)clutter_timeline_get_progress(timeline);

  amtx = 1.6 - amt*2.5; // shrink in x
  amty = 1 - amt*2.5; // shrink in y
  amtp = amt*2 - 1; // particles
  if (amtx<0) amtx=0;
  if (amtx>1) amtx=1;
  if (amty<0) amty=0;
  if (amty>1) amty=1;
  if (amtp<0) amtp=0;
  if (amtp>1) amtp=1;
  /* smooth out movement */
  amtx = (1-cos(amtx * 3.141592)) * 0.45f + 0.1f;
  amty = (1-cos(amty * 3.141592)) * 0.45f + 0.1f;
  particle_opacity = sin(amtp * 3.141592);
  particle_radius = 8 + (1-cos(amtp * 3.141592)) * 32.0f;

  centrex =  data->geo.x + data->geo.width / 2 ;
  centrey =  data->geo.y + data->geo.height / 2 ;
  /* set app location and fold up like a turned-off TV.
   * @actor is anchored in the middle so it needn't be repositioned */
  clutter_actor_set_scale(actor, amtx, amty);
  clutter_actor_set_opacity(actor, (int)(255 * (1-amtp)));
  /* do sparkles... */
  particle_scale = 1-amtp*0.5;
  for (i=0;i<HDCM_UNMAP_PARTICLES;i++)
    if (data->particles[i] && (amtp > 0) && (amtp < 1))
      {
        /* space particles semi-randomly and rotate once */
        float ang = i * 15 +
                    amtp * 3.141592f / 2;
        float radius = particle_radius * (i+1) / HDCM_UNMAP_PARTICLES;
        /* twinkle effect */
        float opacity = particle_opacity * ((1-cos(amt*50+i)) * 0.5f);
        clutter_actor_show( data->particles[i] );
        clutter_actor_set_opacity(data->particles[i],
                (int)(255 * opacity));
        clutter_actor_set_scale(data->particles[i],
                particle_scale, particle_scale);

        clutter_actor_set_positionu(data->particles[i],
                CLUTTER_FLOAT_TO_FIXED(centrex + sin(ang) * radius),
                CLUTTER_FLOAT_TO_FIXED(centrey + cos(ang) * radius));
      }
    else
      if (data->particles[i])
	clutter_actor_hide( data->particles[i] );
}

/* Returns the cubic bezier curve at @t defined by
 * (@p0, @p1) and (@p2, @p3). */
static float __attribute__((const))
bezier(float t, float p0, float p1, float p2, float p3)
{
  /* B(t) = (1-t)^3*P0 + (1-t)^2*t*P1 + (1-t)*t^2*P2 + t^3*P3 */
  return powf((1-t), 3)*p0
    + 3*powf((1-t), 2)*t*p1
    + 3*(1-t)*powf(t, 2)*p2
    + powf(t, 3)*p3;
}

static void
on_notification_timeline_new_frame(ClutterTimeline *timeline,
                                   gint frame_num, HDEffectData *data)
{
  float now;
  ClutterActor *actor;
  guint width, height;
  gint tbw, px, py;

  actor = data->cclient_actor;
  if (!CLUTTER_IS_ACTOR(actor) || hd_dbus_display_is_off)
    return;

  tbw = hd_title_bar_get_button_width( /* Task Button's current width. */
                        HD_TITLE_BAR(hd_render_manager_get_title_bar()));
  clutter_actor_get_size(actor, &width, &height);
  clutter_actor_get_position(actor, &px, &py);
  now = frame_num / (float)clutter_timeline_get_n_frames(timeline);

  if (hd_comp_mgr_is_portrait()
      && hd_transition_get_int("notification", "is_cool", 0))
    {
      /* In portrait fly from right to left, stay in the corner
       * then fly away, following a bezier curve.  At the start
       * the notification's bottom-center is at the screen's
       * top-right.  By the end the notification's bottom-right
       * is at the screen's top-left. */
      static struct
      {
        float x, y;
      } const cpin[] =
      { /* Bezier curve control points for the opening part. */
        {  185, -88 },
        {  185, -32 },
        {  112,   0 },
        {  -32,   0 },
      }, cpout[] =
      { /* and for MBWMCompMgrClientEventUnmap. */
        {  -32,   0 },
        { -176,   0 },
        { -478, -32 },
        { -478, -88 },
      }, *curve;

      /* Set the position to @curve(@now). */
      now = hd_transition_smooth_ramp(now);
      curve = data->event == MBWMCompMgrClientEventUnmap ? cpout : cpin;
      clutter_actor_set_anchor_pointu(actor,
               CLUTTER_FLOAT_TO_FIXED(-bezier(now,
                        curve[0].x, curve[1].x, curve[2].x, curve[3].x)),
               CLUTTER_FLOAT_TO_FIXED(-bezier(now,
                        curve[0].y, curve[1].y, curve[2].y, curve[3].y)));

      /* We should restore the opacity and scaling of @actor in case
       * we were switched orientation during the transition somehow
       * but it seems unlikely enough not to justify the cycles. */
    }
  else if (data->event == MBWMCompMgrClientEventUnmap)
    {
      float t, thr;

      /*
       * Timeline is broken into two pieces.  The first part takes
       * @thr seconds and during that the notification actor is moved
       * to its final place,  The second part is much shorter and
       * during that it's faded to nothingness.
       */
      thr = 400.0 / (150+400);

      if (now < thr)
        { /* fade, move, resize */
          float sc;
          gint cx, cy;
          gint dx, dy, dw, dh;

          /*
           * visual geometry: 366x88+112+0 -> 48x11+32+22 or
           *                  366x88+80+0  -> 16x3+32+26
           *                  scale it down proportionally
           *                  and place it in the middle of the tasks button
           *                  leaving 8 pixels left and right, keeping the
           *                  aspect ratio
           * opacity:         1 -> 0.50
           * use smooth ramping
           */

          /* It's probably best to count it with your fingers to follow
           * this mumble-jumbo. */
          dx = 32;
          dw = tbw - 2*dx;
          dh = (float)dw/width * height;
          dy = (HD_COMP_MGR_TOP_LEFT_BTN_HEIGHT - dh) / 2;

          t = hd_transition_smooth_ramp(now / thr);
          cx = (dx - tbw)*t + tbw - px;
          cy = (dy -  py)*t;
          sc = (((float)dw/width  - 1)*t + 1);

          clutter_actor_set_scale (actor, sc, sc);
          clutter_actor_set_anchor_point (actor, -cx/sc, -cy/sc);
          clutter_actor_set_opacity(actor, 255 * ((0.50 - 1)*t + 1));
        }
      else
        { /* fade: 0.75 -> 0 linearly */
          t = (now - thr) / (1.0 - thr);
          clutter_actor_set_opacity(actor, 255 * (-0.50*t + 0.50));
        }
    }
  else
    {
      /* Opening Animation - we fade in, and move in from the top-right
       * edge of the screen in an arc */
      float amt = hd_transition_smooth_ramp(now);
      float scale =  1 + (1-amt)*0.5f;
      float ang = amt * 3.141592f * 0.5f;
      float corner_x = (hd_comp_mgr_get_current_screen_width()*0.5f
                        - HD_COMP_MGR_TOP_LEFT_BTN_WIDTH) * cos(ang)
                       - px + tbw;
      float corner_y = (sin(ang)-1) * height;
      /* We set anchor point so if the notification resizes/positions
       * in flight, we're ok.  NOTE that the position of the actor
       * (get_position()) still matters, and it is LEFT_BIN_WIDTH. */
      clutter_actor_set_opacity(actor, (int)(255*amt));
      clutter_actor_set_scale(actor, scale, scale);
      clutter_actor_set_anchor_pointu(actor,
               CLUTTER_FLOAT_TO_FIXED( -corner_x / scale ),
               CLUTTER_FLOAT_TO_FIXED( -corner_y / scale ));
    }
}

static void
on_subview_timeline_new_frame(ClutterTimeline *timeline,
                              gint frame_num, HDEffectData *data)
{
  float amt;
  gint n_frames;
  ClutterActor *subview_actor = 0, *main_actor = 0;

  if (data->cclient)
    subview_actor = data->cclient_actor;
  if (data->cclient2)
    main_actor = data->cclient2_actor;

  n_frames = clutter_timeline_get_n_frames(timeline);
  amt = frame_num / (float)n_frames;
  amt = hd_transition_smooth_ramp( amt );
  if (data->event == MBWMCompMgrClientEventUnmap)
    amt = 1-amt;

  {
    float corner_x;
    corner_x = (1-amt) * hd_comp_mgr_get_current_screen_width();
    if (subview_actor)
      {
        clutter_actor_set_anchor_pointu(subview_actor,
           CLUTTER_FLOAT_TO_FIXED( -corner_x ),
           CLUTTER_FLOAT_TO_FIXED( 0 ) );
        /* we have to show this actor, because it'll get hidden by the
         * render manager visibility test if not. */
        clutter_actor_show(subview_actor);
      }
    if (main_actor)
      {
        clutter_actor_set_anchor_pointu(main_actor,
           CLUTTER_FLOAT_TO_FIXED( -(corner_x - hd_comp_mgr_get_current_screen_width()) ),
           CLUTTER_FLOAT_TO_FIXED( 0 ) );
        /* we have to show this actor, because it'll get hidden by the
         * render manager visibility test if not. */
        clutter_actor_show(main_actor);
      }
  }

  /* if we're at the last frame, return our actors to the correct places) */
  if (frame_num == n_frames)
    {
      if (subview_actor)
        {
          clutter_actor_set_anchor_pointu(subview_actor, 0, 0);
          if (data->event == MBWMCompMgrClientEventUnmap)
            clutter_actor_hide(subview_actor);
        }
      if (main_actor)
        {
          clutter_actor_set_anchor_pointu(main_actor, 0, 0);
          /* hide the correct actor - as we overrode the visibility test in hdrm */
          if (data->event == MBWMCompMgrClientEventMap)
            clutter_actor_hide(main_actor);
        }
    }
}

static void
on_rotate_screen_timeline_new_frame(ClutterTimeline *timeline,
                                    gint frame_num, HDEffectData *data)
{
  float amt, dim_amt, angle;
  gint n_frames;
  ClutterActor *actor;

  n_frames = clutter_timeline_get_n_frames(timeline);
  amt = frame_num / (float)n_frames;
  // we want to ease in, but speed up as we go - X^3 does this nicely
  amt = amt*amt;
  if (data->event == MBWMCompMgrClientEventUnmap)
    amt = 1-amt;
  /* dim=1 -> screen is black, dim=0 -> normal. Only
   * dim out right at the end of the animation - and
   * then only by half */
  dim_amt = amt*2 - 1.5;
  if (dim_amt<0)
    dim_amt = 0;
  angle = data->angle * amt;

  actor = CLUTTER_ACTOR(hd_render_manager_get());
  clutter_actor_set_rotation(actor,
      hd_comp_mgr_is_portrait () ? CLUTTER_Y_AXIS : CLUTTER_X_AXIS,
      frame_num < n_frames ? angle : 0,
      hd_comp_mgr_get_current_screen_width()/2,
      hd_comp_mgr_get_current_screen_height()/2, 0);
  clutter_actor_set_depthu(actor, -CLUTTER_FLOAT_TO_FIXED(amt*150));
  /* use this actor to dim out the screen */
  clutter_actor_raise_top(data->particles[0]);
  clutter_actor_set_opacity(data->particles[0], (int)(dim_amt*255));
}

/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

/* #ClutterStage's notify::allocation callback to notice if we are
 * switching between landscape and portrait modes duing an effect. */
static void
on_screen_size_changed (ClutterActor *stage, GParamSpec *unused,
                        HDEffectData *data)
{
  guint scrw, scrh;
  ClutterActor *actor;

  /* Rotate @actor back to the mode it is layed out for.
   * Assume it's anchored in the middle. */
  clutter_actor_get_size (stage, &scrw, &scrh);
  actor = mb_wm_comp_mgr_clutter_client_get_actor (data->cclient);

  if (hd_util_rotate_geometry(&data->geo, scrw, scrh))
    clutter_actor_set_rotation (actor, CLUTTER_Z_AXIS, -90, 0, 0, 0);
  else
    clutter_actor_set_rotation (actor, CLUTTER_Z_AXIS, +90, 0, 0, 0);

  clutter_actor_set_position (actor,
                              data->geo.x + data->geo.width/2,
                              data->geo.y + data->geo.height/2);
}

static void
hd_transition_completed (ClutterTimeline* timeline, HDEffectData *data)
{
  gint i;
  HdCompMgr *hmgr = HD_COMP_MGR (data->hmgr);

  if (data->cclient)
    {
      HD_COMP_MGR_CLIENT (data->cclient)->effect = NULL;
      mb_wm_comp_mgr_clutter_client_unset_flags (data->cclient,
                                        MBWMCompMgrClutterClientDontUpdate |
                                        MBWMCompMgrClutterClientEffectRunning);
      mb_wm_object_unref (MB_WM_OBJECT (data->cclient));
    }

  if (data->event == MBWMCompMgrClientEventUnmap && data->cclient_actor)
    {
      ClutterActor *parent = clutter_actor_get_parent(data->cclient_actor);
      if (CLUTTER_IS_CONTAINER(parent))
        clutter_container_remove_actor(
            CLUTTER_CONTAINER(parent), data->cclient_actor );
    }

  if (data->cclient_actor)
    g_object_unref(data->cclient_actor);

  if (data->cclient2)
    {
      HD_COMP_MGR_CLIENT (data->cclient2)->effect = NULL;
      mb_wm_comp_mgr_clutter_client_unset_flags (data->cclient2,
                                        MBWMCompMgrClutterClientDontUpdate |
                                        MBWMCompMgrClutterClientEffectRunning);
      mb_wm_object_unref (MB_WM_OBJECT (data->cclient2));
    }

  if (data->cclient2_actor)
    g_object_unref(data->cclient2_actor);

/*   dump_clutter_tree (CLUTTER_CONTAINER (clutter_stage_get_default()), 0); */

  if (timeline)
    g_object_unref ( timeline );

  if (hmgr)
    hd_comp_mgr_set_effect_running(hmgr, FALSE);

  for (i=0;i<HDCM_UNMAP_PARTICLES;i++)
    if (data->particles[i]) {
      // if actor was in a group, remove it
      if (CLUTTER_IS_CONTAINER(clutter_actor_get_parent(data->particles[i])))
             clutter_container_remove_actor(
               CLUTTER_CONTAINER(clutter_actor_get_parent(data->particles[i])),
               data->particles[i]);
      g_object_unref(data->particles[i]); // unref ourselves
      data->particles[i] = 0; // for safety, set pointer to 0
    }

  g_signal_handlers_disconnect_by_func (clutter_stage_get_default (),
                                        G_CALLBACK (on_screen_size_changed),
                                        data);

  /* @Transitions_running only accounts for transitions asking for
   * @fixup_visibilities.  If we're finishing off the last one it
   * must be safe (knock-knock-knock) to re-evaluate visibilities. */
  if (data->fixup_visibilities && --Transitions_running == 0
      && STATE_IS_APP(hd_render_manager_get_state()))
    hd_render_manager_set_visibilities();

  g_free (data);

  if (hmgr)
    hd_comp_mgr_reconsider_compositing (MB_WM_COMP_MGR (hmgr));
}

void
hd_transition_popup(HdCompMgr                  *mgr,
                    MBWindowManagerClient      *c,
                    MBWMCompMgrClientEvent     event)
{
  MBWMCompMgrClutterClient * cclient;
  ClutterActor             * actor;
  HDEffectData             * data;
  ClutterGeometry            geo;
  ClutterColor col;

  cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);
  actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);
  if (!actor)
    return;
  clutter_actor_get_geometry(actor, &geo);

  /* Need to store also pointer to the manager, as by the time
   * the effect finishes, the back pointer in the cm_client to
   * MBWindowManagerClient is not longer valid/set.
   */
  data = g_new0 (HDEffectData, 1);
  data->event = event;
  data->cclient = mb_wm_object_ref (MB_WM_OBJECT (cclient));
  data->cclient_actor = g_object_ref (actor);
  data->hmgr = HD_COMP_MGR (mgr);
  data->timeline = hd_transition_timeline_new("popup", event, 250);
  g_signal_connect (data->timeline, "new-frame",
                        G_CALLBACK (on_popup_timeline_new_frame), data);
  g_signal_connect (data->timeline, "completed",
                        G_CALLBACK (hd_transition_completed), data);
  data->geo = geo;
  Transitions_running += data->fixup_visibilities = TRUE;

  /*
   * If @actor is a fullscreen dialog it's not in apptop but in home_blur.
   * This case when its client is unmapped the next client is stacked on
   * the top of it, hiding this @actor, which we want to keep shown until
   * the end of transition.
   */
  if (event == MBWMCompMgrClientEventUnmap)
    {
      hd_render_manager_return_dialog(actor);
      clutter_actor_show(actor);
    }

  mb_wm_comp_mgr_clutter_client_set_flags (cclient,
                              MBWMCompMgrClutterClientDontUpdate |
                              MBWMCompMgrClutterClientEffectRunning);

  hd_comp_mgr_set_effect_running(mgr, TRUE);

  /* Add actor for the background when we pop a bit too far */
  data->particles[0] = g_object_ref(clutter_rectangle_new());
  clutter_actor_set_name(data->particles[0], "popup background");
  clutter_container_add_actor(CLUTTER_CONTAINER(actor), data->particles[0]);
  hd_gtk_style_get_bg_color(HD_GTK_BUTTON_SINGLETON, GTK_STATE_NORMAL,
                              &col);
  clutter_rectangle_set_color(CLUTTER_RECTANGLE(data->particles[0]),
                              &col);

  /* first call to stop flicker */
  on_popup_timeline_new_frame(data->timeline, 0, data);
  clutter_timeline_start (data->timeline);
}

/* For banners, information notes and confirmation notes. */
void
hd_transition_fade(HdCompMgr                  *mgr,
                   MBWindowManagerClient      *c,
                   MBWMCompMgrClientEvent     event)
{
  MBWMCompMgrClutterClient * cclient;
  HDEffectData             * data;

  cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);

  data = g_new0 (HDEffectData, 1);
  data->event = event;
  data->cclient = mb_wm_object_ref (MB_WM_OBJECT (cclient));
  data->cclient_actor = g_object_ref (
      mb_wm_comp_mgr_clutter_client_get_actor( data->cclient ) );
  data->hmgr = HD_COMP_MGR (mgr);
  data->timeline = hd_transition_timeline_new("fade", event, 250);
  Transitions_running += data->fixup_visibilities = TRUE;

  if (HD_IS_BANNER_NOTE(c))
    {
      clutter_actor_get_geometry(data->cclient_actor, &data->geo);
      data->final_alpha = hd_transition_get_double("fade", "banner_note_alpha",
                                                   1.0);
    }
  else if (HD_IS_INFO_NOTE(c))
    {
      clutter_actor_get_geometry(data->cclient_actor, &data->geo);
      data->final_alpha = hd_transition_get_double("fade", "info_note_alpha",
                                                   1.0);
    }
  else
    /* Leave @data->geo 0, we needn't move the actor around. */
    data->final_alpha = 1;

  g_signal_connect (data->timeline, "new-frame",
                        G_CALLBACK (on_fade_timeline_new_frame), data);
  g_signal_connect (data->timeline, "completed",
                        G_CALLBACK (hd_transition_completed), data);

  mb_wm_comp_mgr_clutter_client_set_flags (cclient,
                              MBWMCompMgrClutterClientDontUpdate |
                              MBWMCompMgrClutterClientEffectRunning);
  hd_comp_mgr_set_effect_running(mgr, TRUE);

  /* first call to stop flicker */
  on_fade_timeline_new_frame(data->timeline, 0, data);
  clutter_timeline_start (data->timeline);
}
void
hd_transition_fade_out_loading_screen(ClutterActor *loading_image)
{
    gint duration, fade_delay;
    HDEffectData             * data;

    duration = hd_transition_get_int("launcher_launch", "duration_out", 250);
    /* If duration is <=0 we just return as the loading screen is already
     * removed */
    if (duration<=0)
      return;

    data = g_new0 (HDEffectData, 1);
    data->event = MBWMCompMgrClientEventUnmap;
    data->cclient_actor = g_object_ref ( loading_image );
    data->hmgr = 0;
    data->timeline = clutter_timeline_new_for_duration ( duration );
    data->final_alpha = 1;
    /* the delay before we start to fade out. We implement this by setting
     * the final_alpha value to something *past* opaque */
    fade_delay = hd_transition_get_int("launcher_launch", "delay", 150);
    if (fade_delay>0)
      {
        gint duration = clutter_timeline_get_duration(data->timeline);
        if (fade_delay < duration) {
          data->final_alpha = 1 + fade_delay/(float)(duration-fade_delay);
          // safety in case strange values get put in
          if (data->final_alpha>10)
            data->final_alpha = 10;
        }
      }

    g_signal_connect (data->timeline, "new-frame",
                          G_CALLBACK (on_fade_timeline_new_frame), data);
    g_signal_connect (data->timeline, "completed",
                          G_CALLBACK (hd_transition_completed), data);
    clutter_container_add_actor (
                 hd_render_manager_get_front_group(),
                 loading_image);
    /* first call to stop flicker */
    on_fade_timeline_new_frame(data->timeline, 0, data);
    clutter_timeline_start (data->timeline);
}

void
hd_transition_close_app (HdCompMgr                  *mgr,
                         MBWindowManagerClient      *c)
{
  MBWMClientType c_type = MB_WM_CLIENT_CLIENT_TYPE (c);
  MBWMCompMgrClutterClient * cclient;
  ClutterActor             * actor;
  HdApp                    * app;
  HDEffectData             * data;
  ClutterGeometry            geo;
  ClutterContainer         * parent;
  gint                       i;

  /* proper app close animation */
  if (c_type != MBWMClientTypeApp)
    return;

  /* The switcher will do the effect if it's active,
   * don't interfere. */
  if (hd_render_manager_get_state()==HDRM_STATE_TASK_NAV)
    return;

  /* Don't do the unmap transition if it's a secondary. */
  app = HD_APP (c);
  if (app->stack_index > 0 && app->leader != app)
    {
      /* FIXME: Transitions. */
      g_debug ("%s: Skip non-leading secondary window.", __FUNCTION__);
      return;
    }

  cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);
  actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);
  if (!actor || !CLUTTER_ACTOR_IS_VISIBLE(actor))
    return;

  /* Don't bother for anything tiny */
  clutter_actor_get_geometry(actor, &geo);
  if (geo.width<16 || geo.height<16)
    return;

  /*
   * Need to store also pointer to the manager, as by the time
   * the effect finishes, the back pointer in the cm_client to
   * MBWindowManagerClient is not longer valid/set.
   *
   * It is possible that during the effect we leave portrait mode,
   * so be prepared for it.
   */
  data = g_new0 (HDEffectData, 1);
  data->event = MBWMCompMgrClientEventUnmap;
  data->cclient = mb_wm_object_ref (MB_WM_OBJECT (cclient));
  data->cclient_actor = g_object_ref (actor);
  data->hmgr = HD_COMP_MGR (mgr);
  data->timeline = clutter_timeline_new_for_duration (
                    hd_transition_get_int("app_close", "duration", 500));
  g_signal_connect (data->timeline, "new-frame",
                    G_CALLBACK (on_close_timeline_new_frame), data);
  g_signal_connect (clutter_stage_get_default (), "notify::allocation",
                    G_CALLBACK (on_screen_size_changed), data);
  g_signal_connect (data->timeline, "completed",
                    G_CALLBACK (hd_transition_completed), data);
  data->geo = geo;

  mb_wm_comp_mgr_clutter_client_set_flags (cclient,
                                  MBWMCompMgrClutterClientDontUpdate |
                                  MBWMCompMgrClutterClientEffectRunning);

  parent = hd_render_manager_get_front_group();
  /* reparent our actor so it will be visible when we switch views */
  clutter_actor_reparent(actor, CLUTTER_ACTOR(parent));
  clutter_actor_lower_bottom(actor);
  clutter_actor_move_anchor_point_from_gravity(actor, CLUTTER_GRAVITY_CENTER);

  for (i = 0; i < HDCM_UNMAP_PARTICLES; ++i)
    {
      data->particles[i] = hd_clutter_cache_get_texture(
          HD_THEME_IMG_CLOSING_PARTICLE, TRUE);
      if (data->particles[i])
        {
          g_object_ref(data->particles[i]);
          clutter_actor_set_anchor_point_from_gravity(data->particles[i],
                                                      CLUTTER_GRAVITY_CENTER);
          clutter_container_add_actor(parent, data->particles[i]);
          clutter_actor_hide(data->particles[i]);
        }
    }

  hd_comp_mgr_set_effect_running(mgr, TRUE);
  clutter_timeline_start (data->timeline);

  hd_transition_play_sound (HDCM_WINDOW_CLOSED_SOUND);
}

void
hd_transition_notification(HdCompMgr                  *mgr,
                           MBWindowManagerClient      *c,
                           MBWMCompMgrClientEvent     event)
{
  MBWMCompMgrClutterClient * cclient;
  HDEffectData             * data;

  cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);

  /* Need to store also pointer to the manager, as by the time
   * the effect finishes, the back pointer in the cm_client to
   * MBWindowManagerClient is not longer valid/set.
   */
  data = g_new0 (HDEffectData, 1);
  data->event = event;
  data->cclient = mb_wm_object_ref (MB_WM_OBJECT (cclient));
  data->cclient_actor = g_object_ref (
      mb_wm_comp_mgr_clutter_client_get_actor( data->cclient ) );
  data->hmgr = HD_COMP_MGR (mgr);
  data->timeline = hd_transition_timeline_new("notification", event, 500);

  g_signal_connect (data->timeline, "new-frame",
                        G_CALLBACK (on_notification_timeline_new_frame), data);
  g_signal_connect (data->timeline, "completed",
                        G_CALLBACK (hd_transition_completed), data);

  mb_wm_comp_mgr_clutter_client_set_flags (cclient,
                              MBWMCompMgrClutterClientDontUpdate |
                              MBWMCompMgrClutterClientEffectRunning);
  hd_comp_mgr_set_effect_running(mgr, TRUE);

  /* first call to stop flicker */
  on_notification_timeline_new_frame(data->timeline, 0, data);
  /* Show the actor and add it to the front group */
  clutter_actor_show(data->cclient_actor);
  hd_render_manager_add_to_front_group(data->cclient_actor);
  /* Finally start the timeline... */
  clutter_timeline_start (data->timeline);
}

void
hd_transition_subview(HdCompMgr                  *mgr,
                      MBWindowManagerClient      *subview,
                      MBWindowManagerClient      *mainview,
                      MBWMCompMgrClientEvent     event)
{
  MBWMCompMgrClutterClient * cclient_subview;
  MBWMCompMgrClutterClient * cclient_mainview;
  gboolean                   mainview_in_trans, subview_in_trans;
  HDEffectData             * data;

  if (subview == mainview)
    { /* This happens sometimes for unknown reason. */
      g_critical ("hd_transition_subview: mainview == subview == %p",
                  subview);
      return;
    }
  if (!subview || !subview->cm_client || !mainview || !mainview->cm_client
      || !STATE_IS_APP(hd_render_manager_get_state()))
    return;

  cclient_subview = MB_WM_COMP_MGR_CLUTTER_CLIENT (subview->cm_client);
  cclient_mainview = MB_WM_COMP_MGR_CLUTTER_CLIENT (mainview->cm_client);

  /*
   * Handle views which are already in transition.
   * Two special cases are handled:
   * the client pushes a series of windows or it pops a series of windows.
   * The transitions would overlap but we can replace the finally-to-be-shown
   * actor, making it smooth.
   *
   * NOTE We exploit that currently only this transition sets
   * %HdCompMgrClient::effect and we use it to recognize ongoing
   * subview transitions.
   */
  subview_in_trans = mb_wm_comp_mgr_clutter_client_get_flags (cclient_subview)
    & MBWMCompMgrClutterClientEffectRunning;
  mainview_in_trans = mb_wm_comp_mgr_clutter_client_get_flags (cclient_mainview)
    & MBWMCompMgrClutterClientEffectRunning;
  if (subview_in_trans && mainview_in_trans)
    return;
  if (mainview_in_trans)
    { /* Is the mainview we want to leave sliding in? */
      if (event == MBWMCompMgrClientEventMap
          && (data = HD_COMP_MGR_CLIENT (cclient_mainview)->effect)
          && data->event == MBWMCompMgrClientEventMap
          && data->cclient == cclient_mainview)
        {
          /* Replace the effect's subview with ours. */
          /* Release @cclient and @cclient_actor. */
          clutter_actor_hide (data->cclient_actor);
          clutter_actor_set_anchor_pointu (data->cclient_actor, 0, 0);
          g_object_unref (data->cclient_actor);
          HD_COMP_MGR_CLIENT (data->cclient)->effect = NULL;
          mb_wm_comp_mgr_clutter_client_unset_flags (data->cclient,
                                      MBWMCompMgrClutterClientDontUpdate |
                                      MBWMCompMgrClutterClientEffectRunning);
          mb_wm_object_unref (MB_WM_OBJECT (data->cclient));

          /* Set @cclient_subview. */
          data->cclient = mb_wm_object_ref (MB_WM_OBJECT (cclient_subview));
          data->cclient_actor = g_object_ref (
              mb_wm_comp_mgr_clutter_client_get_actor (cclient_subview));
          mb_wm_comp_mgr_clutter_client_set_flags (cclient_subview,
                                      MBWMCompMgrClutterClientDontUpdate |
                                      MBWMCompMgrClutterClientEffectRunning);
          HD_COMP_MGR_CLIENT (cclient_subview)->effect = data;
        }
      return;
    }
  if (subview_in_trans) /* This is almost the same code. */
    { /* Is the subview we want to leave sliding in? */
      if (event == MBWMCompMgrClientEventUnmap
          && (data = HD_COMP_MGR_CLIENT (cclient_subview)->effect)
          && data->event == MBWMCompMgrClientEventUnmap
          && data->cclient2 == cclient_subview)
        {
          ClutterActor *o;

          /* Replace the effect's mainview with ours. */
          /* Release @cclient2 and @cclient2_actor. */
          clutter_actor_hide (data->cclient2_actor);
          clutter_actor_set_anchor_pointu (data->cclient2_actor, 0, 0);
          g_object_unref (o = data->cclient2_actor);
          HD_COMP_MGR_CLIENT (data->cclient2)->effect = NULL;
          mb_wm_comp_mgr_clutter_client_unset_flags (data->cclient2,
                                      MBWMCompMgrClutterClientDontUpdate |
                                      MBWMCompMgrClutterClientEffectRunning);
          mb_wm_object_unref (MB_WM_OBJECT (data->cclient2));

          /* Set @cclient_mainview. */
          data->cclient2 = mb_wm_object_ref (MB_WM_OBJECT (cclient_mainview));
          data->cclient2_actor = g_object_ref (
              mb_wm_comp_mgr_clutter_client_get_actor (cclient_mainview));
          mb_wm_comp_mgr_clutter_client_set_flags (cclient_mainview,
                                      MBWMCompMgrClutterClientDontUpdate |
                                      MBWMCompMgrClutterClientEffectRunning);
          HD_COMP_MGR_CLIENT (cclient_mainview)->effect = data;
        }
      return;
    }

  /* Need to store also pointer to the manager, as by the time
   * the effect finishes, the back pointer in the cm_client to
   * MBWindowManagerClient is not longer valid/set.
   */
  data = g_new0 (HDEffectData, 1);
  data->event = event;
  data->cclient = mb_wm_object_ref (MB_WM_OBJECT (cclient_subview));
  data->cclient_actor = g_object_ref (
      mb_wm_comp_mgr_clutter_client_get_actor( data->cclient ) );
  data->cclient2 = mb_wm_object_ref (MB_WM_OBJECT (cclient_mainview));
  data->cclient2_actor = g_object_ref (
      mb_wm_comp_mgr_clutter_client_get_actor( data->cclient2 ) );
  data->hmgr = HD_COMP_MGR (mgr);
  Transitions_running += data->fixup_visibilities = TRUE;
  data->timeline = hd_transition_timeline_new("subview", event, 250);

  g_signal_connect (data->timeline, "new-frame",
                        G_CALLBACK (on_subview_timeline_new_frame), data);
  g_signal_connect (data->timeline, "completed",
                        G_CALLBACK (hd_transition_completed), data);

  mb_wm_comp_mgr_clutter_client_set_flags (cclient_subview,
                              MBWMCompMgrClutterClientDontUpdate |
                              MBWMCompMgrClutterClientEffectRunning);
  mb_wm_comp_mgr_clutter_client_set_flags (cclient_mainview,
                                MBWMCompMgrClutterClientDontUpdate |
                                MBWMCompMgrClutterClientEffectRunning);

  hd_comp_mgr_set_effect_running(mgr, TRUE);
  HD_COMP_MGR_CLIENT (cclient_mainview)->effect = data;
  HD_COMP_MGR_CLIENT (cclient_subview)->effect  = data;

  /* first call to stop flicker */
  on_subview_timeline_new_frame(data->timeline, 0, data);
  clutter_timeline_start (data->timeline);
}

/* Stop any currently active transition on the given client (assuming the
 * 'effect' member of the cclient has been set). Currently this is only done
 * for subview. */
void
hd_transition_stop(HdCompMgr                  *mgr,
                   MBWindowManagerClient      *client)
{
  MBWMCompMgrClutterClient * cclient;
  HDEffectData             * data;

  cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (client->cm_client);

  if ((data = HD_COMP_MGR_CLIENT (cclient)->effect))
    {
      gint n_frames = clutter_timeline_get_n_frames(data->timeline);
      clutter_timeline_stop(data->timeline);
      /* Make sure we update to the final state for this transition */
      g_signal_emit_by_name (data->timeline, "new-frame",
                             n_frames, NULL);
      /* Call end-of-transition handler */
      hd_transition_completed(data->timeline, data);
    }
}

/* Start or finish a transition for the rotation
 * (moving into/out of blanking depending on first_part)
 */
static void
hd_transition_fade_and_rotate(gboolean first_part,
                              gboolean goto_portrait,
                              GCallback finished_callback,
                              gpointer finished_callback_data)
{
  ClutterColor black = {0x00, 0x00, 0x00, 0xFF};
  HDEffectData *data = g_new0 (HDEffectData, 1);
  data->event = first_part ? MBWMCompMgrClientEventMap :
                             MBWMCompMgrClientEventUnmap;
  data->timeline = hd_transition_timeline_new("rotate", data->event, 300);

  g_signal_connect (data->timeline, "new-frame",
                        G_CALLBACK (on_rotate_screen_timeline_new_frame), data);
  g_signal_connect (data->timeline, "completed",
                         G_CALLBACK (hd_transition_completed), data);
  if (finished_callback)
    g_signal_connect_swapped (data->timeline, "completed",
                          G_CALLBACK (finished_callback), finished_callback_data);

  data->angle = hd_transition_get_double("rotate", "angle", 40);
  /* Set the direction of movement - we want to rotate backwards if we
   * go back to landscape as it looks better */
  if (first_part == goto_portrait)
    data->angle *= -1;
  /* Add the actor we use to dim out the screen */
  data->particles[0] = g_object_ref(clutter_rectangle_new_with_color(&black));
  clutter_actor_set_name(data->particles[0], "black rotation background");
  clutter_actor_set_size(data->particles[0],
      hd_comp_mgr_get_current_screen_width (),
      hd_comp_mgr_get_current_screen_height ());
  clutter_container_add_actor(
            CLUTTER_CONTAINER(clutter_stage_get_default()),
            data->particles[0]);
  clutter_actor_show(data->particles[0]);
  if (!goto_portrait && first_part)
    {
      /* Add the actor we use to mask out the landscape part of the screen in the
       * portrait half of the animation. This is pretty nasty, but as the home
       * applets aren't repositioned they can sometimes be seen in the background.*/
       data->particles[1] = g_object_ref(clutter_rectangle_new_with_color(&black));
       clutter_actor_set_name(data->particles[1], "other rotation background");
       clutter_actor_set_position(data->particles[1],
           HD_COMP_MGR_LANDSCAPE_HEIGHT, 0);
       clutter_actor_set_size(data->particles[1],
           HD_COMP_MGR_LANDSCAPE_WIDTH-HD_COMP_MGR_LANDSCAPE_HEIGHT,
           HD_COMP_MGR_LANDSCAPE_HEIGHT);
       clutter_container_add_actor(
                 CLUTTER_CONTAINER(hd_render_manager_get()),
                 data->particles[1]);
       clutter_actor_show(data->particles[1]);
    }

  /* stop flicker by calling the first frame directly */
  on_rotate_screen_timeline_new_frame(data->timeline, 0, data);
  clutter_timeline_start (data->timeline);
}

/* Process %_MAEMO_ROTATION_PATIENCE requests. */
static void
patience (XClientMessageEvent *event, void *unused)
{
  /* Is somebody probing our patience? */
  if (event->message_type != hd_comp_mgr_get_atom (
                          HD_COMP_MGR (Orientation_change.wm->comp_mgr),
                          HD_ATOM_MAEMO_ROTATION_PATIENCE))
    return;

  if (event->data.l[0])
    { /* Grant @max:imal patience. */
      gint max;

      if (Orientation_change.timeout_id)
        {
          /* remaining := max(damage_timeout_max-elapsed, 0) */
          max  = hd_transition_get_int("rotate", "damage_timeout_max", 1000);
          max -= g_timer_elapsed(Orientation_change.timer, NULL) * 1000.0;
          if (max > 0)
            Orientation_change.timeout_id->remaining = max;
        }
      if (Orientation_change.phase <= WAIT_FOR_DAMAGES)
        Orientation_change.patience_requests++;
    }
  else
    { /* Get out of WAIT_FOR_DAMAGES as quickly as possible. */
      if (Orientation_change.patience_requests)
        Orientation_change.patience_requests--;
      if (!Orientation_change.patience_requests
          && Orientation_change.timeout_id)
        Orientation_change.timeout_id->remaining = 0;
    }
}

static gboolean
hd_transition_rotating_fsm(void)
{
  HDRMStateEnum state;
  gboolean change_state;

  g_debug ("%s: phase=%d, new_direction=%d, direction=%d", __FUNCTION__,
           Orientation_change.phase, Orientation_change.new_direction,
           Orientation_change.direction);

  switch (Orientation_change.phase)
    {
      case IDLE:
        Orientation_change.phase = TRANS_START;
        Orientation_change.direction = Orientation_change.new_direction;
        /* Take a screenshot of the screen as we currently are... */
        tidy_cached_group_changed(CLUTTER_ACTOR(hd_render_manager_get()));
        tidy_cached_group_set_render_cache(
          CLUTTER_ACTOR(hd_render_manager_get()), 1);
        tidy_cached_group_set_downsampling_factor(
          CLUTTER_ACTOR(hd_render_manager_get()), 1);
        /* Stop displaying the loading screenshot, which was displayed
         * as a small square just over the icon when launching phone.
         * However, leave it alone if it's already there fully grown. */
        if (hd_launcher_transition_is_playing())
          hd_render_manager_set_loading(NULL);
        /* Super massive extra large hack to remove status area when
         * launching phone app from the launcher */
        if (hd_render_manager_get_previous_state()==HDRM_STATE_LAUNCHER)
          {
            ClutterActor *hsm = hd_render_manager_get_status_area();
            if (hsm) clutter_actor_hide(hsm);
          }
        else if (STATE_IS_APP(hd_render_manager_get_previous_state()) &&
                 STATE_IS_APP(hd_render_manager_get_state()))
          /* Less super hack to not show wrong title while rotating - eg from
           * calculator to phone. We must only do it in this case, as if going
           * phone->desktop we will kill the title bar too soon. Or if going
           * phone->task_nav (which sets state to APP first) the buttons are
           * the wrong size. */
          hd_title_bar_update_now(HD_TITLE_BAR(hd_render_manager_get_title_bar()));
        /* Force redraw for screenshot *now*, before windows have a
         * chance to change.  Tell the render manager not to progress
         * the animation, it will be reset anyway. */
        tidy_blur_group_stop_progressing(
                                CLUTTER_ACTOR(hd_render_manager_get()));
        clutter_redraw(CLUTTER_STAGE(clutter_stage_get_default()));
        /* Start rotate transition */
        hd_util_set_rotating_property(Orientation_change.wm, TRUE);
        /* Layout windows at the rotated size */
        hd_util_set_screen_size_property(Orientation_change.wm,
                         Orientation_change.direction == GOTO_PORTRAIT);
        Orientation_change.wm->flags |= MBWindowManagerFlagLayoutRotated;
        mb_wm_layout_update(Orientation_change.wm->layout);
        /* We now call ourselves back on idle. The idea is that the sudden
         * influx of X events from resizing kills our animation as we don't
         * get to idle for a while. So only start the transition once we
         * got to idle at least once! */
        g_idle_add((GSourceFunc)(hd_transition_rotating_fsm), NULL);
        break;
      case TRANS_START:
        if (Orientation_change.direction == Orientation_change.new_direction)
          {
            /* Fade to black ((c) Metallica) */
            Orientation_change.phase = FADE_OUT;
            hd_transition_fade_and_rotate(
                            TRUE, Orientation_change.direction == GOTO_PORTRAIT,
                            G_CALLBACK(hd_transition_rotating_fsm), NULL);
            break;
          }
        else
          {
            Orientation_change.direction = Orientation_change.new_direction;
            goto trans_start_error;
          }
      case FADE_OUT:
        /*
         * We're faded out, now it is time to change HDRM state
         * if requested and possible.  Take care not to switch
         * to states which don't support the orientation we're
         * going to.
         */
        /* remove our flag to bodge layout - because we'll rotate properly
         * soon anyway */
        Orientation_change.wm->flags &= ~MBWindowManagerFlagLayoutRotated;
        /* Don't show our screenshot background any more */
        tidy_cached_group_changed(CLUTTER_ACTOR(hd_render_manager_get()));
        tidy_cached_group_set_render_cache(
                                  CLUTTER_ACTOR(hd_render_manager_get()), 0);
        tidy_cached_group_set_downsampling_factor(
                                  CLUTTER_ACTOR(hd_render_manager_get()), 0);

        state = Orientation_change.goto_state;
        change_state = Orientation_change.new_direction == GOTO_PORTRAIT
          ?  STATE_IS_PORTRAIT(state) || STATE_IS_PORTRAIT_CAPABLE(state)
          : !STATE_IS_PORTRAIT(state) && state != HDRM_STATE_UNDEFINED;
        if (change_state)
          {
            Orientation_change.goto_state = HDRM_STATE_UNDEFINED;
            hd_render_manager_set_state(state);
          }

        if (Orientation_change.direction == Orientation_change.new_direction)
          {
            /*
             * Wait for the screen change. During this period, blank the
             * screen by hiding %HdRenderManager. We wait here until
             * until damage_timeout ms has passed since the last damage event,
             * or until damage_timeout_max is reached. There is a lot of X
             * traffic during this time, so g_timeout is often delayed past
             * damage_timeout_max.
             */
            Orientation_change.phase = WAIT_FOR_ROOT_CONFIG;

            /* Don't allow anything inside the render manager to tell clutter
             * to redraw. actor_hide should have done this, but it doesn't.
             * We now need to totally blank the screen before the rotation,
             * so we explicitly call clutter to redraw *right now*. Note that
             * we don't do this on the stage, because it might conflict with
             * hd_dbus_system_bus_signal_handler */
            clutter_actor_set_allow_redraw(
                CLUTTER_ACTOR(hd_render_manager_get()), FALSE);
            clutter_actor_hide(CLUTTER_ACTOR(hd_render_manager_get()));
            clutter_redraw(CLUTTER_STAGE(clutter_stage_get_default()));

            hd_util_change_screen_orientation(Orientation_change.wm,
                         Orientation_change.direction == GOTO_PORTRAIT);
            Orientation_change.root_config_signal_id = g_signal_connect(
                           clutter_stage_get_default(), "notify::width",
                           G_CALLBACK(hd_transition_rotating_fsm), NULL);
            break;
          }
        else
          Orientation_change.direction = Orientation_change.new_direction;
        /* Fall through */
      case WAIT_FOR_ROOT_CONFIG:
      case WAIT_FOR_DAMAGES:
trans_start_error:
        if (Orientation_change.direction != Orientation_change.new_direction)
          {
            if (Orientation_change.root_config_signal_id)
              {
                g_signal_handler_disconnect(clutter_stage_get_default(),
                              Orientation_change.root_config_signal_id);
                Orientation_change.root_config_signal_id = 0;
              }
            Orientation_change.direction = Orientation_change.new_direction;
            Orientation_change.phase = FADE_OUT;
            hd_transition_rotating_fsm();
          }
        else if (Orientation_change.phase == WAIT_FOR_ROOT_CONFIG)
          {
            g_assert(Orientation_change.root_config_signal_id);
            g_signal_handler_disconnect(clutter_stage_get_default(),
                                Orientation_change.root_config_signal_id);
            Orientation_change.root_config_signal_id = 0;

            /* Call hd_util_change_screen_orientation()s finishing
             * counterpart. */
            hd_util_root_window_configured(Orientation_change.wm);

            g_assert(!Orientation_change.timeout_id);
            Orientation_change.timeout_id = hptimer_new(
                  Orientation_change.patience_requests
                    ? hd_transition_get_int("rotate", "damage_timeout_max",
                                            1000)
                    : hd_transition_get_int("rotate", "damage_timeout", 50),
                  (GSourceFunc)hd_transition_rotating_fsm,
                  &Orientation_change.timeout_id,
                  (GDestroyNotify)g_nullify_pointer);
            g_timer_start(Orientation_change.timer);
            Orientation_change.phase = WAIT_FOR_DAMAGES;
          }
        else /* WAIT_FOR_DAMAGES || FADE_OUT error path || TRANS_START error */
          {
            /* We must update the layout again so the window sizes
             * return to normal relative to the screen. flags is probably
             * already correct. But just for safety. */
            hd_util_set_screen_size_property(Orientation_change.wm,
                         Orientation_change.direction == GOTO_PORTRAIT);
            Orientation_change.wm->flags &= ~MBWindowManagerFlagLayoutRotated;
            mb_wm_layout_update(Orientation_change.wm->layout);
            if (Orientation_change.phase > TRANS_START)
              { /* Fade back in */
                /* Undo the redraw stopping that happened in FADE_OUT */
                Orientation_change.phase = FADE_IN;
                clutter_actor_set_allow_redraw(
                                CLUTTER_ACTOR(hd_render_manager_get()), TRUE);
                clutter_actor_show(CLUTTER_ACTOR(hd_render_manager_get()));
                hd_transition_fade_and_rotate(
                        FALSE, Orientation_change.direction == GOTO_PORTRAIT,
                        G_CALLBACK(hd_transition_rotating_fsm), NULL);
                /* Fix NB#117109 by re-evaluating what is blurred and what isn't */
                hd_render_manager_restack();
              }
            else
              {
                /*
                 * Direction changed in TRANS_START.  Half of what IDLE did
                 * is already undone, but the blur group is still frozen.
                 * Let's wait a bit until the windows are reconfigured again,
                 * then toast it.
                 */
                Orientation_change.phase = RECOVER;
                g_idle_add((GSourceFunc)(hd_transition_rotating_fsm), NULL);
              }
          }
        break;
      case RECOVER:
        /* Thaw the blur group.  Only from the TRANS_START error path can we
         * get here. */
        tidy_cached_group_changed(CLUTTER_ACTOR(hd_render_manager_get()));
        tidy_cached_group_set_render_cache(
                                  CLUTTER_ACTOR(hd_render_manager_get()), 0);
        tidy_cached_group_set_downsampling_factor(
                                  CLUTTER_ACTOR(hd_render_manager_get()), 0);
        /* Fall through */
      case FADE_IN:
        {
          ClutterActor *actor;

          /* Reset values in case for some reason the timeline failed to do it */
          actor = CLUTTER_ACTOR(hd_render_manager_get());
          clutter_actor_set_rotation(actor, CLUTTER_X_AXIS, 0, 0, 0, 0);
          clutter_actor_set_rotation(actor, CLUTTER_Y_AXIS, 0, 0, 0, 0);
          clutter_actor_set_depthu(actor, 0);

          Orientation_change.phase = IDLE;
          if (Orientation_change.direction != Orientation_change.new_direction)
            /* No sense resetting the rotating property. */
            hd_transition_rotating_fsm();
          else
            { /* We're finally settled. */
              hd_comp_mgr_reconsider_compositing (
                            Orientation_change.wm->comp_mgr);
              hd_util_set_rotating_property (Orientation_change.wm, FALSE);
              Orientation_change.patience_requests = 0;
            }
          break;
        }
    }

  return FALSE;
}

/* Start changing the screen's orientation by rotating 90 degrees
 * (portrait mode) or going back to landscape.  Returns FALSE if
 * orientation changing won't take place. */
gboolean
hd_transition_rotate_screen (MBWindowManager *wm, gboolean goto_portrait)
{ g_debug("%s(goto_portrait=%d)", __FUNCTION__, goto_portrait);
  static unsigned long cmsg_id;

  /* Initialize the fsm infrastructure. */
  Orientation_change.wm = wm;
  if (!Orientation_change.timer)
    Orientation_change.timer = g_timer_new();
  if (!cmsg_id)
    cmsg_id = mb_wm_main_context_x_event_handler_add (wm->main_ctx,
                                               wm->root_win->xwindow,
                                               ClientMessage,
                                               (MBWMXEventFunc)patience,
                                               NULL);

  Orientation_change.new_direction = goto_portrait
    ? GOTO_PORTRAIT : GOTO_LANDSCAPE;
  if (Orientation_change.phase == IDLE)
    {
      if (goto_portrait == hd_comp_mgr_is_portrait ())
        {
          g_debug("%s: already in %s mode", __FUNCTION__,
                  goto_portrait ? "portrait" : "landscape");
          return FALSE;
        }

      hd_transition_rotating_fsm();
    }
  else
    g_debug("divert");

  return TRUE;
}

/*
 * Asks the rotating machine to switch to @state if possible
 * when it's faded out.  We'll switch state with best effort,
 * but no promises.  Only effective if a rotation transition
 * is underway.
 */
void
hd_transition_rotate_screen_and_change_state (HDRMStateEnum state)
{
  Orientation_change.goto_state = state;
}

/* Returns whether the rotating fsm is going to change hdrm state.
 * In theory it may be wrong but in practice it's only used for
 * APP->TASK_NAV and hdrm make sure @goto_state is cleared otherwise. */
gboolean
hd_transition_rotation_will_change_state (void)
{
  return (Orientation_change.phase == TRANS_START
          || Orientation_change.phase == FADE_OUT)
    && Orientation_change.goto_state != HDRM_STATE_UNDEFINED;
}

/* Are we in the middle of a change? */
gboolean
hd_transition_is_rotating (void)
{
  return Orientation_change.phase != IDLE;
}

/* Are we about to change to portrait mode? */
gboolean
hd_transition_is_rotating_to_portrait (void)
{
  return Orientation_change.phase != IDLE
    && Orientation_change.direction == GOTO_PORTRAIT;
}

/* Returns whether we are in a state where we should ignore any
 * damage requests. This also checks and possibly prolongs how long
 * we stay in the WAITING state, so we can be sure that all windows
 * have updated before we fade back from black. */
gboolean
hd_transition_rotate_ignore_damage()
{
  if (Orientation_change.phase == WAIT_FOR_ROOT_CONFIG)
    return TRUE;
  if (Orientation_change.phase == WAIT_FOR_DAMAGES)
    {
      gint max;

      /*
       * Only postpone the timeout if we haven't postponed
       * it too long already. This stops us getting stuck
       * in the WAITING state if an app keeps redrawing.
       *
       * remaining := min(max(remaining, damage_timeout_plus),
       *                  max(damage_timeout_max-elapsed, 0))
       */
      max  = hd_transition_get_int("rotate", "damage_timeout_max", 1000);
      max -= g_timer_elapsed(Orientation_change.timer, NULL) * 1000.0;
      if (max > 0)
        {
          gint remaining;

          remaining = hd_transition_get_int("rotate", "damage_timeout_plus",
                                            50);
          if (Orientation_change.timeout_id->remaining < remaining)
            Orientation_change.timeout_id->remaining = remaining;
          if (Orientation_change.timeout_id->remaining > max)
            Orientation_change.timeout_id->remaining = max;
        }
      else
        Orientation_change.timeout_id->remaining = 0;

      return TRUE;
    }
  return FALSE;
}

/* Returns whether @actor will last only as long as the effect
 * (if it has any) takes.  Currently only subview transitions
 * are considered. */
gboolean
hd_transition_actor_will_go_away (ClutterActor *actor)
{
  HdCompMgrClient *hcmgrc;

  if (!(hcmgrc = g_object_get_data(G_OBJECT(actor),
                                   "HD-MBWMCompMgrClutterClient")))
    return FALSE;
  if (!hcmgrc->effect)
    return FALSE;
  if (hcmgrc->effect->event == MBWMCompMgrClientEventMap)
    return hcmgrc->effect->cclient2 == MB_WM_COMP_MGR_CLUTTER_CLIENT (hcmgrc);
  if (hcmgrc->effect->event == MBWMCompMgrClientEventUnmap)
    return hcmgrc->effect->cclient  == MB_WM_COMP_MGR_CLUTTER_CLIENT (hcmgrc);
  return FALSE;
}

/* Start playing @fname asynchronously. */
void
hd_transition_play_sound (const gchar * fname)
{
    static ca_context *ca;
    ca_proplist *pl;
    int ret;
    GTimer *timer;
    gint millisec;

    /* Canberra uses threads. */
    if (hd_volume_profile_is_silent() || hd_disable_threads())
      return;

    /* Initialize the canberra context once. */
    if (!ca)
      {
        if ((ret = ca_context_create (&ca)) != CA_SUCCESS)
          {
            g_warning("ca_context_create: %s", ca_strerror (ret));
            return;
          }
        else if ((ret = ca_context_open (ca)) != CA_SUCCESS)
          {
            g_warning("ca_context_open: %s", ca_strerror (ret));
            ca_context_destroy(ca);
            ca = NULL;
            return;
          }
      }

    timer = g_timer_new();

    ca_proplist_create (&pl);
    ca_proplist_sets (pl, CA_PROP_CANBERRA_CACHE_CONTROL, "permanent");
    ca_proplist_sets (pl, CA_PROP_MEDIA_FILENAME, fname);
    ca_proplist_sets (pl, CA_PROP_MEDIA_ROLE, "event");
    /* set the volume */
    ca_proplist_sets (pl, "module-stream-restore.id", "x-maemo-system-sound");

    if ((ret = ca_context_play_full (ca, 0, pl, NULL, NULL)) != CA_SUCCESS)
      g_warning("%s: %s", fname, ca_strerror (ret));

    ca_proplist_destroy(pl);
    millisec = (gint)(g_timer_elapsed(timer, 0)*1000);
    g_timer_destroy(timer);

    if (millisec > 100) /* [Bug 105635] */
      g_warning("%s: ca_context_play_full is blocking for %d ms to play %s",
          __FUNCTION__, millisec, fname);

}

/* We want to call this when the theme changes, as transitions.ini *could*
 * be loaded from the theme. */
void
hd_transition_set_file_changed(void) {
  g_debug("%s: setting transitions.ini modified", __FUNCTION__);
  transitions_ini_is_dirty = 2*TRUE;
}

static gboolean
transitions_ini_changed(GIOChannel *chnl, GIOCondition cond, gpointer unused)
{
  struct inotify_event ibuf;

  if (g_io_channel_read_chars(chnl, (void *)&ibuf, sizeof(ibuf), NULL, NULL)
      != G_IO_STATUS_NORMAL)
    {
      g_warning("%s: g_io_channel_read_chars failed", __func__);
    }
  else if (ibuf.mask & (IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF | IN_IGNORED))
    {
      g_debug("disposing transitions.ini");
      transitions_ini_is_dirty++;

      /* Track no more if the dirent changed or disappeared. */
      if (ibuf.mask & (IN_MOVE_SELF | IN_DELETE_SELF | IN_IGNORED))
        {
          g_debug("watching no more");
          transitions_ini_is_dirty++;
        }
    }
  return TRUE;
}

static GKeyFile *
hd_transition_get_keyfile(void)
{
  static GKeyFile *transitions_ini;
  static GIOChannel *transitions_ini_watcher;
  GError *error;
  GKeyFile *ini;
  const char *fname;

  if (transitions_ini && !transitions_ini_is_dirty)
    return transitions_ini;

  /* Check for a file in the theme directory first, otherwise
   * use the one in the hildon-desktop dir */
  fname = g_file_test(TRANSITIONS_INI_FROM_THEME, G_FILE_TEST_EXISTS)
    ? TRANSITIONS_INI_FROM_THEME : TRANSITIONS_INI;
  g_debug("%s: %s %s", __FUNCTION__,
          transitions_ini_is_dirty ? "reloading" : "loading", fname);

  error = NULL;
  ini = g_key_file_new();
  if (!g_key_file_load_from_file (ini, fname, 0, &error))
    { /* Use the previous @transitions_ini. */
      g_warning("%s: couldn't load %s: %s", __FUNCTION__,
                fname, error->message);
      if (!transitions_ini)
        g_warning("%s: using default settings", __FUNCTION__);
      g_error_free(error);
      g_key_file_free(ini);
      return transitions_ini;
    }

  /* Use the new @transitions_ini. */
  if (transitions_ini)
    g_key_file_free(transitions_ini);
  transitions_ini = ini;

  if (!transitions_ini_watcher || transitions_ini_is_dirty > TRUE)
    {
      static int inofd = -1, watch = -1;

      /* Create an inotify if we haven't. */
      if (inofd < 0)
        {
          if ((inofd = inotify_init()) < 0)
            {
              g_warning("inotify_init: %s", strerror(errno));
              goto out;
            }

          g_assert(!transitions_ini_watcher);
          transitions_ini_watcher = g_io_channel_unix_new(inofd);
          g_io_channel_set_encoding(transitions_ini_watcher, NULL, NULL);
          g_io_add_watch(transitions_ini_watcher, G_IO_IN,
                         transitions_ini_changed, NULL);
        }
      else if (watch >= 0)
        /* Remove the previous watch. */
        inotify_rm_watch(inofd, watch);

      g_assert(transitions_ini_watcher != NULL);
      watch = inotify_add_watch(inofd, fname,
                                IN_MODIFY|IN_MOVE_SELF|IN_DELETE_SELF);
      if (watch < 0)
        {
          g_warning("inotify_add_watch: %s", strerror(errno));
          goto out;
        }

      g_debug("watching %s", fname);
    }

  /* Stop reloading @transitions_ini if we can watch it. */
  transitions_ini_is_dirty = FALSE;

out:
  return transitions_ini;
}

gint
hd_transition_get_int(const gchar *transition, const char *key,
                      gint default_val)
{
  gint value;
  GError *error;
  GKeyFile *ini;

  if (!(ini = hd_transition_get_keyfile()))
    return default_val;

  error = NULL;
  value = g_key_file_get_integer(ini, transition, key, &error);
  if (error)
    {
      g_debug("couldn't read int %s::%s from transitions.ini: %s",
                transition, key, error->message);
      g_error_free(error);
      return default_val;
    }

  return value;
}

gdouble
hd_transition_get_double(const gchar *transition,
                         const char *key, gdouble default_val)
{
  gdouble value;
  GError *error;
  GKeyFile *ini;

  if (!(ini = hd_transition_get_keyfile()))
    return default_val;

  error = NULL;
  value = g_key_file_get_double (ini, transition, key, &error);
  if (error)
    {
      g_debug("couldn't read double %s::%s from transitions.ini: %s",
                transition, key, error->message);
      g_error_free(error);
      return default_val;
    }

  return value;
}

/* Returns a newly-allocated string that must *always* be freed by the caller */
gchar *
hd_transition_get_string(const gchar *transition, const char *key,
                      gchar *default_val)
{
  gchar *value;
  GError *error;
  GKeyFile *ini;

  if (!(ini = hd_transition_get_keyfile()))
    return default_val;

  error = NULL;
  value = g_key_file_get_string (ini, transition, key, &error);
  if (error)
    {
      g_debug("couldn't read string %s::%s from transitions.ini: %s",
                transition, key, error->message);
      g_error_free(error);
      return g_strdup(default_val);
    }

  return value;
}

HdKeyFrameList *
hd_transition_get_keyframes(const gchar *transition, const char *key,
                            gchar *default_val)
{
  char *keyframetext = hd_transition_get_string(transition, key, default_val);
  HdKeyFrameList *keyframes = hd_key_frame_list_create(keyframetext);
  g_free(keyframetext);
  return keyframes;
}
