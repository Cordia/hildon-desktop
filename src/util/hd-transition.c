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

#include <clutter/clutter.h>
#include <math.h>
#include <canberra.h>

#include "hd-transition.h"
#include "hd-comp-mgr.h"
#include "hd-gtk-style.h"
#include "hd-render-manager.h"
#include "hildon-desktop.h"
#include "hd-theme.h"
#include "hd-clutter-cache.h"
#include "tidy/tidy-sub-texture.h"

#include "hd-app.h"

#define HDCM_UNMAP_PARTICLES 8

#define HD_EFFECT_PARTICLE "white-particle.png"
#define HDCM_NOTIFICATION_END_SIZE 32

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
  /* original/expected position of application/menu */
  ClutterGeometry           geo;
  /* Any extra particles if they are used for this effect */
  ClutterActor             *particles[HDCM_UNMAP_PARTICLES];
} HDEffectData;

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
  smooth_ramp = 1.0f - cos(amt*3.141592);
  converge = sin(0.5*3.141592*(1-amt));
  return offset + (smooth_ramp*0.75)*converge + (1-converge);
}

/* amt goes from 0->1, and the result goes from 0->1 smoothly */
float
hd_transition_smooth_ramp(float amt)
{
  return (1.0f - cos(amt*3.141592)) * 0.5f;
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

/* For the animated progress indicator in the title bar */
void
on_decor_progress_timeline_new_frame(ClutterTimeline *timeline,
                                     gint frame_num,
                                     ClutterActor *progress_texture)
{
  if (TIDY_IS_SUB_TEXTURE(progress_texture) &&
      CLUTTER_ACTOR_IS_VISIBLE(progress_texture))
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

      /* FIXME: We really want to set this to queue damage with an area -
       * like we do for windows. Otherwise we end up updating the whole
       * screen for this. */
      clutter_actor_queue_redraw(progress_texture);
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

  actor = data->cclient_actor;
  if (!CLUTTER_IS_ACTOR(actor))
    return;

  pop_top = data->geo.y==0;
  pop_bottom = data->geo.y+data->geo.height==HD_COMP_MGR_SCREEN_HEIGHT;
  amt =  (float)clutter_timeline_get_progress(timeline);
  /* reverse if we're removing this */
  if (data->event == MBWMCompMgrClientEventUnmap)
    amt = 1-amt;

  overshoot = hd_transition_overshoot(amt);

  if (pop_top)
    {
      status_low = -data->geo.height;
      status_high = data->geo.y;
    }
  else if (pop_bottom)
    {
      status_low = data->geo.y+data->geo.height;
      status_high = data->geo.y;
    }
  else
    {
      status_low = data->geo.y;
      status_high = data->geo.y;
    }
  status_pos = status_low*(1-overshoot) + status_high*overshoot;

  /*clutter_actor_set_positionu(actor,
                             CLUTTER_INT_TO_FIXED(data->geo.x),
                             CLUTTER_FLOAT_TO_FIXED(status_pos));*/
  clutter_actor_set_anchor_pointu(actor, 0,
      CLUTTER_INT_TO_FIXED(data->geo.y) - CLUTTER_FLOAT_TO_FIXED(status_pos));
  clutter_actor_set_opacity(actor, (int)(255*amt));

  /* use a slither of filler to fill in the gap where the menu
   * has jumped a bit too far up */
  filler = data->particles[0];
  if (filler)
    {
      if ((status_pos<=status_high && pop_top) ||
          (status_pos>=status_high && !pop_top))
        clutter_actor_hide(filler);
      else
        {
          clutter_actor_show(filler);
          clutter_actor_set_opacity(filler, (int)(255*amt));
          if (pop_top)
            {
              clutter_actor_set_positionu(filler,
                        CLUTTER_INT_TO_FIXED(data->geo.x),
                        status_high);
              clutter_actor_set_sizeu(filler,
                        CLUTTER_INT_TO_FIXED(data->geo.width),
                        CLUTTER_FLOAT_TO_FIXED(status_pos-status_high));
            }
          else if (pop_bottom)
            {
              clutter_actor_set_positionu(filler,
                        CLUTTER_INT_TO_FIXED(data->geo.x),
                        CLUTTER_FLOAT_TO_FIXED(status_pos + data->geo.height));
              clutter_actor_set_sizeu(filler,
                        CLUTTER_INT_TO_FIXED(data->geo.width),
                        CLUTTER_FLOAT_TO_FIXED(status_high-status_pos));
            }
        }
    }
}

static void
on_fade_timeline_new_frame(ClutterTimeline *timeline,
                            gint frame_num, HDEffectData *data)
{
  float amt;
  ClutterActor *actor;

  actor = data->cclient_actor;
  if (!CLUTTER_IS_ACTOR(actor))
    return;

  amt =  (float)clutter_timeline_get_progress(timeline);
  /* reverse if we're removing this */
  if (data->event == MBWMCompMgrClientEventUnmap)
    amt = 1-amt;

  clutter_actor_set_opacity(actor, (int)(255*amt));
}

static void
on_close_timeline_new_frame(ClutterTimeline *timeline,
                            gint frame_num, HDEffectData *data)
{
  float amt;
  ClutterActor *actor;
  float amtx, amty, amtp;
  int centrex, centrey;
  float particle_opacity, particle_radius;
  gint i;

  actor = data->cclient_actor;
  if (!CLUTTER_IS_ACTOR(actor))
    return;

  amt = (float)clutter_timeline_get_progress(timeline);

  amtx = 1.6 - amt*2;
  amty = 1 - amt*2;
  amtp = amt*2 - 1;
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
  for (i=0;i<HDCM_UNMAP_PARTICLES;i++)
    if (data->particles[i] && (amtp > 0) && (amtp < 1))
      {
        /* space particles equally and rotate once */
        float ang = i * 15 +
                    amtp * 3.141592f / 2;
        float radius = particle_radius * i / HDCM_UNMAP_PARTICLES;
        /* twinkle effect */
        float opacity = particle_opacity * ((1-cos(amt*50+i)) * 0.5f);
        clutter_actor_show( data->particles[i] );
        clutter_actor_set_opacity(data->particles[i],
                (int)(255 * opacity));

        clutter_actor_set_positionu(data->particles[i],
                CLUTTER_FLOAT_TO_FIXED(centrex + sin(ang) * radius),
                CLUTTER_FLOAT_TO_FIXED(centrey + cos(ang) * radius));
      }
    else
      if (data->particles[i])
	clutter_actor_hide( data->particles[i] );
}

static void
on_notification_timeline_new_frame(ClutterTimeline *timeline,
                                   gint frame_num, HDEffectData *data)
{
  float amt;
  ClutterActor *actor;
  guint width, height;

  actor = data->cclient_actor;
  if (!CLUTTER_IS_ACTOR(actor))
    return;

  clutter_actor_get_size(actor, &width, &height);

  amt = frame_num / (float)clutter_timeline_get_n_frames(timeline);
  amt = hd_transition_smooth_ramp( amt );
  if (data->event == MBWMCompMgrClientEventUnmap)
    {
      /* Closing Animation - we shrink down into the top-left button's area,
       * then fade out.*/
      float a1 = MIN(amt*2,1);
      float a2 = MAX(amt*2-1,0);
      /* We set anchor point so if the notification
       * resizes/positions in flight, we're ok */
      float min_scale = HDCM_NOTIFICATION_END_SIZE / (float)width;
      float scale = (1-a1) + a1*min_scale;
      float corner_x = a1*(HD_COMP_MGR_TOP_LEFT_BTN_WIDTH*0.5f/scale
                           - width*0.5);
      float corner_y = a1*(HD_COMP_MGR_TOP_LEFT_BTN_HEIGHT*0.5f/scale
                           - height*0.5);

      clutter_actor_set_opacity(actor, (int)(255*(1-a2)));
      clutter_actor_set_scale(actor, scale, scale);
      clutter_actor_set_anchor_pointu(actor,
         CLUTTER_FLOAT_TO_FIXED( -corner_x ),
         CLUTTER_FLOAT_TO_FIXED( -corner_y ) );
    }
  else
    {
      /* Opening Animation - we fade in, and move in from the top-right
       * edge of the screen in an arc */
      float scale =  1 + (1-amt)*0.5f;
      float ang = amt * 3.141592f * 0.5f;
      float corner_x = HD_COMP_MGR_SCREEN_WIDTH * 0.5f * cos(ang);
      float corner_y = (sin(ang)-1) * height;
      /* We set anchor point so if the notification
       * resizes/positions in flight, we're ok */
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
    corner_x = (1-amt) * HD_COMP_MGR_SCREEN_WIDTH;
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
           CLUTTER_FLOAT_TO_FIXED( -(corner_x - HD_COMP_MGR_SCREEN_WIDTH) ),
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

/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

/* #ClutterStage's notify::allocation callback to notice if we are
 * switching between landscape and portrait modes duing an effect. */
static void
on_screen_size_changed (ClutterActor *stage, GParamSpec *unused,
                        HDEffectData *data)
{
  gint tmp;
  guint scrw, scrh;
  ClutterActor *actor;

  /* Rotate @actor back to the mode it is layed out for.
   * Assume it's anchored in the middle. */
  clutter_actor_get_size (stage, &scrw, &scrh);
  actor = mb_wm_comp_mgr_clutter_client_get_actor (data->cclient);

  /* It is very interesting to observe the dualism here. */
  if (scrw > scrh)
    { /* Coming from portrait to landscape. */
      clutter_actor_set_rotation (actor, CLUTTER_Z_AXIS, -90, 0, 0, 0);

      tmp = data->geo.x;
      data->geo.x = data->geo.y;
      data->geo.y = scrh - (tmp + data->geo.width);
    }
  else
    { /* Coming from landscape to portrait. */
      clutter_actor_set_rotation (actor, CLUTTER_Z_AXIS, +90, 0, 0, 0);

      tmp = data->geo.y;
      data->geo.y = data->geo.x;
      data->geo.x = scrw - (tmp + data->geo.height);
    }

  tmp = data->geo.width;
  data->geo.width = data->geo.height;
  data->geo.height = tmp;

  clutter_actor_set_position (actor,
                              data->geo.x + data->geo.width/2,
                              data->geo.y + data->geo.height/2);
}

static void
hd_transition_completed (ClutterActor* timeline, HDEffectData *data)
{
  gint i;
  HdCompMgr *hmgr = HD_COMP_MGR (data->hmgr);

  if (data->cclient)
    {
      mb_wm_comp_mgr_clutter_client_unset_flags (data->cclient,
                                        MBWMCompMgrClutterClientDontUpdate |
                                        MBWMCompMgrClutterClientEffectRunning);

      mb_wm_object_unref (MB_WM_OBJECT (data->cclient));

      if (data->event == MBWMCompMgrClientEventUnmap && data->cclient_actor)
        {
          ClutterActor *parent = clutter_actor_get_parent(data->cclient_actor);
          if (CLUTTER_IS_CONTAINER(parent))
            clutter_container_remove_actor(
                CLUTTER_CONTAINER(parent), data->cclient_actor );
        }
    }

  if (data->cclient_actor)
    g_object_unref(data->cclient_actor);

  if (data->cclient2)
    {
      mb_wm_comp_mgr_clutter_client_unset_flags (data->cclient2,
                                        MBWMCompMgrClutterClientDontUpdate |
                                        MBWMCompMgrClutterClientEffectRunning);
      mb_wm_object_unref (MB_WM_OBJECT (data->cclient2));
    }

  if (data->cclient2_actor)
    g_object_unref(data->cclient2_actor);

/*   dump_clutter_tree (CLUTTER_CONTAINER (clutter_stage_get_default()), 0); */

  g_object_unref ( timeline );

  hd_comp_mgr_set_effect_running(hmgr, FALSE);

  for (i=0;i<HDCM_UNMAP_PARTICLES;i++)
    if (data->particles[i])
      clutter_actor_destroy(data->particles[i]);

  g_signal_handlers_disconnect_by_func (clutter_stage_get_default (),
                                        G_CALLBACK (on_screen_size_changed),
                                        data);

  g_free (data);
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
  data->timeline =
        g_object_ref( hd_transition_timeline_new("popup", event, 250) );
  g_signal_connect (data->timeline, "new-frame",
                        G_CALLBACK (on_popup_timeline_new_frame), data);
  g_signal_connect (data->timeline, "completed",
                        G_CALLBACK (hd_transition_completed), data);
  data->geo = geo;

  mb_wm_comp_mgr_clutter_client_set_flags (cclient,
                              MBWMCompMgrClutterClientDontUpdate |
                              MBWMCompMgrClutterClientEffectRunning);
  hd_comp_mgr_set_effect_running(mgr, TRUE);

  /* Add actor for the background when we pop a bit too far */
  data->particles[0] = clutter_rectangle_new();
  clutter_actor_hide(data->particles[0]);
  clutter_container_add_actor(
            CLUTTER_CONTAINER(clutter_actor_get_parent(actor)),
            data->particles[0]);
  hd_gtk_style_get_bg_color(HD_GTK_BUTTON_SINGLETON, GTK_STATE_NORMAL,
                              &col);
  clutter_rectangle_set_color(CLUTTER_RECTANGLE(data->particles[0]),
                              &col);

  /* first call to stop flicker */
  on_popup_timeline_new_frame(data->timeline, 0, data);
  clutter_timeline_start (data->timeline);
}

void
hd_transition_fade(HdCompMgr                  *mgr,
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
  data->timeline =
        g_object_ref( hd_transition_timeline_new("fade", event, 250) );
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
  gchar                    * fname;
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
  data->timeline = g_object_ref(
                clutter_timeline_new_for_duration (
                    hd_transition_get_int("app_close", "duration", 500) ) );
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

  fname = g_build_filename (HD_DATADIR, HD_EFFECT_PARTICLE, NULL);

  for (i = 0; i < HDCM_UNMAP_PARTICLES; ++i)
    {
      data->particles[i] = hd_clutter_cache_get_texture(fname, FALSE);
      if (data->particles[i])
        {
          clutter_actor_set_anchor_point_from_gravity(data->particles[i],
                                                      CLUTTER_GRAVITY_CENTER);
          clutter_container_add_actor(parent, data->particles[i]);
          clutter_actor_hide(data->particles[i]);
        }
    }

  g_free (fname);
  hd_comp_mgr_set_effect_running(mgr, TRUE);
  clutter_timeline_start (data->timeline);

  hd_transition_play_sound ("/usr/share/sounds/ui-window_close.wav");
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
  data->timeline =
        g_object_ref( hd_transition_timeline_new("notification", event, 500) );

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
  HDEffectData             * data;

  if (!subview || !subview->cm_client || !mainview || !mainview->cm_client
      || !STATE_IS_APP(hd_render_manager_get_state()))
    return;

  cclient_subview = MB_WM_COMP_MGR_CLUTTER_CLIENT (subview->cm_client);
  cclient_mainview = MB_WM_COMP_MGR_CLUTTER_CLIENT (mainview->cm_client);

  if ((mb_wm_comp_mgr_clutter_client_get_flags (cclient_subview) &
      MBWMCompMgrClutterClientEffectRunning) ||
      (mb_wm_comp_mgr_clutter_client_get_flags (cclient_mainview) &
            MBWMCompMgrClutterClientEffectRunning))
    return;

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
  data->timeline =
        g_object_ref( hd_transition_timeline_new("subview", event, 250) );

  g_signal_connect (data->timeline, "new-frame",
                        G_CALLBACK (on_subview_timeline_new_frame), data);
  g_signal_connect (data->timeline, "completed",
                        G_CALLBACK (hd_transition_completed), data);

  mb_wm_comp_mgr_clutter_client_set_flags (cclient_subview,
                              MBWMCompMgrClutterClientDontUpdate |
                              MBWMCompMgrClutterClientEffectRunning);
  mb_wm_comp_mgr_clutter_client_set_flags (cclient_mainview,
                                /* Stopping updates for main view seems
                                 * to stop them permanently so workaround
                                 * it by not disabling them. */
                                MBWMCompMgrClutterClientEffectRunning);
  hd_comp_mgr_set_effect_running(mgr, TRUE);

  /* first call to stop flicker */
  on_subview_timeline_new_frame(data->timeline, 0, data);
  clutter_timeline_start (data->timeline);
}


/* Tell play() now it's free to play. */
static void
play_finished (ca_context *ctx, uint32_t id, int error_code, void * is_playing)
{
  *(gboolean *)is_playing = FALSE;
}

/* Start playing @fname asynchronously. */
void
hd_transition_play_sound (const gchar * fname)
{
    static ca_context *ca;
    static gboolean is_playing;
    ca_proplist *pl;
    int ret;
    GTimer *timer;
    gint millisec;

    /* FIXME: Take this out when Bug 105635 is fixed */
    g_warning("%s: Canberra sound disabled for speed reasons, %s",
              __FUNCTION__, fname);
    return;

    /* Canberra uses threads. */
    if (hd_disable_threads())
      return;

    /* Canberra may not like it to play multiple sounds at a time
     * with the same context.  This may be totally bogus, though. */
    if (is_playing)
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
    if ((ret = ca_context_play_full (ca, 0, pl, play_finished,
                                     &is_playing)) != CA_SUCCESS)
      g_warning("%s: %s", fname, ca_strerror (ret));
    ca_proplist_destroy(pl);
    is_playing = TRUE;
    millisec = (gint)(g_timer_elapsed(timer, 0)*1000);
    g_timer_destroy(timer);

    if (millisec > 100) /* [Bug 105635] */
      g_debug("%s: ca_context_play_full is blocking for %d ms to play %s",
          __FUNCTION__, millisec, fname);

}

static GKeyFile *
hd_transition_get_keyfile(const gchar *transition,
                      const char *key)
{
  GError *error = NULL;
  GKeyFile *key_file = NULL;
  const char *full_path = "/usr/share/hildon-desktop/transitions.ini";

  key_file = g_key_file_new ();
  g_key_file_load_from_file (key_file, full_path, 0, &error);
  if (error)
    {
      g_warning ("Unable to load file  '%s' : %s",
                 full_path,
                 error->message);

      g_error_free (error);
      g_key_file_free (key_file);
      return NULL;
    }

  if (!g_key_file_has_group (key_file, transition))
    {
      g_key_file_free (key_file);
      return NULL;
    }

  if (!g_key_file_has_key (key_file,
                           transition,
                           key,
                           &error))
    {
      g_key_file_free (key_file);
      return NULL;
    }

  return key_file;
}

gint
hd_transition_get_int(const gchar *transition,
                      const char *key,
                      gint default_val)
{
  gint value;
  GError *error = NULL;
  GKeyFile *key_file =
          hd_transition_get_keyfile(transition, key);
  if (!key_file)
    return default_val;
  /* skip NoDisplay entries */
  value = g_key_file_get_integer (key_file,
                                  transition,
                                  key,
                                  &error);
  if (error)
    {
      g_error_free (error);
      g_key_file_free (key_file);
      return default_val;
    }

  g_key_file_free (key_file);

  return value;
}

gdouble
hd_transition_get_double(const gchar *transition,
                         const char *key,
                         gdouble default_val)
{
  gdouble value;
  GError *error = NULL;
  GKeyFile *key_file =
          hd_transition_get_keyfile(transition, key);
  if (!key_file)
    return default_val;
  /* skip NoDisplay entries */
  value = g_key_file_get_double (key_file,
                                  transition,
                                  key,
                                  &error);
  if (error)
    {
      g_error_free (error);
      g_key_file_free (key_file);
      return default_val;
    }

  g_key_file_free (key_file);

  return value;
}
