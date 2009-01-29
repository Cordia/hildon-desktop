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

GType hd_title_bar_get_type (void) G_GNUC_CONST;

void
hd_title_bar_set_theme(HdTitleBar *bar, MBWMTheme *theme);

void
hd_title_bar_set_show(HdTitleBar *bar, gboolean show);

#endif
