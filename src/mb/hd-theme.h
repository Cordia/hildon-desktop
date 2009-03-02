/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Johan Bilien <johan.bilien@nokia.com>
 *          Tomas Frydrych <tf@o-hand.com>
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

#ifndef _HAVE_HD_THEME_H
#define _HAVE_HD_THEME_H

#include <matchbox/mb-wm-config.h>
#include <matchbox/theme-engines/mb-wm-theme.h>

/* ------------------------------------------------------------------------- */
#define HD_THEME_IMG_TASK_LAUNCHER "wmTaskLauncherIcon.png"
#define HD_THEME_IMG_TASK_SWITCHER "wmTaskSwitcherIcon.png"
#define HD_THEME_IMG_TASK_LAUNCHER_PRESSED "wmTaskLauncherIconPressed.png"
#define HD_THEME_IMG_TASK_SWITCHER_PRESSED "wmTaskSwitcherIconPressed.png"
#define HD_THEME_IMG_TASK_SWITCHER_HIGHLIGHT "wmTaskSwitcherIconHighlight.png"
#define HD_THEME_IMG_LEFT_ATTACHED "wmLeftButtonAttached.png"
#define HD_THEME_IMG_LEFT_END "wmLeftButtonEnd.png"
#define HD_THEME_IMG_LEFT_PRESSED "wmLeftButtonEndPressed.png"
#define HD_THEME_IMG_LEFT_ATTACHED_PRESSED "wmLeftButtonAttachedPressed.png"

#define HD_THEME_IMG_RIGHT_END "wmRightButton.png"
#define HD_THEME_IMG_RIGHT_PRESSED "wmRightButtonPressed.png"
#define HD_THEME_IMG_BACK "wmBackIcon.png"
#define HD_THEME_IMG_BACK_PRESSED "wmBackIconPressed.png"
#define HD_THEME_IMG_CLOSE "wmCloseIcon.png"
#define HD_THEME_IMG_CLOSE_PRESSED "wmCloseIconPressed.png"

#define HD_THEME_IMG_TITLE_BAR "wmTitleBar.png"
#define HD_THEME_IMG_DIALOG_BAR "wmDialog.png"

#define HD_THEME_IMG_SEPARATOR "wmSeparator.png"

#define HD_THEME_IMG_PROGRESS "wmProgressIndicator.png"
#define HD_THEME_IMG_PROGRESS_SIZE 48 /* width/height of frame */
#define HD_THEME_IMG_PROGRESS_FRAMES 8 /*frames in animation */
#define HD_THEME_IMG_PROGRESS_FPS 5 /*frames per second */

/* Edit Button */
#define HD_THEME_IMG_EDIT_ICON "wmEditIcon.png"
#define HD_THEME_IMG_BUTTON_LEFT_HALF "wmButtonLeftHalf.png"
#define HD_THEME_IMG_BUTTON_RIGHT_HALF "wmButtonRightHalf.png"

/* ------------------------------------------------------------------------- */

typedef struct HdThemeClass   HdThemeClass;
typedef struct HdTheme        HdTheme;
typedef struct HdThemePrivate HdThemePrivate;

typedef enum _HdThemeButtomType
{
  HdHomeThemeButtonBack = MBWMDecorButtonHelp + 1,
}HdThemeButtonType;

#define HD_THEME(c)       ((HdTheme*)(c))
#define HD_THEME_CLASS(c) ((HdThemeClass*)(c))
#define HD_TYPE_THEME     (hd_theme_class_type ())

struct HdThemeClass
{
  MBWMThemeClass    parent;

};

struct HdTheme
{
  MBWMTheme     parent;
};

int hd_theme_class_type (void);

MBWMTheme * hd_theme_alloc_func (int theme_type, ...);

/* This is a simple test theme that implements the features of
 * of the real theme engine, but is based on the simple, rather than
 * png them.
 */
typedef struct HdThemeSimpleClass   HdThemeSimpleClass;
typedef struct HdThemeSimple        HdThemeSimple;
typedef struct HdThemeSimplePrivate HdThemeSimplePrivate;

#define HD_THEME_SIMPLE(c)       ((HdThemeSimple*)(c))
#define HD_THEME_SIMPLE_CLASS(c) ((HdThemeSimpleClass*)(c))
#define HD_TYPE_THEME_SIMPLE     (hd_theme_simple_class_type ())

struct HdThemeSimpleClass
{
  MBWMThemeClass    parent;

};

struct HdThemeSimple
{
  MBWMTheme     parent;
};

int hd_theme_simple_class_type (void);

#endif
