/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2010 Moises Martinez
 *
 * Author:  Moises Martinez <moimart@gmail.com>
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

#ifndef _HAVE_HD_KEYBOARD_H
#define _HAVE_HD_KEYBOARD_H

#include <matchbox/core/mb-wm.h>
#include <matchbox/core/mb-wm-client-base.h>

typedef struct HdKeyboard      HdKeyboard;
typedef struct HdKeyboardClass HdKeyboardClass;

#define HD_KEYBOARD(c) ((HdKeyboard*)(c))
#define HD_KEYBOARD_CLASS(c) ((HdKeyboardClass*)(c))
#define HD_TYPE_KEYBOARD (hd_keyboard_class_type ())
#define HD_IS_KEYBOARD(c) (MB_WM_OBJECT_TYPE(c) == HD_TYPE_KEYBOARD)

struct HdKeyboard
{
  MBWMClientBase  parent;
};

struct HdKeyboardClass
{
  MBWMClientBaseClass parent;
};

MBWindowManagerClient* hd_keyboard_new (MBWindowManager *wm,
                                           MBWMClientWindow *win);

int hd_keyboard_class_type (void);

#endif
