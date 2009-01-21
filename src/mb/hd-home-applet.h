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

#ifndef _HAVE_HD_HOME_APPLET_H
#define _HAVE_HD_HOME_APPLET_H

#include <matchbox/core/mb-wm.h>

typedef struct HdHomeApplet      HdHomeApplet;
typedef struct HdHomeAppletClass HdHomeAppletClass;

#define HD_HOME_APPLET(c) ((HdHomeApplet*)(c))
#define HD_HOME_APPLET_CLASS(c) ((HdHomeAppletClass*)(c))
#define HD_TYPE_HOME_APPLET (hd_home_applet_class_type ())
#define HD_IS_HOME_APPLET(c) (MB_WM_OBJECT_TYPE(c)==HD_TYPE_HOME_APPLET)

struct HdHomeApplet
{
  MBWMClientBase    parent;

  MBWMDecorButton  *button_close;
  unsigned int      view_id;
  unsigned int      applet_layer;

  char             *applet_id;
  time_t            modified;
  unsigned int      settings;
};

struct HdHomeAppletClass
{
  MBWMClientBaseClass parent;
};

MBWindowManagerClient*
hd_home_applet_new (MBWindowManager *wm, MBWMClientWindow *win);

int
hd_home_applet_class_type (void);

#endif
