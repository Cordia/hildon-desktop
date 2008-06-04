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

#include "hd-home-applet.h"
#include "hd-wm.h"
#include "hd-util.h"
#include "hd-comp-mgr.h"

#include <matchbox/theme-engines/mb-wm-theme.h>

static Bool
hd_home_applet_request_geometry (MBWindowManagerClient *client,
				 MBGeometry            *new_geometry,
				 MBWMClientReqGeomType  flags);

static void
hd_home_applet_theme_change (MBWindowManagerClient *client);

static void
hd_home_applet_show (MBWindowManagerClient *client)
{
  MBWindowManagerClientClass  *parent_klass = NULL;

  parent_klass = MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS (client));

  if (parent_klass && parent_klass->show)
    parent_klass->show(client);
}

static void
hd_home_applet_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  client->client_type  = HdWmClientTypeHomeApplet;
  client->geometry     = hd_home_applet_request_geometry;
  client->show         = hd_home_applet_show;
  client->theme_change = hd_home_applet_theme_change;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdHomeApplet";
#endif
}

static void
hd_home_applet_destroy (MBWMObject *this)
{
}

static int
hd_home_applet_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient *client = MB_WM_CLIENT (this);
  HdHomeApplet          *applet = HD_HOME_APPLET (this);
  MBWindowManager       *wm = client->wmref;
  MBWMClientWindow      *win = client->window;
  HdCompMgr             *hmgr = HD_COMP_MGR (wm->comp_mgr);
  MBGeometry             geom;
  int                    n, s, w, e;
  int                   *view_id;
  Atom                   view_id_atom;
  int                    n_items;

  Atom actions[] = {
    wm->atoms[MBWM_ATOM_NET_WM_ACTION_CLOSE],
    wm->atoms[MBWM_ATOM_NET_WM_ACTION_MOVE],
  };

  view_id_atom = hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_HOME_VIEW);

  view_id =
    hd_util_get_win_prop_data_and_validate (wm->xdpy,
					    win->xwindow,
					    view_id_atom,
					    XA_CARDINAL,
					    32,
					    1,
					    &n_items);

  if (view_id)
    {
      applet->view_id = *view_id;
      XFree (view_id);
    }

  XChangeProperty (wm->xdpy, win->xwindow,
		   wm->atoms[MBWM_ATOM_NET_WM_ALLOWED_ACTIONS],
		   XA_ATOM, 32, PropModeReplace,
		   (unsigned char *)actions,
		   sizeof (actions)/sizeof (actions[0]));


  mb_wm_client_set_layout_hints (client,
				 LayoutPrefPositionFree |
				 LayoutPrefMovable      |
				 LayoutPrefVisible);

  if (!client->window->undecorated)
    {
      mb_wm_theme_create_decor (wm->theme, client, MBWMDecorTypeNorth);
      mb_wm_theme_create_decor (wm->theme, client, MBWMDecorTypeSouth);
      mb_wm_theme_create_decor (wm->theme, client, MBWMDecorTypeWest);
      mb_wm_theme_create_decor (wm->theme, client, MBWMDecorTypeEast);
    }

  client->stacking_layer = MBWMStackLayerBottomMid;


  mb_wm_client_geometry_mark_dirty (client);
  mb_wm_client_visibility_mark_dirty (client);

  if (!wm->theme)
    return 1;

  /*
   * Since applets are free-sized, they do not necessarily get a request for
   * geometry from the layout manager -- we have to set the initial geometry
   * here
   */
  mb_wm_theme_get_decor_dimensions (wm->theme, client, &n, &s, &w, &e);

  geom.x      = client->window->geometry.x;
  geom.y      = client->window->geometry.y;
  geom.width  = client->window->geometry.width + w + e;
  geom.height = client->window->geometry.height + n + s;

  hd_home_applet_request_geometry (client, &geom, MBWMClientReqGeomForced);

  return 1;
}

int
hd_home_applet_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdHomeAppletClass),
	sizeof (HdHomeApplet),
	hd_home_applet_init,
	hd_home_applet_destroy,
	hd_home_applet_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_BASE, 0);
    }

  return type;
}

static Bool
hd_home_applet_request_geometry (MBWindowManagerClient *client,
				 MBGeometry            *new_geometry,
				 MBWMClientReqGeomType  flags)
{
  const MBGeometry * geom;
  Bool               change_pos;
  Bool               change_size;

  /*
   * When we get an internal geometry request, like from the layout manager,
   * the new geometry applies to the frame; however, if the request is
   * external from ConfigureRequest, it is new geometry of the client window,
   * so we need to take care to handle this right.
   */
  geom = (flags & MBWMClientReqGeomIsViaConfigureReq) ?
    &client->window->geometry : &client->frame_geometry;

  change_pos = (geom->x != new_geometry->x || geom->y != new_geometry->y);

  change_size = (geom->width  != new_geometry->width ||
		 geom->height != new_geometry->height);

  if (change_size)
    {
      int north = 0, south = 0, west = 0, east = 0;
      MBWindowManager *wm = client->wmref;

      if (client->decor)
	mb_wm_theme_get_decor_dimensions (wm->theme, client,
					  &north, &south, &west, &east);

      if (flags & MBWMClientReqGeomIsViaConfigureReq)
	{
	  /*
	   * Calculate the frame size from the window size
	   */
	  MBWM_DBG ("ConfigureRequest [%d,%d;%dx%d] -> [%d,%d;%dx%d]\n",
		    client->window->geometry.x,
		    client->window->geometry.y,
		    client->window->geometry.width,
		    client->window->geometry.height,
		    new_geometry->x,
		    new_geometry->y,
		    new_geometry->width,
		    new_geometry->height);

	  client->window->geometry.x      = new_geometry->x;
	  client->window->geometry.y      = new_geometry->y;
	  client->window->geometry.width  = new_geometry->width;
	  client->window->geometry.height = new_geometry->height;

	  client->frame_geometry.x
	    = client->window->geometry.x - west;
	  client->frame_geometry.y
	    = client->window->geometry.y - north;
	  client->frame_geometry.width
	    = client->window->geometry.width + (west + east);
	  client->frame_geometry.height
	    = client->window->geometry.height + (south + north);
	}
      else
	{
	  /*
	   * Internal request, e.g., from layout manager; work out client
	   * window size from the provided frame size.
	   */
	  client->frame_geometry.x      = new_geometry->x;
	  client->frame_geometry.y      = new_geometry->y;
	  client->frame_geometry.width  = new_geometry->width;
	  client->frame_geometry.height = new_geometry->height;

	  client->window->geometry.x
	    = client->frame_geometry.x + west;
	  client->window->geometry.y
	    = client->frame_geometry.y + north;
	  client->window->geometry.width
	    = client->frame_geometry.width - (west + east);
	  client->window->geometry.height
	    = client->frame_geometry.height - (south + north);
	}

      mb_wm_client_geometry_mark_dirty (client);

      return True; /* Geometry accepted */
    }
  else if (change_pos)
    {
      /*
       * Change of position only, just move both windows, no need to
       * mess about with the decor.
       */
      int x_diff = geom->x - new_geometry->x;
      int y_diff = geom->y - new_geometry->y;

      client->frame_geometry.x -= x_diff;
      client->frame_geometry.y -= y_diff;
      client->window->geometry.x -= x_diff;
      client->window->geometry.y -= y_diff;

      mb_wm_client_geometry_mark_dirty (client);

      return True;
    }

  return True; /* Geometry accepted */
}

static void
hd_home_applet_theme_change (MBWindowManagerClient *client)
{
  MBWMList * l = client->decor;

  while (l)
    {
      MBWMDecor * d = l->data;
      MBWMList * n = l->next;

      mb_wm_object_unref (MB_WM_OBJECT (d));
      free (l);

      l = n;
    }

  client->decor = NULL;

  if (!client->window->undecorated)
    {
      mb_wm_theme_create_decor (client->wmref->theme,
				client, MBWMDecorTypeNorth);

      mb_wm_theme_create_decor (client->wmref->theme,
				client, MBWMDecorTypeSouth);

      mb_wm_theme_create_decor (client->wmref->theme,
				client, MBWMDecorTypeWest);

      mb_wm_theme_create_decor (client->wmref->theme,
				client, MBWMDecorTypeEast);
    }

  mb_wm_client_geometry_mark_dirty (client);
  mb_wm_client_visibility_mark_dirty (client);
}


MBWindowManagerClient*
hd_home_applet_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client
    = MB_WM_CLIENT(mb_wm_object_new (HD_TYPE_HOME_APPLET,
				     MBWMObjectPropWm,           wm,
				     MBWMObjectPropClientWindow, win,
				     NULL));

  return client;
}

