/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Authors:  Tomas Frydrych <tf@o-hand.com>
 *           Kimmo H�m�l�inen <kimmo.hamalainen@nokia.com>
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

#include "hd-status-menu.h"
#include "hd-comp-mgr.h"
#include "hd-wm.h"

#include <matchbox/theme-engines/mb-wm-theme.h>
#include <matchbox/theme-engines/mb-wm-theme-xml.h>

static void
hd_status_menu_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  client->client_type  = HdWmClientTypeStatusMenu;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdStatusMenu";
#endif
}

static void
hd_status_menu_destroy (MBWMObject *this)
{
}

static int
hd_status_menu_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient *client = MB_WM_CLIENT (this);

  mb_wm_client_set_layout_hints (client, LayoutPrefFixedX |
                                 LayoutPrefVisible);

  client->stacking_layer = MBWMStackLayerTopMid;

  return 1;
}

int
hd_status_menu_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMClientNoteClass),
	sizeof (MBWMClientNote),
	hd_status_menu_init,
	hd_status_menu_destroy,
	hd_status_menu_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_NOTE, 0);
    }

  return type;
}

MBWindowManagerClient*
hd_status_menu_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client
    = MB_WM_CLIENT (mb_wm_object_new (HD_TYPE_STATUS_MENU,
				      MBWMObjectPropWm,           wm,
				      MBWMObjectPropClientWindow, win,
				      NULL));

  return client;
}

