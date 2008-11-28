/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Tomas Frydrych <tf@o-hand.com>
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

#ifndef _HAVE_HD_APP_MENU_H
#define _HAVE_HD_APP_MENU_H

#include <matchbox/core/mb-wm.h>
#include <matchbox/client-types/mb-wm-client-menu.h>

typedef struct HdAppMenu      HdAppMenu;
typedef struct HdAppMenuClass HdAppMenuClass;

#define HD_APP_MENU(c) ((HdAppMenu*)(c))
#define HD_APP_MENU_CLASS(c) ((HdAppMenuClass*)(c))
#define HD_TYPE_APP_MENU (hd_app_menu_class_type ())
#define HD_IS_APP_MENU(c) (MB_WM_OBJECT_TYPE(c)==HD_TYPE_APP_MENU)

struct HdAppMenu
{
  MBWMClientMenu  parent;
};

struct HdAppMenuClass
{
  MBWMClientMenuClass parent;
};

MBWindowManagerClient* hd_app_menu_new (MBWindowManager *wm,
					MBWMClientWindow *win);

int hd_app_menu_class_type (void);

#endif
