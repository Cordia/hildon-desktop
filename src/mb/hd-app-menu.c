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

#include "hd-wm.h"
#include "hd-app-menu.h"

static void
hd_app_menu_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  client->client_type  = HdWmClientTypeAppMenu;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdAppMenu";
#endif
}

static void
hd_app_menu_destroy (MBWMObject *this)
{
}

static int
hd_app_menu_init (MBWMObject *this, va_list vap)
{
  return 1;
}

int
hd_app_menu_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdAppMenuClass),
	sizeof (HdAppMenu),
	hd_app_menu_init,
	hd_app_menu_destroy,
	hd_app_menu_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_DIALOG, 0);
    }

  return type;
}

MBWindowManagerClient*
hd_app_menu_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client
    = MB_WM_CLIENT (mb_wm_object_new (HD_TYPE_APP_MENU,
				      MBWMObjectPropWm,           wm,
				      MBWMObjectPropClientWindow, win,
				      NULL));

  return client;
}

