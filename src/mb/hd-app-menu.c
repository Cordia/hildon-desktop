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
#include "hd-util.h"

static Bool
hd_app_menu_request_geometry (MBWindowManagerClient *client,
			      MBGeometry            *new_geometry,
			      MBWMClientReqGeomType  flags);

static void
hd_app_menu_realize (MBWindowManagerClient *client);

static void
hd_app_menu_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  client->client_type  = HdWmClientTypeAppMenu;
  client->realize  = hd_app_menu_realize;
  client->geometry = hd_app_menu_request_geometry;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdAppMenu";
#endif
}

static void
hd_app_menu_destroy (MBWMObject *this)
{
  HdAppMenu             *menu = HD_APP_MENU (this);
  MBWindowManagerClient *c      = MB_WM_CLIENT (this);
  MBWindowManager       *wm     = c->wmref;

  mb_wm_main_context_x_event_handler_remove (wm->main_ctx, ButtonRelease,
					     menu->release_cb_id);
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

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_MENU, 0);
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

static void
hd_app_menu_realize (MBWindowManagerClient *client)
{
  MBWindowManagerClientClass  *parent_klass = NULL;
  HdAppMenu                   *menu = HD_APP_MENU (client);

  parent_klass = MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS (client));

  if (parent_klass->realize)
    parent_klass->realize (client);

  menu->release_cb_id = hd_util_modal_blocker_realize (client, FALSE);
}

static Bool
hd_app_menu_request_geometry (MBWindowManagerClient *client,
			      MBGeometry            *new_geometry,
			      MBWMClientReqGeomType  flags)
{
  client->window->geometry.x      = new_geometry->x;
  client->window->geometry.y      = new_geometry->y;
  client->window->geometry.width  = new_geometry->width;
  client->window->geometry.height = new_geometry->height;

  client->frame_geometry.x        = new_geometry->x;
  client->frame_geometry.y        = new_geometry->y;
  client->frame_geometry.width    = new_geometry->width;
  client->frame_geometry.height   = new_geometry->height;

  mb_wm_client_geometry_mark_dirty (client);

  return True; /* Geometry accepted */

}
