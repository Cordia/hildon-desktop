/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Authors:  Tomas Frydrych <tf@o-hand.com>
 *           Kimmo Hämäläinen <kimmo.hamalainen@nokia.com>
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

#ifndef _HAVE_HD_STATUS_AREA_H
#define _HAVE_HD_STATUS_AREA_H

#include <matchbox/core/mb-wm.h>
#include <matchbox/client-types/mb-wm-client-note.h>

typedef struct HdStatusArea      HdStatusArea;
typedef struct HdStatusAreaClass HdStatusAreaClass;

#define HD_STATUS_AREA(c) ((HdStatusArea*)(c))
#define HD_STATUS_AREA_CLASS(c) ((HdStatusAreaClass*)(c))
#define HD_TYPE_STATUS_AREA (hd_status_area_class_type ())
#define HD_IS_STATUS_AREA(c) (MB_WM_OBJECT_TYPE(c) == HD_TYPE_STATUS_AREA)

struct HdStatusArea
{
  MBWMClientNote  parent;
};

struct HdStatusAreaClass
{
  MBWMClientNoteClass parent;
};

MBWindowManagerClient* hd_status_area_new (MBWindowManager *wm,
                                           MBWMClientWindow *win);

int hd_status_area_class_type (void);

#endif
