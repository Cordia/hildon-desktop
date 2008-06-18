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

#include "hd-desktop.h"
#include "hd-comp-mgr.h"
#include "hd-home-applet.h"
#include <matchbox/theme-engines/mb-wm-theme.h>

static void
hd_desktop_realize (MBWindowManagerClient *client);

static Bool
hd_desktop_request_geometry (MBWindowManagerClient *client,
			     MBGeometry            *new_geometry,
			     MBWMClientReqGeomType  flags);

static MBWMStackLayerType
hd_desktop_stacking_layer (MBWindowManagerClient *client);

static void
hd_desktop_theme_change (MBWindowManagerClient *client);

static void
hd_desktop_stack (MBWindowManagerClient *client, int flags);

static void
hd_desktop_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client     = (MBWindowManagerClientClass *)klass;

  client->client_type    = MBWMClientTypeDesktop;
  client->geometry       = hd_desktop_request_geometry;
  client->stacking_layer = hd_desktop_stacking_layer;
  client->stack          = hd_desktop_stack;
  client->theme_change   = hd_desktop_theme_change;
  client->realize        = hd_desktop_realize;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdDesktop";
#endif
}

static void
hd_desktop_destroy (MBWMObject *this)
{
}

static int
hd_desktop_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient    *client = MB_WM_CLIENT (this);
  MBWindowManager          *wm = NULL;
  MBGeometry                geom;

  wm = client->wmref;

  if (!wm)
    return 0;

  client->stacking_layer = MBWMStackLayerBottom;

  mb_wm_client_set_layout_hints (client,
				 LayoutPrefFullscreen|LayoutPrefVisible);

  /*
   * Initialize window geometry, so that the frame size is correct
   */
  geom.x      = 0;
  geom.y      = 0;
  geom.width  = wm->xdpy_width;
  geom.height = wm->xdpy_height;

  hd_desktop_request_geometry (client, &geom,
					 MBWMClientReqGeomForced);

  return 1;
}

int
hd_desktop_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdDesktopClass),
	sizeof (HdDesktop),
	hd_desktop_init,
	hd_desktop_destroy,
	hd_desktop_class_init
      };
      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_BASE, 0);
    }

  return type;
}

static Bool
hd_desktop_request_geometry (MBWindowManagerClient *client,
			     MBGeometry            *new_geometry,
			     MBWMClientReqGeomType  flags)
{
  if (flags & (MBWMClientReqGeomIsViaLayoutManager|MBWMClientReqGeomForced))
    {
      client->frame_geometry.x      = new_geometry->x;
      client->frame_geometry.y      = new_geometry->y;
      client->frame_geometry.width  = new_geometry->width;
      client->frame_geometry.height = new_geometry->height;

      mb_wm_client_geometry_mark_dirty (client);

      return True; /* Geometry accepted */
    }
  return False;
}

static MBWMStackLayerType
hd_desktop_stacking_layer (MBWindowManagerClient *client)
{
  if (client->wmref->flags & MBWindowManagerFlagDesktop)
    return MBWMStackLayerMid;

  return MBWMStackLayerBottom;
}

static void
hd_desktop_theme_change (MBWindowManagerClient *client)
{
}

static void
hd_desktop_realize (MBWindowManagerClient *client)
{
  /*
   * Must reparent the window to our root, otherwise we restacking of
   * pre-existing windows might fail.
   */
  printf ("#### realizing desktop\n ####");

  XReparentWindow(client->wmref->xdpy, MB_WM_CLIENT_XWIN(client),
		  client->wmref->root_win->xwindow, 0, 0);

  return;
}

static void
hd_desktop_stack (MBWindowManagerClient *client,
		  int                    flags)
{
  /* Stack to highest/lowest possible possition in stack */
  MBWMList * l = mb_wm_client_get_transients (client);
  HdCompMgr *hmgr = HD_COMP_MGR (client->wmref->comp_mgr);
  gint       current_view = hd_comp_mgr_get_current_home_view_id (hmgr);

  mb_wm_stack_move_top(client);

  while (l)
    {
      MBWindowManagerClient *c = l->data;
      if (HD_IS_HOME_APPLET (c))
	{
	  HdHomeApplet *applet = HD_HOME_APPLET (c);

	  if (applet->view_id < 0 || applet->view_id == current_view)
	    mb_wm_client_stack (c, flags);
	}
      else
	mb_wm_client_stack (c, flags);

      l = l->next;
    }

  mb_wm_util_list_free (l);
}

MBWindowManagerClient*
hd_desktop_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client = MB_WM_CLIENT(mb_wm_object_new (HD_TYPE_DESKTOP,
					  MBWMObjectPropWm,           wm,
					  MBWMObjectPropClientWindow, win,
					  NULL));

  return client;
}

