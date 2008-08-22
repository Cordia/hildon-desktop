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

#include "hd-status-area.h"
#include "hd-comp-mgr.h"

#include <matchbox/theme-engines/mb-wm-theme.h>
#include <matchbox/theme-engines/mb-wm-theme-xml.h>

static Bool hd_status_area_request_geometry (MBWindowManagerClient *client,
				      MBGeometry            *new_geometry,
				      MBWMClientReqGeomType  flags);

static void
hd_status_area_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  client->client_type  = MBWMClientTypeNote;
  client->geometry     = hd_status_area_request_geometry;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdStatusArea";
#endif
}

static void
hd_status_area_destroy (MBWMObject *this)
{
}

static int
hd_status_area_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient *client = MB_WM_CLIENT (this);
  MBGeometry             geom;

  geom.x      = 112;
  geom.y      = 0;
  geom.width  = 112;
  geom.height = 56;

  hd_status_area_request_geometry (client, &geom, MBWMClientReqGeomForced);

  return 1;
}

int
hd_status_area_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMClientNoteClass),
	sizeof (MBWMClientNote),
	hd_status_area_init,
	hd_status_area_destroy,
	hd_status_area_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_NOTE, 0);
    }

  return type;
}

static Bool
hd_status_area_request_geometry (MBWindowManagerClient *client,
			  MBGeometry            *new_geometry,
			  MBWMClientReqGeomType  flags)
{
  client->frame_geometry.x = new_geometry->x;
  client->frame_geometry.y = new_geometry->y;
  client->frame_geometry.width  = new_geometry->width;
  client->frame_geometry.height = new_geometry->height;
  client->window->geometry.x = new_geometry->x;
  client->window->geometry.y = new_geometry->y;
  client->window->geometry.width  = new_geometry->width;
  client->window->geometry.height = new_geometry->height;

  mb_wm_client_geometry_mark_dirty (client);

  return True; /* Geometry accepted */
}

MBWindowManagerClient*
hd_status_area_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client
    = MB_WM_CLIENT (mb_wm_object_new (HD_TYPE_STATUS_AREA,
				      MBWMObjectPropWm,           wm,
				      MBWMObjectPropClientWindow, win,
				      NULL));

  return client;
}

