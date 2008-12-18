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

#include "hd-render-manager.h"

#include <clutter/clutter.h>

#include "tidy/tidy-blur-group.h"

#include "hd-comp-mgr.h"
#include "hd-home.h"
#include "hd-switcher.h"
#include "hd-launcher.h"
#include "hd-task-navigator.h"
#include "hd-transition.h"

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

#define HDRM_WIDTH 800
#define HDRM_HEIGHT 480
#define HDRM_TOP 70

#define VISIBILITY_CODE 1

/* ------------------------------------------------------------------------- */

/*
 *
 * HDRM ---> home_blur         ---> home
 *       |                      --> apps (not app_top)
 *       |
 *       --> task_nav_blur     ---> task_nav
 *       |
 *       --> launcher
 *       |
 *       --> app_top
 *       |
 *       --> front             ---> button_task_nav
 *                              --> button_launcher
 *                              --> button_menu
 */

struct _HdRenderManagerPrivate {
  HDRMStateEnum state;

  TidyBlurGroup *home_blur;
  TidyBlurGroup *task_nav_blur;
  ClutterGroup  *app_top;
  ClutterGroup  *front;

  /* external */
  HdCompMgr            *comp_mgr;
  HdTaskNavigator      *task_nav;
  ClutterActor         *launcher;
  HdHome               *home;
  ClutterActor         *status_area;
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
};

/* ------------------------------------------------------------------------- */
static void
on_timeline_blur_new_frame(ClutterTimeline *timeline,
                           gint frame_num, gpointer data);
static void
on_timeline_blur_completed(ClutterActor* timeline, gpointer data);
/*static gboolean
hd_render_manager_notify_modified (ClutterActor          *actor,
                                   ClutterActor          *child);*/
static void
hd_render_manager_set_order (HdRenderManager *manager);

static const char *
hd_render_manager_state_str(HDRMStateEnum state);
#if VISIBILITY_CODE
static void
hd_render_manager_set_visibilities(void);
#endif // VISIBILITY_CODE
/* ------------------------------------------------------------------------- */
/* -------------------------------------------------------------    INIT     */
/* ------------------------------------------------------------------------- */

HdRenderManager *hd_render_manager_get (void)
{
  if (G_UNLIKELY (!the_render_manager))
    the_render_manager = HD_RENDER_MANAGER(g_object_ref (
        g_object_new (HD_TYPE_RENDER_MANAGER, NULL)));
  return the_render_manager;
}

static void
hd_render_manager_finalize (GObject *gobject)
{
  /* TODO: unref objects */
  G_OBJECT_CLASS (hd_render_manager_parent_class)->finalize (gobject);
}


static void
hd_render_manager_class_init (HdRenderManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
 // ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdRenderManagerPrivate));

  gobject_class->finalize = hd_render_manager_finalize;
 // actor_class->notify_modified = hd_render_manager_notify_modified;
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

  priv->home_blur_cur = priv->home_blur_a = priv->home_blur_b = 0;
  priv->task_nav_blur_cur = priv->task_nav_blur_a = priv->task_nav_blur_b = 0;
  priv->home_zoom_cur = priv->home_zoom_a = priv->home_zoom_b = 1;
  priv->task_nav_zoom_cur = priv->task_nav_zoom_a = priv->task_nav_zoom_b = 1;

  priv->timeline_blur = clutter_timeline_new_for_duration(HDRM_BLUR_DURATION);
  g_signal_connect (priv->timeline_blur, "new-frame",
                    G_CALLBACK (on_timeline_blur_new_frame), self);
  g_signal_connect (priv->timeline_blur, "completed",
                      G_CALLBACK (on_timeline_blur_completed), self);

  priv->in_notify = FALSE;
  priv->in_set_state = FALSE;
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
  HdRenderManager *rmgr;
  HdRenderManagerPrivate *priv;
  float amt;

  if (!HD_IS_RENDER_MANAGER(data))
    return;
  rmgr = HD_RENDER_MANAGER(data);
  priv = rmgr->priv;

  amt = (float)clutter_timeline_get_progress(timeline);

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
on_timeline_blur_completed (ClutterActor* timeline, gpointer data)
{
  HdRenderManager *rmgr;
  HdRenderManagerPrivate *priv;

  if (!HD_IS_RENDER_MANAGER(data))
    return;
  rmgr = HD_RENDER_MANAGER(data);
  priv = rmgr->priv;

  hd_comp_mgr_set_effect_running(priv->comp_mgr, FALSE);
  hd_comp_mgr_sync_stacking(priv->comp_mgr);
}

/* ------------------------------------------------------------------------- */
/* -------------------------------------------------------------    PRIVATE  */
/* ------------------------------------------------------------------------- */

static
gboolean hd_render_manager_initialised()
{
  HdRenderManagerPrivate *priv;

  /* we're not even created yet */
  if (!the_render_manager)
    return FALSE;

  /* check that all the items have been set */
  priv = HD_RENDER_MANAGER_GET_PRIVATE (hd_render_manager_get());
  return priv->comp_mgr
    && priv->task_nav
    && priv->launcher
    && priv->home
    && priv->operator
    && priv->button_task_nav
    && priv->button_launcher
    && priv->button_menu
    && priv->button_launcher_back
    && priv->button_home_back
    && priv->button_edit;
}

static
void hd_render_manager_set_blur (HdRenderManager *manager, HDRMBlurEnum blur)
{
  HdRenderManagerPrivate *priv;
  float blur_amt = 1.0f;
  float zoom_amt = 1.0f;

  if (!HD_IS_RENDER_MANAGER(manager))
    return;
  priv = HD_RENDER_MANAGER_GET_PRIVATE (manager);

  if (clutter_timeline_is_playing(priv->timeline_blur))
    {
      clutter_timeline_stop(priv->timeline_blur);
      on_timeline_blur_completed(CLUTTER_ACTOR(priv->timeline_blur), manager);
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
hd_render_manager_set_input_viewport(HdRenderManager *manager)
{
  ClutterGeometry geom[HDRM_BUTTON_COUNT];
  int geom_count = 0;
  HdRenderManagerPrivate *priv = HD_RENDER_MANAGER_GET_PRIVATE (manager);
  gint i;

  if (!STATE_NEED_GRAB(priv->state))
    {
      /* Now look at what buttons we have showing, and add each visible button X
       * to the X input viewport */
      for (i=1;i<=HDRM_BUTTON_COUNT;i++)
        {
          ClutterActor *button = hd_render_manager_get_button((HDRMButtonEnum)i);
          if (CLUTTER_ACTOR_IS_VISIBLE(button))
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
void hd_render_manager_set_order (HdRenderManager *manager)
{
  HdRenderManagerPrivate *priv;

  if (!HD_IS_RENDER_MANAGER(manager))
    return;
  priv = HD_RENDER_MANAGER_GET_PRIVATE (manager);

  if (!hd_render_manager_initialised())
  {
    g_debug("%s: !hd_render_manager_initialised()", __FUNCTION__);
    return;
  }

  /* why would this get hidden? */
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
        clutter_actor_show(CLUTTER_ACTOR(priv->front));
        hd_render_manager_set_blur(manager, HDRM_BLUR_NONE);
        hd_home_update_layout (priv->home);
        break;
      case HDRM_STATE_HOME_EDIT:
        visible_top_left = HDRM_BUTTON_MENU;
        visible_top_right = HDRM_BUTTON_HOME_BACK;
        clutter_actor_hide(CLUTTER_ACTOR(priv->task_nav_blur));
        clutter_actor_hide(CLUTTER_ACTOR(priv->launcher));
        clutter_actor_show(CLUTTER_ACTOR(priv->front));
        hd_render_manager_set_blur(manager, HDRM_BLUR_NONE);
        hd_home_update_layout (priv->home);
        break;
      case HDRM_STATE_APP:
        visible_top_left = HDRM_BUTTON_TASK_NAV;
        visible_top_right = HDRM_BUTTON_NONE;
        clutter_actor_hide(CLUTTER_ACTOR(priv->task_nav_blur));
        clutter_actor_show(CLUTTER_ACTOR(priv->launcher));
        clutter_actor_show(CLUTTER_ACTOR(priv->front));
        hd_render_manager_set_blur(manager, HDRM_BLUR_NONE);
        break;
      case HDRM_STATE_APP_FULLSCREEN:
        visible_top_left = HDRM_BUTTON_NONE;
        visible_top_right = HDRM_BUTTON_NONE;
        clutter_actor_hide(CLUTTER_ACTOR(priv->task_nav_blur));
        clutter_actor_hide(CLUTTER_ACTOR(priv->launcher));
        clutter_actor_hide(CLUTTER_ACTOR(priv->front));
        hd_render_manager_set_blur(manager, HDRM_BLUR_NONE);
        break;
      case HDRM_STATE_TASK_NAV:
        visible_top_left = HDRM_BUTTON_LAUNCHER;
        visible_top_right = HDRM_BUTTON_NONE;
        clutter_actor_hide(CLUTTER_ACTOR(priv->launcher));
        clutter_actor_show(CLUTTER_ACTOR(priv->task_nav_blur));
        clutter_actor_show(CLUTTER_ACTOR(priv->front));
        clutter_actor_raise_top(CLUTTER_ACTOR(priv->task_nav_blur));
        hd_render_manager_set_blur(manager, HDRM_BLUR_HOME | HDRM_ZOOM_HOME);
        break;
      case HDRM_STATE_LAUNCHER:
        visible_top_left = HDRM_BUTTON_NONE;
        visible_top_right = HDRM_BUTTON_LAUNCHER_BACK;

        clutter_actor_show(CLUTTER_ACTOR(priv->task_nav_blur));
        clutter_actor_show(CLUTTER_ACTOR(priv->launcher));
        clutter_actor_show(CLUTTER_ACTOR(priv->front));
        clutter_actor_raise_top(CLUTTER_ACTOR(priv->task_nav_blur));
        clutter_actor_raise_top(CLUTTER_ACTOR(priv->launcher));
        hd_render_manager_set_blur(manager,
            HDRM_BLUR_HOME | HDRM_BLUR_TASK_NAV |
            HDRM_ZOOM_HOME  | HDRM_ZOOM_TASK_NAV );
        break;
    }

  clutter_actor_show(CLUTTER_ACTOR(priv->home_blur));
  clutter_actor_show(CLUTTER_ACTOR(priv->app_top));
  clutter_actor_raise_top(CLUTTER_ACTOR(priv->app_top));
  clutter_actor_raise_top(CLUTTER_ACTOR(priv->front));

  if (STATE_SHOW_OPERATOR(priv->state))
    clutter_actor_show(priv->operator);
  else
    clutter_actor_hide(priv->operator);

  if (STATE_SHOW_STATUS_AREA(priv->state) && priv->status_area)
    {
      clutter_actor_show(priv->status_area);
      /* we shouldn't need what comes next. First off something is grabbing
       * this from front - also it should be set to be high enough with
       * hd_comp_mgr_set_status_area_stacking that it is ABOVE the window.
       * But it's not right now, and this works great. */
      if (clutter_actor_get_parent(priv->status_area) !=
            CLUTTER_ACTOR(priv->front))
        clutter_actor_reparent(priv->status_area, CLUTTER_ACTOR(priv->front));
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

  /* Now look at what buttons we have showing, and add each visible button X
   * to the X input viewport */
  hd_render_manager_set_input_viewport(manager);
}

/* ------------------------------------------------------------------------- */
/* -------------------------------------------------------------    PUBLIC   */
/* ------------------------------------------------------------------------- */

void hd_render_manager_set_comp_mgr (HdRenderManager *manager,
                                     HdCompMgr *comp_mgr)
{
  HdRenderManagerPrivate *priv;

  if (!HD_IS_RENDER_MANAGER(manager))
    return;
  priv = HD_RENDER_MANAGER_GET_PRIVATE (manager);
  g_assert(!priv->comp_mgr);
  /* FIXME: Why can't we object_ref this??? */
  priv->comp_mgr = comp_mgr;
}

void hd_render_manager_set_task_nav (HdRenderManager *manager,
                                     ClutterActor *item)
{
  HdRenderManagerPrivate *priv;

  if (!HD_IS_RENDER_MANAGER(manager))
    return;
  priv = HD_RENDER_MANAGER_GET_PRIVATE (manager);
  g_assert(!priv->task_nav);
  priv->task_nav = HD_TASK_NAVIGATOR(CLUTTER_ACTOR(g_object_ref(item)));
  clutter_container_add_actor(CLUTTER_CONTAINER(priv->task_nav_blur),
                                  CLUTTER_ACTOR(priv->task_nav));
}

void hd_render_manager_set_home (HdRenderManager *manager,
                                 ClutterActor *item)
{
  HdRenderManagerPrivate *priv;

  if (!HD_IS_RENDER_MANAGER(manager))
    return;
  priv = HD_RENDER_MANAGER_GET_PRIVATE (manager);
  g_assert(!priv->home);
  priv->home = HD_HOME(CLUTTER_ACTOR(g_object_ref(item)));
  clutter_container_add_actor(CLUTTER_CONTAINER(priv->home_blur),
                              CLUTTER_ACTOR(priv->home));
}

void hd_render_manager_set_launcher (HdRenderManager *manager,
                                     ClutterActor *item)
{
  HdRenderManagerPrivate *priv;

  if (!HD_IS_RENDER_MANAGER(manager))
    return;
  priv = HD_RENDER_MANAGER_GET_PRIVATE (manager);
  g_assert(!priv->launcher);
  priv->launcher = CLUTTER_ACTOR(g_object_ref(item));
  clutter_container_add_actor(CLUTTER_CONTAINER(manager),
                              CLUTTER_ACTOR(priv->launcher));
}

void hd_render_manager_set_status_area (HdRenderManager *manager,
                                        ClutterActor *item)
{
  HdRenderManagerPrivate *priv;

  if (!HD_IS_RENDER_MANAGER(manager))
    return;
  priv = HD_RENDER_MANAGER_GET_PRIVATE (manager);
  if (priv->status_area)
    {
      g_object_unref(priv->status_area);
    }
  priv->status_area = CLUTTER_ACTOR(g_object_ref(item));
  clutter_actor_reparent(CLUTTER_ACTOR(priv->status_area),
                         CLUTTER_ACTOR(priv->front));
}

void hd_render_manager_set_operator (HdRenderManager *manager,
                                     ClutterActor *item)
{
  HdRenderManagerPrivate *priv;

  if (!HD_IS_RENDER_MANAGER(manager))
    return;
  priv = HD_RENDER_MANAGER_GET_PRIVATE (manager);
  g_assert(!priv->operator);
  priv->operator = CLUTTER_ACTOR(g_object_ref(item));
  /*clutter_container_add_actor(CLUTTER_CONTAINER(priv->front),
                              CLUTTER_ACTOR(priv->operator));*/
}

void hd_render_manager_set_button (HdRenderManager *manager,
                                   HDRMButtonEnum btn,
                                   ClutterActor *item)
{
  HdRenderManagerPrivate *priv;

  if (!HD_IS_RENDER_MANAGER(manager))
    return;
  priv = HD_RENDER_MANAGER_GET_PRIVATE (manager);

  switch (btn)
    {
      case HDRM_BUTTON_NONE:
        g_warning("%s: Invalid Enum", __FUNCTION__);
        break;
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
        g_assert(!priv->button_home_back);
        priv->button_home_back = CLUTTER_ACTOR(g_object_ref(item));
        break;
      case HDRM_BUTTON_LAUNCHER_BACK:
        g_assert(!priv->button_launcher_back);
        priv->button_launcher_back = CLUTTER_ACTOR(g_object_ref(item));
        break;
      case HDRM_BUTTON_EDIT:
        g_assert(!priv->button_edit);
        priv->button_edit = CLUTTER_ACTOR(g_object_ref(item));
        break;
    }
  if (clutter_actor_get_parent(CLUTTER_ACTOR(item)))
    clutter_actor_reparent(CLUTTER_ACTOR(item), CLUTTER_ACTOR(priv->front));
  else
    clutter_container_add_actor(CLUTTER_CONTAINER(priv->front),
                                CLUTTER_ACTOR(item));
}

ClutterActor *hd_render_manager_get_button(HDRMButtonEnum button)
{
  HdRenderManagerPrivate *priv;

  priv = HD_RENDER_MANAGER_GET_PRIVATE (hd_render_manager_get());
  switch (button)
    {
      case HDRM_BUTTON_NONE:
        g_warning("%s: Invalid Enum", __FUNCTION__);
        return 0;
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
    }
  return 0;
}

void hd_render_manager_set_visible(HDRMButtonEnum button, gboolean visible)
{
  HdRenderManager *manager = hd_render_manager_get();
  HdRenderManagerPrivate *priv = HD_RENDER_MANAGER_GET_PRIVATE (manager);

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
      hd_render_manager_set_input_viewport(manager);
      break;
    default:
      g_warning("%s: Not supposed to set visibility for %d",
                __FUNCTION__, button);
  }
}

gboolean hd_render_manager_get_visible(HDRMButtonEnum button)
{
  HdRenderManagerPrivate *priv;
  priv = HD_RENDER_MANAGER_GET_PRIVATE (hd_render_manager_get());
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
  HdRenderManagerPrivate *priv;
  priv = HD_RENDER_MANAGER_GET_PRIVATE (hd_render_manager_get());

  return hd_task_navigator_has_apps(priv->task_nav);
}

ClutterContainer *hd_render_manager_get_front_group(void)
{
  HdRenderManagerPrivate *priv;

  priv = HD_RENDER_MANAGER_GET_PRIVATE (hd_render_manager_get());
  return CLUTTER_CONTAINER(priv->front);
}

void hd_render_manager_set_state(HDRMStateEnum state)
{
  HdRenderManager *manager;
  HdRenderManagerPrivate *priv;
  MBWMCompMgr          *cmgr;
  MBWindowManager      *wm;

  if (!hd_render_manager_initialised())
    {
      g_warning("%s: hd_render_manager not initialised before call",
                __FUNCTION__);
      return;
    }

  manager = hd_render_manager_get();
  priv = HD_RENDER_MANAGER_GET_PRIVATE (manager);
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
        }
      if (STATE_NEED_TASK_NAV(oldstate) &&
          !STATE_NEED_TASK_NAV(state))
        hd_task_navigator_exit(priv->task_nav);
      if (state == HDRM_STATE_LAUNCHER)
        hd_launcher_show();
      if (oldstate == HDRM_STATE_LAUNCHER)
        hd_launcher_hide();

      if (STATE_NEED_DESKTOP(state) != STATE_NEED_DESKTOP(oldstate))
        mb_wm_handle_show_desktop (wm, STATE_NEED_DESKTOP(state));

      if (STATE_SHOW_STATUS_AREA(state) != STATE_SHOW_STATUS_AREA(oldstate))
        hd_comp_mgr_set_status_area_stacking(priv->comp_mgr,
                    STATE_SHOW_STATUS_AREA(state));

      /* we always need to restack here */
      hd_comp_mgr_restack(MB_WM_COMP_MGR(priv->comp_mgr));

      if (STATE_NEED_GRAB(state) != STATE_NEED_GRAB(state))
        {
          if (STATE_NEED_GRAB(state))
            hd_home_grab_pointer(priv->home);
          else
            hd_home_ungrab_pointer(priv->home);
        }

      hd_render_manager_set_order(manager);
    }
  priv->in_set_state = FALSE;

  //hd_comp_mgr_dump_debug_info("");
}

HDRMStateEnum  hd_render_manager_get_state()
{
  HdRenderManagerPrivate *priv;

  priv = HD_RENDER_MANAGER_GET_PRIVATE (hd_render_manager_get());
  return priv->state;
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
  HdRenderManagerPrivate *priv;

  priv = HD_RENDER_MANAGER_GET_PRIVATE (hd_render_manager_get());
  return hd_render_manager_state_str(priv->state);
}

gboolean hd_render_manager_in_transition(void)
{
  HdRenderManagerPrivate *priv;

  priv = HD_RENDER_MANAGER_GET_PRIVATE (hd_render_manager_get());
  return clutter_timeline_is_playing(priv->timeline_blur);
}

void hd_render_manager_return_windows()
{
  HdRenderManagerPrivate *priv;
  MBWindowManager *wm;
  MBWindowManagerClient *c;

  priv = HD_RENDER_MANAGER_GET_PRIVATE (hd_render_manager_get());
  wm = MB_WM_COMP_MGR(priv->comp_mgr)->wm;
  c = wm->stack_bottom;

  /* Order and choose which window actors will be visible */
  while (c)
    {
      if (c->cm_client && c->desktop>=0)
        {
          ClutterActor *actor = 0;
          ClutterActor *desktop = mb_wm_comp_mgr_clutter_get_nth_desktop(
              MB_WM_COMP_MGR_CLUTTER(priv->comp_mgr), c->desktop);
          actor = mb_wm_comp_mgr_clutter_client_get_actor(
              MB_WM_COMP_MGR_CLUTTER_CLIENT(c->cm_client));
          if (actor)
            {
              /* else we put it back into the arena */
              if (clutter_actor_get_parent(actor) != desktop)
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
  HdRenderManagerPrivate *priv;
  MBWindowManager *wm;
  MBWindowManagerClient *c;
  gboolean past_desktop = FALSE;

  priv = HD_RENDER_MANAGER_GET_PRIVATE (hd_render_manager_get());
  wm = MB_WM_COMP_MGR(priv->comp_mgr)->wm;
  c = wm->stack_bottom;

  /* Order and choose which window actors will be visible */
  while (c)
    {
      past_desktop |= (wm->desktop == c);
      /* If we're past the desktop then add us to the stuff that will be
       * visible */

      if (c->cm_client && c->desktop>=0)
        {
          ClutterActor *actor = 0;
          ClutterActor *desktop = mb_wm_comp_mgr_clutter_get_nth_desktop(
              MB_WM_COMP_MGR_CLUTTER(priv->comp_mgr), c->desktop);
          actor = mb_wm_comp_mgr_clutter_client_get_actor(
              MB_WM_COMP_MGR_CLUTTER_CLIENT(c->cm_client));
          if (actor)
            {
              if (past_desktop)
                {
                  /* if we want to render this, add it */
                  if (clutter_actor_get_parent(actor) == desktop ||
                      clutter_actor_get_parent(actor) ==
                                             CLUTTER_ACTOR(priv->app_top))
                    clutter_actor_reparent(actor,
                                           CLUTTER_ACTOR(priv->home_blur));
                  clutter_actor_raise_top(actor);
                }
              else
                {
                  /* else we put it back into the arena */
                  if (clutter_actor_get_parent(actor) ==
                                              CLUTTER_ACTOR(priv->home_blur) ||
                      clutter_actor_get_parent(actor) ==
                                              CLUTTER_ACTOR(priv->app_top))
                    clutter_actor_reparent(actor, desktop);
                }
            }
        }

      c = c->stacked_above;
    }

  /* grab whatever is focused and bring it right to the front */
  if (wm->focused_client &&
      wm->focused_client->cm_client)
    {
      ClutterActor              *actor;
      MBWMCompMgrClutterClient  *cclient =
          MB_WM_COMP_MGR_CLUTTER_CLIENT(wm->focused_client->cm_client);

      actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);
      if (actor)
        {
          ClutterActor              *parent;
          parent = clutter_actor_get_parent(actor);
          if (parent == CLUTTER_ACTOR(priv->home_blur))
            clutter_actor_reparent(actor, CLUTTER_ACTOR(priv->app_top));
        }
    }

#if VISIBILITY_CODE
  /* And for speed of rendering, work out what is visible and what
   * isn't, and hide anything that would be rendered over by another app */
  hd_render_manager_set_visibilities();
#endif // VISIBILITY_CODE

  /* because swapping parents doesn't appear to fire a redraw */
  tidy_blur_group_set_source_changed(CLUTTER_ACTOR(priv->home_blur));
}

void hd_render_manager_set_blur_app(gboolean blur)
{
  HdRenderManager *manager = hd_render_manager_get();
  HdRenderManagerPrivate *priv = HD_RENDER_MANAGER_GET_PRIVATE(manager);

  g_debug("%s: %s", __FUNCTION__, blur ? "BLUR":"UNBLUR");

  hd_render_manager_set_blur(manager, blur ? HDRM_BLUR_HOME : HDRM_BLUR_NONE);
  /* calling this after a blur transition will force a restack after
   * it has ended */
  hd_comp_mgr_restack(MB_WM_COMP_MGR(priv->comp_mgr));
}

/* This is called when in the launcher subview so that we can blur and
 * darken the background even more */
void hd_render_manager_set_launcher_subview(gboolean subview)
{
  HdRenderManager *manager = hd_render_manager_get();
  HdRenderManagerPrivate *priv = HD_RENDER_MANAGER_GET_PRIVATE(manager);

  g_debug("%s: %s", __FUNCTION__, subview ? "SUBVIEW":"MAIN");

  if (subview)
    hd_render_manager_set_blur(manager, priv->current_blur | HDRM_BLUR_MORE);
  else
    hd_render_manager_set_blur(manager, priv->current_blur & ~HDRM_BLUR_MORE);
}

/* Sets whether any of the buttons will actually be set to do anything */
void hd_render_manager_set_reactive(gboolean reactive)
{
  gint i;

  for (i=1;i<=HDRM_BUTTON_COUNT;i++)
    {
      ClutterActor *button = hd_render_manager_get_button((HDRMButtonEnum)i);
      clutter_actor_set_reactive(button, reactive);
    }
}

#if VISIBILITY_CODE

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
  priv = HD_RENDER_MANAGER_GET_PRIVATE (hd_render_manager_get());
  /* first append all the top elements... */
  clutter_container_foreach(CLUTTER_CONTAINER(priv->app_top),
                            hd_render_manager_append_geo_cb,
                            (gpointer)&blockers);
  /* Now check to see if the whole screen is covered, and if so
   * don't even bother rendering blurring */
  if (hd_render_manager_is_visible(blockers, fullscreen_geo))
    clutter_actor_show(CLUTTER_ACTOR(priv->home_blur));
  else
    clutter_actor_hide(CLUTTER_ACTOR(priv->home_blur));
  /* Then work BACKWARDS through the other items, working out if they are
   * visible or not */
  n_elements = clutter_group_get_n_children(CLUTTER_GROUP(priv->home_blur));
  for (i=n_elements-1;i>=0;i--)
    {
      ClutterActor *child =
        clutter_group_get_nth_child(CLUTTER_GROUP(priv->home_blur), i);
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

  /* now free blockers */
  it = g_list_first(blockers);
  while (it)
    {
      g_free(it->data);
      it = it->next;
    }
  g_list_free(blockers);
}
#endif // VISIBILITY_CODE
