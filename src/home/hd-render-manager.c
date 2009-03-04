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
#include "hd-util.h"
#include "hd-title-bar.h"

#include <matchbox/core/mb-wm.h>
#include <matchbox/theme-engines/mb-wm-theme.h>

/* ------------------------------------------------------------------------- */
#define I_(str) (g_intern_static_string ((str)))

GType
hd_render_manager_state_get_type (void)
{
  static GType gtype = 0;

  if (G_UNLIKELY (gtype == 0))
    {
      static GEnumValue values[] = {
        { HDRM_STATE_UNDEFINED,      "HDRM_STATE_UNDEFINED",      "Undefined" },
        { HDRM_STATE_HOME,           "HDRM_STATE_HOME",           "Home" },
        { HDRM_STATE_HOME_EDIT,      "HDRM_STATE_HOME_EDIT",      "Home edit" },
        { HDRM_STATE_HOME_EDIT_DLG,  "HDRM_STATE_HOME_EDIT_DLG",  "Home edit dialog" },
        { HDRM_STATE_APP,            "HDRM_STATE_APP",            "Application" },
        { HDRM_STATE_APP_FULLSCREEN, "HDRM_STATE_APP_FULLSCREEN", "Application fullscreen" },
        { HDRM_STATE_TASK_NAV,       "HDRM_STATE_TASK_NAV",       "Task switcher" },
        { HDRM_STATE_LAUNCHER,       "HDRM_STATE_LAUNCHER",       "Task launcher" },
        { 0, NULL, NULL }
      };

      gtype = g_enum_register_static (I_("HdRenderManagerStateType"), values);
    }

  return gtype;
}
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */

G_DEFINE_TYPE (HdRenderManager, hd_render_manager, CLUTTER_TYPE_GROUP);
#define HD_RENDER_MANAGER_GET_PRIVATE(obj) \
                (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                HD_TYPE_RENDER_MANAGER, HdRenderManagerPrivate))

/* The HdRenderManager singleton */
static HdRenderManager *the_render_manager = NULL;

/* HdRenderManager properties */
enum
{
  PROP_0,
  PROP_STATE
};
/* ------------------------------------------------------------------------- */

typedef enum
{
  HDRM_BLUR_NONE = 0,
  HDRM_BLUR_HOME = 1,
  HDRM_SHOW_TASK_NAV = 2,
  HDRM_ZOOM_HOME = 4,
  HDRM_ZOOM_TASK_NAV = 8,
  HDRM_BLUR_MORE = 16,
  HDRM_BLUR_BACKGROUND = 32, /* like BLUR_HOME, but for dialogs, etc */
} HDRMBlurEnum;

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
 *       |                                         --> home_get_front (!STATE_HOME_FRONT)
 *       |                      --> apps (not app_top)
 *       |                      --> blur_front    ---> button_menu
 *       |                                         --> status_area
 *       |                                         --> title_bar
 *       |                                         --> home_get_front (STATE_HOME_FRONT)
 *       |
 *       --> task_nav
 *       |
 *       --> launcher
 *       |
 *       --> app_top           ---> dialogs
 *       |
 *       --> front             ---> status_menu
 *
 */

typedef struct _Range {
  float a, b, current;
} Range;

struct _HdRenderManagerPrivate {
  HDRMStateEnum state;

  TidyBlurGroup *home_blur;
  ClutterGroup  *task_nav_container;
  ClutterGroup  *app_top;
  ClutterGroup  *front;
  ClutterGroup  *blur_front;
  HdTitleBar    *title_bar;

  /* external */
  HdCompMgr            *comp_mgr;
  HdTaskNavigator      *task_nav;
  ClutterActor         *launcher;
  HdHome               *home;
  ClutterActor         *status_area;
  MBWindowManagerClient *status_area_client;
  ClutterActor         *status_menu;
  ClutterActor         *operator;
  ClutterActor         *button_task_nav;
  ClutterActor         *button_launcher;
  ClutterActor         *button_back;
  ClutterActor         *button_edit;

  /* these are current, from + to variables for doing the blurring animations */
  Range         home_radius;
  Range         home_zoom;
  Range         home_brightness;
  Range         home_saturation;
  Range         task_nav_opacity;
  Range         task_nav_zoom;

  HDRMBlurEnum  current_blur;

  ClutterTimeline    *timeline_blur;
  /* Timeline works by signals, so we get horrible flicker if we ask it if it
   * is playing right after saying _start() - so we have a boolean to figure
   * out for ourselves */
  gboolean            timeline_playing;

  gboolean            in_set_state;
  gboolean            queued_redraw;

  /* has_fullscreen if and only if set_visibilities() finds a
   * MBWMClientWindowEWMHStateFullscreen client in the client stack. */
  gboolean            has_fullscreen;
};

/* ------------------------------------------------------------------------- */
static void
stage_allocation_changed(ClutterActor *actor, GParamSpec *unused,
                         ClutterActor *stage);
static void
hd_render_manager_damage_redraw_notify(void);
static void
hd_render_manager_paint_notify(void);
static void
on_timeline_blur_new_frame(ClutterTimeline *timeline,
                           gint frame_num, gpointer data);
static void
on_timeline_blur_completed(ClutterTimeline *timeline, gpointer data);

static void
hd_render_manager_sync_clutter_before(void);
static void
hd_render_manager_sync_clutter_after(void);

static const char *
hd_render_manager_state_str(HDRMStateEnum state);
static void
hd_render_manager_set_visibilities(void);

static void
hd_render_manager_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec);
static void
hd_render_manager_set_property (GObject      *gobject,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec);

/* ------------------------------------------------------------------------- */
/* -------------------------------------------------------------  RANGE      */
/* ------------------------------------------------------------------------- */
static inline void range_set(Range *range, float val)
{
  range->a = range->b = range->current = val;
}
static inline void range_interpolate(Range *range, float n)
{
  range->current = (range->a*(1-n)) + range->b*n;
}
static inline void range_next(Range *range, float x)
{
  range->a = range->current;
  range->b = x;
}
static inline gboolean range_equal(Range *range)
{
  return range->a == range->b;
}

/* ------------------------------------------------------------------------- */
/* -------------------------------------------------------------    INIT     */
/* ------------------------------------------------------------------------- */

HdRenderManager *hd_render_manager_create (HdCompMgr *hdcompmgr,
                                           HdLauncher *launcher,
                                           ClutterActor *launcher_group,
                                           HdHome *home,
                                           HdTaskNavigator *task_nav
                                           )
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

  priv->home = g_object_ref(home);
  g_signal_connect_swapped(clutter_stage_get_default(), "notify::allocation",
                           G_CALLBACK(stage_allocation_changed), priv->home);
  clutter_container_add_actor(CLUTTER_CONTAINER(priv->home_blur),
                              CLUTTER_ACTOR(priv->home));
  priv->button_edit = g_object_ref(hd_home_get_edit_button(priv->home));
  clutter_container_add_actor(CLUTTER_CONTAINER(priv->blur_front),
                              priv->button_edit);
  hd_render_manager_set_operator(g_object_ref(hd_home_get_operator(priv->home)));
  clutter_actor_reparent(priv->operator, CLUTTER_ACTOR(priv->blur_front));

  priv->task_nav = g_object_ref(task_nav);
  clutter_container_add_actor(CLUTTER_CONTAINER(priv->task_nav_container),
                              CLUTTER_ACTOR(priv->task_nav));

  priv->title_bar = g_object_ref(g_object_new(HD_TYPE_TITLE_BAR, NULL));
  g_signal_connect_swapped(clutter_stage_get_default(), "notify::allocation",
                           G_CALLBACK(stage_allocation_changed), priv->title_bar);
  clutter_container_add_actor(CLUTTER_CONTAINER(priv->blur_front),
                              CLUTTER_ACTOR(priv->title_bar));

  g_signal_connect (priv->button_back,
                    "button-release-event",
                    G_CALLBACK (hd_launcher_back_button_clicked),
                    hd_launcher_get());
  g_signal_connect (hd_render_manager_get_button(HDRM_BUTTON_BACK),
                    "button-release-event",
                    G_CALLBACK (hd_home_back_button_clicked),
                    home);

  return the_render_manager;
}

HdRenderManager *
hd_render_manager_get (void)
{
  return the_render_manager;
}

static void
hd_render_manager_finalize (GObject *gobject)
{
  HdRenderManagerPrivate *priv = HD_RENDER_MANAGER_GET_PRIVATE(gobject);
  g_object_unref(priv->launcher);
  g_object_unref(priv->home);
  g_object_unref(priv->task_nav);
  g_object_unref(priv->title_bar);
  G_OBJECT_CLASS (hd_render_manager_parent_class)->finalize (gobject);
}

static void
hd_render_manager_class_init (HdRenderManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (HdRenderManagerPrivate));

  gobject_class->get_property = hd_render_manager_get_property;
  gobject_class->set_property = hd_render_manager_set_property;
  gobject_class->finalize = hd_render_manager_finalize;

  signals[DAMAGE_REDRAW] =
      g_signal_new ("damage-redraw",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_CLEANUP,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);

  pspec = g_param_spec_enum ("state",
                             "State", "Render manager state",
                             HD_TYPE_RENDER_MANAGER_STATE,
                             HDRM_STATE_UNDEFINED,
                             G_PARAM_READABLE    |
                             G_PARAM_WRITABLE    |
                             G_PARAM_STATIC_NICK |
                             G_PARAM_STATIC_NAME |
                             G_PARAM_STATIC_BLURB);
  g_object_class_install_property (gobject_class, PROP_STATE, pspec);
}

static void
hd_render_manager_init (HdRenderManager *self)
{
  ClutterActor *stage;
  HdRenderManagerPrivate *priv;

  stage = clutter_stage_get_default();

  self->priv = priv = HD_RENDER_MANAGER_GET_PRIVATE (self);
  clutter_actor_set_name(CLUTTER_ACTOR(self), "HdRenderManager");
  g_signal_connect_swapped(stage, "notify::allocation",
                           G_CALLBACK(stage_allocation_changed), self);

  priv->state = HDRM_STATE_UNDEFINED;
  priv->current_blur = HDRM_BLUR_NONE;

  priv->home_blur = TIDY_BLUR_GROUP(tidy_blur_group_new());
  clutter_actor_set_name(CLUTTER_ACTOR(priv->home_blur),
                         "HdRenderManager:home_blur");
  clutter_actor_set_visibility_detect(CLUTTER_ACTOR(priv->home_blur), FALSE);
  tidy_blur_group_set_use_alpha(CLUTTER_ACTOR(priv->home_blur), FALSE);
  tidy_blur_group_set_use_mirror(CLUTTER_ACTOR(priv->home_blur), TRUE);
  g_signal_connect_swapped(stage, "notify::allocation",
                           G_CALLBACK(stage_allocation_changed), priv->home_blur);
  clutter_container_add_actor(CLUTTER_CONTAINER(self),
                              CLUTTER_ACTOR(priv->home_blur));

  priv->task_nav_container = CLUTTER_GROUP(clutter_group_new());
  clutter_actor_set_name(CLUTTER_ACTOR(priv->task_nav_container),
                         "HdRenderManager:task_nav_container");
  clutter_actor_set_visibility_detect(CLUTTER_ACTOR(priv->task_nav_container), FALSE);
  clutter_actor_set_size (CLUTTER_ACTOR(priv->task_nav_container),
                          HD_COMP_MGR_LANDSCAPE_WIDTH,
                          HD_COMP_MGR_LANDSCAPE_HEIGHT);
  /* we set the anchor point this way, so when we zoom we zoom from the
   * middle */
  clutter_actor_set_anchor_point_from_gravity(
                          CLUTTER_ACTOR(priv->task_nav_container),
                          CLUTTER_GRAVITY_CENTER);
  clutter_actor_set_position (CLUTTER_ACTOR(priv->task_nav_container),
                              HD_COMP_MGR_LANDSCAPE_WIDTH/2,
                              HD_COMP_MGR_LANDSCAPE_HEIGHT/2);
  clutter_container_add_actor(CLUTTER_CONTAINER(self),
                              CLUTTER_ACTOR(priv->task_nav_container));

  priv->app_top = CLUTTER_GROUP(clutter_group_new());
  clutter_actor_set_name(CLUTTER_ACTOR(priv->app_top),
                         "HdRenderManager:app_top");
  g_signal_connect_swapped(stage, "notify::allocation",
                           G_CALLBACK(stage_allocation_changed), priv->app_top);
  clutter_actor_set_visibility_detect(CLUTTER_ACTOR(priv->app_top), FALSE);
  clutter_container_add_actor(CLUTTER_CONTAINER(self),
                              CLUTTER_ACTOR(priv->app_top));

  priv->front = CLUTTER_GROUP(clutter_group_new());
  clutter_actor_set_name(CLUTTER_ACTOR(priv->front),
                         "HdRenderManager:front");
  g_signal_connect_swapped(stage, "notify::allocation",
                           G_CALLBACK(stage_allocation_changed), priv->front);
  clutter_actor_set_visibility_detect(CLUTTER_ACTOR(priv->front), FALSE);
  clutter_container_add_actor(CLUTTER_CONTAINER(self),
                              CLUTTER_ACTOR(priv->front));

  priv->blur_front = CLUTTER_GROUP(clutter_group_new());
  clutter_actor_set_name(CLUTTER_ACTOR(priv->blur_front),
                         "HdRenderManager:blur_front");
  g_signal_connect_swapped(stage, "notify::allocation",
                           G_CALLBACK(stage_allocation_changed), priv->blur_front);
  clutter_actor_set_visibility_detect(CLUTTER_ACTOR(priv->blur_front), FALSE);
  clutter_container_add_actor(CLUTTER_CONTAINER(priv->home_blur),
                              CLUTTER_ACTOR(priv->blur_front));

  /* Animation stuff */
  range_set(&priv->home_radius, 0);
  range_set(&priv->home_zoom, 1);
  range_set(&priv->home_saturation, 1);
  range_set(&priv->home_brightness, 1);
  range_set(&priv->task_nav_opacity, 0);
  range_set(&priv->task_nav_zoom, 1);

  priv->timeline_blur = clutter_timeline_new_for_duration(250);
  g_signal_connect (priv->timeline_blur, "new-frame",
                    G_CALLBACK (on_timeline_blur_new_frame), self);
  g_signal_connect (priv->timeline_blur, "completed",
                      G_CALLBACK (on_timeline_blur_completed), self);
  priv->timeline_playing = FALSE;

  g_signal_connect (self, "damage-redraw",
                    G_CALLBACK (hd_render_manager_damage_redraw_notify),
                    0);
  g_signal_connect (self, "paint",
                      G_CALLBACK (hd_render_manager_paint_notify),
                      0);

  priv->in_set_state = FALSE;
  priv->queued_redraw = FALSE;
}

/* ------------------------------------------------------------------------- */
/* -------------------------------------------------------------  CALLBACK   */
/* ------------------------------------------------------------------------- */

/* Resize @actor to the current screen dimensions.
 * Also can be used to set @actor's initial size. */
static void
stage_allocation_changed(ClutterActor *actor, GParamSpec *unused,
                         ClutterActor *stage)
{
  clutter_actor_set_size(actor,
                         HD_COMP_MGR_SCREEN_WIDTH,
                         HD_COMP_MGR_SCREEN_HEIGHT);
}

static void
on_timeline_blur_new_frame(ClutterTimeline *timeline,
                           gint frame_num, gpointer data)
{
  HdRenderManagerPrivate *priv;
  float amt;
  gint task_opacity;

  priv = the_render_manager->priv;

  amt = frame_num / (float)clutter_timeline_get_n_frames(timeline);

  range_interpolate(&priv->home_radius, amt);
  range_interpolate(&priv->home_zoom, amt);
  range_interpolate(&priv->home_saturation, amt);
  range_interpolate(&priv->home_brightness, amt);
  range_interpolate(&priv->task_nav_opacity, amt);
  range_interpolate(&priv->task_nav_zoom, amt);

  tidy_blur_group_set_blur      (CLUTTER_ACTOR(priv->home_blur),
                                 priv->home_radius.current);
  tidy_blur_group_set_saturation(CLUTTER_ACTOR(priv->home_blur),
                                 priv->home_saturation.current);
  tidy_blur_group_set_brightness(CLUTTER_ACTOR(priv->home_blur),
                                 priv->home_brightness.current);
  tidy_blur_group_set_zoom(CLUTTER_ACTOR(priv->home_blur),
                                 priv->home_zoom.current);

  task_opacity = priv->task_nav_opacity.current*255;
  clutter_actor_set_opacity(CLUTTER_ACTOR(priv->task_nav_container),
                            task_opacity);
  if (task_opacity==0)
    clutter_actor_hide(CLUTTER_ACTOR(priv->task_nav_container));
  else
    clutter_actor_show(CLUTTER_ACTOR(priv->task_nav_container));
  clutter_actor_set_scale(CLUTTER_ACTOR(priv->task_nav_container),
                          priv->task_nav_zoom.current,
                          priv->task_nav_zoom.current);
}

static void
on_timeline_blur_completed (ClutterTimeline *timeline, gpointer data)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  priv->timeline_playing = FALSE;
  hd_comp_mgr_set_effect_running(priv->comp_mgr, FALSE);

  /* to trigger a change after the transition */
  hd_render_manager_sync_clutter_after();
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
  gboolean blur_home;
  gboolean more;

  priv = the_render_manager->priv;

  if (priv->timeline_playing)
    {
      clutter_timeline_stop(priv->timeline_blur);
      on_timeline_blur_completed(priv->timeline_blur, the_render_manager);
    }

  priv->current_blur = blur;

  range_next(&priv->home_radius, 0);
  range_next(&priv->home_saturation, 1);
  range_next(&priv->home_brightness, 1);
  range_next(&priv->home_zoom, 1);
  range_next(&priv->task_nav_opacity, 0);
  range_next(&priv->task_nav_zoom, 1);

  more = blur & HDRM_BLUR_MORE;

  blur_home = blur & (HDRM_BLUR_BACKGROUND | HDRM_BLUR_HOME);

  /* FIXME: cache the settings file */
  if (blur_home)
    {
      priv->home_saturation.b =
              hd_transition_get_double("blur","home_saturation", 1);
      priv->home_brightness.b =
              hd_transition_get_double("blur","home_brightness", 1);
      priv->home_radius.b =
              hd_transition_get_double("blur",
                  more?"home_radius_more":"home_radius", 8);
    }

  if (blur & HDRM_ZOOM_HOME)
    {
      priv->home_zoom.b =
              hd_transition_get_double("blur",
                  more?"home_zoom_more":"home_zoom", 1);
    }

  if (blur & HDRM_SHOW_TASK_NAV)
    {
      priv->task_nav_opacity.b = 1;
    }
  if (blur & HDRM_ZOOM_TASK_NAV)
    {
      priv->task_nav_zoom.b =
              hd_transition_get_double("blur",
                  more?"task_nav_zoom_more":"task_nav_zoom", 1);
    }

  /* no point animating if everything is already right */
  if (range_equal(&priv->home_radius) &&
      range_equal(&priv->home_saturation) &&
      range_equal(&priv->home_brightness) &&
      range_equal(&priv->home_zoom) &&
      range_equal(&priv->task_nav_opacity) &&
      range_equal(&priv->task_nav_zoom))
    return;

  hd_comp_mgr_set_effect_running(priv->comp_mgr, TRUE);
  /* Set duration here so we reload from the file every time */
  clutter_timeline_set_duration(priv->timeline_blur,
      hd_transition_get_int("blur", "duration", 250));
  clutter_timeline_start(priv->timeline_blur);
  priv->timeline_playing = TRUE;
}

static void
hd_render_manager_set_input_viewport()
{
  ClutterGeometry geom[HDRM_BUTTON_COUNT + 1];
  int geom_count = 0;
  HdRenderManagerPrivate *priv = the_render_manager->priv;
  gboolean app_mode = STATE_IS_APP(priv->state);

  if (!STATE_NEED_GRAB(priv->state))
    {
      gint i;
      /* Now look at what buttons we have showing, and add each visible button X
       * to the X input viewport. We unfortunately have to ignore
       * HDRM_BUTTON_BACK in app mode, because matchbox wants to pick them up
       * from X */
      for (i = 1; i <= HDRM_BUTTON_COUNT; i++)
        {
          ClutterActor *button;
          button = hd_render_manager_get_button((HDRMButtonEnum)i);
          if (button &&
              CLUTTER_ACTOR_IS_VISIBLE(button) &&
              CLUTTER_ACTOR_IS_VISIBLE(clutter_actor_get_parent(button)) &&
              (CLUTTER_ACTOR_IS_REACTIVE(button)) &&
              (i!=HDRM_BUTTON_BACK || !app_mode))
            {
              clutter_actor_get_geometry (button, &geom[geom_count]);
              geom_count++;
            }
        }

      /* Block status area?  If so refer to the client geometry,
       * because we might be right after a place_titlebar_elements()
       * which could just have moved it. */
      if ((priv->state == HDRM_STATE_APP_PORTRAIT
	   && priv->status_area
           && CLUTTER_ACTOR_IS_VISIBLE (priv->status_area)) ||
	  /* also in the case of "dialog blur": */
	  (priv->state == HDRM_STATE_APP
	   && priv->status_area
           && CLUTTER_ACTOR_IS_VISIBLE (priv->status_area)
	   /* FIXME: the following check does not work when there are
	    * two levels of dialogs */
           && (priv->current_blur == HDRM_BLUR_BACKGROUND ||
	       priv->current_blur == HDRM_BLUR_HOME)))
        {
          g_assert(priv->status_area_client);
          const MBGeometry *src = &priv->status_area_client->frame_geometry;
          ClutterGeometry *dst = &geom[geom_count++];
          dst->x = src->x;
          dst->y = src->y;
          dst->width  = src->width;
          dst->height = src->height;
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

/* The syncing with clutter that is done before a transition */
static
void hd_render_manager_sync_clutter_before ()
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  HDRMButtonEnum visible_top_left = HDRM_BUTTON_NONE;
  HDRMButtonEnum visible_top_right = HDRM_BUTTON_NONE;
  HdTitleBarVisEnum btn_state = hd_title_bar_get_state(priv->title_bar) &
    ~(HDTB_VIS_BTN_LEFT_MASK | HDTB_VIS_FULL_WIDTH | HDTB_VIS_BTN_RIGHT_MASK);

  switch (priv->state)
    {
      case HDRM_STATE_UNDEFINED:
        g_error("%s: NEVER supposed to be in HDRM_STATE_UNDEFINED", __func__);
	return;
      case HDRM_STATE_HOME:
        if (hd_task_navigator_is_empty(priv->task_nav))
          visible_top_left = HDRM_BUTTON_LAUNCHER;
        else
          visible_top_left = HDRM_BUTTON_TASK_NAV;
        visible_top_right = HDRM_BUTTON_NONE;
        clutter_actor_show(CLUTTER_ACTOR(priv->home));
        hd_render_manager_set_blur(HDRM_BLUR_NONE);
        hd_home_update_layout (priv->home);
        break;
      case HDRM_STATE_HOME_EDIT:
      case HDRM_STATE_HOME_EDIT_DLG:
        visible_top_left = HDRM_BUTTON_NONE;
        visible_top_right = HDRM_BUTTON_NONE;
        clutter_actor_show(CLUTTER_ACTOR(priv->home));
        hd_render_manager_set_blur(HDRM_BLUR_HOME);
        hd_home_update_layout (priv->home);
        break;
      case HDRM_STATE_APP:
        visible_top_left = HDRM_BUTTON_TASK_NAV;
        visible_top_right = HDRM_BUTTON_NONE;
        /* Fall through */
      case HDRM_STATE_APP_PORTRAIT:
        clutter_actor_hide(CLUTTER_ACTOR(priv->home));
        hd_render_manager_set_blur(HDRM_BLUR_NONE | HDRM_SHOW_TASK_NAV);
        break;
      case HDRM_STATE_APP_FULLSCREEN:
        visible_top_left = HDRM_BUTTON_NONE;
        visible_top_right = HDRM_BUTTON_NONE;
        hd_render_manager_set_blur(HDRM_BLUR_NONE);
        break;
      case HDRM_STATE_TASK_NAV:
        visible_top_left = HDRM_BUTTON_LAUNCHER;
        visible_top_right = HDRM_BUTTON_NONE;
        clutter_actor_show(CLUTTER_ACTOR(priv->home));
        hd_render_manager_set_blur(HDRM_BLUR_HOME | HDRM_ZOOM_HOME | HDRM_SHOW_TASK_NAV);
        break;
      case HDRM_STATE_LAUNCHER:
        visible_top_left = HDRM_BUTTON_NONE;
        visible_top_right = HDRM_BUTTON_BACK;
        clutter_actor_show(CLUTTER_ACTOR(priv->home));
        clutter_actor_show(CLUTTER_ACTOR(priv->launcher));
        clutter_actor_raise_top(CLUTTER_ACTOR(priv->launcher));
        hd_render_manager_set_blur(
            HDRM_BLUR_HOME |
            HDRM_ZOOM_HOME | HDRM_ZOOM_TASK_NAV );
        break;
    }

  clutter_actor_show(CLUTTER_ACTOR(priv->home_blur));
  clutter_actor_show(CLUTTER_ACTOR(priv->app_top));
  clutter_actor_show(CLUTTER_ACTOR(priv->front));
  clutter_actor_raise_top(CLUTTER_ACTOR(priv->app_top));
  clutter_actor_raise_top(CLUTTER_ACTOR(priv->front));

  hd_title_bar_set_show(priv->title_bar, STATE_SHOW_TITLE(priv->state));

  if (STATE_SHOW_OPERATOR(priv->state))
    clutter_actor_show(priv->operator);
  else
    clutter_actor_hide(priv->operator);

  if (STATE_SHOW_STATUS_AREA(priv->state) && priv->status_area
      && (!priv->has_fullscreen || !STATE_IS_APP(priv->state)))
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
      break;
    case HDRM_BUTTON_LAUNCHER:
      btn_state |= HDTB_VIS_BTN_LAUNCHER;
      break;
    case HDRM_BUTTON_TASK_NAV:
      btn_state |= HDTB_VIS_BTN_SWITCHER;
      break;
    default:
      g_warning("%s: Invalid button %d in top-left",
          __FUNCTION__, visible_top_left);
  }
  switch (visible_top_right)
  {
    case HDRM_BUTTON_NONE:
      break;
    case HDRM_BUTTON_BACK:
      btn_state |= HDTB_VIS_BTN_BACK;
      break;
    default:
      g_warning("%s: Invalid button %d in top-right",
          __FUNCTION__, visible_top_right);
  }

  if (priv->status_menu)
    clutter_actor_raise_top(CLUTTER_ACTOR(priv->status_menu));

  if (!STATE_BLUR_BUTTONS(priv->state) &&
      clutter_actor_get_parent(CLUTTER_ACTOR(priv->blur_front)) !=
              CLUTTER_ACTOR(priv->front))
    {
      /* raise the blur_front out of the blur group so we can still
       * see it unblurred */
      clutter_actor_reparent(CLUTTER_ACTOR(priv->blur_front),
                             CLUTTER_ACTOR(priv->front));
      hd_render_manager_blurred_changed();
    }
  clutter_actor_raise_top(CLUTTER_ACTOR(priv->blur_front));

  if (STATE_HOME_FRONT(priv->state))
    {
      clutter_actor_reparent(
          hd_home_get_front(priv->home),
          CLUTTER_ACTOR(priv->blur_front));
      clutter_actor_lower_bottom(hd_home_get_front(priv->home));
    }
  else
    {
      clutter_actor_reparent(
          hd_home_get_front(priv->home),
          CLUTTER_ACTOR(priv->home));
    }

  hd_title_bar_set_state(priv->title_bar, btn_state);
  hd_render_manager_place_titlebar_elements();

  /* update our fixed title bar at the top of the screen */
  hd_title_bar_update(priv->title_bar, MB_WM_COMP_MGR(priv->comp_mgr));

  /* Now look at what buttons we have showing, and add each visible button X
   * to the X input viewport */
  hd_render_manager_set_input_viewport();
}

/* The syncing with clutter that is done after a transition ends */
static
void hd_render_manager_sync_clutter_after ()
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  hd_title_bar_left_pressed(priv->title_bar, FALSE);

  if (STATE_BLUR_BUTTONS(priv->state))
    {
      /* raise the blur_front to the top of the home_blur group so
       * we still see the apps */
      clutter_actor_reparent(CLUTTER_ACTOR(priv->blur_front),
                             CLUTTER_ACTOR(priv->home_blur));
    }

  /* If we've gone back to the app state, make
   * sure we blur the right things (fix problem where going from
   * task launcher->app breaks blur)
   */
  if (STATE_IS_APP(priv->state))
    hd_render_manager_update_blur_state(0);

  /* The launcher transition should hide the launcher, so we shouldn't
   * need this.
  if (priv->state != HDRM_STATE_LAUNCHER)
    clutter_actor_hide(CLUTTER_ACTOR(priv->launcher));*/
}

/* ------------------------------------------------------------------------- */
/* -------------------------------------------------------------    PUBLIC   */
/* ------------------------------------------------------------------------- */

void hd_render_manager_stop_transition()
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;

  if (priv->timeline_playing)
    {
      guint frames;
      clutter_timeline_stop(priv->timeline_blur);
      frames = clutter_timeline_get_n_frames(priv->timeline_blur);
      on_timeline_blur_new_frame(priv->timeline_blur, frames, the_render_manager);
      on_timeline_blur_completed(priv->timeline_blur, the_render_manager);
    }

  hd_launcher_transition_stop();
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
      MBWMCompMgrClient *cc;

      cc = g_object_get_data(G_OBJECT(item), "HD-MBWMCompMgrClutterClient");
      priv->status_area_client = cc->wm_client;
      priv->status_area = g_object_ref(item);
      clutter_actor_reparent(priv->status_area, CLUTTER_ACTOR(priv->blur_front));
      g_signal_connect(item, "notify::allocation",
                       G_CALLBACK(hd_render_manager_place_titlebar_elements),
                       NULL);
    }
  else
    {
      priv->status_area = NULL;
      priv->status_area_client = NULL;
    }

  hd_render_manager_place_titlebar_elements();
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
    g_object_unref(priv->operator);
  priv->operator = CLUTTER_ACTOR(g_object_ref(item));
}

void hd_render_manager_set_button (HDRMButtonEnum btn,
                                   ClutterActor *item)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;
  gboolean reparent = TRUE;

  switch (btn)
    {
      case HDRM_BUTTON_TASK_NAV:
        g_assert(!priv->button_task_nav);
        priv->button_task_nav = CLUTTER_ACTOR(g_object_ref(item));
        reparent = FALSE;
        return; /* Don't reparent, it's fine where it is. */
      case HDRM_BUTTON_LAUNCHER:
        g_assert(!priv->button_launcher);
        priv->button_launcher = CLUTTER_ACTOR(g_object_ref(item));
        reparent = FALSE;
        return; /* Likewise */
      case HDRM_BUTTON_BACK:
        g_assert(!priv->button_back);
        priv->button_back = CLUTTER_ACTOR(g_object_ref(item));
        reparent = FALSE;
        break;
      case HDRM_BUTTON_EDIT:
        g_warning("%s: edit button must be set at creation time!", __FUNCTION__);
	g_assert(FALSE);
        break;
      default:
        g_warning("%s: Invalid Enum %d", __FUNCTION__, btn);
	g_assert(FALSE);
    }
  if (reparent)
    {
      if (clutter_actor_get_parent(CLUTTER_ACTOR(item)))
        clutter_actor_reparent(CLUTTER_ACTOR(item),
                               CLUTTER_ACTOR(priv->blur_front));
      else
        clutter_container_add_actor(CLUTTER_CONTAINER(priv->blur_front),
                                    CLUTTER_ACTOR(item));
    }
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
      case HDRM_BUTTON_BACK:
        return priv->button_back;
      case HDRM_BUTTON_EDIT:
        return priv->button_edit;
      default:
        g_warning("%s: Invalid Enum %d", __FUNCTION__, button);
	g_assert(FALSE);
    }
  return 0;
}

ClutterActor *hd_render_manager_get_title_bar(void)
{
  return CLUTTER_ACTOR(the_render_manager->priv->title_bar);
}

ClutterActor *hd_render_manager_get_status_area(void)
{
  return CLUTTER_ACTOR(the_render_manager->priv->status_area);
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
      hd_render_manager_set_input_viewport();
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
    case HDRM_BUTTON_TASK_NAV:
      return CLUTTER_ACTOR_IS_VISIBLE(priv->button_task_nav);
    case HDRM_BUTTON_LAUNCHER:
      return CLUTTER_ACTOR_IS_VISIBLE(priv->button_launcher);
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

          hd_wm_current_app_is (wm, 0);
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

          mb_wm_handle_show_desktop(wm, show);
        }

      /* we always need to restack here */
      hd_comp_mgr_restack(MB_WM_COMP_MGR(priv->comp_mgr));

      /*
       * Coming not from portrait mode going to APP state.
       * Consider APP_PORTRAIT state instead.  Needs to check
       * after restacking because should_be_portrait() needs
       * visibilities to be sorted out.
       */
      if (oldstate != HDRM_STATE_APP_PORTRAIT && state == HDRM_STATE_APP
          && hd_comp_mgr_should_be_portrait (priv->comp_mgr))
        {
          priv->in_set_state = FALSE;
          hd_render_manager_set_state (HDRM_STATE_APP_PORTRAIT);
          return;
        }

      hd_render_manager_sync_clutter_before();

      /* Switch between portrait <=> landscape modes. */
      if (state == HDRM_STATE_APP_PORTRAIT)
        hd_util_change_screen_orientation (wm, TRUE);
      else if (oldstate == HDRM_STATE_APP_PORTRAIT)
        hd_util_change_screen_orientation (wm, FALSE);

      /* Signal the state has changed. */
      g_object_notify (G_OBJECT (the_render_manager), "state");
    }
  priv->in_set_state = FALSE;
}

/* Returns whether set_state() is in progress. */
gboolean hd_render_manager_is_changing_state(void)
{
  return the_render_manager->priv->in_set_state;
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
    case HDRM_STATE_HOME_EDIT_DLG : return "HDRM_STATE_HOME_EDIT_DLG";
    case HDRM_STATE_APP : return "HDRM_STATE_APP";
    case HDRM_STATE_APP_FULLSCREEN : return "HDRM_STATE_APP_FULLSCREEN";
    case HDRM_STATE_APP_PORTRAIT: return "HDRM_STATE_APP_PORTRAIT";
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

static void
hd_render_manager_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  switch (property_id)
    {
    case PROP_STATE:
      g_value_set_enum (value, hd_render_manager_get_state ());
      break;
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
hd_render_manager_set_property (GObject      *gobject,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_STATE:
      hd_render_manager_set_state (g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
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
  gboolean      blur_changed = FALSE;

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
                {
                  if (parent == CLUTTER_ACTOR(priv->home_blur))
                    blur_changed = TRUE;
                  clutter_actor_reparent(actor, desktop);
                }
              g_object_unref(actor);
            }
        }

      c = c->stacked_above;
    }

  if (blur_changed)
    hd_render_manager_blurred_changed();
}

/* Return @actor, an actor of a %HdApp to HDRM's care. */
void hd_render_manager_return_app(ClutterActor *actor)
{
  clutter_actor_reparent(actor,
                         CLUTTER_ACTOR(the_render_manager->priv->home_blur));
  clutter_actor_hide (actor);
}

/* Same for dialogs. */
void hd_render_manager_return_dialog(ClutterActor *actor)
{
  clutter_actor_reparent(actor,
                         CLUTTER_ACTOR(the_render_manager->priv->app_top));
  clutter_actor_hide (actor);
}

/* Called to restack the windows in the way we use for rendering... */
void hd_render_manager_restack()
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;
  MBWindowManager *wm;
  MBWindowManagerClient *c;
  gboolean past_desktop = FALSE;
  gboolean blur_changed = FALSE;
  gint i;
  GList *previous_home_blur = 0;


  wm = MB_WM_COMP_MGR(priv->comp_mgr)->wm;
  /* Add all actors currently in the home_blur group */

  for (i=0;i<clutter_group_get_n_children(CLUTTER_GROUP(priv->home_blur));i++)
    previous_home_blur = g_list_prepend(previous_home_blur,
        clutter_group_get_nth_child(CLUTTER_GROUP(priv->home_blur), i));

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
                  if (parent == CLUTTER_ACTOR(priv->home_blur))
                    clutter_actor_raise_top(actor);
                }
              else
                {
                  /* else we put it back into the arena */
                  if (parent == CLUTTER_ACTOR(priv->home_blur) ||
                      parent == CLUTTER_ACTOR(priv->app_top))
                    clutter_actor_reparent(actor, desktop);
                }
              g_object_unref(actor);
            }
        }
    }

  /* Now start at the top and put actors in the non-blurred group
   * until we find one that fills the screen. If we didn't find
   * any that filled the screen then add the window that does. */
  {
    gint i, n_elements;

    n_elements = clutter_group_get_n_children(CLUTTER_GROUP(priv->home_blur));
    for (i=n_elements-1;i>=0;i--)
      {
        ClutterActor *child =
          clutter_group_get_nth_child(CLUTTER_GROUP(priv->home_blur), i);


        if (child != CLUTTER_ACTOR(priv->home) &&
            child != CLUTTER_ACTOR(priv->blur_front))
          {
            ClutterGeometry geo;
            gboolean maximized;

            clutter_actor_get_geometry(child, &geo);
            maximized = HD_COMP_MGR_CLIENT_IS_MAXIMIZED(geo);
            /* Maximized stuff should never be blurred (unless there
             * is nothing else) */
            if (!maximized)
              {
                clutter_actor_reparent(child, CLUTTER_ACTOR(priv->app_top));
                clutter_actor_lower_bottom(child);
                clutter_actor_show(child); /* because it is in app-top, vis
                                              check does not get applied */
              }
            /* If this is maximized, or in dialog's position, don't
             * blur anything after */
            if (maximized || (
                geo.width == HD_COMP_MGR_SCREEN_WIDTH &&
                geo.y + geo.height == HD_COMP_MGR_SCREEN_HEIGHT
                ))
              break;

          }
      }
  }

  clutter_actor_raise_top(CLUTTER_ACTOR(priv->blur_front));

  /* And for speed of rendering, work out what is visible and what
   * isn't, and hide anything that would be rendered over by another app */
  hd_render_manager_set_visibilities();

  /* now compare the contents of home_blur to see if the blur group has
   * actually changed... */
  if (g_list_length(previous_home_blur) ==
      clutter_group_get_n_children(CLUTTER_GROUP(priv->home_blur)))
    {
      GList *it;
      for (i = 0, it = g_list_last(previous_home_blur);
           (i<clutter_group_get_n_children(CLUTTER_GROUP(priv->home_blur))) && it;
           i++, it=it->prev)
        {
          ClutterActor *child =
              clutter_group_get_nth_child(CLUTTER_GROUP(priv->home_blur), i);
          if (CLUTTER_ACTOR(it->data) != child)
            {
              //g_debug("*** RE-BLURRING *** because contents changed at pos %d", i);
              blur_changed = TRUE;
              break;
            }
        }
    }
  else
    {
      /*g_debug("*** RE-BLURRING *** because contents changed size %d -> %d",
          g_list_length(previous_home_blur),
          clutter_group_get_n_children(CLUTTER_GROUP(priv->home_blur)));*/
      blur_changed = TRUE;
    }
  g_list_free(previous_home_blur);
/*
  for (i = 0;i<clutter_group_get_n_children(CLUTTER_GROUP(priv->home_blur));i++)
    {
      ClutterActor *child =
                clutter_group_get_nth_child(CLUTTER_GROUP(priv->home_blur), i);
      const char *name = clutter_actor_get_name(child);
      g_debug("STACK[%d] %s %s", i, name?name:"?",
          CLUTTER_ACTOR_IS_VISIBLE(child)?"":"(invisible)");
    }
*/
  /* because swapping parents doesn't appear to fire a redraw */
  if (blur_changed)
    hd_render_manager_blurred_changed();

  /* update our fixed title bar at the top of the screen */
  hd_title_bar_update(priv->title_bar, MB_WM_COMP_MGR(priv->comp_mgr));
}

void hd_render_manager_update_blur_state(MBWindowManagerClient *ignore)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;
  HDRMBlurEnum blur_flags;
  MBWindowManager *wm = MB_WM_COMP_MGR(priv->comp_mgr)->wm;
  MBWindowManagerClient *c;
  gboolean blur = FALSE;

  /* Now look through the MBWM stack and see if we need to blur or not.
   * This happens when we have a dialog/menu in front of the main app */
  for (c=wm->stack_top;c;c=c->stacked_below)
    {
      int c_type = MB_WM_CLIENT_CLIENT_TYPE(c);
      if (hd_comp_mgr_ignore_window(c) || c==ignore)
        continue;
      if (c_type == MBWMClientTypeApp ||
          c_type == MBWMClientTypeDesktop)
        break;
      if (c_type == MBWMClientTypeDialog ||
          c_type == MBWMClientTypeMenu ||
          c_type == HdWmClientTypeAppMenu ||
          c_type == HdWmClientTypeStatusMenu ||
          HD_IS_CONFIRMATION_NOTE (c))
        {
          /* If this is a dialog that is maximised, it will be put in the
           * blur group - so do NOT blur the background. */
          if (HD_COMP_MGR_CLIENT_IS_MAXIMIZED(c->window->geometry))
            break;

          /*g_debug("%s: Blurring caused by window type %d, geo=%d,%d,%d,%d name '%s'",
              __FUNCTION__, c_type,
              c->window->geometry.x, c->window->geometry.y,
              c->window->geometry.width, c->window->geometry.height,
              c->name?c->name:"(null)");*/
          blur=TRUE;
          break;
        }
    }

  blur_flags = priv->current_blur;

  if (blur)
    blur_flags = blur_flags | HDRM_BLUR_BACKGROUND;
  else
    blur_flags = blur_flags & ~HDRM_BLUR_BACKGROUND;
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

  for (i = 1; i <= HDRM_BUTTON_COUNT; ++i)
    {
      ClutterActor *button = hd_render_manager_get_button((HDRMButtonEnum)i);
      clutter_actor_set_reactive(button, reactive);
    }

  hd_home_set_reactive (the_render_manager->priv->home, reactive);
}

/* Work out if rect is visible after being clipped to avoid every
 * rect in blockers */
static gboolean
hd_render_manager_is_visible(GList *blockers,
                             ClutterGeometry rect)
{
  /* clip for every block */
  for (; blockers; blockers = blockers->next)
    {
      ClutterGeometry blocker = *(ClutterGeometry*)blockers->data;
      guint rect_b, blocker_b;

      /* If rect does not fit inside blocker in the X axis... */
      if (!(blocker.x <= rect.x &&
            rect.x+rect.width <= blocker.x+blocker.width))
        continue;

      /* Because most windows will go edge->edge, just do a very simplistic
       * clipping in the Y direction */
      rect_b    = rect.y + rect.height;
      blocker_b = blocker.y + blocker.height;

      if (rect.y < blocker.y)
        { /* top of rect is above blocker */
          if (rect_b < blocker.y)
            /* rect is above blocker */
            continue;
          if (rect_b < blocker_b)
            /* rect is half above blocker, clip the rest */
            rect.height -= rect_b - blocker.y;
          else
            { /* rect is split into two pieces by blocker */
              rect.height = blocker.y - rect.y;
              if (hd_render_manager_is_visible(blockers, rect))
                /* upper half is visible */
                return TRUE;

              /* continue with the lower half */
              rect.y = blocker_b;
              rect.height = rect_b - blocker_b;
            }
        }
      else if (rect.y < blocker_b)
        { /* top of rect is inside blocker */
          if (rect_b < blocker_b)
            /* rect is confined in blocker */
            return FALSE;
          else
            { /* rect is half below blocker, clip the rest */
              rect.height -= blocker_b - rect.y;
              rect.y       = blocker_b;
            }
        }
      else
        /* rect is completely below blocker */;

      if (blocker.x <= rect.x &&
          rect.x+rect.width <= blocker.x+blocker.width)
        {
          if (blocker.y <= rect.y &&
              rect.y+rect.height <= blocker.y+blocker.height)
            {
              /* If rect fits inside blocker in the Y axis,
               * it is def. not visible */
              return FALSE;
            }
          else if (rect.y < blocker.y)
            {
              /* safety - if the blocker sits in the middle of us
               * it makes 2 rects, so don't use it */
              if (blocker.y+blocker.height >= rect.y+rect.height)
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
    }

  return TRUE;
}

static
MBWindowManagerClient*
hd_render_manager_get_wm_client_from_actor(ClutterActor *actor)
{
  MBWindowManager *wm;
  MBWindowManagerClient *c;

  /* TODO: use g_object_get_data on actor - "MBWMCompMgrClutterClient" */

  wm = MB_WM_COMP_MGR(the_render_manager->priv->comp_mgr)->wm;
  /* Order and choose which window actors will be visible */
  for (c = wm->stack_bottom; c; c = c->stacked_above)
    if (c->cm_client) {
      ClutterActor *cactor = mb_wm_comp_mgr_clutter_client_get_actor(
                               MB_WM_COMP_MGR_CLUTTER_CLIENT(c->cm_client));
      g_object_unref(cactor);
      if (actor == cactor)
        return c;
    }
  return 0;
}

static
gboolean hd_render_manager_actor_opaque(ClutterActor *actor)
{
  MBWindowManager *wm;
  MBWindowManagerClient *wm_client;

  /* First off, try and find out from the actor if it is opaque or not */
  if (CLUTTER_IS_GROUP(actor) &&
      clutter_group_get_n_children(CLUTTER_GROUP(actor))==1)
    {
      ClutterActor *texture =
                        clutter_group_get_nth_child(CLUTTER_GROUP(actor), 0);
      if (CLUTTER_IS_TEXTURE(texture))
        {
          CoglHandle *tex = clutter_texture_get_cogl_texture(
                              CLUTTER_TEXTURE(texture));
          if (tex)
            return (cogl_texture_get_format(tex) & COGL_A_BIT) == 0;
        }
    }
  /* this is ugly and slow, but is hopefully just a fallback... */
  wm = MB_WM_COMP_MGR(the_render_manager->priv->comp_mgr)->wm;
  wm_client = hd_render_manager_get_wm_client_from_actor(actor);
  return wm_client &&
         !wm_client->is_argb32 &&
         !mb_wm_theme_is_client_shaped(wm->theme, wm_client);
}

static
void hd_render_manager_append_geo_cb(ClutterActor *actor, gpointer data)
{
  GList **list = (GList**)data;
  if (hd_render_manager_actor_opaque(actor))
    {
      ClutterGeometry *geo = g_malloc(sizeof(ClutterGeometry));
      clutter_actor_get_geometry(actor, geo);
      *list = g_list_prepend(*list, geo);
    }
}

static
void hd_render_manager_set_visibilities()
{
  HdRenderManagerPrivate *priv;
  GList *blockers = 0;
  GList *it;
  gint i, n_elements;
  ClutterGeometry fullscreen_geo = {0, 0, HDRM_WIDTH, HDRM_HEIGHT};
  MBWindowManager *wm;
  MBWindowManagerClient *c;

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
          ClutterGeometry geo;
          clutter_actor_get_geometry(child, &geo);
          /*TEST clutter_actor_set_opacity(child, 63);*/
          if (hd_render_manager_is_visible(blockers, geo))
            clutter_actor_show(child);
          else
            clutter_actor_hide(child);
          /* Add the geometry to our list of blockers and go to next... */
          if (hd_render_manager_actor_opaque(child))
            blockers = g_list_prepend(blockers, g_memdup(&geo, sizeof(geo)));
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

  /* Do we have a fullscreen client visible? */
  priv->has_fullscreen = FALSE;
  wm = MB_WM_COMP_MGR(priv->comp_mgr)->wm;
  for (c = wm->stack_top; c && !priv->has_fullscreen; c = c->stacked_below)
    {
      if (c->cm_client && c->desktop >= 0) /* FIXME: should check against
                                              current desktop? */
        {
          ClutterActor *actor = mb_wm_comp_mgr_clutter_client_get_actor(
              MB_WM_COMP_MGR_CLUTTER_CLIENT(c->cm_client));
          if (actor && CLUTTER_ACTOR_IS_VISIBLE(actor))
            {
              g_object_unref(actor);
              if (c->window)
                priv->has_fullscreen |= c->window->ewmh_state &
                  MBWMClientWindowEWMHStateFullscreen;
            }
        }
    }

  /* If we have a fullscreen something hide the blur_front
   * and move SA out of the way.  BTW blur_front is implcitly
   * shown by clutter when reparented. */
  c = priv->status_area_client;
  if (priv->has_fullscreen)
    {
      clutter_actor_hide(CLUTTER_ACTOR(priv->blur_front));
      if (c && c->frame_geometry.y >= 0)
        { /* Move SA out of the way. */
          c->frame_geometry.y   = -c->frame_geometry.height;
          c->window->geometry.y = -c->window->geometry.height;
          mb_wm_client_geometry_mark_dirty(c);
        }
    }
  else
    {
      clutter_actor_show(CLUTTER_ACTOR(priv->blur_front));
      if (c && c->frame_geometry.y < 0)
        { /* Restore the position of SA. */
          c->frame_geometry.y = c->window->geometry.y = 0;
          mb_wm_client_geometry_mark_dirty(c);
        }
    }
  hd_render_manager_set_input_viewport();
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

/* Called by hd-task-navigator when its state changes, as when notifications
 * arrive the button in the top-left may need to change */
void hd_render_manager_update()
{
  hd_render_manager_sync_clutter_before();
}

/* Returns whether @c's actor is visible in clutter sense.  If so, then
 * it most probably is visible to the user as well.  It is assumed that
 * set_visibilities() have been sorted out for the current stacking. */
gboolean hd_render_manager_is_client_visible(MBWindowManagerClient *c)
{
  ClutterActor *a;
  MBWMCompMgrClutterClient *cc;

  if (!(cc = MB_WM_COMP_MGR_CLUTTER_CLIENT(c->cm_client)))
    return FALSE;
  if (!(a  = mb_wm_comp_mgr_clutter_client_get_actor(cc)))
    return FALSE;
  g_object_unref(a);

  /* It is necessary to check the parents because sometimes
   * hd_render_manager_set_visibilities() hides the container
   * altogether.  Stage is never visible. */
  while (a != CLUTTER_ACTOR (the_render_manager))
    {
      g_return_val_if_fail (a != NULL, FALSE);
      if (!CLUTTER_ACTOR_IS_VISIBLE(a))
        return FALSE;
      a = clutter_actor_get_parent(a);
    }

  return TRUE;
}

/* Place the status area, the operator logo and the title bar,
 * depending on the visible visual elements. */
void hd_render_manager_place_titlebar_elements (void)
{
  HdRenderManagerPrivate *priv = the_render_manager->priv;
  guint x;

  x = 0;

  if (CLUTTER_ACTOR_IS_VISIBLE(priv->button_task_nav)
      || CLUTTER_ACTOR_IS_VISIBLE(priv->button_launcher))
    x += HD_COMP_MGR_TOP_LEFT_BTN_WIDTH;

  if (priv->status_area && CLUTTER_ACTOR_IS_VISIBLE(priv->status_area))
    {
      g_assert(priv->status_area_client && priv->status_area_client->window);
      if (priv->status_area_client->frame_geometry.x != x)
        {
          /* Reposition the status area. */
          MBWindowManagerClient *c = priv->status_area_client;
          c->frame_geometry.x = c->window->geometry.x = x;
          mb_wm_client_geometry_mark_dirty(c);
          x += c->window->geometry.width;
        }
      else
        x += priv->status_area_client->frame_geometry.width;
    }

  if (priv->operator && CLUTTER_ACTOR_IS_VISIBLE(priv->operator))
    /* Don't update @x since operator and app title are not shown at once. */
    clutter_actor_set_x(priv->operator, x + HD_COMP_MGR_OPERATOR_PADDING);

  if (STATE_ONE_OF(priv->state, HDRM_STATE_APP | HDRM_STATE_APP_PORTRAIT))
    {
      /* Otherwise we don't show a title. */
      /* g_debug("application title at %u", x); */
      mb_adjust_dialog_title_position(MB_WM_COMP_MGR(priv->comp_mgr)->wm, x);
    }
}

void hd_render_manager_blurred_changed()
{
  if (!the_render_manager) return;

  tidy_blur_group_set_source_changed(
      CLUTTER_ACTOR(the_render_manager->priv->home_blur));
}
