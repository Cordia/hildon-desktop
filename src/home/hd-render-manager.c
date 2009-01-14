/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation. All rights reserved.
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

#include "hd-render-manager.h"

#include <clutter/clutter.h>

#include "tidy/tidy-blur-group.h"

#include "hd-comp-mgr.h"
#include "hd-home.h"
#include "hd-switcher.h"
#include "hd-launcher.h"
#include "hd-task-navigator.h"
#include "hd-transition.h"
#include "hd-wm.h"

#include <matchbox/core/mb-wm.h>

/* ------------------------------------------------------------------------- */

G_DEFINE_TYPE (HdRenderManager, hd_render_manager, CLUTTER_TYPE_GROUP);
#define HD_RENDER_MANAGER_GET_PRIVATE(obj) \
                (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                HD_TYPE_RENDER_MANAGER, HdRenderManagerPrivate))

/* The HdRenderManager singleton */
static HdRenderManager *the_render_manager = NULL;

/* ------------------------------------------------------------------------- */

typedef enum
{
  HDRM_BLUR_NONE = 0,
  HDRM_BLUR_HOME = 1,
  HDRM_BLUR_TASK_NAV = 2,
  HDRM_ZOOM_HOME = 4,
  HDRM_ZOOM_TASK_NAV = 8,
  HDRM_BLUR_MORE = 16,
} HDRMBlurEnum;

#define HDRM_BLUR_DURATION 250

#define HDRM_WIDTH  HD_COMP_MGR_SCREEN_WIDTH
#define HDRM_HEIGHT HD_COMP_MGR_SCREEN_HEIGHT

enum
{
  DAMAGE_REDRAW,
  LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0, };

/* ------------------------------------------------------------------------- */

/*
 *
 * HDRM ---> home_blur         ---> home
 *       |                      --> apps (not app_top)
 *       |                      --> blurred_front ---> button_task_nav
 *       |                                         --> button_launcher
 *       |                                         --> button_menu
 *       |                                         --> status_area
 *       |
 *       --> task_nav_blur     ---> task_nav
 *       |
 *       --> launcher
 *       |
 *       --> app_top           ---> dialogs
 *       |
 *       --> front             ---> status_menu
 *
 */

struct _HdRenderManagerPrivate {
  HDRMStateEnum state;

  TidyBlurGroup *home_blur;
  TidyBlurGroup *task_nav_blur;
  ClutterGroup  *app_top;
  ClutterGroup  *front;
  ClutterGroup  *blur_front;

  /* external */
  HdCompMgr            *comp_mgr;
  HdTaskNavigator      *task_nav;
  ClutterActor         *launcher;
  HdHome               *home;
  ClutterActor         *status_area;
  ClutterActor         *status_menu;
  ClutterActor         *operator;
  ClutterActor         *button_task_nav;
  ClutterActor         *button_launcher;
  ClutterActor         *button_menu;
  ClutterActor         *button_home_back;
  ClutterActor         *button_launcher_back;
  ClutterActor         *button_edit;

  /* these are current, from + to variables for doing the blurring animations */
  float         home_blur_cur, home_blur_a, home_blur_b;
  float         task_nav_blur_cur, task_nav_blur_a, task_nav_blur_b;
  float         home_zoom_cur, home_zoom_a, home_zoom_b;
  float         task_nav_zoom_cur, task_nav_zoom_a, task_nav_zoom_b;
  HDRMBlurEnum  current_blur;

  ClutterTimeline    *timeline_blur;

  gboolean            in_notify;
  gboolean            in_set_state;
  gboolean            queued_redraw;
};

/* ------------------------------------------------------------------------- */
static void
hd_render_manager_damage_redraw_notify(void);
static void
hd_render_manager_paint_notify(void);
static void
on_timeline_blur_new_frame(ClutterTimeline *timeline,
                           gint frame_num, gpointer data);
static void
on_timeline_blur_completed(ClutterTimeline *timeline, gpointer data);
/*static gboolean
hd_render_manager_notify_modified (ClutterActor          *actor,
                                   ClutterActor          *child);*/
static void
hd_render_manager_set_order(void);

static const char *
hd_render_manager_state_str(HDRMStateEnum state);
static void
hd_render_manager_set_visibilities(void);
/* ------------------------------------------------------------------------- */
/* -------------------------------------------------------------    INIT     */
/* ------------------------------------------------------------------------- */

HdRenderManager *hd_render_manager_create (HdCompMgr *hdcompmgr,
                                           HdLauncher *launcher,
                                           ClutterActor *launcher_group,
                                           HdHome *home,
                                           HdTaskNavigator *task_nav)
{
  HdRenderManagerPrivate *priv;

  g_assert(the_render_manager == NULL);

  the_render_manager = HD_RENDER_MANAGER(g_object_ref (
        g_object_new (HD_TYPE_RENDER_MANAGER, NULL)));
  priv = the_render_manager->priv;

  priv->comp_mgr = hdcompmgr;

  priv->launcher = g_object_ref(launcher_group);
  clutter_container_add_actor(CLUTTER_CONTAINER(the_render_manager),
		              priv->launcher);
  priv->button_launcher_back = g_object_ref(hd_launcher_get_back_button(launcher));

  priv->home = g_object_ref(home);
  clutter_container_add_actor(CLUTTER_CONTAINER(priv->home_blur),
                              CLUTTER_ACTOR(priv->home));
  priv->button_home_back = g_object_ref(hd_home_get_back_button(priv->home));
  clutter_container_add_actor(CLUTTER_CONTAINER(priv->blur_front),
                              priv->button_home_back);
  priv->button_edit = g_object_ref(hd_home_get_edit_button(priv->home));
  clutter_container_add_actor(CLUTTER_CONTAINER(priv->blur_front),
                              priv->button_edit);
  priv->operator = g_object_ref(hd_home_get_operator(priv->home));
  clutter_container_add_actor(CLUTTER_CONTAINER(priv->blur_front),
                              priv->operator);

  priv->task_nav = g_object_ref(task_nav);
  clutter_container_add_actor(CLUTTER_CONTAINER(priv->task_nav_blur),
                              CLUTTER_ACTOR(priv->task_nav));

  return the_render_manager;
}

static void
hd_render_manager_finalize (GObject *gobject)
{
  HdRenderManagerPrivate *priv = HD_RENDER_MANAGER_GET_PRIVATE(gobject);
  g_object_unref(priv->launcher);
  g_object_unref(priv->home);
  g_object_unref(priv->task_nav);
  G_OBJECT_CLASS (hd_render_manager_parent_class)->finalize (gobject);
}

static void
hd_render_manager_class_init (HdRenderManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdRenderManagerPrivate));

  gobject_class->finalize = hd_render_manager_finalize;

  signals[DAMAGE_REDRAW] =
      g_signal_new ("damage-redraw",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_CLEANUP,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);
}

static void
hd_render_manager_init (HdRenderManager *self)
{
  HdRenderManagerPrivate *priv;

  self->priv = priv = HD_RENDER_MANAGER_GET_PRIVATE (self);
  clutter_actor_set_name(CLUTTER_ACTOR(self), "HdRenderManager");

  priv->state = HDRM_STATE_UNDEFINED;
  priv->current_blur = HDRM_BLUR_NONE;
  priv->in_notify = TRUE;

  priv->home_blur = TIDY_BLUR_GROUP(tidy_blur_group_new());
  clutter_actor_set_name(CLUTTER_ACTOR(priv->home_blur),
                         "HdRenderManager:home_blur");
  clutter_actor_set_visibility_detect(CLUTTER_ACTOR(priv->home_blur), FALSE);
  tidy_blur_group_set_use_alpha(CLUTTER_ACTOR(priv->home_blur), FALSE);
  tidy_blur_group_set_use_mirror(CLUTTER_ACTOR(priv->home_blur), TRUE);
  clutter_actor_set_size(CLUTTER_ACTOR(priv->home_blur),
                         HDRM_WIDTH, HDRM_HEIGHT);
  clutter_container_add_actor(CLUTTER_CONTAINER(self),
                              CLUTTER_ACTOR(priv->home_blur));

  priv->task_nav_blur = TIDY_BLUR_GROUP(tidy_blur_group_new());
  clutter_actor_set_name(CLUTTER_ACTOR(priv->task_nav_blur),
                         "HdRenderManager:task_nav_blur");
  clutter_actor_set_visibility_detect(CLUTTER_ACTOR(priv->task_nav_blur), FALSE);
  tidy_blur_group_set_use_alpha(CLUTTER_ACTOR(priv->task_nav_blur), TRUE);
  tidy_blur_group_set_use_mirror(CLUTTER_ACTOR(priv->task_nav_blur), FALSE);
  clutter_actor_set_size(CLUTTER_ACTOR(priv->task_nav_blur),
                         HDRM_WIDTH, HDRM_HEIGHT);
  clutter_container_add_actor(CLUTTER_CONTAINER(self),
                              CLUTTER_ACTOR(priv->task_nav_blur));

  priv->app_top = CLUTTER_GROUP(clutter_group_new());
  clutter_actor_set_name(CLUTTER_ACTOR(priv->app_top),
                         "HdRenderManager:app_top");
  clutter_actor_set_size(CLUTTER_ACTOR(priv->app_top),
                           HDRM_WIDTH, HDRM_HEIGHT);
  clutter_actor_set_visibility_detect(CLUTTER_ACTOR(priv->app_top), FALSE);
  clutter_container_add_actor(CLUTTER_CONTAINER(self),
                              CLUTTER_ACTOR(priv->app_top));

  priv->front = CLUTTER_GROUP(clutter_group_new());
  clutter_actor_set_name(CLUTTER_ACTOR(priv->front),
                         "HdRenderManager:front");
  clutter_actor_set_size(CLUTTER_ACTOR(priv->front),
                             HDRM_WIDTH, HDRM_HEIGHT);
  clutter_actor_set_visibility_detect(CLUTTER_ACTOR(priv->front), FALSE);
  clutter_container_add_actor(CLUTTER_CONTAINER(self),
                              CLUTTER_ACTOR(priv->front));

  priv->blur_front = CLUTTER_GROUP(clutter_group_new());
  clutter_actor_set_name(CLUTTER_ACTOR(priv->blur_front),
                         "HdRenderManager:blur_front");
  clutter_actor_set_size(CLUTTER_ACTOR(priv->blur_front),
                             HDRM_WIDTH, HDRM_HEIGHT);
  clutter_actor_set_visibility_detect(CLUTTER_ACTOR(priv->blur_front), FALSE);
  clutter_container_add_actor(CLUTTER_CONTAINER(priv->home_blur),
                              CLUTTER_ACTOR(priv->blur_front));

  priv->home_blur_cur = priv->home_blur_a = priv->home_blur_b = 0;
  priv->task_nav_blur_cur = priv->task_nav_blur_a = priv->task_nav_blur_b = 0;
  priv->home_zoom_cur = priv->home_zoom_a = priv->home_zoom_b = 1;
  priv->task_nav_zoom_cur = priv->task_nav_zoom_a = priv->task_nav_zoom_b = 1;

  priv->timeline_blur = clutter_timeline_new_for_duration(HDRM_BLUR_DURATION);
  g_signal_connect (priv->timeline_blur, "new-frame",
                    G_CALLBACK (on_timeline_blur_new_frame), self);
  g_signal_connect (priv->timeline_blur, "completed",
                      G_CALLBACK (on_timeline_blur_completed), self);

  g_signal_connect (self, "damage-redraw",
                    G_CALLBACK (hd_render_manager_damage_redraw_notify),
                    0);
  g_signal_connect (self, "paint",
                      G_CALLBACK (hd_render_manager_paint_notify),
                      0);

  priv->in_notify = FALSE;
  priv->in_set_state = FALSE;
  priv->queued_redraw = FALSE;
}

/* ------------------------------------------------------------------------- */
/* -------------------------------------------------------------  CALLBACK   */
/* ------------------------------------------------------------------------- */
/*
static gboolean
hd_render_manager_notify_modified (ClutterActor          *actor,
                                   ClutterActor          *child)
{
  if (!HD_IS_RENDER_MANAGER(actor))
    return TRUE;

  HdRenderManager *manager = HD_RENDER_MANAGER(actor);
  HdRenderManagerPrivate *priv = manager->priv;

  if (priv->in_notify)
    return FALSE;

  priv->in_notify = TRUE;
  hd_render_manager_set_order(manager);
  priv->in_notify = FALSE;
  return TRUE;
}*/

static void
on_timeline_blur_new_frame(ClutterTimeline *timeline,
                           gint frame_num, gpointer data)
{
  HdRenderManagerPrivate *priv;
  float amt;

  priv = the_render_manager->priv;

  amt = frame_num / (float)clutter_timeline_get_n_frames(timeline);

  priv->home_blur_cur = priv->home_blur_a*(1-amt) +
                        priv->home_blur_b*amt;
  priv->task_nav_blur_cur = priv->task_nav_blur_a*(1-amt) +
                            priv->task_nav_blur_b*amt;
  priv->home_zoom_cur = priv->home_zoom_a*(1-amt) +
                        priv->home_zoom_b*amt;
  priv->task_nav_zoom_cur = priv->task_nav_zoom_a*(1-amt) +
                            priv->task_nav_zoom_b*amt;

  tidy_blur_group_set_blur      (CLUTTER_ACTOR(priv->home_blur),
                                 priv->home_blur_cur*4.0f);
  tidy_blur_group_set_saturation(CLUTTER_ACTOR(priv->home_blur),
                                 1.0f - priv->home_blur_cur*0.75f);
  tidy_blur_group_set_brightness(CLUTTER_ACTOR(priv->home_blur),
                                 (2.0f-priv->home_blur_cur) * 0.5f);
  tidy_blur_group_set_zoom(CLUTTER_ACTOR(priv->home_blur),
                1.0f - hd_transition_overshoot(priv->home_zoom_cur)*0.15f);

  tidy_blur_group_set_blur      (CLUTTER_ACTOR(priv->task_nav_blur),
                                 priv->task_nav_blur_cur*4.0f);
  tidy_blur_group_set_saturation(CLUTTER_ACTOR(priv->task_nav_blur),
                                 1.0f - priv->task_nav_blur_cur*0.75f);
  tidy_blur_group_set_brightness(CLUTTER_ACTOR(priv->task_nav_blur),
                                 (2.0f-priv->task_nav_blur_cur) * 0.5f);
  tidy_blur_group_set_zoom(CLUTTER_ACTOR(priv->task_nav_blur),
                1.0f - hd_transition_overshoot(priv->task_nav_zoom_cur)*0.15f);
}

static void
on_timeline_blur_completed (ClutterTimeline *timeline, gpointer data)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  hd_comp_mgr_set_effect_running(priv->comp_mgr, FALSE);
}

/* ------------------------------------------------------------------------- */
/* -------------------------------------------------------------    PRIVATE  */
/* ------------------------------------------------------------------------- */

/* called on a damage_redraw signal, this will queue the screen to be redrawn.
 * We do this with signals so we can wait until all the update_area signals
 * have already come in. */
static
void hd_render_manager_damage_redraw_notify()
{
  ClutterActor *stage = clutter_stage_get_default();
  clutter_actor_queue_redraw_damage(stage);
}

static
void hd_render_manager_paint_notify()
{
  if (!the_render_manager)
    return;
  the_render_manager->priv->queued_redraw = FALSE;
}

static
void hd_render_manager_set_blur (HDRMBlurEnum blur)
{
  HdRenderManagerPrivate *priv;
  float blur_amt = 1.0f;
  float zoom_amt = 1.0f;

  priv = the_render_manager->priv;

  if (clutter_timeline_is_playing(priv->timeline_blur))
    {
      clutter_timeline_stop(priv->timeline_blur);
      on_timeline_blur_completed(priv->timeline_blur, the_render_manager);
    }

  priv->current_blur = blur;

  priv->home_blur_a = priv->home_blur_cur;
  priv->task_nav_blur_a = priv->task_nav_blur_cur;
  priv->home_zoom_a = priv->home_zoom_cur;
  priv->task_nav_zoom_a = priv->task_nav_zoom_cur;

  if (blur & HDRM_BLUR_MORE)
    {
      blur_amt = 1.5f;
      zoom_amt = 2.0f;
    }

  priv->home_blur_b = (blur & HDRM_BLUR_HOME) ? blur_amt : 0;
  priv->task_nav_blur_b = (blur & HDRM_BLUR_TASK_NAV) ? blur_amt : 0;
  priv->home_zoom_b = (blur & HDRM_ZOOM_HOME) ? zoom_amt : 0;
  priv->task_nav_zoom_b = (blur & HDRM_ZOOM_TASK_NAV) ? zoom_amt : 0;

  hd_comp_mgr_set_effect_running(priv->comp_mgr, TRUE);
  clutter_timeline_start(priv->timeline_blur);
}

static void
hd_render_manager_set_input_viewport()
{
  ClutterGeometry geom[HDRM_BUTTON_COUNT];
  int geom_count = 0;
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  if (!STATE_NEED_GRAB(priv->state))
    {
      gint i;
      /* Now look at what buttons we have showing, and add each visible button X
       * to the X input viewport */
      for (i = 1; i <= HDRM_BUTTON_COUNT; i++)
        {
          ClutterActor *button;
	  button = hd_render_manager_get_button((HDRMButtonEnum)i);
          if (CLUTTER_ACTOR_IS_VISIBLE(button) &&
              CLUTTER_ACTOR_IS_VISIBLE(clutter_actor_get_parent(button)))
            {
              clutter_actor_get_geometry (button, &geom[geom_count]);
              geom_count++;
            }
        }
    }
  else
    {
      /* get the whole screen! */
      geom[0].x = 0;
      geom[0].y = 0;
      geom[0].width = HDRM_WIDTH;
      geom[0].height = HDRM_HEIGHT;
      geom_count = 1;
    }

  hd_comp_mgr_setup_input_viewport(priv->comp_mgr, geom, geom_count);
}

static
void hd_render_manager_set_order ()
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  /* FIXME why would this get hidden? */
  clutter_actor_show(CLUTTER_ACTOR(priv->home));

  HDRMButtonEnum visible_top_left = HDRM_BUTTON_LAUNCHER;
  HDRMButtonEnum visible_top_right = HDRM_BUTTON_NONE;

  switch (priv->state)
    {
      case HDRM_STATE_UNDEFINED:
        g_warning("%s: NEVER supposed to be in HDRM_STATE_UNDEFINED",
                  __FUNCTION__);
      case HDRM_STATE_HOME:
        if (hd_task_navigator_is_empty(priv->task_nav))
          visible_top_left = HDRM_BUTTON_LAUNCHER;
        else
          visible_top_left = HDRM_BUTTON_TASK_NAV;
        visible_top_right = HDRM_BUTTON_NONE;
        clutter_actor_hide(CLUTTER_ACTOR(priv->task_nav_blur));
        clutter_actor_hide(CLUTTER_ACTOR(priv->launcher));
        hd_render_manager_set_blur(HDRM_BLUR_NONE);
        hd_home_update_layout (priv->home);
        break;
      case HDRM_STATE_HOME_EDIT:
        visible_top_left = HDRM_BUTTON_MENU;
        visible_top_right = HDRM_BUTTON_HOME_BACK;
        clutter_actor_hide(CLUTTER_ACTOR(priv->task_nav_blur));
        clutter_actor_hide(CLUTTER_ACTOR(priv->launcher));
        hd_render_manager_set_blur(HDRM_BLUR_NONE);
        hd_home_update_layout (priv->home);
        break;
      case HDRM_STATE_APP:
        visible_top_left = HDRM_BUTTON_TASK_NAV;
        visible_top_right = HDRM_BUTTON_NONE;
        clutter_actor_hide(CLUTTER_ACTOR(priv->task_nav_blur));
        clutter_actor_show(CLUTTER_ACTOR(priv->launcher));
        hd_render_manager_set_blur(HDRM_BLUR_NONE);
        break;
      case HDRM_STATE_APP_FULLSCREEN:
        visible_top_left = HDRM_BUTTON_NONE;
        visible_top_right = HDRM_BUTTON_NONE;
        clutter_actor_hide(CLUTTER_ACTOR(priv->task_nav_blur));
        clutter_actor_hide(CLUTTER_ACTOR(priv->launcher));
        hd_render_manager_set_blur(HDRM_BLUR_NONE);
        break;
      case HDRM_STATE_TASK_NAV:
        visible_top_left = HDRM_BUTTON_LAUNCHER;
        visible_top_right = HDRM_BUTTON_NONE;
        clutter_actor_hide(CLUTTER_ACTOR(priv->launcher));
        clutter_actor_show(CLUTTER_ACTOR(priv->task_nav_blur));
        clutter_actor_raise_top(CLUTTER_ACTOR(priv->task_nav_blur));
        hd_render_manager_set_blur(HDRM_BLUR_HOME | HDRM_ZOOM_HOME);
        break;
      case HDRM_STATE_LAUNCHER:
        visible_top_left = HDRM_BUTTON_NONE;
        visible_top_right = HDRM_BUTTON_LAUNCHER_BACK;

        clutter_actor_show(CLUTTER_ACTOR(priv->task_nav_blur));
        clutter_actor_show(CLUTTER_ACTOR(priv->launcher));
        clutter_actor_raise_top(CLUTTER_ACTOR(priv->task_nav_blur));
        clutter_actor_raise_top(CLUTTER_ACTOR(priv->launcher));
        hd_render_manager_set_blur(
            HDRM_BLUR_HOME | HDRM_BLUR_TASK_NAV |
            HDRM_ZOOM_HOME  | HDRM_ZOOM_TASK_NAV );
        break;
    }

  clutter_actor_show(CLUTTER_ACTOR(priv->home_blur));
  clutter_actor_show(CLUTTER_ACTOR(priv->app_top));
  clutter_actor_show(CLUTTER_ACTOR(priv->front));
  clutter_actor_raise_top(CLUTTER_ACTOR(priv->app_top));
  clutter_actor_raise_top(CLUTTER_ACTOR(priv->front));

  if (STATE_SHOW_OPERATOR(priv->state))
    clutter_actor_show(priv->operator);
  else
    clutter_actor_hide(priv->operator);

  if (STATE_SHOW_STATUS_AREA(priv->state) && priv->status_area)
    {
      clutter_actor_show(priv->status_area);
      clutter_actor_raise_top(priv->status_area);
    }
  else if (priv->status_area)
      clutter_actor_hide(priv->status_area);

  /* Set button state */
  switch (visible_top_left)
  {
    case HDRM_BUTTON_NONE:
      clutter_actor_hide(CLUTTER_ACTOR(priv->button_launcher));
      clutter_actor_hide(CLUTTER_ACTOR(priv->button_menu));
      clutter_actor_hide(CLUTTER_ACTOR(priv->button_task_nav));
      break;
    case HDRM_BUTTON_LAUNCHER:
      clutter_actor_show(CLUTTER_ACTOR(priv->button_launcher));
      clutter_actor_hide(CLUTTER_ACTOR(priv->button_menu));
      clutter_actor_hide(CLUTTER_ACTOR(priv->button_task_nav));
      break;
    case HDRM_BUTTON_MENU:
      clutter_actor_hide(CLUTTER_ACTOR(priv->button_launcher));
      clutter_actor_show(CLUTTER_ACTOR(priv->button_menu));
      clutter_actor_hide(CLUTTER_ACTOR(priv->button_task_nav));
      break;
    case HDRM_BUTTON_TASK_NAV:
      clutter_actor_hide(CLUTTER_ACTOR(priv->button_launcher));
      clutter_actor_hide(CLUTTER_ACTOR(priv->button_menu));
      clutter_actor_show(CLUTTER_ACTOR(priv->button_task_nav));
      break;
    default:
      g_warning("%s: Invalid button %d in top-left",
          __FUNCTION__, visible_top_left);
  }
  switch (visible_top_right)
  {
    case HDRM_BUTTON_NONE:
      clutter_actor_hide(CLUTTER_ACTOR(priv->button_home_back));
      clutter_actor_hide(CLUTTER_ACTOR(priv->button_launcher_back));
      break;
    case HDRM_BUTTON_HOME_BACK:
      clutter_actor_show(CLUTTER_ACTOR(priv->button_home_back));
      clutter_actor_hide(CLUTTER_ACTOR(priv->button_launcher_back));
      break;
    case HDRM_BUTTON_LAUNCHER_BACK:
      clutter_actor_hide(CLUTTER_ACTOR(priv->button_home_back));
      clutter_actor_show(CLUTTER_ACTOR(priv->button_launcher_back));
      break;
    default:
      g_warning("%s: Invalid button %d in top-right",
          __FUNCTION__, visible_top_right);
  }

  if (priv->status_menu)
    clutter_actor_raise_top(CLUTTER_ACTOR(priv->status_menu));

  if (STATE_BLUR_BUTTONS(priv->state))
    {
      /* raise the blur_front to the top of the home_blur group so
       * we still see the apps */
      clutter_actor_reparent(CLUTTER_ACTOR(priv->blur_front),
                             CLUTTER_ACTOR(priv->home_blur));
    }
  else
    {
      /* raise the blur_front out of the blur group so we can still
       * see it unblurred */
      clutter_actor_reparent(CLUTTER_ACTOR(priv->blur_front),
                             CLUTTER_ACTOR(priv->front));
    }
  clutter_actor_raise_top(CLUTTER_ACTOR(priv->blur_front));

  /* Now look at what buttons we have showing, and add each visible button X
   * to the X input viewport */
  hd_render_manager_set_input_viewport();
}

/* ------------------------------------------------------------------------- */
/* -------------------------------------------------------------    PUBLIC   */
/* ------------------------------------------------------------------------- */

void hd_render_manager_stop_transition()
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  if (clutter_timeline_is_playing(priv->timeline_blur))
    {
      guint frames;
      clutter_timeline_stop(priv->timeline_blur);
      frames = clutter_timeline_get_n_frames(priv->timeline_blur);
      on_timeline_blur_new_frame(priv->timeline_blur, frames, the_render_manager);
      on_timeline_blur_completed(priv->timeline_blur, the_render_manager);
    }
}

void hd_render_manager_add_to_front_group (ClutterActor *item)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  clutter_actor_reparent(item, CLUTTER_ACTOR(priv->front));
}

void hd_render_manager_set_status_area (ClutterActor *item)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  if (priv->status_area)
    {
      g_object_unref(priv->status_area);
    }

  if (item)
    {
      priv->status_area = g_object_ref(item);
      clutter_actor_reparent(priv->status_area, CLUTTER_ACTOR(priv->blur_front));
    }
  else
    priv->status_area = NULL;
}

void hd_render_manager_set_status_menu (ClutterActor *item)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  if (priv->status_menu)
    {
      g_object_unref(priv->status_menu);
    }

  if (item)
    {
      priv->status_menu = g_object_ref(item);
      clutter_actor_reparent(priv->status_menu, CLUTTER_ACTOR(priv->front));
      clutter_actor_raise_top(CLUTTER_ACTOR(priv->status_menu));
    }
  else
    priv->status_menu = NULL;
}

void hd_render_manager_set_operator (ClutterActor *item)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  if (priv->operator)
    {
      g_object_unref(priv->operator);
    }
  priv->operator = CLUTTER_ACTOR(g_object_ref(item));
}

void hd_render_manager_set_button (HDRMButtonEnum btn,
                                   ClutterActor *item)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  switch (btn)
    {
      case HDRM_BUTTON_TASK_NAV:
        g_assert(!priv->button_task_nav);
        priv->button_task_nav = CLUTTER_ACTOR(g_object_ref(item));
        break;
      case HDRM_BUTTON_LAUNCHER:
        g_assert(!priv->button_launcher);
        priv->button_launcher = CLUTTER_ACTOR(g_object_ref(item));
        break;
      case HDRM_BUTTON_MENU:
        g_assert(!priv->button_menu);
        priv->button_menu = CLUTTER_ACTOR(g_object_ref(item));
        break;
      case HDRM_BUTTON_HOME_BACK:
        g_warning("%s: back button must be set at creation time!", __FUNCTION__);
	g_assert(FALSE);
        break;
      case HDRM_BUTTON_LAUNCHER_BACK:
        g_warning("%s: launcher back button must be set at creation time!",
		  __FUNCTION__);
	g_assert(FALSE);
        break;
      case HDRM_BUTTON_EDIT:
        g_warning("%s: edit button must be set at creation time!", __FUNCTION__);
	g_assert(FALSE);
        break;
      default:
        g_warning("%s: Invalid Enum %d", __FUNCTION__, btn);
	g_assert(FALSE);
    }
  if (clutter_actor_get_parent(CLUTTER_ACTOR(item)))
    clutter_actor_reparent(CLUTTER_ACTOR(item), CLUTTER_ACTOR(priv->blur_front));
  else
    clutter_container_add_actor(CLUTTER_CONTAINER(priv->blur_front),
                                CLUTTER_ACTOR(item));
}

ClutterActor *hd_render_manager_get_button(HDRMButtonEnum button)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  switch (button)
    {
      case HDRM_BUTTON_TASK_NAV:
        return priv->button_task_nav;
      case HDRM_BUTTON_LAUNCHER:
        return priv->button_launcher;
      case HDRM_BUTTON_MENU:
        return priv->button_menu;
      case HDRM_BUTTON_HOME_BACK:
        return priv->button_home_back;
      case HDRM_BUTTON_LAUNCHER_BACK:
        return priv->button_launcher_back;
      case HDRM_BUTTON_EDIT:
        return priv->button_edit;
      default:
        g_warning("%s: Invalid Enum %d", __FUNCTION__, button);
	g_assert(FALSE);
    }
  return 0;
}

void hd_render_manager_set_visible(HDRMButtonEnum button, gboolean visible)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  switch (button)
  {
    case HDRM_BUTTON_EDIT:
      if (visible == CLUTTER_ACTOR_IS_VISIBLE(priv->button_edit))
        return;
      if (visible)
        clutter_actor_show(priv->button_edit);
      else
        clutter_actor_hide(priv->button_edit);
      /* we need this so we can set up the X input area */
      hd_render_manager_set_input_viewport(the_render_manager);
      break;
    default:
      g_warning("%s: Not supposed to set visibility for %d",
                __FUNCTION__, button);
  }
}

gboolean hd_render_manager_get_visible(HDRMButtonEnum button)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  switch (button)
  {
    case HDRM_BUTTON_EDIT:
      return CLUTTER_ACTOR_IS_VISIBLE(priv->button_edit);
    default:
      g_warning("%s: Not supposed to be asking for visibility of %d",
                __FUNCTION__, button);
  }
  return FALSE;
}

gboolean hd_render_manager_has_apps()
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;
  return hd_task_navigator_has_apps(priv->task_nav);
}

/* FIXME: this should not be exposed */
ClutterContainer *hd_render_manager_get_front_group(void)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;
  return CLUTTER_CONTAINER(priv->front);
}

void hd_render_manager_set_state(HDRMStateEnum state)
{
  HdRenderManagerPrivate *priv;
  MBWMCompMgr          *cmgr;
  MBWindowManager      *wm;

  priv = the_render_manager->priv;
  cmgr = MB_WM_COMP_MGR (priv->comp_mgr);

  g_debug("%s: STATE %s -> STATE %s", __FUNCTION__,
      hd_render_manager_state_str(priv->state),
      hd_render_manager_state_str(state));

  if (!priv->comp_mgr)
  {
    g_warning("%s: HdCompMgr not defined", __FUNCTION__);
    return;
  }
  wm = cmgr->wm;

  if (priv->in_set_state)
      {
        g_warning("%s: State change ignored as already in "
                  "hd_render_manager_set_state", __FUNCTION__);
        return;
      }
  priv->in_set_state = TRUE;

  if (state != priv->state)
    {
      HDRMStateEnum oldstate = priv->state;
      priv->state = state;

      /* Make everything reactive again if we switched state, or we risk
       * getting into a mode where noithing works */
      hd_render_manager_set_reactive(TRUE);

      if (STATE_NEED_TASK_NAV(state) &&
          !STATE_NEED_TASK_NAV(oldstate))
        {
          MBWindowManagerClient *mbclient;
          MBWMCompMgrClutterClient *cmgrcc;
          /* We want to return all of our windows at this point, as the
           * task navigator will want to grab them */
          hd_render_manager_return_windows();
          /* Are we in application view?  Then zoom out, otherwise just enter
           * the navigator without animation. */
          if ((mbclient = mb_wm_get_visible_main_client (wm)) &&
              (cmgrcc = MB_WM_COMP_MGR_CLUTTER_CLIENT (mbclient->cm_client)))
            {
              ClutterActor *actor;
              actor = mb_wm_comp_mgr_clutter_client_get_actor (cmgrcc);
              if (CLUTTER_ACTOR_IS_VISIBLE (actor)
                  && hd_task_navigator_has_window (priv->task_nav, actor))
                hd_task_navigator_zoom_out (priv->task_nav, actor, NULL, NULL);
              else
                hd_task_navigator_enter (priv->task_nav);
              g_object_unref (actor);
            }
          else /* TODO can @mbclient be NULL at all? */
            hd_task_navigator_enter (priv->task_nav);

          hd_wm_update_current_app_property (wm, 0);
        }
      if (STATE_NEED_TASK_NAV(oldstate) &&
          !STATE_NEED_TASK_NAV(state))
        hd_task_navigator_exit(priv->task_nav);
      if (state == HDRM_STATE_LAUNCHER)
        hd_launcher_show();
      if (oldstate == HDRM_STATE_LAUNCHER)
        hd_launcher_hide();

      if (STATE_NEED_DESKTOP(state) != STATE_NEED_DESKTOP(oldstate))
        {
          gboolean show = STATE_NEED_DESKTOP(state);
          g_debug("%s: show_desktop %s",
                  __FUNCTION__, show?"TRUE":"FALSE");

          mb_wm_handle_show_desktop(MB_WM_COMP_MGR(priv->comp_mgr)->wm, show);
        }

      if (STATE_SHOW_STATUS_AREA(state) != STATE_SHOW_STATUS_AREA(oldstate))
        hd_comp_mgr_set_status_area_stacking(priv->comp_mgr,
                    STATE_SHOW_STATUS_AREA(state));

      /* we always need to restack here */
      hd_comp_mgr_restack(MB_WM_COMP_MGR(priv->comp_mgr));

      hd_render_manager_set_order();
    }
  priv->in_set_state = FALSE;
}

HDRMStateEnum  hd_render_manager_get_state()
{
  if (!the_render_manager)
    return HDRM_STATE_UNDEFINED;
  return the_render_manager->priv->state;
}

static const char *hd_render_manager_state_str(HDRMStateEnum state)
{
  switch (state)
  {
    case HDRM_STATE_UNDEFINED : return "HDRM_STATE_UNDEFINED";
    case HDRM_STATE_HOME : return "HDRM_STATE_HOME";
    case HDRM_STATE_HOME_EDIT : return "HDRM_STATE_HOME_EDIT";
    case HDRM_STATE_APP : return "HDRM_STATE_APP";
    case HDRM_STATE_APP_FULLSCREEN : return "HDRM_STATE_APP_FULLSCREEN";
    case HDRM_STATE_TASK_NAV : return "HDRM_STATE_TASK_NAV";
    case HDRM_STATE_LAUNCHER : return "HDRM_STATE_LAUNCHER";
  }
  return "";
}

const char *hd_render_manager_get_state_str()
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;
  return hd_render_manager_state_str(priv->state);
}

gboolean hd_render_manager_in_transition(void)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;
  return clutter_timeline_is_playing(priv->timeline_blur);
}

void hd_render_manager_return_windows()
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;
  MBWindowManager *wm;
  MBWindowManagerClient *c;

  wm = MB_WM_COMP_MGR(priv->comp_mgr)->wm;
  c = wm->stack_bottom;

  /* Order and choose which window actors will be visible */
  while (c)
    {
      if (!(MB_WM_CLIENT_CLIENT_TYPE (c) & HdWmClientTypeHomeApplet)
          && c->cm_client && c->desktop >= 0)
        {
          ClutterActor *actor;
          ClutterActor *desktop = mb_wm_comp_mgr_clutter_get_nth_desktop(
              MB_WM_COMP_MGR_CLUTTER(priv->comp_mgr), c->desktop);
          actor = mb_wm_comp_mgr_clutter_client_get_actor(
              MB_WM_COMP_MGR_CLUTTER_CLIENT(c->cm_client));
          if (actor)
            {
              ClutterActor *parent = clutter_actor_get_parent(actor);
              /* else we put it back into the arena */
              if (parent == CLUTTER_ACTOR(priv->app_top) ||
                  parent == CLUTTER_ACTOR(priv->home_blur))
                clutter_actor_reparent(actor, desktop);
            }
        }

      c = c->stacked_above;
    }

  /* because swapping parents doesn't appear to fire a redraw */
  tidy_blur_group_set_source_changed(CLUTTER_ACTOR(priv->home_blur));
}

/* Called to restack the windows in the way we use for rendering... */
void hd_render_manager_restack()
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;
  MBWindowManager *wm;
  MBWindowManagerClient *c;
  gboolean past_desktop = FALSE;

  wm = MB_WM_COMP_MGR(priv->comp_mgr)->wm;

  /* Order and choose which window actors will be visible */
  for (c = wm->stack_bottom; c; c = c->stacked_above)
    {
      past_desktop |= (wm->desktop == c);
      /* If we're past the desktop then add us to the stuff that will be
       * visible */

      if (c->cm_client && c->desktop >= 0) /* FIXME: should check against
					      current desktop? */
        {
          ClutterActor *actor = 0;
          ClutterActor *desktop = mb_wm_comp_mgr_clutter_get_nth_desktop(
              MB_WM_COMP_MGR_CLUTTER(priv->comp_mgr), c->desktop);
          actor = mb_wm_comp_mgr_clutter_client_get_actor(
              MB_WM_COMP_MGR_CLUTTER_CLIENT(c->cm_client));
          if (actor)
            {
              ClutterActor *parent = clutter_actor_get_parent(actor);
              if (past_desktop)
                {
                  /* if we want to render this, add it */
                  if (parent == desktop ||
		      parent == CLUTTER_ACTOR(priv->app_top))
                    clutter_actor_reparent(actor,
                                           CLUTTER_ACTOR(priv->home_blur));
                  clutter_actor_raise_top(actor);
                }
              else
                {
                  /* else we put it back into the arena */
                  if (parent == CLUTTER_ACTOR(priv->home_blur) ||
                      parent == CLUTTER_ACTOR(priv->app_top))
                    clutter_actor_reparent(actor, desktop);
                }
            }
        }
    }

  /* Now start at the top and put actors in the non-blurred group
   * until we find one that fills the screen. If we didn't find
   * any that filled the screen then add the window that does. */
  {
    gint i, n_elements;
    gboolean have_foreground = FALSE;

    n_elements = clutter_group_get_n_children(CLUTTER_GROUP(priv->home_blur));
    for (i=n_elements-1;i>=0;i--)
      {
        ClutterActor *child =
          clutter_group_get_nth_child(CLUTTER_GROUP(priv->home_blur), i);


        if (child != CLUTTER_ACTOR(priv->home) &&
            child != CLUTTER_ACTOR(priv->blur_front))
          {
            ClutterGeometry geo;

            clutter_actor_get_geometry(child, &geo);
            if (!HD_COMP_MGR_CLIENT_IS_MAXIMIZED(geo))
              {
                clutter_actor_reparent(child, CLUTTER_ACTOR(priv->app_top));
                clutter_actor_lower_bottom(child);
                clutter_actor_show(child); /* because it is in app-top, vis
                                              check does not get applied */
                have_foreground = TRUE;
              }
            else
              {
                /* if it is fullscreen and there was nothing in front,
                 * add it to our front list */
                if (!have_foreground)
                  {
                    clutter_actor_reparent(child, CLUTTER_ACTOR(priv->app_top));
                    clutter_actor_lower_bottom(child);
                    clutter_actor_show(child); /* because it is in app-top, vis
                                                  check does not get applied */
                  }
                break;
              }
          }
      }
  }

  clutter_actor_raise_top(CLUTTER_ACTOR(priv->blur_front));

  /* And for speed of rendering, work out what is visible and what
   * isn't, and hide anything that would be rendered over by another app */
  hd_render_manager_set_visibilities();

  /* because swapping parents doesn't appear to fire a redraw */
  tidy_blur_group_set_source_changed(CLUTTER_ACTOR(priv->home_blur));
}

void hd_render_manager_set_blur_app(gboolean blur)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;
  HDRMBlurEnum blur_flags;

  g_debug("%s: %s", __FUNCTION__, blur ? "BLUR":"UNBLUR");

  blur_flags = priv->current_blur;
  if (blur)
    blur_flags = blur_flags | HDRM_BLUR_HOME;
  else
    blur_flags = blur_flags & ~HDRM_BLUR_HOME;
  hd_render_manager_set_blur(blur_flags);
  /* calling this after a blur transition will force a restack after
   * it has ended */
  hd_comp_mgr_restack(MB_WM_COMP_MGR(priv->comp_mgr));
}

/* This is called when we are in the launcher subview so that we can blur and
 * darken the background even more */
void hd_render_manager_set_launcher_subview(gboolean subview)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  g_debug("%s: %s", __FUNCTION__, subview ? "SUBVIEW":"MAIN");

  if (subview)
    hd_render_manager_set_blur(priv->current_blur | HDRM_BLUR_MORE);
  else
    hd_render_manager_set_blur(priv->current_blur & ~HDRM_BLUR_MORE);
}

/* Sets whether any of the buttons will actually be set to do anything */
void hd_render_manager_set_reactive(gboolean reactive)
{
  gint i;
  GList *li;

  for (i = 1; i <= HDRM_BUTTON_COUNT; ++i)
    {
      ClutterActor *button = hd_render_manager_get_button((HDRMButtonEnum)i);
      clutter_actor_set_reactive(button, reactive);
    }

  li = hd_home_get_active_views (the_render_manager->priv->home);
  for (; li; li = li->next)
    clutter_actor_set_reactive(li->data, reactive);
}

/* Work out if rect is visible after being clipped to avoid every
 * rect in blockers */
static gboolean
hd_render_manager_is_visible(GList *blockers,
                             ClutterGeometry rect)
{
  GList *bit;
  /* clip for every block */
  bit = g_list_first(blockers);
  while (bit)
    {
      ClutterGeometry blocker = *(ClutterGeometry*)bit->data;
      /* Because most windows will go edge->edge, just do a very simplistic
       * clipping in the Y direction */
      /* If rect fits inside blocker in the X axis... */
      if (blocker.width >= rect.width &&
          blocker.x <= rect.x+(blocker.width-rect.width))
        {
          if (blocker.height >= rect.height &&
              blocker.y <= rect.y+(blocker.height-rect.height))
            {
              /* If rect fits inside blocker in the Y axis,
               * it is def. not visible */
              return FALSE;
            }
          else if (rect.y < blocker.y)
            {
              /* rect out the bottom, clip to the blocker */
              rect.height = blocker.y - rect.y;
            }
          else
            { /* rect must be out the top, clip to the blocker */
              rect.height = (rect.y + rect.height) -
                            (blocker.y + blocker.height);
              rect.y = blocker.y + blocker.height;
            }
        }
      bit = bit->next;
    }
  return TRUE;
}

static
void hd_render_manager_append_geo_cb(ClutterActor *actor, gpointer data)
{
  GList **list = (GList**)data;
  ClutterGeometry *geo = g_malloc(sizeof(ClutterGeometry));
  clutter_actor_get_geometry(actor, geo);
  /*TEST clutter_actor_set_opacity(actor, 127);*/
  *list = g_list_append(*list, geo);
}

static
void hd_render_manager_set_visibilities()
{
  HdRenderManagerPrivate *priv;
  GList *blockers = 0;
  GList *it;
  gint i, n_elements;
  ClutterGeometry fullscreen_geo = {0, 0, HDRM_WIDTH, HDRM_HEIGHT};

  priv = the_render_manager->priv;
  /* first append all the top elements... */
  clutter_container_foreach(CLUTTER_CONTAINER(priv->app_top),
                            hd_render_manager_append_geo_cb,
                            (gpointer)&blockers);
  /* Now check to see if the whole screen is covered, and if so
   * don't bother rendering blurring */
  if (hd_render_manager_is_visible(blockers, fullscreen_geo))
    {
      clutter_actor_show(CLUTTER_ACTOR(priv->home_blur));
    }
  else
    {
      clutter_actor_hide(CLUTTER_ACTOR(priv->home_blur));
    }
  /* Then work BACKWARDS through the other items, working out if they are
   * visible or not */
  n_elements = clutter_group_get_n_children(CLUTTER_GROUP(priv->home_blur));
  for (i=n_elements-1;i>=0;i--)
    {
      ClutterActor *child =
        clutter_group_get_nth_child(CLUTTER_GROUP(priv->home_blur), i);
      if (child != CLUTTER_ACTOR(priv->blur_front))
        {
          ClutterGeometry *geo = g_malloc(sizeof(ClutterGeometry));
          clutter_actor_get_geometry(child, geo);
          /*TEST clutter_actor_set_opacity(child, 63);*/
          if (hd_render_manager_is_visible(blockers, *geo))
            clutter_actor_show(child);
          else
            clutter_actor_hide(child);
          /* Add the geometry to our list of blockers and go to next... */
          blockers = g_list_append(blockers, geo);
        }
    }

  /* now free blockers */
  it = g_list_first(blockers);
  while (it)
    {
      g_free(it->data);
      it = it->next;
    }
  g_list_free(blockers);
  blockers = 0;

  /* now we have to find the status area, and see if it has something that
   * blocks it in front of it. If it does, make it (and any fake actor at
   * its level - blur_front level) invisible.  */
  if (priv->status_area)
    {
      MBWindowManager *wm;
      MBWindowManagerClient *c;
      gboolean fullscreen_before = FALSE;

      wm = MB_WM_COMP_MGR(priv->comp_mgr)->wm;

      /* Order and choose which window actors will be visible */
      for (c = wm->stack_top; c; c = c->stacked_below)
        {
          if (c->cm_client && c->desktop >= 0) /* FIXME: should check against
                                                  current desktop? */
            {
              ClutterActor *actor = mb_wm_comp_mgr_clutter_client_get_actor(
                  MB_WM_COMP_MGR_CLUTTER_CLIENT(c->cm_client));
              if (actor)
                {
                  if (actor != priv->status_area)
                    {
                      if (c->window)
                        fullscreen_before |=
                                    c->window->ewmh_state &
                                    MBWMClientWindowEWMHStateFullscreen;
                    }
                  else
                    {
                      /* if it is the status area, then show/hide the entire
                       * BLUR FRONT group depending on if it is covered
                       * by a fullscreen window */
                      if (!fullscreen_before)
                        clutter_actor_show(CLUTTER_ACTOR(priv->blur_front));
                      else
                        clutter_actor_hide(CLUTTER_ACTOR(priv->blur_front));
                      hd_render_manager_set_input_viewport();
                      break;
                    }
                }
            }
        }
    }
  else /* we have no window to check visibility with, so render anyway */
    clutter_actor_show(CLUTTER_ACTOR(priv->blur_front));
}

void hd_render_manager_queue_delay_redraw()
{
  if (!the_render_manager)
    return;
  if (!the_render_manager->priv->queued_redraw)
    {
      g_signal_emit (the_render_manager, signals[DAMAGE_REDRAW], 0);
      the_render_manager->priv->queued_redraw = TRUE;
    }
}

