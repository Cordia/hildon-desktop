/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2010 Moises Martinez
 *
 * Author:  Moises Martinez <moimart@gmail.com>
 *	    inspired by old hd-task-navigator.[ch] which
 *          didn't indicate license or author
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

#include "hd-task-navigator.h"
#include "hd-tn-layout.h"

#include <math.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <clutter/clutter.h>
#include <tidy/tidy-finger-scroll.h>

#include <matchbox/core/mb-wm.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr-clutter.h>

#include "hildon-desktop.h"
#include "hd-atoms.h"
#include "hd-comp-mgr.h"
#include "hd-scrollable-group.h"
#include "hd-switcher.h"
#include "hd-render-manager.h"
#include "hd-title-bar.h"
#include "hd-clutter-cache.h"
#include "hd-transition.h"
#include "hd-theme.h"
#include "hd-util.h"
#include "hd-theme-config.h"

#undef  G_LOG_DOMAIN
#define G_LOG_DOMAIN              "hd-task-navigator"

#define MARGIN_HALF                4
#define ICON_FINGER               48
#define ICON_STYLUS               32

/* Metrics inside a thumbnail. */
/*
 * %CLOSE_ICON_SIZE:              Now this *is* the graphics size of the
 *                                close button; used to calculate where
 *                                to clip the title.  The graphics is
 *                                located in the top-right corner of the
 *                                thumbnail.
 * %CLOSE_AREA_SIZE:              The size of the area where the user can
 *                                click to close the thumbnail.  Thie area
 *                                and the graphics are centered at the same
 *                                point.
 */
#define CLOSE_ICON_SIZE           32
#define CLOSE_AREA_SIZE           64
#define TITLE_LEFT_MARGIN         8
#define TITLE_RIGHT_MARGIN        MARGIN_HALF
#define TITLE_HEIGHT              32

/* These are NOT the dimensions of the frame graphics but marings. */
#define FRAME_TOP_HEIGHT          TITLE_HEIGHT
#define FRAME_WIDTH                2
#define FRAME_BOTTOM_HEIGHT        2
#define PRISON_XPOS               FRAME_WIDTH
#define PRISON_YPOS               FRAME_TOP_HEIGHT

/* Read: reduce by 4 pixels; only relevant if the time(label) wraps. */
#define NOTE_TIME_LINESPACING     pango_units_from_double(-4)
#define NOTE_MARGINS              8 
#define NOTE_BOTTOM_MARGIN        MARGIN_HALF
#define NOTE_ICON_GAP             MARGIN_HALF
#define NOTE_SEPARATOR_PADDING    0

/*
 * %ZOOM_EFFECT_DURATION:         Determines how many miliseconds should
 *                                it take to zoom thumbnails.  Tunable.
 *                                Increase for the better observation of
 *                                effects or decrase for faster feedback.
 *                                the windows are repositioned.
 * %NOTIFADE_IN_DURATION,
 * %NOTIFADE_OUT_DURATION:        Milisecs to fade in and out notifications.
 *                                These effects are executed in an independent
 *                                timeline, except when zooming in/out, when
 *                                they are coupled with @Zoom_effect_timeline.
 */
#if 1
# define ZOOM_EFFECT_DURATION     \
  hd_transition_get_int("task_nav", "zoom_duration", 250)
# define FLY_EFFECT_DURATION      \
  hd_transition_get_int("task_nav", "fly_duration",  250)
#else
# define ZOOM_EFFECT_DURATION     1000
# define FLY_EFFECT_DURATION      1000
#endif

#define NOTIFADE_IN_DURATION      \
  hd_transition_get_int("task_nav", "notifade_in", 250)
#define NOTIFADE_OUT_DURATION     \
  hd_transition_get_int("task_nav", "notifade_out", 250)

/*
 *  These are based on the UX Guidance.
 *
 * %MIN_CLICK_TIME:   Clicks shorter than this...
 * %MAX_CLICK_TIME:   or longer than this microseconds are not clicks.
 *                    In practice this is 30-300 ms.
 */
#define MIN_CLICK_TIME           30000
#define MAX_CLICK_TIME          300000
/* Standard definitions }}} */

typedef struct
{
  HdTaskNavigatorFunc fun;
  ClutterActor *actor;
  gpointer	param;
  HdTaskNavigator *navigator;
} TaskNavigatorClosure;

G_DEFINE_TYPE (HdTaskNavigator, hd_task_navigator, CLUTTER_TYPE_GROUP);
#define HD_TASK_NAVIGATOR_GET_PRIVATE(obj) \
                (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                HD_TYPE_TASK_NAVIGATOR, HdTaskNavigatorPrivate))

G_DEFINE_TYPE (HdTnThumbnail, hd_tn_thumbnail, CLUTTER_TYPE_GROUP);
#define HD_TN_THUMBNAIL_GET_PRIVATE(obj) \
                (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                HD_TYPE_TN_THUMBNAIL, HdTnThumbnailPrivate))


struct _HdTaskNavigatorPrivate
{
  ClutterActor *scroller;

  HdScrollableGroup *grid;

  GList	*thumbnails;

  guint n_thumbnails;

  ClutterTimeline *zoom_timeline;
  ClutterBehaviourScale *sbehaviour;
  ClutterBehaviourPath  *mbehaviour;
  ClutterKnot		origin;
  ClutterKnot		destination;
  ClutterActor	       *zoomed_actor;

  HdTnLayout	       *layout;
  gboolean		zoomed_out;
};

static void
hd_task_navigator_show (ClutterActor *actor)
{
  HdTaskNavigatorPrivate *priv =
    HD_TASK_NAVIGATOR_GET_PRIVATE (actor);

  CLUTTER_ACTOR_CLASS (hd_task_navigator_parent_class)->show (actor); 

  /* Reset the position of @Scroller, which may have been changed when
   * we zoomed in last time.  If the caller wants to zoom_out() it will
   * set them up properly. */
  clutter_actor_set_scale (priv->scroller, 1, 1);
  clutter_actor_set_position (priv->scroller, 0, 0);

  /* Take all application windows we know about into our care
   * because we are responsible for showing them now. */
  GList *l;
   
  for (l = priv->thumbnails; l != NULL; l = l->next)
    hd_tn_thumbnail_claim_window (HD_TN_THUMBNAIL (l->data));
 
  /* Flash the scrollbar.  %TidyFingerScroll will do the right thing. */
  tidy_finger_scroll_show_scrollbars (priv->scroller);

  /* When you zoom out from a window the layout will be calculated.
     If we calculate here then we will be doing it twice. As layouts could
     contain animations that should be just done once */
  if (!priv->zoomed_out)
    hd_tn_layout_calculate (priv->layout, 
			    priv->thumbnails, 
			    CLUTTER_ACTOR (priv->grid));
  else
    priv->zoomed_out = FALSE;
}

static void
hd_task_navigator_hide (ClutterActor *actor)
{
  HdTaskNavigatorPrivate *priv = 
    HD_TASK_NAVIGATOR_GET_PRIVATE (actor);

  CLUTTER_ACTOR_CLASS (hd_task_navigator_parent_class)->hide (actor);

  GList *l;

  /* Finish in-progress animations, allowing for the final placement of
   * the involved actors. */
  if (clutter_timeline_is_playing (priv->zoom_timeline))
    {
      /* %HdSwitcher must make sure it doesn't do silly things if the
       * user cancelled the zooming. */
      clutter_timeline_stop (priv->zoom_timeline);
    }

  if (hd_tn_layout_animation_in_progress (priv->layout))
    {
      hd_tn_layout_stop_animation (priv->layout);
    }

  for (l = priv->thumbnails; l != NULL; l = l->next)
    hd_tn_thumbnail_release_window (HD_TN_THUMBNAIL (l->data));
}

static gboolean
grid_touched (ClutterActor * grid, ClutterEvent * event, HdTaskNavigator *navigator)
{
  HdTaskNavigatorPrivate *priv = 
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);

  return (event->type == CLUTTER_BUTTON_RELEASE 
	  && hd_tn_layout_within_grid (priv->layout, &event->button, priv->thumbnails, grid) 
	  && !hd_scrollable_group_is_clicked (HD_SCROLLABLE_GROUP (grid)));
}

static gboolean
grid_clicked (HdTaskNavigator *navigator, ClutterButtonEvent * event)
{ /* Don't propagate the signal to @Navigator if it happened within_grid(). */
  HdTaskNavigatorPrivate *priv = 
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);

 return hd_tn_layout_within_grid (priv->layout, 
				  event, 
				  priv->thumbnails, 
				  CLUTTER_ACTOR (priv->grid));
}

static gboolean
unclip (ClutterActor *actor, GParamSpec * prop)
{
  /* Make sure not to recurse infinitely. */
  if (clutter_actor_has_clip (actor))
    clutter_actor_remove_clip (actor);
  return TRUE;
}

static void
hd_task_navigator_class_init (HdTaskNavigatorClass * klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->show = hd_task_navigator_show;
  actor_class->hide = hd_task_navigator_hide;

  g_type_class_add_private (klass, sizeof (HdTaskNavigatorPrivate));
 
  /* background_clicked() is emitted when the navigator is active
   * and the background (outside thumbnails and notifications) is
   * clicked (not to scroll the navigator). */
  g_signal_new ("background-clicked", G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                0, NULL, NULL, g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);

  /* thumbnail_clicked(@apwin) is emitted when the navigator is active
   * and @apwin's thumbnail is clicked by the user to open the task. */
  g_signal_new ("thumbnail-clicked", G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1, CLUTTER_TYPE_ACTOR);

  /* thumbnail_closed(@apwin) is emitted when the navigator is active
   * and @apwin's thumbnail is clicked by the user to close the task. */
  g_signal_new ("thumbnail-closed", G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
                G_TYPE_NONE, 1, CLUTTER_TYPE_ACTOR);

  /* notification_clicked(@hdnote) is emitted when the notification window
   * is clicked by the user to activate its action. */
  g_signal_new ("notification-clicked", G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                0, NULL, NULL, g_cclosure_marshal_VOID__POINTER,
                G_TYPE_NONE, 1, G_TYPE_POINTER);

  /* Like thumbnail_closed. */
  g_signal_new ("notification-closed", G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                0, NULL, NULL, g_cclosure_marshal_VOID__POINTER,
                G_TYPE_NONE, 1, G_TYPE_POINTER);

  /* When the zoom in transition has finished */
  g_signal_new ("zoom-in-complete", G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  0, NULL, NULL, g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static gboolean
scroller_touched (ClutterActor *scroller, ClutterEvent *event, HdTaskNavigator *navigator)
{
  HdTaskNavigatorPrivate *priv =
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);
 
  /* Don't start scrolling outside the grid. */
  if (event->type == CLUTTER_BUTTON_PRESS 
      && !hd_tn_layout_within_grid (priv->layout, 
				    &event->button, 
				    priv->thumbnails, 
				    CLUTTER_ACTOR (priv->grid)))
    {
      g_signal_stop_emission_by_name (scroller, "captured-event");
    }

  return FALSE;
}

static ClutterActor *
clicked_widget (HdTaskNavigator *navigator,
		ClutterButtonEvent *event)
{
  HdTaskNavigatorPrivate *priv = 
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);

  if (event->source != CLUTTER_ACTOR (navigator)
      && event->source != CLUTTER_ACTOR (priv->scroller)
      && event->source != CLUTTER_ACTOR (priv->grid))
    {
      return event->source;
    }
  else 
  if (hd_tn_layout_within_grid (priv->layout, event, priv->thumbnails, CLUTTER_ACTOR (priv->grid)))
    return CLUTTER_ACTOR (priv->grid);
  else
    return CLUTTER_ACTOR (navigator);
}
/* @Navigator::button-release-event handler.  Handles events propagated
 * by grid_clicked(). */
static gboolean
navigator_clicked (ClutterActor *navigator, ClutterButtonEvent *event)
{ 
  /* Tell %HdSwitcher about it, which will hide us. */
  g_signal_emit_by_name (navigator, "background-clicked");

  return TRUE;
}

static gboolean
navigator_touched (ClutterActor *navigator, ClutterEvent *event)
{
  static struct timeval last_press_time;
  static ClutterActor *pressed_widget;

  if (event->type == CLUTTER_BUTTON_PRESS)
    {
      gettimeofday (&last_press_time, NULL);
      pressed_widget = clicked_widget (HD_TASK_NAVIGATOR (navigator), 
				       &event->button);
    }
  else 
  if (event->type == CLUTTER_BUTTON_RELEASE && pressed_widget)
    {
      struct timeval now;
      ClutterActor *widget;
      int dt;

      /* Don't interfere if we somehow get release events without
       * corresponding press events. */
      widget = pressed_widget;
      pressed_widget = NULL;

      /* Check press-release time. */
      gettimeofday (&now, NULL);
      if (now.tv_usec > last_press_time.tv_usec)
        dt = now.tv_usec-last_press_time.tv_usec
          + (now.tv_sec-last_press_time.tv_sec) * 1000000;
      else /* now.sec > last.sec */
        dt = (1000000-last_press_time.tv_usec)+now.tv_usec
          + (now.tv_sec-last_press_time.tv_sec-1) * 1000000;
      if (!(MIN_CLICK_TIME <= dt && dt <= MAX_CLICK_TIME))
        return TRUE;

      /* Verify that the pressed widget is the same as clicked_widget(). */
      if (widget != clicked_widget (HD_TASK_NAVIGATOR (navigator),&event->button))
        return TRUE;
    }

  return FALSE;
}

static HdTnThumbnail *
find_thumbnail_by_apwin (HdTaskNavigatorPrivate *priv, ClutterActor *apwin)
{
  GList *l;
  ClutterActor *actor;

  for (l = priv->thumbnails; l != NULL; l = l->next)
    {
      g_object_get (l->data,
		    "window",
		    &actor,
		    NULL);

      if (actor == apwin)
        return HD_TN_THUMBNAIL (l->data);
    }

  return NULL;
}

static void
hd_task_navigator_timeline_end (HdTaskNavigator *navigator)
{
  HdTaskNavigatorPrivate *priv =
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);

  if (priv->zoomed_actor != NULL)
    {
       clutter_behaviour_remove (CLUTTER_BEHAVIOUR (priv->sbehaviour),
				priv->zoomed_actor);
       clutter_behaviour_remove (CLUTTER_BEHAVIOUR (priv->mbehaviour),
				priv->zoomed_actor);

       priv->zoomed_actor = NULL;
    }
}

static void
hd_task_navigator_init (HdTaskNavigator *navigator)
{
  HdTaskNavigatorPrivate *priv =
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);
  ClutterAlpha *alpha;

  priv->thumbnails = NULL;

  priv->n_thumbnails = 0;

  priv->layout = hd_tn_layout_factory_get_layout ();

  clutter_actor_set_reactive (CLUTTER_ACTOR (navigator), TRUE);

  clutter_actor_set_size (CLUTTER_ACTOR (navigator), 
			  hd_comp_mgr_get_current_screen_width (),
			  hd_comp_mgr_get_current_screen_height ());

  priv->zoomed_out = FALSE;

  priv->zoomed_actor = NULL;

  priv->zoom_timeline = 
#ifndef CLUTTER_08
    clutter_timeline_new_for_duration (ZOOM_EFFECT_DURATION); 
#else
    clutter_timeline_new (ZOOM_EFFECT_DURATION);
#endif  

  g_signal_connect_swapped (priv->zoom_timeline,
			    "completed",
			    G_CALLBACK (hd_task_navigator_timeline_end),
			    navigator);

#ifndef CLUTTER_08
  alpha = clutter_alpha_new_full (priv->zoom_timeline,
				  CLUTTER_ALPHA_SMOOTHSTEP_INC,
			          NULL, NULL);
#else
  alpha = clutter_alpha_new_full (priv->zoom_timeline,
				  CLUTTER_LINEAR);
#endif

  priv->sbehaviour = 
   CLUTTER_BEHAVIOUR_SCALE (clutter_behaviour_scale_new (alpha, 0, 0, 1, 1));
  priv->mbehaviour = 
   CLUTTER_BEHAVIOUR_PATH (clutter_behaviour_path_new (alpha, &priv->destination, 1));

  g_signal_connect (navigator,
		    "captured-event",
                    G_CALLBACK (navigator_touched),
		    NULL);

  g_signal_connect (navigator,
		    "button-release-event",
                    G_CALLBACK (navigator_clicked),
		    NULL);

  /* Turn off visibility detection for @Scroller to it won't be clipped by it. */
  priv->scroller = 
    tidy_finger_scroll_new (TIDY_FINGER_SCROLL_MODE_KINETIC);

  clutter_actor_set_name (priv->scroller, "Scroller");

  clutter_actor_set_size (priv->scroller,
			  hd_comp_mgr_get_current_screen_width (),
			  hd_comp_mgr_get_current_screen_height ());
#ifndef CLUTTER_08
  clutter_actor_set_visibility_detect(priv->scroller, FALSE);
#endif
  clutter_container_add_actor (CLUTTER_CONTAINER (navigator), 
			       priv->scroller);

  g_signal_connect (priv->scroller,
		    "captured-event",
                    G_CALLBACK (scroller_touched),
		    navigator);

  /*
   * When we zoom in we may need to move the @Scroller up or downwards.
   * If we leave clipping on that becomes visible then, by cutting one
   * half of the zooming window.  Circumvent it by removing clipping
   * at the same time it is set.  TODO This can be considered a hack.
   */
  priv->grid = 
    HD_SCROLLABLE_GROUP (hd_scrollable_group_new ());

  clutter_actor_set_name (CLUTTER_ACTOR (priv->grid), "Grid");

  clutter_actor_set_reactive (CLUTTER_ACTOR (priv->grid), TRUE);
#ifndef CLUTTER_08
  clutter_actor_set_visibility_detect(CLUTTER_ACTOR (priv->grid), FALSE);
#endif
  g_signal_connect (priv->grid, 
		    "notify::has-clip",
		    G_CALLBACK (unclip),
		    NULL);

  g_signal_connect_after (priv->grid, 
			  "captured-event",
                          G_CALLBACK (grid_touched),
			  navigator);

  g_signal_connect_swapped (priv->grid, 
		    	    "button-release-event",
                    	    G_CALLBACK (grid_clicked),
		    	    navigator);

  clutter_container_add_actor (CLUTTER_CONTAINER (priv->scroller),
                               CLUTTER_ACTOR (priv->grid));

  /* We don't have anything to show yet, so let's hide. */
  clutter_actor_hide (CLUTTER_ACTOR (navigator));
}

static void
closure_zoom_in (TaskNavigatorClosure *tnc, ClutterTimeline *timeline);

static void
closure_zoom_in (TaskNavigatorClosure *tnc, ClutterTimeline *timeline)
{
   if (tnc->fun != NULL)
     tnc->fun (tnc->actor, tnc->param);


   g_signal_handlers_disconnect_by_func (timeline,
                                         closure_zoom_in,
					 tnc);

   g_free (tnc);
}

static void
hd_task_navigator_postpone_remove (TaskNavigatorClosure *tnc,
				   ClutterTimeline *timeline);

static void
hd_task_navigator_postpone_remove (TaskNavigatorClosure *tnc,
				   ClutterTimeline *timeline)
{
  hd_task_navigator_remove_window (tnc->navigator,
				   tnc->actor,
				   tnc->fun,
				   tnc->param);

  
  g_signal_handlers_disconnect_by_func (timeline,
                                        hd_task_navigator_postpone_remove,
					tnc);

  g_free (tnc);
}

static void
zoom_fun (gint * xposp, gint * yposp,
          gdouble * xscalep, gdouble * yscalep)
{
  /* The prison represents what's in @App_window_geometry in app view. */
  *xscalep = 1 / *xscalep;
  *yscalep = 1 / *yscalep;
  *xposp = -*xposp * *xscalep;
  *yposp = -*yposp * *yscalep + HD_COMP_MGR_TOP_MARGIN;
}

static void
hd_task_navigator_real_zoom_in (HdTaskNavigator *navigator,
				HdTnThumbnail *thumbnail)
{
  HdTaskNavigatorPrivate *priv = 
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);

  gint scrxpos, scrypos, xpos, ypos;
  gdouble xscale, yscale, scrx, scry;

  /* @xpos, @ypos := .prison's absolute coordinates. */
  clutter_actor_get_position  (CLUTTER_ACTOR (thumbnail),  &xpos,    &ypos);
  hd_tn_thumbnail_get_jail_scale (thumbnail, &xscale,  &yscale);

  ypos -= hd_scrollable_group_get_viewport_y (priv->grid);
  xpos += PRISON_XPOS;
  ypos += PRISON_YPOS;

  /* If zoom-in is already in progress this will just change its direction
   * such that it will focus on @apthumb's current position. */
  zoom_fun (&xpos, &ypos, &xscale, &yscale);

  clutter_actor_get_scale (CLUTTER_ACTOR (priv->scroller), &scrx, &scry);

  clutter_behaviour_scale_set_bounds (priv->sbehaviour, 
				     scrx, scry,
				     xscale, yscale);

  clutter_behaviour_apply (CLUTTER_BEHAVIOUR (priv->sbehaviour),
			   CLUTTER_ACTOR (priv->scroller));

  clutter_actor_get_position (CLUTTER_ACTOR (priv->scroller),
			      &scrxpos, &scrypos);

  priv->origin.x = scrxpos;
  priv->origin.y = scrypos;
  priv->destination.x = xpos; 
  priv->destination.y = ypos;

  clutter_behaviour_path_clear (priv->mbehaviour);
  
  clutter_behaviour_path_append_knot (priv->mbehaviour, &priv->origin);
  clutter_behaviour_path_append_knot (priv->mbehaviour, &priv->destination);

  clutter_behaviour_apply (CLUTTER_BEHAVIOUR (priv->mbehaviour),
                           CLUTTER_ACTOR (priv->scroller));

  priv->zoomed_actor = CLUTTER_ACTOR (priv->scroller);
   
  clutter_timeline_start (priv->zoom_timeline); 
}

static void
hd_task_navigator_real_zoom_out (HdTaskNavigator *navigator,
				 HdTnThumbnail *thumbnail)
{
  HdTaskNavigatorPrivate *priv = 
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);
  gdouble xscale, yscale;
  gint yarea, xpos, ypos, th_height;

  hd_tn_layout_calculate (priv->layout,
			  priv->thumbnails,
			  CLUTTER_ACTOR (priv->grid));

  priv->zoomed_out = TRUE;

  g_object_get (G_OBJECT (priv->layout),
		"height", &th_height,
		NULL);

  clutter_actor_get_position (CLUTTER_ACTOR (thumbnail), &xpos, &ypos); 

  /*
   * Scroll the @Grid so that @apthumb is closest the middle of the screen.
   * #HdScrollableGroup does not let us scroll the viewport out of the real
   * estate, but in return we need to ask how much we actually managed to
   * scroll.
   */
  yarea = ypos - (hd_comp_mgr_get_current_screen_height () - th_height) / 2;
  hd_scrollable_group_set_viewport_y (priv->grid, yarea);
  yarea = hd_scrollable_group_get_viewport_y (priv->grid);

  /* Make @ypos absolute (relative to the top of the screen). */
  ypos -= yarea;

  /* @xpos, @ypos := absolute position of .prison. */
  xpos += PRISON_XPOS;
  ypos += PRISON_YPOS;
  
  hd_tn_thumbnail_get_jail_scale (thumbnail, &xscale, &yscale);

  /* Reposition and rescale the @Scroller so that .apwin is shown exactly
   * in the same position and size as in the application view. */
  zoom_fun (&xpos, &ypos, &xscale, &yscale);

  clutter_actor_set_scale     (priv->scroller, xscale,  yscale);
  clutter_actor_set_position  (priv->scroller, xpos,    ypos);

  clutter_behaviour_scale_set_bounds (priv->sbehaviour, 
				     xscale, yscale,
				     1, 1);

  clutter_behaviour_apply (CLUTTER_BEHAVIOUR (priv->sbehaviour),
			   CLUTTER_ACTOR (priv->scroller));

  priv->origin.x = xpos;
  priv->origin.y = ypos;
  priv->destination.x = 0; 
  priv->destination.y = 0;

  clutter_behaviour_path_clear (priv->mbehaviour);
  
  clutter_behaviour_path_append_knot (priv->mbehaviour, &priv->origin);
  clutter_behaviour_path_append_knot (priv->mbehaviour, &priv->destination);

  clutter_behaviour_apply (CLUTTER_BEHAVIOUR (priv->mbehaviour),
                           CLUTTER_ACTOR (priv->scroller));

  priv->zoomed_actor = CLUTTER_ACTOR (priv->scroller);
   
  clutter_timeline_start (priv->zoom_timeline); 
}

#if 0
static ClutterActor *
load_icon (const gchar * iname, guint isize)
{
  static const gchar *anyad[2];
  GtkIconInfo *icinf;
  ClutterActor *icon;

  anyad[0] = iname;
  if (!(icinf = gtk_icon_theme_choose_icon (gtk_icon_theme_get_default (),
                                           anyad, isize, 0)))
    return NULL;

  icon = (iname = gtk_icon_info_get_filename (icinf)) != NULL
    ? clutter_texture_new_from_file (iname, NULL) : NULL;
  gtk_icon_info_free (icinf);

  return icon;
}

static ClutterActor *
get_icon (const gchar * iname, guint isize)
{
  static GHashTable *cache;
  ClutterActor *icon;
  gchar *ikey;
  guint w, h;

  if (!iname)
    goto out;

  /* Is it cached?  We can't use %HdClutterCache because that doesn't
   * handle icons and we may need to load the same icon with different
   * sizes. */
  ikey = g_strdup_printf ("%s-%u", iname, isize);
  if (cache && (icon = g_hash_table_lookup (cache, ikey)) != NULL)
    { /* Yeah */
      g_free (ikey);
    }
  else if ((icon = load_icon (iname, isize)) != NULL)
    { /* No, but we could load it. */
      if (!cache)
        cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                       g_free, g_object_unref);
      g_hash_table_insert (cache, ikey, icon);
    }
  else
    { /* Couldn't load it. */
      g_free (ikey);
      g_critical ("%s: failed to load icon", iname);
      goto out;
    }

  /* Icon found.  Set its anchor such that if @icon's real size differs
   * from the requested @isize then @icon would look as if centered on
   * an @isize large area. */
  icon = clutter_clone_texture_new (CLUTTER_TEXTURE (icon));
  clutter_actor_set_name (icon, iname);
  clutter_actor_get_size (icon, &w, &h);
  clutter_actor_move_anchor_point (icon,
                                   (gint)(w-isize)/2, (gint)(h-isize)/2);
  return icon;

out: /* Return something. */
  icon = clutter_rectangle_new ();
  clutter_actor_set_size (icon, isize, isize);
  clutter_actor_hide (icon);
  return icon;
}
#endif

/* Tells whether the switcher is shown. */
gboolean
hd_task_navigator_is_active (HdTaskNavigator *navigator)
{
  return CLUTTER_ACTOR_IS_VISIBLE (navigator);
}

/* Tells whether the navigator is empty. */
gboolean
hd_task_navigator_is_empty (HdTaskNavigator *navigator)
{
  HdTaskNavigatorPrivate *priv = 
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);

  return (priv->n_thumbnails == 0);
}

/* Returns whether at least 2 thumbnails populate the switcher. */
gboolean
hd_task_navigator_is_crowded (HdTaskNavigator *navigator)
{
  HdTaskNavigatorPrivate *priv = 
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);

  return (priv->n_thumbnails > 1);
}

/* Tells whether we have any application thumbnails. */
gboolean
hd_task_navigator_has_apps (HdTaskNavigator *navigator)
{
  HdTaskNavigatorPrivate *priv = 
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);

  return (priv->n_thumbnails > 0);
}

/* Tells whether we have any notification, either in the
 * notification area or shown in the title area of some thumbnail. */
gboolean
hd_task_navigator_has_notifications (HdTaskNavigator *navigator)
{
  return FALSE;
}

/* Returns whether we can and will show @win in the navigator.
 * @win can be %NULL, in which case this function returns %FALSE. */
gboolean
hd_task_navigator_has_window (HdTaskNavigator * navigator, ClutterActor * win)
{
  HdTaskNavigatorPrivate *priv = 
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);
  ClutterActor *window;
  GList *l;

  for (l = priv->thumbnails; l != NULL; l = l->next)
    {
       g_object_get (G_OBJECT (l->data),
		     "window",
		     &window,
		     NULL);

       if (window == win)
         return TRUE;
    }

  return FALSE;
}

/* Find which thumbnail represents the requested app. */
ClutterActor *
hd_task_navigator_find_app_actor (HdTaskNavigator * navigator, const gchar * id)
{
  HdTaskNavigatorPrivate *priv =
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);

  GList *l;

  for (l = priv->thumbnails; l != NULL; l = l->next) 
    {
      const gchar *appid;
      ClutterActor *apwin;

      g_object_get (G_OBJECT (l->data), 
		    "window",
		    &apwin,
		    NULL);

      appid = g_object_get_data (G_OBJECT (apwin), "HD-ApplicationId");

      if (appid && !g_strcmp0 (appid, id))
        return apwin;
    }

  return NULL;
}

/* Scroll @Grid to the top. */
void
hd_task_navigator_scroll_back (HdTaskNavigator * navigator)
{
  //hd_scrollable_group_set_viewport_y (Grid, 0);
}

/* Updates our #HdScrollableGroup's idea about the @Grid's height. */

/*
 * Asks the navigator to forget about @win.  If @fun is not %NULL it is
 * called when @win is actually removed from the screen; this may take
 * some time if there is an effect.  If not, it is called immedeately
 * with @funparam.  If there is zooming in progress the operation is
 * delayed until it's completed.
 */

void
hd_task_navigator_remove_window (HdTaskNavigator *navigator,
                                 ClutterActor *win,
                                 HdTaskNavigatorFunc fun,
                                 gpointer funparam)
{ 
  HdTaskNavigatorPrivate *priv = 
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);

  GList *l;

  if (clutter_timeline_is_playing (priv->zoom_timeline))
    {
      TaskNavigatorClosure *tnc = g_new0 (TaskNavigatorClosure, 1);

      tnc->fun   = fun;
      tnc->actor = win;
      tnc->param = funparam;
      tnc->navigator = navigator;

      g_signal_connect_swapped (priv->zoom_timeline,
				"completed",
				G_CALLBACK (hd_task_navigator_postpone_remove),
			        tnc);

      return;
    }

  for (l = priv->thumbnails; l != NULL; l = l->next)
    {
       ClutterActor *window = NULL;

       g_object_get (G_OBJECT (l->data), 
		     "window",
		     &window,
		     NULL);

       if (window == win)
	   break;
    }

  if (hd_task_navigator_is_active (navigator))
    {
      MBWMCompMgrClutterClient *cmgrcc;

      /* Hold a reference on @win's clutter client. */
      if (!(cmgrcc = g_object_get_data (G_OBJECT (win),"HD-MBWMCompMgrClutterClient")))
        { /* No point in trying to animate, the actor is destroyed. */
          g_critical ("cmgrcc is already unset for %p", win);
          goto damage_control;
        }

      mb_wm_object_ref (MB_WM_OBJECT (cmgrcc));
      mb_wm_comp_mgr_clutter_client_set_flags (cmgrcc,
                                 MBWMCompMgrClutterClientDontUpdate
                               | MBWMCompMgrClutterClientEffectRunning);

      /* At the end of effect free @apthumb and release @cmgrcc. */
      clutter_actor_raise_top (CLUTTER_ACTOR (l->data));

      /* Animate and destroy the window when finished
      turnoff_effect (Fly_effect_timeline, apthumb->thwin);
      add_effect_closure (Fly_effect_timeline,
                          (ClutterEffectCompleteFunc)appthumb_turned_off_1,
                          apthumb->thwin, apthumb);
      add_effect_closure (Fly_effect_timeline,
                          (ClutterEffectCompleteFunc)appthumb_turned_off_2,
                          apthumb->thwin, cmgrcc);*/

      /* This code should be moved to the end of the effect */
      mb_wm_comp_mgr_clutter_client_unset_flags 
        (cmgrcc, MBWMCompMgrClutterClientDontUpdate | MBWMCompMgrClutterClientEffectRunning);

      mb_wm_object_unref (MB_WM_OBJECT (cmgrcc));

      hd_tn_thumbnail_release_window (HD_TN_THUMBNAIL (l->data));
      goto damage_control;
    }
  else
    {
damage_control:
      clutter_actor_destroy (CLUTTER_ACTOR (l->data));
    }   

  priv->thumbnails = g_list_delete_link (priv->thumbnails, l);
  priv->n_thumbnails--;

  /* Re order the stuff. This should be animated */
  hd_tn_layout_calculate (priv->layout,
			  priv->thumbnails,
			  CLUTTER_ACTOR (priv->grid));

  if (fun != NULL)
    fun (CLUTTER_ACTOR (navigator), funparam);

  hd_render_manager_update ();
}

/*
 * Tells the swicher to show @win in a thumbnail when active.  If the
 * navigator is active now it starts managing @win.  When @win is managed
 * by the navigator it is not changed in any means other than reparenting.
 * It is an error to add @win multiple times.
 */
void
hd_task_navigator_add_window (HdTaskNavigator * navigator,
                              ClutterActor * win)
{ 
  HdTaskNavigatorPrivate *priv = 
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);

  ClutterActor *thumbnail = hd_tn_thumbnail_new (win);

  clutter_container_add_actor (CLUTTER_CONTAINER (priv->grid), thumbnail);

  if (hd_task_navigator_is_active (navigator))
    hd_tn_thumbnail_claim_window (HD_TN_THUMBNAIL (thumbnail));
 
  priv->thumbnails = g_list_prepend (priv->thumbnails, thumbnail);

  g_object_set_data (G_OBJECT (thumbnail), "task-nav", navigator);

  g_object_ref (thumbnail);

  priv->n_thumbnails++;
}

/* Remove @dialog from its application's thumbnail
 * and don't show it anymore. */
void
hd_task_navigator_remove_dialog (HdTaskNavigator * navigator,
                                 ClutterActor * dialog)
{ 
  HdTaskNavigatorPrivate *priv = 
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);

  GList *l;
  HdTnThumbnail *t;

  for (l = priv->thumbnails; l != NULL; l = l->next)
    { 
       t = HD_TN_THUMBNAIL (l->data);

       if (hd_tn_thumbnail_has_dialog (t, dialog))
         {
           hd_tn_thumbnail_remove_dialog (t, dialog);
           break;
         }
    }

   if (hd_task_navigator_is_active (navigator))
     hd_render_manager_return_dialog (dialog);
}

/*
 * Register a @dialog to show on the top of an application's thumbnail.
 * @dialog needn't be a dialog, actually, it can be just about anything
 * you want to show along with the application, like menus.
 * @parent should be the actor of the window @dialog is transient for.
 * The @dialog is expected to have been positioned already.  It is an
 * error to add the same @dialog more than once.
 */
void
hd_task_navigator_add_dialog (HdTaskNavigator * navigator,
                              ClutterActor * parent,
                              ClutterActor * dialog)
{
  HdTaskNavigatorPrivate *priv =
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);
  GList *l;
  HdTnThumbnail *thumbnail, *t;
  ClutterActor *apwin;
  gboolean has_dialog = FALSE;

  for (l = priv->thumbnails; l != NULL; l = l->next)
    {
       t = HD_TN_THUMBNAIL (l->data);

       g_object_get (t, "window", &apwin, NULL);

       has_dialog = hd_tn_thumbnail_has_dialog (t, dialog);

       if (apwin == parent && !has_dialog)
         break;
       else
       if (has_dialog)
         return; /* No need to add it again */

       if (hd_tn_thumbnail_has_dialog (t, parent))
         break;
    } 

  if (l == NULL)
    return; /* ? */
 
  thumbnail = t;

    /* Claim @dialog now if we're active. */
  if (hd_task_navigator_is_active (navigator))
    hd_tn_thumbnail_reparent_dialog (thumbnail, dialog);

  /* Add @dialog to @apthumb->dialogs. */
  hd_tn_thumbnail_add_dialog (thumbnail, dialog);
}

/* Prepare for the client of @win being hibernated.
 * Undone when @win is replaced by the woken-up client's new actor. */
void
hd_task_navigator_hibernate_window (HdTaskNavigator * navigator,
                                    ClutterActor * win)
{

}

/* Tells us to show @new_win in place of @old_win, and forget about
 * the latter one entirely.  Replaceing the window doesn't affect
 * its dialogs if any was set earlier. */
void
hd_task_navigator_replace_window (HdTaskNavigator *navigator,
                                  ClutterActor * old_win,
                                  ClutterActor * new_win)
{ 
  HdTaskNavigatorPrivate *priv =
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);
  ClutterActor *window = NULL;
  GList *l;

  for (l = priv->thumbnails; l != NULL; l = l->next)
    {
       g_object_get (G_OBJECT (l->data),
		     "window",
		     &window,
		     NULL);

       if (new_win == window)
	 return; /* Don't do anything if we replace the same */

       if (window == old_win)
         break;
    }

  if (l == NULL)
    {
      g_warning ("Trying to replace a window with no thumbnail???");
      return;
    }

  hd_tn_thumbnail_replace_window (HD_TN_THUMBNAIL (l->data),
				  new_win);
}

/*
 * Sets the @win's %Thumbnail's @nodest.  @nodest == %NULL clears it.
 * It is an error if the associated %Thumbnail cannot be found, but
 * we'll do nothing then.  If it already has a notification it will be
 * swapped out.  If there's a notification destined for @nothread it
 * will be taken by @win's thumbnail.  If other application thumbnail
 * has such a notification it will be taken away and the other thumbnail
 * will be deprived of its @nodest.  This is designed to allow clients
 * to change threads in arbitrary order to transfer notifications from
 * one thumbnail to another.
 *
 * The callee is responsible for managing @nothread.
 */
void
hd_task_navigator_notification_thread_changed (HdTaskNavigator * navigator,
                                               ClutterActor * win,
                                               char * nothread)
{

}

void
hd_task_navigator_add_notification (HdTaskNavigator * navigator,
                                    HdNote * hdnote)
{ 
  hd_render_manager_update();
}

/* Remove a notification from the navigator, either if
 * it's shown on its own or in a thumbnail title area. */
void
hd_task_navigator_remove_notification (HdTaskNavigator * navigator,
                                       HdNote * hdnote)
{ 

}


/* Click-handling {{{
 * {{{
 * Is easy, but we need to consider a few circumstances:
 * a) The "grid" (the thumbnailed area) may be non-rectangular
 *    and it has its own behaviour.  within_grid() help us in this.
 * b) We need to filter out clicks which are not clicks per UX Guidance.
 *    We have three criteria:
 *    1. the clicked widget (must be unambigous)
 *    2. click time (neither too fast or too slow)
 *    3. panning (differentiate between panning and clicking)
 *    The first two are guarded by a low level captured-event handler,
 *    which simply removes unwanted release-events.  The third is
 *    guarded by the captured-event handler of @Grid.
 *
 * Talking high-level our widgets have these behaviours:
 * -- close:      close thumbnails
 * -- thumbnail:  zoom in
 * -- grid:       do nothing
 * -- navigator:  exit
 * }}}
 */
/*
 * @Navigator::captured-event handler.  Its purpose is to filter out
 * non-clicks, ie. button-release-events which together with their
 * button-press-event counterpart didn't meet the click-critiera.
 *
 * In order for a press-release to be considered a click:
 * -- the button must be released above the pressed widget
 * -- the time between press and release must satisfy a static
 *    constraint.
 *
 * If these conditions remain unmet this handler prevents the propagation
 * of the button-release-event.  This does not interfere with scrolling
 * (%TidyFingerScroll grabs the pointer when it needs it) but would break
 * things if, for example, some widget wanted to have a highlighted state.
 */

/* re-calc layout when HdRenderManager's rotation is triggered */
void 
hd_task_navigator_rotate (HdTaskNavigator *tn, 
			  Rotation rotation) 
{

}


HdTaskNavigator *
hd_task_navigator_new (void)
{
  return g_object_new (HD_TYPE_TASK_NAVIGATOR, NULL);
}

static void
zoom_in_completed (HdTnThumbnail *thumbnail, 
                   ClutterTimeline *timeline);

static void
zoom_in_completed (HdTnThumbnail *thumbnail, 
		   ClutterTimeline *timeline)
{
  HdTaskNavigator *navigator =
    g_object_get_data (G_OBJECT (thumbnail),
		       "task-nav");

  if (navigator != NULL)
    {
      ClutterActor *window = NULL;

      g_signal_handlers_disconnect_by_func (timeline,
					    zoom_in_completed,
					    thumbnail);
  
      g_object_get (G_OBJECT (thumbnail),
		    "window", &window,
		    NULL);

      g_signal_emit_by_name (navigator, 
			     "zoom-in-complete",
			     window);
    }					
}

void 
hd_task_navigator_zoom_in (HdTaskNavigator *navigator,
                           ClutterActor *win,
                           HdTaskNavigatorFunc fun,
                           gpointer funparam)
{
  HdTnThumbnail *thumbnail;
  HdTaskNavigatorPrivate *priv =
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);

  g_assert (hd_task_navigator_is_active (navigator));

  if (!(thumbnail = find_thumbnail_by_apwin (priv, win)))
    goto damage_control;

  /* Must have gotten a gtk_window_present() during a zooming, ignore it. */
  if (clutter_timeline_is_playing (priv->zoom_timeline))
    goto damage_control;

  /* This is the actual zooming, but we do other effects as well. */
  hd_render_manager_unzoom_background ();
  hd_task_navigator_real_zoom_in (navigator, thumbnail);

  /*
   * rezoom() if @apthumb changes its position while we're trying to
   * zoom it in.  This may happen if somebody adds a window or adds
   * or removes a notification.  While zooming out it's not important
   * but when zooming in we want @win to be in the right position.
   * During the animation @apthumb is valid.  The only way it could
   * disappear is by removing it, but remove_window() is deferred
   * until zomming is finished.
   
  g_signal_connect (apthumb->thwin, "notify::allocation",
                    G_CALLBACK(rezoom), (Thumbnail *)apthumb);
  g_signal_connect (apthumb->jail, "notify::scale-x",
                    G_CALLBACK(rezoom), (Thumbnail *)apthumb);
  g_signal_connect (apthumb->jail, "notify::scale-y",
                    G_CALLBACK(rezoom), (Thumbnail *)apthumb);*/

  /* Clean up, exit and call @fun when then animation is finished.
  add_effect_closure (Zoom_effect_timeline,
                      (ClutterEffectCompleteFunc)zoom_in_complete,
                      CLUTTER_ACTOR (self), (Thumbnail *)apthumb);
  add_effect_closure (Zoom_effect_timeline,
                      (ClutterEffectCompleteFunc)fun, win, funparam);
*/
  TaskNavigatorClosure *tnc = 
    g_new0 (TaskNavigatorClosure, 1);

  tnc->fun   = fun;
  tnc->actor = win;
  tnc->param = funparam;

  g_signal_connect_swapped (priv->zoom_timeline,
		    	    "completed",
		    	    G_CALLBACK (closure_zoom_in),
                    	    tnc);
/*
  g_signal_connect_swapped (thumbnail,
		    	    "notify::allocation",
		    	    G_CALLBACK (hd_task_navigator_real_zoom_in),
			    navigator);
*/
  g_signal_connect_swapped (priv->zoom_timeline,
 			    "completed",
			    G_CALLBACK (zoom_in_completed),
			    thumbnail);
  return;

damage_control:
  if (fun != NULL)
    fun (win, funparam);
}

void 
hd_task_navigator_zoom_out  (HdTaskNavigator *navigator,
                             ClutterActor *win,
                             HdTaskNavigatorFunc fun,
                             gpointer funparam)
{
 HdTnThumbnail *thumbnail;
  HdTaskNavigatorPrivate *priv =
    HD_TASK_NAVIGATOR_GET_PRIVATE (navigator);

  /* Our "show" callback will grab the butts of @win. */
  clutter_actor_show (CLUTTER_ACTOR (navigator));

  if (!(thumbnail = find_thumbnail_by_apwin (priv, win)))
    goto damage_control;

  hd_tn_layout_last_active_window (priv->layout, win);

  hd_task_navigator_real_zoom_out (navigator, thumbnail);

damage_control:
  if (fun != NULL)
    fun (win, funparam);
}

/* THUMBNAIL */

typedef enum
{
  THUMBNAIL_APP,
  THUMBNAIL_NOTIFICATION
} ThumbnailType;

enum
{
  PROP_0,
  PROP_WINDOW
};

struct _HdTnThumbnailPrivate
{
  ThumbnailType type;

  /*
   * -- @thwin:       The @Grid's thumbnail window and event responder.
   *                  Can be faded in/out if the thumbnail is a notification.
   * -- @plate:       Groups the @title and the @frame graphics; used to fade
   *                  them all at once.  It sits on the top and can be thought
   *                  of as a boilerplate.
   * -- @title:       What to put in the thumbnail's title area.
   *                  Centered vertically within TITLE_HEIGHT.
   * -- @close:       An invisible actor reacting to user taps to close
   *                  the thumbnail.  Slightly reaches out of the thumbnail
   *                  bounds.  Also contains the icons.
   * -- @close_app_icon, @close_notif_icon: these.  They're included in
   *                  the generic structure in order to keep their position
   *                  together.  When there are neither adopt_notification()
   *                  nor orphane_notification() transitions going on at most
   *                  one of them is shown at a time according to one of the
   *                  three states of the thumbail.  Otherwise they are faded
   *                  in and out.
   *
   *                  When the thumbnail is a %NOTIFICATION both icons are
   *                  normally opaque.  Otherwise if thumb_has_notification()
   *                  @close_app_icon is normally transparent and hidden,
   *                  and the other is opaque.  Otherwise the opposit holds.
   */
  ClutterActor        *thwin, *plate;
  ClutterActor        *title, *close;
  ClutterActor        *close_app_icon, *close_notif_icon;

  union
  {
    /* Application-thumbnail-specific fields */
    struct
    {
      /*
       * -- @apwin:       The pristine #MBWMCompMgrClutterClient::actor,
       *                  not to be touched.
       * -- @dialogs:     The application's dialogs, popup menus and whatsnot
       *                  if it has or had any earlier, otherwise %NULL.
       *                  They are shown along with .apwin.  Hidden if
       *                  we have a .video.
       * -- @cemetery:    This is the resting place for .apwin:s that have
       *                  been hd_task_navigator_replace_window()ed.
       *                  When we're active they are hidden.  Otherwise
       *                  we don't care.  We don't keep a string reference
       *                  on them, but a weak reference callback removes
       *                  them from this array when the actor is destroyed.
       *                  The primary use of the cemetery is to grab all the
       *                  subviews of an application when we are activated,
       *                  so hd_render_set_visibilities() doesn't need to
       *                  worry.
       * -- @windows:     Just 0-dimension container for @apwin and @dialogs,
       *                  its sole purpose is to make it easier to hide them
       *                  when the %Thumbnail has a @video.  Also clips its
       *                  contents to @App_window_geometry, making sure that
       *                  really nothing is shown outside the thumbnail.
       * -- @titlebar:    An actor that looks like the original title bar.
       *                  Faded in/out when zooming in/out, but normally
       *                  transparent or not visible at all.
       * -- @jail:      Scales, positions and anchors all the above actors
       *                  and more.  It's surrounded closely by @frame and it
       *                  represents the application area of application view.
       *                  That is, whatever is shown in @jail is what you
       *                  see in that area.  Hidden if the thumbnails has a
       *                  notification.
       */
      ClutterActor        *apwin, *windows, *titlebar, *jail;
      GPtrArray           *cemetery;
      GList 		  *dialogs;

      /* Frame decoration.  The graphics are updated automatically whenever
       * the theme changes.  Pieces in the middle are scaled horizontally
       * xor vertically. */
      struct
      {
        /* The container of all @pieces.  If the thumbnail is an APPLICATION
         * but it has a notification it's normally transparent and hidden,
         * otherwise it's normally opaque. */
        ClutterActor *all;
        union
        {
          struct
          { /* north: west, middle, east; middle; south */
            ClutterActor *nw, *nm, *ne;
            ClutterActor *mw,      *me;
            ClutterActor *sw, *sm, *se;
          };

          ClutterActor *pieces[8];
        };
      } frame;

      /*
       * -- @win_changed_cb_id: Tracks the window's name to keep the title
       *                        of the thumbnail up to date.  It's only
       *                        valid as long as @win is.
       * -- @win:               The window of the client @apwin belongs to.
       *                        Kept around in order to be able to disconnect
       *                        @win_changed_cb_id.  Replaced when @apwin is
       *                        replaced.  It's not ref'd specifically.
       *                        %NULL if the client is in hibernation.
       * -- @saved_title:       What the application window's title was when
       *                        it left for hibernation.  It's only use is
       *                        to know what to reset the thumb title to if
       *                        the client's notification was removed when
       *                        it was still in hibernation.  Cleared when
       *                        the window actor is replaced, presumably
       *                        because it's woken up.
       * -- @title_had_markup:  Mirrors @win::name_has_markup similarly to
       *                        @saved_title.
       */
      MBWMClientWindow    *win;
      unsigned long        win_changed_cb_id;
      gchar               *saved_title;
      gboolean             title_had_markup;

      /*
       * -- @nodest:      What notifications this thumbnails is destination for.
       *                  Taken from the _HILDON_NOTIFICATION_THREAD property
       *                  of the thumbnail's client or its WM_CLASS hint.
       *                  Once checked when the window is added then whenever
       *                  hd_task_navigator_notification_thread() is called.
       *                  TODO How about replace_window()?
       *                  Normally two or more application thumbnails should
       *                  not have the same @nodest.  It is undefined how to
       *                  handale this case but we'll try our best.  Used in
       *                  matching the appropriate TNote for this application.
       */
      gchar               *nodest;

      /*
       * -- @video_fname: Where to look for the last-frame video screenshot
       *                  for this application.  Deduced from some property
       *                  in the application's .desktop file.
       * -- @video_mtime: The last modification time of the image loaded as
       *                  .video.  Used to decide if it should be refreshed.
       * -- @video:       The downsampled texture of the image loaded from
       *                  .video_fname or %NULL.
       */
      ClutterActor        *video;
      const gchar         *video_fname;
      time_t               video_mtime;
    };

    /* Currently we don't have notification-specific fields. */
  };

   /*
   * -- @tnote:       Notifications always have a %TNote in one of the
   *                  @Thumbnails: either in a %Thumbnail of their own,
   *                  or in the application's they belong to.
   */
  //TNote               *tnote;

  gboolean pressed;

  gint start_drag_x;
  gint start_drag_y;
};

/* Returns the window whose client's clutter client's texture is @win.
 * If @hcmgrcp is not %NULL also returns the clutter client. */
static MBWMClientWindow *
actor_to_client_window (ClutterActor * win, const HdCompMgrClient **hcmgrcp)
{
  const MBWMCompMgrClient *cmgrc;

  cmgrc = g_object_get_data (G_OBJECT (win),
                             "HD-MBWMCompMgrClutterClient");

  if (!cmgrc || !cmgrc->wm_client || !cmgrc->wm_client->window)
    return NULL;

  if (hcmgrcp)
    *hcmgrcp = HD_COMP_MGR_CLIENT (cmgrc);
  return cmgrc->wm_client->window;
}

static void
hd_tn_thumbnail_reset_title (HdTnThumbnail *thumbnail);

static Bool
win_title_changed (MBWMClientWindow * win, 
		   gint unused1,
		   HdTnThumbnail *thumb)
{
  hd_tn_thumbnail_reset_title (thumb);
  return True;
}

static void 
hd_tn_thumbnail_set_property (GObject *object,
		              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  HdTnThumbnailPrivate *priv =
    HD_TN_THUMBNAIL_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_WINDOW:
      priv->apwin = g_object_ref (g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void 
hd_tn_thumbnail_get_property (GObject    *object,
			      guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  HdTnThumbnailPrivate *priv = 
    HD_TN_THUMBNAIL_GET_PRIVATE (object);

  switch (prop_id)
    {
      case PROP_WINDOW:
        g_value_set_object (value, priv->apwin);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
         break;
    }

}

static void 
clip_on_resize (ClutterActor *actor) 
{ 
  ClutterUnit width, height; 
 
  clutter_actor_get_sizeu (actor, &width, &height); 
  clutter_actor_set_clipu (actor, 0, 0, width, height); 
}

static void
create_apthumb_frame (HdTnThumbnail *thumbnail)
{
  HdTnThumbnailPrivate *priv = 
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);

  static struct
  {
    const gchar *fname;
    ClutterGravity gravity;
  } frames[] =
  {
    { "TaskSwitcherThumbnailTitleLeft.png",    CLUTTER_GRAVITY_NORTH_WEST },
    { "TaskSwitcherThumbnailTitleCenter.png",  CLUTTER_GRAVITY_NORTH_WEST },
    { "TaskSwitcherThumbnailTitleRight.png",   CLUTTER_GRAVITY_NORTH_EAST },
    { "TaskSwitcherThumbnailBorderLeft.png",   CLUTTER_GRAVITY_NORTH_WEST },
    { "TaskSwitcherThumbnailBorderRight.png",  CLUTTER_GRAVITY_NORTH_EAST },
    { "TaskSwitcherThumbnailBottomLeft.png",   CLUTTER_GRAVITY_SOUTH_WEST },
    { "TaskSwitcherThumbnailBottomCenter.png", CLUTTER_GRAVITY_SOUTH_WEST },
    { "TaskSwitcherThumbnailBottomRight.png",  CLUTTER_GRAVITY_SOUTH_EAST },
  };

  guint i;

  priv->frame.all = clutter_group_new ();
  clutter_actor_set_name (priv->frame.all, "apthumb frame");

  for (i = 0; i < G_N_ELEMENTS (frames); i++)
    {
      priv->frame.pieces[i] = hd_clutter_cache_get_texture (frames[i].fname, TRUE);
      clutter_actor_set_anchor_point_from_gravity (priv->frame.pieces[i],frames[i].gravity);

      clutter_container_add_actor (CLUTTER_CONTAINER (priv->frame.all),
                                   priv->frame.pieces[i]);
    }
}

static gboolean
thumbnail_pressed (HdTnThumbnail *thumbnail, 
		   ClutterButtonEvent *event)
{
  HdTnThumbnailPrivate *priv =
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);

  priv->pressed = TRUE;
  priv->start_drag_x = event->x;
  priv->start_drag_y = event->y;

  return TRUE;
}

static gboolean
thumbnail_clicked (HdTnThumbnail *thumbnail, 
		   ClutterButtonEvent *event)
{
  if (hd_render_manager_get_state () != HDRM_STATE_TASK_NAV)
    /* Bloke clicked a home applet while exiting the switcher,
     * which got through the input viewport and would mess up
     * things in hd_switcher_zoom_in_complete(). */
    return TRUE;

  HdTaskNavigator *navigator;
  HdTnThumbnailPrivate *priv =
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);

  if (priv->pressed)
    {
	gboolean event_processed = FALSE;
        
	if ((priv->start_drag_x - event->x) > 100) /* FIXME: Define better these values */
          {
            g_signal_emit_by_name (thumbnail, "drag-left");
            event_processed = TRUE;
          }

        if ((priv->start_drag_x - event->x) < -100) /* FIXME: Define better these values */
          {
	    g_signal_emit_by_name (thumbnail, "drag-right");
            event_processed = TRUE;
          }


        if ((priv->start_drag_y - event->y) > 100) /* FIXME: Define better these values */
          {
	    g_signal_emit_by_name (thumbnail, "drag-down");
            event_processed = TRUE;
          }


        if ((priv->start_drag_y - event->y) < -100) /* FIXME: Define better these values */
          {  
	    g_signal_emit_by_name (thumbnail, "drag-up");
            event_processed = TRUE;
          }

	if (event_processed)
	  return TRUE;
    }

  priv->pressed = FALSE;

  navigator = 
    HD_TASK_NAVIGATOR (g_object_get_data (G_OBJECT (thumbnail), "task-nav"));

    /* Behave like a notification if we have one. */
  if (FALSE)//thumb_has_notification (apthumb))
    g_signal_emit_by_name (navigator, "notification-clicked",NULL);//priv->tnote->hdnote);
  else
    g_signal_emit_by_name (navigator, "thumbnail-clicked", priv->apwin);

  return TRUE;
}


static gboolean
appthumb_close_clicked (HdTnThumbnail *thumbnail)
{
  if (hd_render_manager_get_state () != HDRM_STATE_TASK_NAV)
    /* Be consistent with appthumb_clicked(). */
    return TRUE;
#if 0
  if (animation_in_progress (Fly_effect_timeline)
      || animation_in_progress (Zoom_effect_timeline))
    /* Closing an application while it's zooming would crash us. */
    /* Maybe not anymore but let's play safe. */
    return TRUE;
#endif

  HdTaskNavigator *navigator;
  HdTnThumbnailPrivate *priv =
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);

  navigator = 
    HD_TASK_NAVIGATOR (g_object_get_data (G_OBJECT (thumbnail), "task-nav"));

  if (FALSE)//thumb_has_notification (apthumb))
    g_signal_emit_by_name (navigator, "notification-closed",NULL);
                           //apthumb->tnote->hdnote);
  else
    /* Report a regular click on the thumbnail (and make %HdSwitcher zoom in)
     * if the application has open dialogs. */
    g_signal_emit_by_name (navigator,
                           //apthumb_has_dialogs (apthumb)
                           //  ? "thumbnail-clicked" : "thumbnail-closed",
			   "thumbnail-closed",
                           priv->apwin);
  return TRUE;
}

static ClutterActor *
set_label_text_and_color (ClutterActor * label, const char * newtext,
                          const ClutterColor * color)
{
  const gchar *text;

  /* Only change the text if it's different from the current one.
   * Setting a #ClutterLabel's text causes relayout. */
  if (color)
    clutter_label_set_color (CLUTTER_LABEL (label), color);

  if (newtext && (!(text = clutter_label_get_text (CLUTTER_LABEL (label)))
                  || strcmp (newtext, text)))
    clutter_label_set_text (CLUTTER_LABEL (label), newtext);
  return label;
}

static void
hd_tn_thumbnail_reset_title (HdTnThumbnail *thumbnail)
{
  HdTnThumbnailPrivate *priv =
   HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);

  gboolean use_markup;
  const gchar *new_title;

  /* What to reset the title to? */
  if (FALSE)//thumb_has_notification (thumb))
    { /* To the notification summary. */
      //new_title = hd_note_get_summary (thumb->tnote->hdnote);
      use_markup = FALSE;
    }
  else if (priv->win)
    { /* Normal case. */
      new_title = priv->win->name;
      use_markup = priv->win->name_has_markup;
    }
  else
    { /* Client must be having its sweet dreams in hibernation. */
      new_title = priv->saved_title;
      use_markup = priv->title_had_markup;
    }

  g_assert (priv->title != NULL);
  ClutterColor DefaultTextColor; 

  hd_theme_config_get_color (HD_TXT_COLOR, &DefaultTextColor);

  set_label_text_and_color (priv->title, new_title,
                            /*thumb_has_notification (thumb)
                              ? &NotificationTextColor : */&DefaultTextColor);

  clutter_label_set_use_markup (CLUTTER_LABEL (priv->title), use_markup);
}
   
static GObject *
hd_tn_thumbnail_constructor (GType                  gtype,
                             guint                  n_properties,
                             GObjectConstructParam *properties)
{
  GObject *obj;

  obj =
    G_OBJECT_CLASS(hd_tn_thumbnail_parent_class)->constructor (gtype,
                                                               n_properties,
                                                               properties);

  HdTnThumbnailPrivate *priv =
    HD_TN_THUMBNAIL_GET_PRIVATE (obj);

  const HdLauncherApp *app;
  const HdCompMgrClient *hmgrc;

  priv->type = THUMBNAIL_APP;

  /* We're just in a MapNotify, it shouldn't happen.
   * mb_wm_object_signal_connect() will take reference
   * of apthumb->win. */
  priv->win = actor_to_client_window (priv->apwin, &hmgrc);

  g_assert (priv->win != NULL);

  priv->win_changed_cb_id = 
    mb_wm_object_signal_connect (MB_WM_OBJECT (priv->win),
				 MBWM_WINDOW_PROP_NAME,
                     		 (MBWMObjectCallbackFunc)win_title_changed, 
				 obj);

  /* .nodest: try the property first then fall back to the WM_CLASS hint.
   * TODO This is temporary, just not to break the little functionality
   *      we already have. */
  mb_wm_util_async_trap_x_errors (priv->win->wm->xdpy);

  priv->nodest = 
    hd_util_get_x_window_string_property (priv->win->wm,
					  priv->win->xwindow,
                                	  HD_ATOM_NOTIFICATION_THREAD);

  if (!priv->nodest)
    {
      XClassHint xwinhint;

      if (XGetClassHint (priv->win->wm->xdpy, priv->win->xwindow, &xwinhint))
        {
          priv->nodest = xwinhint.res_class;
          XFree (xwinhint.res_name);
        }
      else
        g_warning ("XGetClassHint(%lx): failed", priv->win->xwindow);
    }

  mb_wm_util_async_untrap_x_errors ();

  app = HD_LAUNCHER_APP (hd_comp_mgr_client_get_launcher (HD_COMP_MGR_CLIENT (hmgrc)));
  /* .video_fname */
  if (app != NULL)
    priv->video_fname = hd_launcher_app_get_switcher_icon (HD_LAUNCHER_APP (app));

  /* Now the actors: .apwin (from set_property at construction), .titlebar, .windows. */
  priv->titlebar = 
    hd_title_bar_create_fake (hd_comp_mgr_get_current_screen_width ());

  priv->windows = clutter_group_new ();

  clutter_actor_set_name (priv->windows, "windows");
  clutter_actor_set_clip (priv->windows,
                          0, HD_COMP_MGR_TOP_MARGIN,
                          hd_comp_mgr_get_current_screen_width (), 
			  hd_comp_mgr_get_current_screen_height () - HD_COMP_MGR_TOP_MARGIN);

  /* .jail: anchor it so that we can ignore the UI framework area
   * of its contents.  Do so even if @apwin is really fullscreen,
   * ie. ignore the parts that would be in place of the title bar. */
  priv->jail = clutter_group_new ();
  clutter_actor_set_name (CLUTTER_ACTOR (priv->jail), "jail");
  clutter_actor_set_anchor_point (CLUTTER_ACTOR (priv->jail), 0, HD_COMP_MGR_TOP_MARGIN);

  clutter_actor_set_position (CLUTTER_ACTOR (priv->jail), PRISON_XPOS, PRISON_YPOS);
  clutter_container_add (CLUTTER_CONTAINER (priv->jail),
                         priv->titlebar, 
			 priv->windows, NULL);

  /* Now we really create the thumbnail */
  /* Master pieces */
  const gchar *LargeSystemFont, *SystemFont, *SmallSystemFont;
  

  LargeSystemFont = hd_theme_config_get_font (HD_LARGE_SYSTEM_FONT);
  SystemFont      = hd_theme_config_get_font (HD_SYSTEM_FONT);
  SmallSystemFont = hd_theme_config_get_font (HD_SMALL_SYSTEM_FONT);

  ClutterColor NotificationTextColor,
	       NotificationSecondaryTextColor;

  hd_theme_config_get_color (HD_NOTIFICATION_TXT_COLOR, &NotificationTextColor);
  hd_theme_config_get_color (HD_NOTIFICATION_2TXT_COLOR, &NotificationSecondaryTextColor);


    /* .title */
  priv->title = 
#ifndef CLUTTER_08
    clutter_label_new ();
  clutter_label_set_font_name  (CLUTTER_LABEL (priv->title), SmallSystemFont);
  clutter_label_set_use_markup (CLUTTER_LABEL (priv->title), TRUE);
#else
    clutter_text_new ();
  clutter_text_set_font_name  (CLUTTER_TEXT (priv->title), SmallSystemFont);
  clutter_text_set_use_markup (CLUTTER_TEXT (priv->title), TRUE);
#endif

  clutter_actor_set_anchor_point_from_gravity (priv->title, CLUTTER_GRAVITY_WEST);

  clutter_actor_set_position (priv->title,
                              TITLE_LEFT_MARGIN, TITLE_HEIGHT / 2);

  g_signal_connect (priv->title, 
		    "notify::allocation",
                    G_CALLBACK (clip_on_resize),
		    NULL);

  /* .close, anchored at the top-right corner of the close graphics. */
  priv->close = clutter_group_new ();
  clutter_actor_set_name (priv->close, "close area");
  clutter_actor_set_size (priv->close, CLOSE_AREA_SIZE, CLOSE_AREA_SIZE);
  clutter_actor_set_anchor_point (priv->close,
                                  CLOSE_AREA_SIZE/2 + CLOSE_ICON_SIZE/2,
                                  CLOSE_AREA_SIZE/2 - CLOSE_ICON_SIZE/2);

  clutter_actor_set_reactive (priv->close, TRUE);

 /* .close_app_icon, .close_notif_icon: anchor them at top-right. */
  priv->close_app_icon =
    hd_clutter_cache_get_texture ("TaskSwitcherThumbnailTitleCloseIcon.png", TRUE);

  clutter_actor_set_anchor_point (priv->close_app_icon,
                                  CLOSE_ICON_SIZE/2 - CLOSE_AREA_SIZE/2,
                                  CLOSE_ICON_SIZE/2 - CLOSE_AREA_SIZE/2);

  priv->close_notif_icon = 
    hd_clutter_cache_get_texture ("TaskSwitcherNotificationThumbnailCloseIcon.png", TRUE);

  clutter_actor_set_anchor_point (priv->close_notif_icon,
                                  CLOSE_ICON_SIZE/2 - CLOSE_AREA_SIZE/2,
                                  CLOSE_ICON_SIZE/2 - CLOSE_AREA_SIZE/2);

  clutter_container_add (CLUTTER_CONTAINER (priv->close),
                         priv->close_app_icon, 
			 priv->close_notif_icon,
                         NULL);

  if (FALSE)//thumb_has_notification (thumb))
    clutter_actor_hide (priv->close_app_icon);
  else
    clutter_actor_hide (priv->close_notif_icon);

  /* .plate */
  priv->plate = clutter_group_new ();
  clutter_actor_set_name (priv->plate, "plate");
  clutter_container_add (CLUTTER_CONTAINER (priv->plate),
                         priv->title, 
			 priv->close,
			 NULL);

  ClutterActor *thumb = CLUTTER_ACTOR (obj);
  clutter_actor_set_name (thumb, "thumbnail");
  clutter_actor_set_reactive (thumb, TRUE);
  clutter_container_add (CLUTTER_CONTAINER (thumb),
                         priv->jail, 
			 priv->plate,
			 NULL);

  g_signal_connect (obj,
		    "button-press-event",
		    G_CALLBACK (thumbnail_pressed),
		    NULL);

  g_signal_connect (obj, 
		    "button-release-event",
                    G_CALLBACK (thumbnail_clicked),
		    NULL);

  g_signal_connect_swapped (priv->close, 
			    "button-release-event",
                            G_CALLBACK (appthumb_close_clicked),
                            obj);

  /* create frame */ 
  create_apthumb_frame (HD_TN_THUMBNAIL (obj));
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->plate),
                               priv->frame.all);
  clutter_actor_lower_bottom (priv->frame.all);

  hd_tn_thumbnail_reset_title (HD_TN_THUMBNAIL (obj));

  return obj;
}

static void
hd_tn_thumbnail_finalize (GObject *object)
{
  HdTnThumbnailPrivate *priv = 
    HD_TN_THUMBNAIL_GET_PRIVATE (object);

  g_object_unref (priv->apwin);

  G_OBJECT_CLASS (hd_tn_thumbnail_parent_class)->finalize (object); 
}
 
static void
hd_tn_thumbnail_init (HdTnThumbnail *thumbnail)
{
  HdTnThumbnailPrivate *priv =
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);

  priv->dialogs = NULL;
  priv->jail = NULL;
  priv->pressed = FALSE;
}

static void
hd_tn_thumbnail_class_init (HdTnThumbnailClass * klass)
{
  GObjectClass	    *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class  = CLUTTER_ACTOR_CLASS (klass);

  GParamSpec *pspec;

  object_class->constructor  = hd_tn_thumbnail_constructor;
  object_class->set_property = hd_tn_thumbnail_set_property;
  object_class->get_property = hd_tn_thumbnail_get_property;
  object_class->finalize     = hd_tn_thumbnail_finalize;

  actor_class = NULL;

  g_type_class_add_private (klass, sizeof (HdTnThumbnailPrivate));

  pspec = g_param_spec_object ("window",
			       "window",
			       "Application window",
			       CLUTTER_TYPE_ACTOR,
			       (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_WINDOW, pspec);

  g_signal_new ("drag-up", G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                0, NULL, NULL, g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);

  g_signal_new ("drag-down", G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                0, NULL, NULL, g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);

  g_signal_new ("drag-left", G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                0, NULL, NULL, g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);

  g_signal_new ("drag-right", G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                0, NULL, NULL, g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);
}

/* Returns whether the application represented by @apthumb has
 * a video screenshot and it should be loaded or reloaded.
 * If so it refreshes @apthumb->video_mtime. */
static gboolean
hd_tn_thumbnail_need_load_video (HdTnThumbnail *thumbnail)
{
  HdTnThumbnailPrivate *priv =
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);

  /* Already has a video loaded? */
  if (priv->video)
    {
      struct stat sbuf;
      gboolean clear_video, load_video;

      /* Refresh or unload it? */
      load_video = clear_video = FALSE;

      g_assert (priv->video_fname);

      if (stat (priv->video_fname, &sbuf) < 0)
        {
          if (errno != ENOENT)
            g_warning ("%s: %m", priv->video_fname);
          clear_video = TRUE;
        }
      else if (sbuf.st_mtime > priv->video_mtime)
        {
          clear_video = load_video = TRUE;
          priv->video_mtime = sbuf.st_mtime;
        }

      if (clear_video)
        {
          clutter_container_remove_actor (CLUTTER_CONTAINER (priv->jail),
                                          priv->video);
          priv->video = NULL;
        }

      return load_video;
    }
  else 
  if (priv->video_fname)
    {
      struct stat sbuf;

      /* Do we need to load it? */
      if (stat (priv->video_fname, &sbuf) == 0)
        {
          priv->video_mtime = sbuf.st_mtime;
          return TRUE;
        }
      else 
      if (errno != ENOENT)
        g_warning ("%s: %m", priv->video_fname);
    }

  return FALSE; 
}

ClutterActor *
hd_tn_thumbnail_new (ClutterActor *window)
{
  return g_object_new (HD_TYPE_TN_THUMBNAIL,
		       "window",
		       window,
		       NULL);
}

/* Destroying @pixbuf, turns it into a #ClutterTexture.
 * Returns %NULL on failure. */
static ClutterActor *
pixbuf2texture (GdkPixbuf *pixbuf)
{
  GError *err;
  gboolean isok;
  ClutterActor *texture;

#ifndef G_DISABLE_CHECKS
  if (gdk_pixbuf_get_colorspace (pixbuf) != GDK_COLORSPACE_RGB
      || gdk_pixbuf_get_bits_per_sample (pixbuf) != 8
      || gdk_pixbuf_get_n_channels (pixbuf) !=
         (gdk_pixbuf_get_has_alpha (pixbuf) ? 4 : 3))
    {
      g_critical ("image not in expected rgb/8bps format");
      goto damage_control;
    }
#endif

  err = NULL;
  texture = clutter_texture_new ();
  isok = clutter_texture_set_from_rgb_data (CLUTTER_TEXTURE (texture),
                                            gdk_pixbuf_get_pixels (pixbuf),
                                            gdk_pixbuf_get_has_alpha (pixbuf),
                                            gdk_pixbuf_get_width (pixbuf),
                                            gdk_pixbuf_get_height (pixbuf),
                                            gdk_pixbuf_get_rowstride (pixbuf),
                                            gdk_pixbuf_get_n_channels (pixbuf),
                                            0, &err);
  if (!isok)
    {
      g_warning ("clutter_texture_set_from_rgb_data: %s", err->message);
      g_object_unref (texture);
damage_control: __attribute__((unused))
      texture = NULL;
    }

  g_object_unref (pixbuf);
  return texture;
}

/* Loads @fname, resizing and cropping it as necessary to fit
 * in a @aw x @ah rectangle.  Returns %NULL on error. */
static ClutterActor *
load_image (char const * fname, guint aw, guint ah)
{
  GError *err;
  GdkPixbuf *pixbuf;
  gint dx, dy;
  gdouble dsx, dsy, scale;
  guint vw, vh, sw, sh, dw, dh;
  ClutterActor *final;
  ClutterActor *texture;

  /* On error the caller sure has better recovery plan than an
   * empty rectangle.  (ie. showing the real application window). */
  err = NULL;
  if (!(pixbuf = gdk_pixbuf_new_from_file (fname, &err)))
    {
      g_warning ("%s: %s", fname, err->message);
      return NULL;
    }

  /* @sw, @sh := size in pixels of the untransformed image. */
  sw = gdk_pixbuf_get_width (pixbuf);
  sh = gdk_pixbuf_get_height (pixbuf);

  /*
   * @vw and @wh tell how many pixels should the image have at most
   * in horizontal and vertical dimensions.  If the image would have
   * more we will scale it down before we create its #ClutterTexture.
   * This is to reduce texture memory consumption.
   */
  vw = aw / 2;
  vh = ah / 2;

  /*
   * Detemine if we need to and how much to scale @pixbuf.  If the image
   * is larger than requested (@aw and @ah) then scale to the requested
   * amount but do not scale more than @vw/@aw ie. what the memory saving
   * would demand.  Instead crop it later.  If one direction needs more
   * scaling than the other choose that factor.
   */
  dsx = dsy = 1;
  if (sw > vw)
    dsx = (gdouble)vw / MIN (aw, sw);
  if (sh > vh)
    dsy = (gdouble)vh / MIN (ah, sh);
  scale = MIN (dsx, dsy);

  /* If the image is too large (even if we scale it) crop the center.
   * These are the final parameters to gdk_pixbuf_scale().
   * @dw, @dh := the final pixel width and height. */
  dx = dy = 0;
  dw = sw * scale;
  dh = sh * scale;

  if (dw > vw)
    {
      dx = -(gint)(dw - vw) / 2;
      dw = vw;
    }

  if (dh > vh)
    {
      dy = -(gint)(dh - vh) / 2;
      dh = vh;
    }

  /* Crop and scale @pixbuf if we need to. */
  if (scale < 1)
    {
      GdkPixbuf *tmp;

      /* Make sure to allocate the new pixbuf with the same
       * properties as the old one has, gdk_pixbuf_scale()
       * may not like it otherwise. */
      tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                            gdk_pixbuf_get_has_alpha (pixbuf),
                            gdk_pixbuf_get_bits_per_sample (pixbuf),
                            dw, dh);

      gdk_pixbuf_scale (pixbuf, tmp, 0, 0,
                        dw, dh, dx, dy, scale, scale,
                        GDK_INTERP_BILINEAR);
      g_object_unref (pixbuf);
      pixbuf = tmp;
    }

  if (!(texture = pixbuf2texture (pixbuf)))
    return NULL;

  /* If @pixbuf is smaller than desired place it centered
   * on a @vw x @vh size black background. */
  if (dw < vw || dh < vh)
    {
      static const ClutterColor bgcolor = { 0x00, 0x00, 0x00, 0xFF };
      ClutterActor *bg;

      bg = clutter_rectangle_new_with_color (&bgcolor);
      clutter_actor_set_size (bg, vw, vh);

      if (dw < vw)
        clutter_actor_set_x (texture, (vw - dw) / 2);
      if (dh < vh)
        clutter_actor_set_y (texture, (vh - dh) / 2);

      final = clutter_group_new ();
      clutter_container_add (CLUTTER_CONTAINER (final),
                             bg, texture, NULL);
    }
  else
    final = texture;

  /* @final is @vw x @vh large, make it appear as if it @aw x @ah. */
  clutter_actor_set_scale (final, (gdouble)aw/vw, (gdouble)ah/vh);

  return final;
}

void 
hd_tn_thumbnail_claim_window (HdTnThumbnail *thumbnail)
{
  HdTnThumbnailPrivate *priv =
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);
  
  /*
   * Take @apthumb->apwin into our care even if there is a video screenshot.
   * If we don't @apthumb->apwin will be managed by its current parent and
   * we cannot force hiding it which would result in a full-size apwin
   * appearing in the background if it's added in switcher mode.
   * TODO This may not be true anymore.
   */
  clutter_actor_reparent (priv->apwin, priv->windows);

  if (priv->cemetery)
    {
      guint i;

      for (i = 0; i < priv->cemetery->len; i++)
        {
          clutter_actor_reparent (priv->cemetery->pdata[i],
                                  priv->windows);

          clutter_actor_hide (priv->cemetery->pdata[i]);
        }
    }

  if (priv->dialogs)
    g_list_foreach (priv->dialogs,
                    (GFunc)clutter_actor_reparent,
                    priv->windows);

  /* Load the video screenshot and place its actor in the hierarchy. */
  if (hd_tn_thumbnail_need_load_video (thumbnail))
    {
      /* Make it appear as if .video were .apwin,
       * having the same geometry. */
      g_assert (!priv->video);
      priv->video = load_image (priv->video_fname,
                                hd_comp_mgr_get_current_screen_width (),
                                hd_comp_mgr_get_current_screen_height ());
      if (priv->video)
        {
          clutter_actor_set_name (priv->video, "video");
          clutter_actor_set_position (priv->video,
				      0, 0);

          clutter_container_add_actor (CLUTTER_CONTAINER (thumbnail),
                                       priv->video);
        }
    }

  if (!priv->video)
    /* Needn't bother with show_all() the contents of .windows,
     * they are shown anyway because of reparent(). */
    clutter_actor_show (priv->windows);
  else
    /* Only show @apthumb->video. */
    clutter_actor_hide (priv->windows);

  /* Restore the opacity/visibility of the actors that have been faded out
   * while zooming, so we won't have trouble if we happen to to need to enter
   * the navigator directly. */
  //clutter_actor_show (priv->titlebar);
  clutter_actor_set_opacity (priv->plate, 255);
  if (FALSE)//thumb_has_notification (apthumb))
    {
      clutter_actor_hide (CLUTTER_ACTOR (priv->jail));
      //clutter_actor_set_opacity (apthumb->tnote->notwin, 255);
    }
}

void
hd_tn_thumbnail_release_window (HdTnThumbnail *thumbnail)
{
  HdTnThumbnailPrivate *priv =
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);

  ClutterActor *apwin;

  g_object_get (thumbnail,
		"window",
		&apwin,
		NULL);
  
  hd_render_manager_return_app (apwin);

  if (priv->cemetery)
    g_ptr_array_foreach (priv->cemetery,
                         (GFunc)hd_render_manager_return_app, NULL);
  if (priv->dialogs)
    g_list_foreach (priv->dialogs,
                    (GFunc)hd_render_manager_return_dialog, NULL);

}

void 
hd_tn_thumbnail_replace_window (HdTnThumbnail *thumbnail, 
				ClutterActor *new_window)
{
  HdTnThumbnailPrivate *priv =
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);

  ClutterActor *old_window = priv->apwin;

 /* Resurrect @new_win in the .cemetery. */
  if (priv->cemetery)
    {
      guint i;

      /* Verify that @old_win (current .apwin) is not in .cemetery yet. */
      for (i = 0; i < priv->cemetery->len; i++)
        g_assert (priv->cemetery->pdata[i] != old_window);

      if (g_ptr_array_remove_fast (priv->cemetery, new_window))
        g_object_weak_unref (G_OBJECT (new_window),
                             (GWeakNotify)g_ptr_array_remove_fast,
                             priv->cemetery);
    }
  else
    priv->cemetery = g_ptr_array_new ();

  /* Add @old_win to .cemetery.  Do it before we unref @old_win,
   * so we don't possibly add a dangling pointer there. */
  g_ptr_array_add (priv->cemetery, old_window);

  g_object_weak_ref (G_OBJECT (old_window),
                     (GWeakNotify)g_ptr_array_remove_fast,
                     priv->cemetery);

   HdTaskNavigator *navigator =
    HD_TASK_NAVIGATOR (g_object_get_data (G_OBJECT (thumbnail), "task-nav"));

  gboolean show = hd_task_navigator_is_active (navigator);

  /* Replace .apwin */
  if (show)  /* .apwin is in the cemetery */
    clutter_actor_hide (priv->apwin);

  g_object_unref (priv->apwin);

  g_object_set (G_OBJECT (thumbnail),
		"window",
                new_window,
		NULL);

  if (show)
    clutter_actor_reparent (priv->apwin, priv->windows);

  /* Replace the client window structure with @new_win's. */
  if (priv->win)
    mb_wm_object_signal_disconnect (MB_WM_OBJECT (priv->win),
                                    priv->win_changed_cb_id);

  priv->win = actor_to_client_window (new_window, NULL);

  if (priv->win)
    {
      g_free (priv->saved_title);

      priv->saved_title = NULL;
      priv->win_changed_cb_id = 
 	mb_wm_object_signal_connect (MB_WM_OBJECT (priv->win),
				     MBWM_WINDOW_PROP_NAME,
                     		     (MBWMObjectCallbackFunc)win_title_changed,
				     thumbnail);

      /* Update the title now if it's shown (and not a notification). */
      if (FALSE)//!thumb_has_notification (apthumb))
        hd_tn_thumbnail_reset_title (thumbnail);
    }
}

void 
hd_tn_thumbnail_add_dialog (HdTnThumbnail *thumbnail, ClutterActor *dialog)
{
  HdTnThumbnailPrivate *priv = 
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);

  priv->dialogs = g_list_prepend (priv->dialogs, dialog);

  clutter_actor_hide (priv->close_app_icon);
}

void 
hd_tn_thumbnail_remove_dialog (HdTnThumbnail *thumbnail, ClutterActor *dialog)
{
  HdTnThumbnailPrivate *priv = 
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);
/* Not necessary as the only caller of this function checks for this 
  if (!hd_tn_thumbnail_has_dialog (thumbnail, dialog))
    {
      g_warning ("I can't remove what I don't have!");
      return;
    }
*/
  GList *l;

  for (l = priv->dialogs; l != NULL; l = l->next)
    {
      if (CLUTTER_ACTOR (l->data) == dialog)
        {
          g_object_unref (dialog);
          priv->dialogs = g_list_delete_link (priv->dialogs, l);
	  break;
        }
    }

  /* this must be safe as per above */
  if (!priv->dialogs /* && !thumb_has_notification (apthumb))*/)
    clutter_actor_show (priv->close_app_icon);
}

void
hd_tn_thumbnail_reparent_dialog (HdTnThumbnail *thumbnail, ClutterActor *dialog)
{
  HdTnThumbnailPrivate *priv = 
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);

  clutter_actor_reparent (dialog, priv->windows);
}

gboolean 
hd_tn_thumbnail_has_dialog (HdTnThumbnail *thumbnail, ClutterActor *dialog)
{
  HdTnThumbnailPrivate *priv = 
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);
  GList *l;

  for (l = priv->dialogs; l != NULL; l = l->next)
    if (CLUTTER_ACTOR (l->data) == dialog)
      return TRUE; 

  return FALSE;
}

void 
hd_tn_thumbnail_set_jail_scale (HdTnThumbnail *thumbnail, 
				gdouble x, 
				gdouble y)
{
  HdTnThumbnailPrivate *priv =
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);

  clutter_actor_set_scale (priv->jail, x, y);
}

void 
hd_tn_thumbnail_get_jail_scale (HdTnThumbnail *thumbnail, 
				gdouble *x, 
				gdouble *y)
{
  gdouble jail_x, jail_y;
  HdTnThumbnailPrivate *priv = 
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);

  if (priv->jail == NULL)
    {
      if (x != NULL)
        clutter_actor_get_scale (CLUTTER_ACTOR (thumbnail), x, NULL);

      if (y != NULL)
        clutter_actor_get_scale (CLUTTER_ACTOR (thumbnail), NULL, y);

      return;
    } 
 
  clutter_actor_get_scale (priv->jail, &jail_x, &jail_y);

  if (x != NULL)
    *x = jail_x;

  if (y != NULL)
    *y = jail_y;
}

gboolean
hd_tn_thumbnail_is_app (HdTnThumbnail *thumbnail)
{
  HdTnThumbnailPrivate *priv =
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);

  return (priv->type == THUMBNAIL_APP);
}

void 
hd_tn_thumbnail_update_inners (HdTnThumbnail *thumbnail, gint title_size)
{
   HdTnThumbnailPrivate *priv =
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);

   guint width  = clutter_actor_get_width  (CLUTTER_ACTOR (thumbnail));
   guint height = clutter_actor_get_height (CLUTTER_ACTOR (thumbnail));

   clutter_actor_set_position (priv->close, 
			       width,
			       0);

   clutter_actor_set_size (priv->title, 
			   title_size,
			   clutter_actor_get_height (priv->title)*2);

   guint wt, ht, wb, hb;
   /* This is quite boring. */
   clutter_actor_get_size(priv->frame.nw, &wt, &ht);
   clutter_actor_get_size(priv->frame.sw, &wb, &hb);

   clutter_actor_set_position (priv->frame.nm, wt, 0);
   clutter_actor_set_position (priv->frame.ne, width, 0);
   clutter_actor_set_position (priv->frame.mw, 0, ht);
   clutter_actor_set_position (priv->frame.me, width, ht);
   clutter_actor_set_position (priv->frame.sw, 0, height);
   clutter_actor_set_position (priv->frame.sm, wb, height);
   clutter_actor_set_position (priv->frame.se, width, height);

   clutter_actor_set_scale (priv->frame.nm,
            (gdouble)(width - 2*wt)
                / clutter_actor_get_width (priv->frame.nm), 1);
   clutter_actor_set_scale (priv->frame.sm,
              (gdouble)(width - 2*wb)
                / clutter_actor_get_width (priv->frame.sm), 1);
   clutter_actor_set_scale (priv->frame.mw, 1,
              (gdouble)(height - (ht+hb))
                / clutter_actor_get_height (priv->frame.mw));
   clutter_actor_set_scale (priv->frame.me, 1,
              (gdouble)(height - (ht+hb))
                / clutter_actor_get_height (priv->frame.me));

   if (hd_tn_thumbnail_is_app (thumbnail))
     {
       guint wwidth, wheight, wjail, hjail;

       wwidth  = hd_comp_mgr_get_current_screen_width ();
       wheight = hd_comp_mgr_get_current_screen_height () - HD_COMP_MGR_TOP_MARGIN;

       wjail = width  - 2*FRAME_WIDTH;
       hjail = height - (FRAME_TOP_HEIGHT+FRAME_BOTTOM_HEIGHT);
       hd_tn_thumbnail_set_jail_scale (thumbnail,
                     		       (gdouble)wjail / wwidth,
                      		       (gdouble)hjail / wheight);     
     }
}

ClutterActor *
hd_tn_thumbnail_get_app_window (HdTnThumbnail *thumbnail)
{
  HdTnThumbnailPrivate *priv = 
    HD_TN_THUMBNAIL_GET_PRIVATE (thumbnail);

  return priv->apwin;
}
