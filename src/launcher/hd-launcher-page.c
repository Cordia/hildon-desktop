/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Marc Ordinas i Llopis <marc.ordinasillopis@collabora.co.uk>
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

#include "hd-launcher-page.h"

#include <glib-object.h>
#include <clutter/clutter.h>
#include <clutter/clutter-timeline.h>
#include <tidy/tidy-finger-scroll.h>
#include <tidy/tidy-scroll-view.h>
#include <tidy/tidy-scroll-bar.h>
#include <tidy/tidy-adjustment.h>
#include <hildon/hildon-defines.h>
#include <math.h>

#include "hd-transition.h"
#include "hildon-desktop.h"
#include "hd-launcher.h"
#include "hd-launcher-grid.h"
#include "hd-gtk-utils.h"
#include "hd-gtk-style.h"
#include "hd-comp-mgr.h"


#define I_(str) (g_intern_static_string ((str)))
#define HD_PARAM_READWRITE (G_PARAM_READWRITE | \
                            G_PARAM_STATIC_NICK | \
                            G_PARAM_STATIC_NAME | \
                            G_PARAM_STATIC_BLURB)

#define HD_LAUNCHER_PAGE_SUB_OPACITY (0.33f)

#define HD_LAUNCHER_PAGE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HD_TYPE_LAUNCHER_PAGE, HdLauncherPagePrivate))

struct _HdLauncherPagePrivate
{
  ClutterActor *icon;
  ClutterActor *label;
  ClutterActor *scroller;
  ClutterActor *grid;
  ClutterActor *empty_label;

  HdLauncherPageTransition transition_type;
  ClutterTimeline *transition;
  /* Timeline works by signals, so we get horrible flicker if we ask it if it
   * is playing right after saying _start() - so we have a boolean to figure
   * out for ourselves */
  gboolean         transition_playing;

  /* When the user clicks and drags more than a certain amount, we want
   * to deselect what they had clicked on - so we must keep track of
   * movement */
  gint drag_distance;
  gint drag_last_x, drag_last_y;
};

enum
{
  BACK_BUTTON_PRESSED,
  TILE_CLICKED,

  LAST_SIGNAL
};

static guint launcher_page_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (HdLauncherPage, hd_launcher_page, CLUTTER_TYPE_GROUP);


/* Forward declarations. */
static void hd_launcher_page_constructed (GObject *object);
static void hd_launcher_page_dispose (GObject *gobject);
static void hd_launcher_page_show (ClutterActor *actor);

static void hd_launcher_page_tile_clicked (HdLauncherTile *tile,
                                           gpointer data);
static void hd_launcher_page_new_frame(ClutterTimeline *timeline,
                                       gint frame_num, gpointer data);
static void hd_launcher_page_transition_end(ClutterTimeline *timeline,
                                            gpointer data);

static void
hd_launcher_page_class_init (HdLauncherPageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdLauncherPagePrivate));

  gobject_class->constructed  = hd_launcher_page_constructed;
  gobject_class->dispose     = hd_launcher_page_dispose;

  launcher_page_signals[BACK_BUTTON_PRESSED] =
    g_signal_new (I_("back-button-pressed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  launcher_page_signals[TILE_CLICKED] =
    g_signal_new (I_("tile-clicked"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  HD_TYPE_LAUNCHER_TILE);
}

static void
hd_launcher_page_init (HdLauncherPage *page)
{
  page->priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  clutter_actor_set_size(CLUTTER_ACTOR(page),
        HD_LAUNCHER_PAGE_WIDTH, HD_LAUNCHER_PAGE_HEIGHT);
  clutter_actor_set_reactive (CLUTTER_ACTOR(page), FALSE);
  g_signal_connect(page, "show", G_CALLBACK(hd_launcher_page_show), 0);
}

static gboolean
captured_event_cb (TidyFingerScroll *scroll,
                 ClutterEvent *event,
                 HdLauncherPage *page)
{
  HdLauncherPagePrivate *priv;

  if (!HD_IS_LAUNCHER_PAGE(page))
    return FALSE;
  priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  if (event->type == CLUTTER_BUTTON_PRESS)
    {
      ClutterButtonEvent *bevent = (ClutterButtonEvent *)event;
      priv->drag_distance = 0.0f;
      priv->drag_last_x = bevent->x;
      priv->drag_last_y = bevent->y;
    }

  return FALSE;
}

static gboolean
motion_event_cb (TidyFingerScroll *scroll,
                 ClutterMotionEvent *event,
                 HdLauncherPage *page)
{
  HdLauncherPagePrivate *priv;
  gint dx, dy;

  if (!HD_IS_LAUNCHER_PAGE(page))
    return FALSE;

  priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  dx = priv->drag_last_x - event->x;
  dy = priv->drag_last_y - event->y;
  priv->drag_last_x = event->x;
  priv->drag_last_y = event->y;
  priv->drag_distance += (int)sqrt(dx*dx + dy*dy);

  /* If we dragged too far, deselect (and de-glow) */
  if (priv->drag_distance > HD_LAUNCHER_TILE_MAX_DRAG)
    {
      hd_launcher_grid_reset(HD_LAUNCHER_GRID(priv->grid));
    }

  return FALSE;
}

static void
hd_launcher_page_constructed (GObject *object)
{
  ClutterColor text_color;
  gchar *font_string;
  HdLauncherPage *page = HD_LAUNCHER_PAGE(object);
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (object);
  guint x1, y1;
  guint label_width, label_height;

  /* Create the label that says this page is empty */
  hd_gtk_style_get_text_color (HD_GTK_BUTTON_SINGLETON,
                               GTK_STATE_NORMAL,
                               &text_color);
  font_string = hd_gtk_style_get_font_string (HD_GTK_BUTTON_SINGLETON);
  priv->empty_label = clutter_label_new_full(font_string,
                                             _("tana_li_of_noapps"),
                                             &text_color);
  clutter_label_set_line_wrap (CLUTTER_LABEL (priv->empty_label), TRUE);
  clutter_label_set_ellipsize (CLUTTER_LABEL (priv->empty_label),
                               PANGO_ELLIPSIZE_NONE);
  clutter_label_set_alignment (CLUTTER_LABEL (priv->empty_label),
                               PANGO_ALIGN_CENTER);
  clutter_label_set_line_wrap_mode (CLUTTER_LABEL (priv->empty_label),
                                    PANGO_WRAP_WORD);

  clutter_actor_get_size(priv->empty_label, &label_width, &label_height);
  /* Position the 'empty label' item in the centre */
  x1 = (HD_LAUNCHER_PAGE_WIDTH - label_width) / 2;
  y1 = ((HD_LAUNCHER_PAGE_HEIGHT - HD_LAUNCHER_PAGE_YMARGIN - label_height)/2) +
        HD_LAUNCHER_PAGE_YMARGIN;
  clutter_actor_set_position (priv->empty_label, x1, y1);
  clutter_container_add_actor (CLUTTER_CONTAINER (page), priv->empty_label);
  g_free (font_string);

  priv->scroller = tidy_finger_scroll_new (TIDY_FINGER_SCROLL_MODE_KINETIC);
  clutter_container_add_actor (CLUTTER_CONTAINER (page),
                               priv->scroller);
  clutter_actor_set_size(priv->scroller, HD_LAUNCHER_PAGE_WIDTH,
                                         HD_LAUNCHER_PAGE_HEIGHT);

  priv->grid = hd_launcher_grid_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->scroller),
                               priv->grid);
  priv->transition = clutter_timeline_new_for_duration (1000);
  g_signal_connect (priv->transition, "new-frame",
                    G_CALLBACK (hd_launcher_page_new_frame), object);
  g_signal_connect (priv->transition, "completed",
                    G_CALLBACK (hd_launcher_page_transition_end), object);
  priv->transition_playing = FALSE;

  /* Add callbacks for de-selecting an icon after the user has moved
   * their finger more than a certain amount */
  g_signal_connect (priv->scroller,
                    "captured-event",
                    G_CALLBACK (captured_event_cb),
                    object);
  g_signal_connect (priv->scroller,
                    "motion-event",
                    G_CALLBACK (motion_event_cb),
                    object);
}

ClutterActor *
hd_launcher_page_new (void)
{
  return g_object_new (HD_TYPE_LAUNCHER_PAGE,
                       NULL);
}

static void
hd_launcher_page_dispose (GObject *gobject)
{
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (gobject);

  if (priv->empty_label)
    {
      clutter_actor_destroy (priv->empty_label);
      priv->empty_label = NULL;
    }

  if (priv->transition)
    priv->transition = (g_object_unref(priv->transition), NULL);

  if (priv->scroller)
    {
      clutter_actor_destroy (priv->scroller);
      priv->scroller = NULL;
    }

  G_OBJECT_CLASS (hd_launcher_page_parent_class)->dispose (gobject);
}

static void
hd_launcher_page_show (ClutterActor *actor)
{
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (actor);
  /* make the scrollbars appear and then fade out (they won't be shown
   * if the scrollable area is less than the screen size) */
  tidy_finger_scroll_show_scrollbars(priv->scroller);
}

ClutterActor *
hd_launcher_page_get_grid (HdLauncherPage *page)
{
  return (HD_LAUNCHER_PAGE_GET_PRIVATE (page))->grid;
}

void
hd_launcher_page_add_tile (HdLauncherPage *page, HdLauncherTile* tile)
{
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);
  g_return_if_fail(HD_IS_LAUNCHER_PAGE(page));

  if (priv->empty_label)
    {
      clutter_actor_destroy (priv->empty_label);
      priv->empty_label = NULL;
    }

  clutter_container_add_actor (CLUTTER_CONTAINER (priv->grid),
                               CLUTTER_ACTOR (tile));

  g_signal_connect (tile, "clicked",
                    G_CALLBACK (hd_launcher_page_tile_clicked),
                    page);
}

static void
hd_launcher_page_tile_clicked (HdLauncherTile *tile, gpointer data)
{
  g_signal_emit (HD_LAUNCHER_PAGE(data),
                 launcher_page_signals[TILE_CLICKED],
                 0, tile);
}

const char *
hd_launcher_page_get_transition_string(
    HdLauncherPageTransition trans_type)
{
  switch (trans_type)
  {
    case HD_LAUNCHER_PAGE_TRANSITION_IN :
      return "launcher_in";
    case HD_LAUNCHER_PAGE_TRANSITION_OUT :
      return "launcher_out";
    case HD_LAUNCHER_PAGE_TRANSITION_OUT_BACK :
      return "launcher_out_back";
    case HD_LAUNCHER_PAGE_TRANSITION_LAUNCH :
      return "launcher_launch";
    case HD_LAUNCHER_PAGE_TRANSITION_IN_SUB :
      return "launcher_in_sub";
    case HD_LAUNCHER_PAGE_TRANSITION_OUT_SUB :
      return "launcher_out_sub";
    case HD_LAUNCHER_PAGE_TRANSITION_BACK :
      return "launcher_back";
    case HD_LAUNCHER_PAGE_TRANSITION_FORWARD :
      return "launcher_forward";
  }
  return "unknown";
}

void hd_launcher_page_transition(HdLauncherPage *page, HdLauncherPageTransition trans_type)
{
  HdLauncherPagePrivate *priv;

  if (!HD_IS_LAUNCHER_PAGE(page))
    return;

  priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  /* check for the case where we're hiding when already hidden */
  if (!CLUTTER_ACTOR_IS_VISIBLE(page) &&
      trans_type == HD_LAUNCHER_PAGE_TRANSITION_OUT)
    return;
  /* check for the case where we're launching and then hiding, and just use
   * the launching animation */
  if (clutter_timeline_is_playing(priv->transition) &&
      priv->transition_type == HD_LAUNCHER_PAGE_TRANSITION_LAUNCH &&
      trans_type == HD_LAUNCHER_PAGE_TRANSITION_OUT)
    return;
  /* if we were already playing, stop the animation */
  if (priv->transition_playing)
    hd_launcher_page_transition_stop(page);
  /* Reset all the tiles in the grid, so they don't have any blurring */
  hd_launcher_grid_reset(HD_LAUNCHER_GRID(priv->grid));
  hd_launcher_grid_transition_begin(HD_LAUNCHER_GRID(priv->grid), trans_type);

  priv->transition_type = trans_type;
  switch (priv->transition_type) {
    case HD_LAUNCHER_PAGE_TRANSITION_IN:
    case HD_LAUNCHER_PAGE_TRANSITION_IN_SUB:
    case HD_LAUNCHER_PAGE_TRANSITION_FORWARD:
         clutter_actor_show(CLUTTER_ACTOR(page));
         clutter_actor_show(CLUTTER_ACTOR(page));
         break;
    case HD_LAUNCHER_PAGE_TRANSITION_OUT:
    case HD_LAUNCHER_PAGE_TRANSITION_OUT_SUB:
         tidy_finger_scroll_hide_scrollbars_now (priv->scroller);
    case HD_LAUNCHER_PAGE_TRANSITION_OUT_BACK:
    case HD_LAUNCHER_PAGE_TRANSITION_LAUNCH:
    case HD_LAUNCHER_PAGE_TRANSITION_BACK:
         /* already shown */
         break;
  }

  clutter_timeline_pause(priv->transition);
  clutter_timeline_rewind(priv->transition);

  clutter_timeline_set_duration(priv->transition,
      hd_transition_get_int(
          hd_launcher_page_get_transition_string(priv->transition_type),
          "duration",
          500 /* default value */));

  clutter_timeline_start(priv->transition);
  priv->transition_playing = TRUE;

  /* force a call to lay stuff out before it gets drawn properly */
  hd_launcher_page_new_frame(priv->transition, 0, page);
}

void hd_launcher_page_transition_stop(HdLauncherPage *page)
{
  HdLauncherPagePrivate *priv;

  if (!HD_IS_LAUNCHER_PAGE(page))
    return;

  priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  if (priv->transition_playing)
    {
      gint frames;
      /* force a call to lay stuff out as if the transition has ended */
      frames = clutter_timeline_get_n_frames(priv->transition);
      hd_launcher_page_new_frame(priv->transition, frames, page);
      hd_launcher_page_transition_end(priv->transition, page);
    }
}

static void
hd_launcher_page_new_frame(ClutterTimeline *timeline,
                          gint frame_num, gpointer data)
{
  HdLauncherPage *page = HD_LAUNCHER_PAGE(data);
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);
  gint frames;
  float amt;

  if (!HD_IS_LAUNCHER_PAGE(data))
    return;

  frames = clutter_timeline_get_n_frames(timeline);
  amt = frame_num / (float)frames;

  hd_launcher_grid_transition(HD_LAUNCHER_GRID(priv->grid),
                              page,
                              priv->transition_type,
                              amt);

    switch (priv->transition_type)
      {
    case HD_LAUNCHER_PAGE_TRANSITION_IN:
    case HD_LAUNCHER_PAGE_TRANSITION_IN_SUB:
        break;
    case HD_LAUNCHER_PAGE_TRANSITION_OUT:
    case HD_LAUNCHER_PAGE_TRANSITION_LAUNCH:
    case HD_LAUNCHER_PAGE_TRANSITION_OUT_SUB:
        break;
    case HD_LAUNCHER_PAGE_TRANSITION_BACK:
    case HD_LAUNCHER_PAGE_TRANSITION_FORWARD:
        /* Everything is done in the grid here */
        break;
    case HD_LAUNCHER_PAGE_TRANSITION_OUT_BACK:
        /* nothing to do here as we're already hidden */
        break;
      }
}

static void
hd_launcher_page_transition_end(ClutterTimeline *timeline,
                                gpointer data)
{
  HdLauncherPage *page = HD_LAUNCHER_PAGE(data);
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  if (!HD_IS_LAUNCHER_PAGE(data))
    return;

  hd_launcher_grid_transition(HD_LAUNCHER_GRID(priv->grid),
                              page,
                              priv->transition_type,
                              1.0f);
  hd_launcher_grid_transition_end(HD_LAUNCHER_GRID(priv->grid));

  switch (priv->transition_type) {
    case HD_LAUNCHER_PAGE_TRANSITION_IN:
    case HD_LAUNCHER_PAGE_TRANSITION_IN_SUB:
    case HD_LAUNCHER_PAGE_TRANSITION_FORWARD:
         /* already shown */
         break;
    case HD_LAUNCHER_PAGE_TRANSITION_OUT:
    case HD_LAUNCHER_PAGE_TRANSITION_OUT_BACK:
    case HD_LAUNCHER_PAGE_TRANSITION_LAUNCH:
         clutter_actor_hide(CLUTTER_ACTOR(page));
         hd_launcher_hide_final();
         break;
    case HD_LAUNCHER_PAGE_TRANSITION_OUT_SUB:
    case HD_LAUNCHER_PAGE_TRANSITION_BACK:
         clutter_actor_hide(CLUTTER_ACTOR(page));
         break;
  }

  priv->transition_playing = FALSE;
}

ClutterFixed hd_launcher_page_get_scroll_y(HdLauncherPage *page)
{
  HdLauncherPagePrivate *priv;
  ClutterActor *bar;
  TidyAdjustment *adjust;

  if (!HD_IS_LAUNCHER_PAGE(page))
    return 0;

  priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  bar = tidy_scroll_view_get_vscroll_bar (TIDY_SCROLL_VIEW(priv->scroller));
  adjust = tidy_scroll_bar_get_adjustment (TIDY_SCROLL_BAR(bar));
  return tidy_adjustment_get_valuex( adjust );
}

void hd_launcher_page_set_drag_distance(HdLauncherPage *page, float d)
{
  HdLauncherPagePrivate *priv;

  if (!HD_IS_LAUNCHER_PAGE(page))
      return;

  priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);
  priv->drag_distance = d;
}

/* Return how far the user has dragged */
float hd_launcher_page_get_drag_distance(HdLauncherPage *page)
{
  HdLauncherPagePrivate *priv;

  if (!HD_IS_LAUNCHER_PAGE(page))
      return 0;

  priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);
  return priv->drag_distance;
}
