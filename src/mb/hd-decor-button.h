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

#ifndef _HAVE_HD_DECOR_BUTTON_H
#define _HAVE_HD_DECOR_BUTTON_H

#include <matchbox/mb-wm-config.h>
#include <matchbox/core/mb-window-manager.h>
#include <matchbox/core/mb-wm-decor.h>
#include <clutter/clutter.h>

typedef struct HdDecorButtonClass   HdDecorButtonClass;
typedef struct HdDecorButton        HdDecorButton;

#define HD_DECOR_BUTTON(c)       ((HdDecorButton*)(c))
#define HD_DECOR_BUTTON_CLASS(c) ((HdDecorButtonClass*)(c))
#define HD_TYPE_DECOR_BUTTON     (hd_decor_button_class_type ())
#define HD_IS_DECOR_BUTTON(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_DECOR_BUTTON))

struct HdDecorButtonClass
{
  MBWMDecorClass    parent;
};

struct HdDecorButton
{
  MBWMDecorButton     parent;
};

int hd_decor_button_class_type (void);

HdDecorButton* hd_decor_button_new (MBWindowManager               *wm,
                                    MBWMDecorButtonType            type,
                                    MBWMDecorButtonPack            pack,
                                    HdDecor                       *decor,
                                    MBWMDecorButtonPressedFunc     press,
                                    MBWMDecorButtonReleasedFunc    release,
                                    MBWMDecorButtonFlags           flags);

void
hd_decor_button_sync(HdDecorButton *button);


#endif
