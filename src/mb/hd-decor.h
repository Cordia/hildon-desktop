/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Thomas Thurman <thomas.thurman@collabora.co.uk>
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

#ifndef _HAVE_HD_DECOR_H
#define _HAVE_HD_DECOR_H

#include <matchbox/mb-wm-config.h>
#include <matchbox/core/mb-window-manager.h>
#include <matchbox/core/mb-wm-decor.h>
#include <clutter/clutter.h>

typedef struct HdDecorClass   HdDecorClass;
typedef struct HdDecor        HdDecor;

#define HD_DECOR(c)       ((HdDecor*)(c))
#define HD_DECOR_CLASS(c) ((HdDecorClass*)(c))
#define HD_TYPE_DECOR     (hd_decor_class_type ())
#define HD_IS_DECOR(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_DECOR))

struct HdDecorClass
{
  MBWMDecorClass    parent;
};

struct HdDecor
{
  MBWMDecor     parent;


  /* private? */
  ClutterActor          *title_bar_actor;
  ClutterActor          *title_actor;
  ClutterActor          *progress_texture;
  ClutterTimeline       *progress_timeline;
};

int hd_decor_class_type (void);

HdDecor* hd_decor_new (MBWindowManager      *wm,
                       MBWMDecorType         type);

void hd_decor_sync(HdDecor   *decor);

gboolean
hd_decor_window_is_waiting (MBWindowManager *wm, Window w);
gboolean
hd_decor_window_has_menu_indicator (MBWindowManager *wm, Window w);

#endif
