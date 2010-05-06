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
#include <matchbox/theme-engines/mb-wm-theme.h>

static void
hd_app_menu_realize (MBWindowManagerClient *client);
static Bool
hd_app_menu_request_geometry (MBWindowManagerClient *client,
                              MBGeometry            *new_geometry,
                              MBWMClientReqGeomType  flags);

static void
hd_app_menu_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  client->client_type  = HdWmClientTypeAppMenu;
  client->geometry = hd_app_menu_request_geometry;
  client->realize  = hd_app_menu_realize;

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
  MBWindowManagerClient *client = MB_WM_CLIENT (this);
  MBWindowManager       *wm = client->wmref;
  MBWindowManagerClient *c;

  for (c = wm->stack_top; c; c = c->stacked_below)
    {
      if (MB_WM_CLIENT_CLIENT_TYPE (c) == HdWmClientTypeAppMenu ||
          MB_WM_CLIENT_CLIENT_TYPE (c) == MBWMClientTypeMenu)
        {
          mb_wm_client_deliver_delete (c);
        }
    }

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
  MBWindowManager *wm = client->wmref;
  int north = 0, south = 0, west = 0, east = 0;
  MBGeometry wm_geometry, frame_geometry;

  mb_wm_get_display_geometry (wm, &wm_geometry, True);
  if (client->decor && !client->window->undecorated)
      mb_wm_theme_get_decor_dimensions (wm->theme, client,
                                        &north, &south, &west, &east);

  /* Calculate the frame geometry, whose width is fixed,
   * horizontal position is centered and aligned to the top. */
  frame_geometry.y = 0;
  frame_geometry.height = new_geometry->height + north + south;
  frame_geometry.x = 1; /* one pixel border drawn by %HdDecor */
  if (wm_geometry.width > wm_geometry.height)
    frame_geometry.x += 56; /* the specified margin in landscape */
  frame_geometry.width = wm_geometry.width - 2*frame_geometry.x;

  /* Calculate window size from frame */
  client->window->geometry.x      = frame_geometry.x + west;
  client->window->geometry.y      = frame_geometry.y + north;
  client->window->geometry.width  = frame_geometry.width - (west + east);
  client->window->geometry.height = frame_geometry.height - (south + north);
  client->frame_geometry          = frame_geometry;

  mb_wm_client_geometry_mark_dirty (client);
  return True; /* Geometry accepted */
}
