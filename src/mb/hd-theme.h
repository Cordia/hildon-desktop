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
#include <matchbox/theme-engines/mb-wm-theme-png.h>

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
  MBWMThemePngClass    parent;

};

struct HdTheme
{
  MBWMThemePng     parent;
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
