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

/* This class handles drawing of things in the title bar right at the top
 * of the screen - currently just top-left buttons (which have to animate)
 */

#include "tidy/tidy-sub-texture.h"

#include "hd-title-bar.h"
#include "hd-clutter-cache.h"
#include "mb/hd-app.h"
#include "mb/hd-comp-mgr.h"
#include "mb/hd-decor.h"
#include "mb/hd-theme.h"
#include "hd-render-manager.h"
#include "hd-gtk-utils.h"

#include <matchbox/theme-engines/mb-wm-theme-png.h>
#include <matchbox/theme-engines/mb-wm-theme-xml.h>
#include <math.h>

enum
{
  BTN_BG_ATTACHED = 0,
  BTN_BG_LEFT_END,
  BTN_BG_RIGHT_END,
  BTN_BG_LEFT_PRESSED,
  BTN_BG_RIGHT_PRESSED,
  BTN_SEPARATOR_LEFT,
  BTN_SEPARATOR_STATUS,
  BTN_SEPARATOR_RIGHT,
  BTN_SWITCHER,
  BTN_SWITCHER_HIGHLIGHT,
  BTN_SWITCHER_PRESSED,
  BTN_LAUNCHER,
  BTN_LAUNCHER_PRESSED,
  BTN_BACK,
  BTN_BACK_PRESSED,
  BTN_CLOSE,
  BTN_CLOSE_PRESSED,
  BTN_COUNT
};

const char *BTN_FILENAMES[BTN_COUNT] = {
    HD_THEME_IMG_LEFT_ATTACHED,
    HD_THEME_IMG_LEFT_END,
    HD_THEME_IMG_RIGHT_END,
    HD_THEME_IMG_LEFT_PRESSED,
    HD_THEME_IMG_RIGHT_PRESSED,
    HD_THEME_IMG_SEPARATOR,
    HD_THEME_IMG_SEPARATOR,
    HD_THEME_IMG_SEPARATOR,
    HD_THEME_IMG_TASK_SWITCHER,
    HD_THEME_IMG_TASK_SWITCHER_HIGHLIGHT,
    HD_THEME_IMG_TASK_SWITCHER_PRESSED,
    HD_THEME_IMG_TASK_LAUNCHER,
    HD_THEME_IMG_TASK_LAUNCHER_PRESSED,
    HD_THEME_IMG_BACK,
    HD_THEME_IMG_BACK_PRESSED,
    HD_THEME_IMG_CLOSE,
    HD_THEME_IMG_CLOSE_PRESSED,
};

gboolean ALIGN_RIGHT[BTN_COUNT] = {
   FALSE, // BTN_BG_ATTACHED = 0,
   FALSE, // BTN_BG_LEFT_END,
   TRUE,  // BTN_BG_RIGHT_END,
   FALSE, // BTN_BG_LEFT_PRESSED,
   TRUE,  // BTN_BG_RIGHT_PRESSED,
   FALSE, // BTN_SEPARATOR_LEFT,
   FALSE, // BTN_SEPARATOR_STATUS,
   FALSE, // BTN_SEPARATOR_RIGHT,
   FALSE, // BTN_SWITCHER,
   FALSE, // BTN_SWITCHER_HIGHLIGHT,
   FALSE, // BTN_SWITCHER_PRESSED,
   FALSE, // BTN_LAUNCHER,
   FALSE, // BTN_LAUNCHER_PRESSED,
   TRUE,  // BTN_BACK,
   TRUE,  // BTN_BACK_PRESSED,
   TRUE,  // BTN_CLOSE,
   TRUE,  // BTN_CLOSE_PRESSED,
};

/* We try and set sizes for what we can, because if we get images
 * we couldn't load, they won't show properly otherwise
 */
gboolean SET_SIZE[BTN_COUNT] = {
   TRUE, // BTN_BG_ATTACHED = 0,
   TRUE, // BTN_BG_LEFT_END,
   TRUE,  // BTN_BG_RIGHT_END,
   TRUE, // BTN_BG_LEFT_PRESSED,
   TRUE,  // BTN_BG_RIGHT_PRESSED,
   FALSE, // BTN_SEPARATOR_LEFT,
   FALSE, // BTN_SEPARATOR_STATUS,
   FALSE, // BTN_SEPARATOR_RIGHT,
   TRUE, // BTN_SWITCHER,
   TRUE, // BTN_SWITCHER_HIGHLIGHT,
   TRUE, // BTN_SWITCHER_PRESSED,
   TRUE, // BTN_LAUNCHER,
   TRUE, // BTN_LAUNCHER_PRESSED,
   TRUE,  // BTN_BACK,
   TRUE,  // BTN_BACK_PRESSED,
   TRUE,  // BTN_CLOSE,
   TRUE,  // BTN_CLOSE_PRESSED,
};

struct _HdTitleBarPrivate
{
  /* All the images we need for buttons */
  ClutterActor          *buttons[BTN_COUNT];

  ClutterActor          *btn_launcher;
  ClutterActor          *btn_switcher;

  ClutterActor          *title_bg;
  ClutterLabel          *title;
  /* Pulsing animation for switcher */
  ClutterTimeline       *switcher_timeline;

  HdTitleBarVisEnum      state;
};

/* HdHomeThemeButtonBack, MBWMDecorButtonClose */

/* ------------------------------------------------------------------------- */

G_DEFINE_TYPE (HdTitleBar, hd_title_bar, CLUTTER_TYPE_GROUP);
#define HD_TITLE_BAR_GET_PRIVATE(obj) \
                (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                HD_TYPE_TITLE_BAR, HdTitleBarPrivate))

static void
hd_title_bar_add_signals(HdTitleBar *bar, ClutterActor *actor);
static void
on_switcher_timeline_new_frame(ClutterTimeline *timeline,
                               gint frame_num, HdTitleBar *bar);
static void
hd_title_bar_set_full_width(HdTitleBar *bar, gboolean full_size);

/* ------------------------------------------------------------------------- */

#define HD_TITLE_BAR_SWITCHER_PULSE_DURATION 2000

/* margin to left of the app title */
#define HD_TITLE_BAR_TITLE_MARGIN 24

enum
{
  CLICKED_TOP_LEFT,
  PRESS_TOP_LEFT,
  LEAVE_TOP_LEFT,
  LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0, };

/* ------------------------------------------------------------------------- */

static void
hd_title_bar_init (HdTitleBar *bar)
{
  ClutterActor *actor = CLUTTER_ACTOR(bar);
  ClutterColor white = {0xFF, 0xFF, 0xFF, 0xFF};
  HdTitleBarPrivate *priv = bar->priv = HD_TITLE_BAR_GET_PRIVATE(bar);
  gint i;

  priv->state = HDTB_VIS_NONE;

  clutter_actor_set_visibility_detect(actor, FALSE);
  clutter_actor_set_position(actor, 0, 0);
  clutter_actor_set_size(actor,
                    HD_COMP_MGR_SCREEN_WIDTH, HD_COMP_MGR_TOP_MARGIN);
  /* Task Navigator Button */
  priv->btn_switcher = clutter_group_new();
  clutter_actor_set_size(priv->btn_switcher,
      HD_COMP_MGR_TOP_LEFT_BTN_WIDTH,
      HD_COMP_MGR_TOP_LEFT_BTN_HEIGHT);
  hd_render_manager_set_button (HDRM_BUTTON_TASK_NAV, priv->btn_switcher);

  /* Task Launcher Button */
  priv->btn_launcher = clutter_group_new();
  clutter_actor_set_size(priv->btn_launcher,
        HD_COMP_MGR_TOP_LEFT_BTN_WIDTH,
        HD_COMP_MGR_TOP_LEFT_BTN_HEIGHT);
  hd_render_manager_set_button(HDRM_BUTTON_LAUNCHER, priv->btn_launcher);

  /* Title background */
  priv->title_bg = hd_clutter_cache_get_texture(
      HD_THEME_IMG_TITLE_BAR, TRUE);
  clutter_container_add_actor(CLUTTER_CONTAINER(bar), CLUTTER_ACTOR(priv->title_bg));

  /* Load every button we need */
  for (i=0;i<BTN_COUNT;i++)
    {
      priv->buttons[i] = hd_clutter_cache_get_texture(BTN_FILENAMES[i], TRUE);
      clutter_container_add_actor(CLUTTER_CONTAINER(bar), priv->buttons[i]);
      clutter_actor_hide(priv->buttons[i]);

      if (ALIGN_RIGHT[i])
        {
          clutter_actor_set_position(priv->buttons[i],
              HD_COMP_MGR_SCREEN_WIDTH-HD_COMP_MGR_TOP_RIGHT_BTN_WIDTH, 0);
          if (SET_SIZE[i])
            clutter_actor_set_size(priv->buttons[i],
              HD_COMP_MGR_TOP_RIGHT_BTN_WIDTH,
              HD_COMP_MGR_TOP_RIGHT_BTN_HEIGHT);
        }
      else
        {
          if (SET_SIZE[i])
            clutter_actor_set_size(priv->buttons[i],
              HD_COMP_MGR_TOP_LEFT_BTN_WIDTH,
              HD_COMP_MGR_TOP_LEFT_BTN_HEIGHT);
        }
    }

  /* TODO: setup BTN_SWITCHER_HIGHLIGHT for adding here... */

  hd_title_bar_add_signals(bar, priv->buttons[BTN_SWITCHER]);
  hd_title_bar_add_signals(bar, priv->buttons[BTN_LAUNCHER]);

  /* Create the title */
  priv->title = CLUTTER_LABEL(clutter_label_new());
  clutter_label_set_color(priv->title, &white);
  clutter_label_set_use_markup(priv->title, TRUE);
  clutter_container_add_actor(CLUTTER_CONTAINER(bar), CLUTTER_ACTOR(priv->title));
  clutter_actor_hide(CLUTTER_ACTOR(priv->title));

  /* Create timeline animation */
  priv->switcher_timeline =
    clutter_timeline_new_for_duration(HD_TITLE_BAR_SWITCHER_PULSE_DURATION);
  clutter_timeline_set_loop(priv->switcher_timeline, TRUE);
  g_signal_connect (priv->switcher_timeline, "new-frame",
                        G_CALLBACK (on_switcher_timeline_new_frame), bar);
}

static void
hd_title_bar_dispose (GObject *obj)
{
  HdTitleBarPrivate *priv = HD_TITLE_BAR(obj)->priv;
  gint i;

  for (i=0;i<BTN_COUNT;i++)
    clutter_actor_destroy(priv->buttons[i]);
}

static void
hd_title_bar_class_init (HdTitleBarClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdTitleBarPrivate));

  gobject_class->dispose = hd_title_bar_dispose;

  signals[CLICKED_TOP_LEFT] =
      g_signal_new ("clicked-top-left",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);
  signals[PRESS_TOP_LEFT] =
        g_signal_new ("press-top-left",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
  signals[LEAVE_TOP_LEFT] =
        g_signal_new ("leave-top-left",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}

void hd_title_bar_set_state(HdTitleBar *bar,
                            HdTitleBarVisEnum button)
{
  HdTitleBarPrivate *priv;
  if (!HD_IS_TITLE_BAR(bar))
    return;
  priv = bar->priv;

  priv->state = button;

  /* if a button isn't in the top-left or right, we want to make
   * sure we don't display the pressed state */
  if (!(button & HDTB_VIS_BTN_LEFT_MASK))
    clutter_actor_hide(priv->buttons[BTN_BG_LEFT_PRESSED]);
  if (!(button & HDTB_VIS_BTN_RIGHT_MASK))
      clutter_actor_hide(priv->buttons[BTN_BG_RIGHT_PRESSED]);


  if (button & HDTB_VIS_BTN_LAUNCHER)
    {
      clutter_actor_show(priv->buttons[BTN_LAUNCHER]);
      clutter_actor_show(priv->btn_launcher);
    }
  else
    {
      clutter_actor_hide(priv->buttons[BTN_LAUNCHER]);
      clutter_actor_hide(priv->buttons[BTN_LAUNCHER_PRESSED]);
      clutter_actor_hide(priv->btn_launcher);
    }

  if (button & HDTB_VIS_BTN_SWITCHER)
    {
      clutter_actor_show(priv->buttons[BTN_SWITCHER]);
      clutter_actor_show(priv->buttons[BTN_SWITCHER_HIGHLIGHT]);
      clutter_actor_set_opacity(priv->buttons[BTN_SWITCHER_HIGHLIGHT], 0);
      clutter_actor_show(priv->btn_switcher);
    }
  else
    {
      clutter_actor_hide(priv->buttons[BTN_SWITCHER]);
      clutter_actor_hide(priv->buttons[BTN_SWITCHER_PRESSED]);
      clutter_actor_hide(priv->buttons[BTN_SWITCHER_HIGHLIGHT]);
      clutter_actor_hide(priv->btn_switcher);
    }

  if (button & HDTB_VIS_BTN_BACK)
    {
      clutter_actor_show(priv->buttons[BTN_BACK]);
    }
  else
    {
      clutter_actor_hide(priv->buttons[BTN_BACK]);
      clutter_actor_hide(priv->buttons[BTN_BACK_PRESSED]);
    }

  if (button & HDTB_VIS_BTN_CLOSE)
    {
      clutter_actor_show(priv->buttons[BTN_CLOSE]);
    }
  else
    {
      clutter_actor_hide(priv->buttons[BTN_CLOSE]);
      clutter_actor_hide(priv->buttons[BTN_CLOSE_PRESSED]);
    }

  hd_title_bar_set_full_width(bar, button & HDTB_VIS_FULL_WIDTH);
}

HdTitleBarVisEnum hd_title_bar_get_state(HdTitleBar *bar)
{
  HdTitleBarPrivate *priv;
  if (!HD_IS_TITLE_BAR(bar))
    return 0;
  priv = bar->priv;

  return priv->state;
}

void
hd_title_bar_set_show(HdTitleBar *bar, gboolean show)
{
 /* if (show)
    clutter_actor_show(CLUTTER_ACTOR(bar));
  else
    clutter_actor_hide(CLUTTER_ACTOR(bar));*/
}

static void
hd_title_bar_left_pressed(HdTitleBar *bar, gboolean pressed)
{
  HdTitleBarPrivate *priv;
  if (!HD_IS_TITLE_BAR(bar))
    return;
  priv = bar->priv;

  if (pressed)
    {
      clutter_actor_show(priv->buttons[BTN_BG_LEFT_PRESSED]);
      if (priv->state & HDTB_VIS_BTN_LAUNCHER)
        clutter_actor_show(priv->buttons[BTN_LAUNCHER_PRESSED]);
      if (priv->state & HDTB_VIS_BTN_SWITCHER)
        clutter_actor_show(priv->buttons[BTN_SWITCHER_PRESSED]);
    }
  else
    {
      clutter_actor_hide(priv->buttons[BTN_BG_LEFT_PRESSED]);
      clutter_actor_hide(priv->buttons[BTN_LAUNCHER_PRESSED]);
      clutter_actor_hide(priv->buttons[BTN_SWITCHER_PRESSED]);
    }
}

static void
hd_title_bar_set_full_width(HdTitleBar *bar, gboolean full_size)
{
  HdTitleBarPrivate *priv;
  ClutterActor *status_area;
  if (!HD_IS_TITLE_BAR(bar))
    return;
  priv = bar->priv;

  status_area = hd_render_manager_get_status_area();

  if (full_size)
    {
      clutter_actor_hide(priv->buttons[BTN_BG_LEFT_END]);
      clutter_actor_hide(priv->buttons[BTN_BG_RIGHT_END]);
      clutter_actor_hide(priv->buttons[BTN_BG_ATTACHED]);
      /* set up background image */
      clutter_actor_show(priv->title_bg);
      clutter_actor_set_width(priv->title_bg,
          HD_COMP_MGR_SCREEN_WIDTH);

      /* set up separator positions */
      if (priv->state & HDTB_VIS_BTN_LEFT_MASK)
        {
          clutter_actor_show(priv->buttons[BTN_SEPARATOR_LEFT]);
          clutter_actor_set_x(priv->buttons[BTN_SEPARATOR_LEFT],
              HD_COMP_MGR_TOP_LEFT_BTN_WIDTH -
              clutter_actor_get_width(priv->buttons[BTN_SEPARATOR_LEFT]));
        }
      else
        clutter_actor_hide(priv->buttons[BTN_SEPARATOR_LEFT]);

      if (status_area && CLUTTER_ACTOR_IS_VISIBLE(status_area))
        {
          clutter_actor_show(priv->buttons[BTN_SEPARATOR_STATUS]);
          clutter_actor_set_x(priv->buttons[BTN_SEPARATOR_STATUS],
              HD_COMP_MGR_TOP_LEFT_BTN_WIDTH +
              clutter_actor_get_width(status_area));
        }
      else
        clutter_actor_hide(priv->buttons[BTN_SEPARATOR_STATUS]);

      if (priv->state & HDTB_VIS_BTN_RIGHT_MASK)
        {
          clutter_actor_show(priv->buttons[BTN_SEPARATOR_RIGHT]);
          clutter_actor_set_x(priv->buttons[BTN_SEPARATOR_RIGHT],
              HD_COMP_MGR_SCREEN_WIDTH - HD_COMP_MGR_TOP_LEFT_BTN_WIDTH);
        }
      else
        clutter_actor_hide(priv->buttons[BTN_SEPARATOR_RIGHT]);
    }
  else
    {
      gint left_width = 0;

      if (priv->state & HDTB_VIS_BTN_LEFT_MASK)
        left_width = HD_COMP_MGR_TOP_LEFT_BTN_WIDTH;

      if (status_area && CLUTTER_ACTOR_IS_VISIBLE(status_area))
        {
          left_width += clutter_actor_get_width(status_area);
          clutter_actor_show(priv->buttons[BTN_SEPARATOR_LEFT]);
          clutter_actor_set_x(priv->buttons[BTN_SEPARATOR_LEFT],
              HD_COMP_MGR_TOP_LEFT_BTN_WIDTH);
        }
      else
        clutter_actor_hide(priv->buttons[BTN_SEPARATOR_LEFT]);

      clutter_actor_hide(priv->buttons[BTN_SEPARATOR_STATUS]);
      clutter_actor_hide(priv->buttons[BTN_SEPARATOR_RIGHT]);
      clutter_actor_hide(priv->title_bg);

      /* move the rounded actor to the furthest right we want it
       * (edge of status area or button) */
      clutter_actor_show(priv->buttons[BTN_BG_LEFT_END]);
      clutter_actor_set_x(priv->buttons[BTN_BG_LEFT_END],
          left_width-HD_COMP_MGR_TOP_LEFT_BTN_WIDTH);
      /* Use the 'attached' actor to fill in the gap on the left */
      if (left_width>HD_COMP_MGR_TOP_LEFT_BTN_WIDTH)
        {
          clutter_actor_show(priv->buttons[BTN_BG_ATTACHED]);
          clutter_actor_set_width(priv->buttons[BTN_BG_ATTACHED],
              left_width-HD_COMP_MGR_TOP_LEFT_BTN_WIDTH);
        }
      else
        {
          clutter_actor_hide(priv->buttons[BTN_BG_ATTACHED]);
        }
      /* just put the right actor up if we need it... */
      if (priv->state & HDTB_VIS_BTN_RIGHT_MASK)
        clutter_actor_show(priv->buttons[BTN_BG_RIGHT_END]);
      else
        clutter_actor_hide(priv->buttons[BTN_BG_RIGHT_END]);
    }
}

void
hd_title_bar_right_pressed(HdTitleBar *bar, gboolean pressed)
{
  HdTitleBarPrivate *priv;
  if (!HD_IS_TITLE_BAR(bar))
    return;
  priv = bar->priv;

  if (pressed)
    {
      clutter_actor_show(priv->buttons[BTN_BG_RIGHT_PRESSED]);
      if (priv->state & HDTB_VIS_BTN_BACK)
        clutter_actor_show(priv->buttons[BTN_BACK_PRESSED]);
      if (priv->state & HDTB_VIS_BTN_CLOSE)
        clutter_actor_show(priv->buttons[BTN_CLOSE_PRESSED]);
    }
  else
    {
      clutter_actor_hide(priv->buttons[BTN_BG_RIGHT_PRESSED]);
      clutter_actor_hide(priv->buttons[BTN_BACK_PRESSED]);
      clutter_actor_hide(priv->buttons[BTN_CLOSE_PRESSED]);
    }
}

static void
hd_title_bar_set_window(HdTitleBar *bar, MBWindowManagerClient *client)
{
  MBWMClientType    c_type;
  MBWMDecor         *decor = 0;
  MBWMXmlClient     *c;
  MBWMXmlDecor      *d;
  HdTitleBarVisEnum state;
  gboolean pressed = FALSE;
  MBWMList *l;
  HdTitleBarPrivate *priv;
  if (!HD_IS_TITLE_BAR(bar))
    return;
  priv = bar->priv;

  if (client == NULL)
    {
      /* we have nothing, make sure we're back to normal */
      clutter_actor_hide(CLUTTER_ACTOR(priv->title));
      hd_title_bar_set_state(bar,
          hd_title_bar_get_state(bar) &
            (~(HDTB_VIS_BTN_RIGHT_MASK|HDTB_VIS_FULL_WIDTH)));
      return;
    }

  c_type = MB_WM_CLIENT_CLIENT_TYPE (client);

  if (MB_WM_CLIENT_CLIENT_TYPE (client) != MBWMClientTypeApp)
    {
      g_critical("%s: should only be called on MBWMClientTypeApp", __FUNCTION__);
      return;
    }

  for (l = client->decor;l;l = l->next)
    {
      MBWMDecor *d = l->data;
      if (d->type == MBWMDecorTypeNorth)
        decor = d;
    }

  if (!decor)
    {
      g_critical("%s: No north decor found", __FUNCTION__);
      return;
    }

  if (!((c = mb_wm_xml_client_find_by_type
                    (client->wmref->theme->xml_clients, c_type)) &&
      (d = mb_wm_xml_decor_find_by_type (c->decors, decor->type))))
  return;

  /* add the title */
  const char* title = mb_wm_client_get_name (client);
  if (d->show_title && title && strlen(title)) {
    ClutterActor *status_area;
    char font_name[512];
    guint w,h;
    int x_start = 0;
    if (priv->state & HDTB_VIS_BTN_LEFT_MASK)
      x_start += HD_COMP_MGR_TOP_LEFT_BTN_WIDTH;
    status_area = hd_render_manager_get_status_area();
    if (status_area && CLUTTER_ACTOR_IS_VISIBLE(status_area))
      x_start += clutter_actor_get_width(status_area);

    snprintf (font_name, sizeof (font_name), "%s %i%s",
              d->font_family ? d->font_family : "Sans",
              d->font_size ? d->font_size : 18,
              d->font_units == MBWMXmlFontUnitsPoints ? "" : "px");
    clutter_label_set_font_name(priv->title, font_name);
    clutter_label_set_text(priv->title, title);

    clutter_actor_get_size(CLUTTER_ACTOR(priv->title), &w, &h);
    clutter_actor_set_position(CLUTTER_ACTOR(priv->title),
                               (x_start+decor->geom.width-w)/2,
                               (decor->geom.height-h)/2);
    clutter_actor_show(CLUTTER_ACTOR(priv->title));
  }
  else
    clutter_actor_hide(CLUTTER_ACTOR(priv->title));

  /* Go through all buttons and set the ones visible that are required */
  state = hd_title_bar_get_state(bar) & (~HDTB_VIS_BTN_RIGHT_MASK);

  for (l = MB_WM_DECOR(decor)->buttons;l;l = l->next)
    {
      MBWMDecorButton * button = l->data;
      if (button->type == MBWMDecorButtonClose)
        state |= HDTB_VIS_BTN_CLOSE;
      if (button->type == HdHomeThemeButtonBack)
        state |= HDTB_VIS_BTN_BACK;

      pressed |= button->state != MBWMDecorButtonStateInactive;
    }
  /* Also set us to be full width */
  state |= HDTB_VIS_FULL_WIDTH;

  hd_title_bar_set_state(bar, state);
  hd_title_bar_right_pressed(bar, pressed);
}

void
hd_title_bar_update(HdTitleBar *bar, MBWMCompMgr *wmcm)
{
  MBWindowManagerClient *client = 0;
  if (STATE_IS_APP(hd_render_manager_get_state()))
    {
      /* find the topmost application above the desktop, and use this
       * for setting our title - or have NULL */
      client = wmcm->wm->stack_top;
      while (client)
        {
          MBWMClientType c_type = MB_WM_CLIENT_CLIENT_TYPE(client);
          if (c_type == MBWMClientTypeApp)
            break;
          if (c_type == MBWMClientTypeDesktop)
            {
              client = 0;
              break;
            }
          client = client->stacked_below;
        }
    }
  hd_title_bar_set_window(bar, client);
}

void
hd_title_bar_set_switcher_pulse(HdTitleBar *bar, gboolean pulse)
{
  HdTitleBarPrivate *priv;

  if (!HD_IS_TITLE_BAR(bar))
    return;
  priv = bar->priv;

  if (pulse)
    {
      clutter_timeline_start(priv->switcher_timeline);
    }
  else
    {
      clutter_timeline_stop(priv->switcher_timeline);
      clutter_actor_set_opacity(priv->buttons[BTN_SWITCHER_HIGHLIGHT], 0);
    }
}

/* ------------------------------------------------------------------------- */

static void
on_switcher_timeline_new_frame(ClutterTimeline *timeline,
                               gint frame_num, HdTitleBar *bar)
{
  HdTitleBarPrivate *priv;
  float amt;
  gint opacity;

  if (!HD_IS_TITLE_BAR(bar))
    return;
  priv = bar->priv;

  amt =  (float)clutter_timeline_get_progress(timeline);
  if (priv->state & HDTB_VIS_BTN_SWITCHER)
    {
      opacity = (gint)((1-cos(amt*2*3.141592))*127);
      clutter_actor_set_opacity(priv->buttons[BTN_SWITCHER_HIGHLIGHT], opacity);
    }
}

static void
hd_title_bar_top_left_clicked (HdTitleBar *bar)
{
  g_signal_emit (bar, signals[CLICKED_TOP_LEFT], 0);
}

static void
hd_title_bar_top_left_press (HdTitleBar *bar)
{
  hd_title_bar_left_pressed(bar, TRUE);

  g_signal_emit (bar, signals[PRESS_TOP_LEFT], 0);
}

static void
hd_title_bar_top_left_leave (HdTitleBar *bar)
{
  hd_title_bar_left_pressed(bar, FALSE);

  g_signal_emit (bar, signals[LEAVE_TOP_LEFT], 0);
}

static void
hd_title_bar_add_signals(HdTitleBar *bar, ClutterActor *actor)
{
  clutter_actor_set_reactive(actor, TRUE);
  g_signal_connect_swapped (actor, "button-release-event",
                            G_CALLBACK (hd_title_bar_top_left_clicked),
                            bar);
  g_signal_connect_swapped (actor, "button-press-event",
                            G_CALLBACK (hd_title_bar_top_left_press),
                            bar);
  g_signal_connect_swapped (actor, "leave-event",
                            G_CALLBACK (hd_title_bar_top_left_leave),
                            bar);
}
