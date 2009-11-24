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

#ifndef _HAVE_HD_TITLE_BAR_H
#define _HAVE_HD_TITLE_BAR_H

#include <clutter/clutter.h>
#include <matchbox/theme-engines/mb-wm-theme.h>
#include <matchbox/core/mb-wm-decor.h>

typedef struct _HdTitleBar        HdTitleBar;
typedef struct _HdTitleBarClass   HdTitleBarClass;
typedef struct _HdTitleBarPrivate HdTitleBarPrivate;

#define HD_TITLE_BAR(c)       ((HdTitleBar*)(c))
#define HD_TITLE_BAR_CLASS(c) ((HdTitleBarClass*)(c))
#define HD_TYPE_TITLE_BAR     (hd_title_bar_get_type ())
#define HD_IS_TITLE_BAR(c)    (MB_WM_OBJECT_TYPE(c)==HD_TYPE_TITLE_BAR)

struct _HdTitleBar
{
  ClutterGroup     parent;
  HdTitleBarPrivate *priv;
};

struct _HdTitleBarClass
{
  ClutterGroupClass parent;
};

typedef enum {
  HDTB_VIS_NONE            = 0,
  /* LEFT */
  HDTB_VIS_BTN_LAUNCHER    = 1,
  HDTB_VIS_BTN_SWITCHER    = 2,
  HDTB_VIS_BTN_MENU        = 4,
  /* RIGHT */
  HDTB_VIS_BTN_BACK        = 8,
  HDTB_VIS_BTN_CLOSE       = 16,
  HDTB_VIS_BTN_DONE        = 32,

  HDTB_VIS_FULL_WIDTH      = 64,
  HDTB_VIS_BTN_SWITCHER_HIGHLIGHT = 128,

  /* do we want a 'foreground' part shown out the front of the blur?
   * As in when a non-system-modal dialog blurs the background */
  HDTB_VIS_FOREGROUND      = 256,
  /* do we use small buttons (for portrait) or not */
  HDTB_VIS_SMALL_BUTTONS   = 512,

  HDTB_VIS_BTN_LEFT_MASK   = HDTB_VIS_BTN_LAUNCHER |
                             HDTB_VIS_BTN_SWITCHER |
                             HDTB_VIS_BTN_MENU,
  HDTB_VIS_BTN_RIGHT_MASK  = HDTB_VIS_BTN_BACK |
                             HDTB_VIS_BTN_CLOSE |
                             HDTB_VIS_BTN_DONE,
} HdTitleBarVisEnum;

#define HD_TITLE_BAR_PROGRESS_MARGIN (8)

GType hd_title_bar_get_type (void) G_GNUC_CONST;

void hd_title_bar_print_state(HdTitleBar *bar, gboolean current);
void hd_title_bar_set_state(HdTitleBar *bar,
                            HdTitleBarVisEnum button);
HdTitleBarVisEnum hd_title_bar_get_state(HdTitleBar *bar);
void hd_title_bar_update(HdTitleBar *bar);
void hd_title_bar_update_now(HdTitleBar *bar);

void hd_title_bar_set_loading_title   (HdTitleBar *bar,
                                       const char *title);

gboolean
hd_title_bar_is_title_bar_decor(HdTitleBar *bar, MBWMDecor *decor);
/* Get the foreground group - we need to put the status area in this so it
 * gets displayed unblurred when non-system-modal dialogs are up*/
ClutterGroup *
hd_title_bar_get_foreground_group(HdTitleBar *bar);

/* Whether to show pulsing animation for task switcher button
 * (for when notifications arrive) or not */
void
hd_title_bar_set_switcher_pulse(HdTitleBar *bar, gboolean pulse);

void
hd_title_bar_left_pressed(HdTitleBar *bar, gboolean pressed);
void
hd_title_bar_right_pressed(HdTitleBar *bar, gboolean pressed);

ClutterActor *
hd_title_bar_create_fake(gboolean portrait);

void
hd_title_bar_get_xy (HdTitleBar *bar, int *x, int *y);

gint
hd_title_bar_get_button_width(HdTitleBar *bar);

#endif
