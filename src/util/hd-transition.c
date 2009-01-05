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

#include "hd-app.h"

#define HDCM_UNMAP_DURATION 500
#define HDCM_BLUR_DURATION 300
#define HDCM_POPUP_DURATION 250
#define HDCM_FADE_DURATION 150
#define HDCM_UNMAP_PARTICLES 8
#define HD_EFFECT_PARTICLE "white-particle.png"

typedef struct _HDEffectData
{
  MBWMCompMgrClientEvent   event;
  ClutterTimeline          *timeline;
  MBWMCompMgrClutterClient *cclient;
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

  actor = mb_wm_comp_mgr_clutter_client_get_actor (data->cclient);
  if (!CLUTTER_IS_ACTOR(actor))
    return;

  pop_top = data->geo.y==0;
  pop_bottom = data->geo.y+data->geo.height==480;
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

  clutter_actor_set_positionu(actor,
                             CLUTTER_INT_TO_FIXED(data->geo.x),
                             CLUTTER_FLOAT_TO_FIXED(status_pos));
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

  actor = mb_wm_comp_mgr_clutter_client_get_actor (data->cclient);
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

  actor = mb_wm_comp_mgr_clutter_client_get_actor (data->cclient);
  if (!CLUTTER_IS_ACTOR(actor))
    return;

  amt = (float)clutter_timeline_get_progress(timeline);

  amtx = 2 - amt*3;
  amty = 1 - amt*3;
  amtp = amt*2 - 1;
  if (amtx<0.1) amtx=0.1;
  if (amtx>1) amtx=1;
  if (amty<0.1) amty=0.1;
  if (amty>1) amty=1;
  if (amtp<0) amtp=0;
  if (amtp>1) amtp=1;
  /* smooth out movement */
  amtx = (1-cos(amtx * 3.141592)) * 0.5f;
  amty = (1-cos(amty * 3.141592)) * 0.5f;
  particle_opacity = sin(amtp * 3.141592);
  particle_radius = 8 + (1-cos(amtp * 3.141592)) * 32.0f;

  centrex =  data->geo.x + data->geo.width / 2 ;
  centrey =  data->geo.y + data->geo.height / 2 ;
  /* set app location and fold up like a turned-off TV */
  clutter_actor_set_scale(actor, amtx, amty);
  clutter_actor_set_position(actor,
                        centrex - data->geo.width * amtx / 2,
                        centrey - data->geo.height * amty / 2);
  clutter_actor_set_opacity(actor, (int)(255 * (1-amtp)));
  /* do sparkles... */
  for (i=0;i<HDCM_UNMAP_PARTICLES;i++)
    if (data->particles[i] && (amtp > 0) && (amtp < 1))
      {
        /* space particles equally and rotate once */
        float ang = i * 2 * 3.141592f / HDCM_UNMAP_PARTICLES +
                    amtp * 2 * 3.141592f;
        clutter_actor_show( data->particles[i] );
        clutter_actor_set_opacity(data->particles[i],
                (int)(255 * particle_opacity));
        clutter_actor_set_positionu(data->particles[i],
                CLUTTER_FLOAT_TO_FIXED(centrex +  sin(ang) * particle_radius),
                CLUTTER_FLOAT_TO_FIXED(centrey + cos(ang) * particle_radius));
      }
    else
      clutter_actor_hide( data->particles[i] );
}


/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */

static void
hd_transition_completed (ClutterActor* timeline, HDEffectData *data)
{
  gint i;
  HdCompMgr *hmgr = HD_COMP_MGR (data->hmgr);
  ClutterActor *actor;

  mb_wm_comp_mgr_clutter_client_unset_flags (data->cclient,
                                        MBWMCompMgrClutterClientDontUpdate |
                                        MBWMCompMgrClutterClientEffectRunning);
  actor = mb_wm_comp_mgr_clutter_client_get_actor (data->cclient);

/*   dump_clutter_tree (CLUTTER_CONTAINER (clutter_stage_get_default()), 0); */

  mb_wm_object_unref (MB_WM_OBJECT (data->cclient));

  if (data->event == MBWMCompMgrClientEventUnmap && actor)
    {
      ClutterActor *parent = clutter_actor_get_parent(actor);
      if (CLUTTER_IS_CONTAINER(parent))
        clutter_container_remove_actor( CLUTTER_CONTAINER(parent), actor );
    }
  g_object_unref ( timeline );

  hd_comp_mgr_set_effect_running(hmgr, FALSE);

  for (i=0;i<HDCM_UNMAP_PARTICLES;i++)
    if (data->particles[i])
      clutter_actor_destroy(data->particles[i]);

  hd_comp_mgr_sync_stacking (hmgr);

  g_free (data);
};

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
  data->hmgr = HD_COMP_MGR (mgr);
  data->timeline = g_object_ref(
            clutter_timeline_new_for_duration (HDCM_POPUP_DURATION) );
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
  ClutterActor             * actor;
  HDEffectData             * data;
  ClutterGeometry            geo;

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
  data->hmgr = HD_COMP_MGR (mgr);
  data->timeline = g_object_ref(
            clutter_timeline_new_for_duration (HDCM_FADE_DURATION) );
  g_signal_connect (data->timeline, "new-frame",
                        G_CALLBACK (on_fade_timeline_new_frame), data);
  g_signal_connect (data->timeline, "completed",
                        G_CALLBACK (hd_transition_completed), data);
  data->geo = geo;

  mb_wm_comp_mgr_clutter_client_set_flags (cclient,
                              MBWMCompMgrClutterClientDontUpdate |
                              MBWMCompMgrClutterClientEffectRunning);
  hd_comp_mgr_set_effect_running(mgr, TRUE);

  /* first call to stop flicker */
  on_fade_timeline_new_frame(data->timeline, 0, data);
  clutter_timeline_start (data->timeline);
}

static ClutterActor *particle_tex = NULL;

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
  gint i;

  /* proper app close animation */
  if (c_type != MBWMClientTypeApp)
    return;

  /* The switcher will do the effect if it's active,
   * don't interfere. */
  if (hd_render_manager_get_state()==HDRM_STATE_TASK_NAV)
    return;

  /* Don't do the unmap transition if it's a secondary. */
  app = HD_APP (c);
  if (app->secondary_window)
    {
      /* FIXME: Transitions. */
      g_debug ("%s: Unmapping secondary window.\n", __FUNCTION__);
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

  /* Need to store also pointer to the manager, as by the time
   * the effect finishes, the back pointer in the cm_client to
   * MBWindowManagerClient is not longer valid/set.
   */
  data = g_new0 (HDEffectData, 1);
  data->event = MBWMCompMgrClientEventUnmap;
  data->cclient = mb_wm_object_ref (MB_WM_OBJECT (cclient));
  data->hmgr = HD_COMP_MGR (mgr);
  data->timeline = g_object_ref(
                clutter_timeline_new_for_duration (HDCM_UNMAP_DURATION) );
  g_signal_connect (data->timeline, "new-frame",
                    G_CALLBACK (on_close_timeline_new_frame), data);
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

  if (!particle_tex)
  {
    /* we need to load some actors for this animation... */
    particle_tex = clutter_texture_new_from_file (
           g_build_filename (HD_DATADIR, HD_EFFECT_PARTICLE, NULL), 0);
  }

  for (i = 0; i < HDCM_UNMAP_PARTICLES; ++i)
    {
      if (particle_tex)
        data->particles[i] = clutter_clone_texture_new(
			     		CLUTTER_TEXTURE(particle_tex));
      if (data->particles[i])
        {
          clutter_actor_set_anchor_point_from_gravity(data->particles[i],
                                                      CLUTTER_GRAVITY_CENTER);
          clutter_container_add_actor(parent, data->particles[i]);
          clutter_actor_hide(data->particles[i]);
        }
    }
  hd_comp_mgr_set_effect_running(mgr, TRUE);
  clutter_timeline_start (data->timeline);

  hd_transition_play_sound ("/usr/share/sounds/ui-window_close.wav");
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

    ca_proplist_create (&pl);
    ca_proplist_sets (pl, CA_PROP_MEDIA_FILENAME, fname);
    if ((ret = ca_context_play_full (ca, 0, pl, play_finished,
                                     &is_playing)) != CA_SUCCESS)
      g_warning("%s: %s", fname, ca_strerror (ret));
    ca_proplist_destroy(pl);
    is_playing = TRUE;
}
