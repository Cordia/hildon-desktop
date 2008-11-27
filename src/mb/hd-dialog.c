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

#include "hd-dialog.h"
#include <matchbox/theme-engines/mb-wm-theme.h>

static Bool
hd_dialog_request_geometry (MBWindowManagerClient *client,
			    MBGeometry            *new_geometry,
			    MBWMClientReqGeomType  flags);

static void
hd_dialog_realize (MBWindowManagerClient *client);

static void
hd_dialog_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  client->realize  = hd_dialog_realize;
  client->geometry = hd_dialog_request_geometry;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdDialog";
#endif
}

static void
hd_dialog_destroy (MBWMObject *this)
{
  HdDialog              *dialog = HD_DIALOG (this);
  MBWindowManagerClient *c      = MB_WM_CLIENT (this);
  MBWindowManager       *wm     = c->wmref;

  mb_wm_main_context_x_event_handler_remove (wm->main_ctx, ButtonRelease,
					     dialog->release_cb_id);
}

static int
hd_dialog_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient *client = MB_WM_CLIENT (this);
  MBGeometry             geom;
  int                    n, s, w, e;
  MBWindowManager       *wm = client->wmref;

  /*
   * Fix up the hints; hildon dialogs are not movable.
   */
  mb_wm_client_set_layout_hints (client,
				 LayoutPrefPositionFree |
				 LayoutPrefVisible);

  if (!wm->theme)
    return 1;

  /*
   * Since dialogs are free-sized, they do not necessarily get a request for
   * geometry from the layout manager -- we have to set the initial geometry
   * here.
   */
  mb_wm_theme_get_decor_dimensions (wm->theme, client, &n, &s, &w, &e);

  geom.x      = 0;
  geom.width  = wm->xdpy_width;
  geom.height = client->window->geometry.height + n + s;
  geom.y      = wm->xdpy_height - geom.height;

  /*
   * Our request geometry function only makes actual change if the height
   * of the client has changed -- force change by reseting the frame height.
   */
  client->frame_geometry.height = 0;

  hd_dialog_request_geometry (client, &geom, MBWMClientReqGeomForced);

  return 1;
}

int
hd_dialog_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdDialogClass),
	sizeof (HdDialog),
	hd_dialog_init,
	hd_dialog_destroy,
	hd_dialog_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_DIALOG, 0);
    }

  return type;
}

static Bool
hd_dialog_release_handler (XButtonEvent    *xev,
			   void            *userdata)
{
  MBWindowManagerClient *c = userdata;

  g_debug ("%s: c %p", __FUNCTION__, c);
  mb_wm_client_deliver_delete (c);
  return False;
}

static void
hd_dialog_realize (MBWindowManagerClient *client)
{
  MBWindowManagerClientClass  *parent_klass = NULL;
  HdDialog                    *dialog = HD_DIALOG (client);
  MBWindowManager             *wm = client->wmref;

  parent_klass = MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS (client));

  if (parent_klass->realize)
    parent_klass->realize (client);

  if (!client->xwin_modal_blocker)
    {
      XSetWindowAttributes   attr;

      attr.override_redirect = True;
      attr.event_mask        = MBWMChildMask|ButtonPressMask|ButtonReleaseMask|
	                       ExposureMask;

      client->xwin_modal_blocker =
	XCreateWindow (wm->xdpy,
		       wm->root_win->xwindow,
		       0, 0,
		       wm->xdpy_width,
		       wm->xdpy_height,
		       0,
		       CopyFromParent,
		       InputOnly,
		       CopyFromParent,
		       CWOverrideRedirect|CWEventMask,
		       &attr);
      mb_wm_rename_window (wm, client->xwin_modal_blocker, "hdmodalblocker");
      g_debug ("%s: created modal blocker %lx", __FUNCTION__, client->xwin_modal_blocker);
    }
  else
    {
      /* make sure ButtonRelease is caught */
      XWindowAttributes attrs = { 0 };
      XGetWindowAttributes (wm->xdpy, client->xwin_modal_blocker, &attrs);
      attrs.your_event_mask |= ButtonReleaseMask;
      XSelectInput (wm->xdpy, client->xwin_modal_blocker, attrs.your_event_mask);
    }

  dialog->release_cb_id =
    mb_wm_main_context_x_event_handler_add (wm->main_ctx,
				client->xwin_modal_blocker,
				ButtonRelease,
			        (MBWMXEventFunc)hd_dialog_release_handler,
			        client);
}

static Bool
hd_dialog_request_geometry (MBWindowManagerClient *client,
			    MBGeometry            *new_geometry,
			    MBWMClientReqGeomType  flags)
{
  const MBGeometry *geom;
  Bool              change_size;

  /*
   * When we get an internal geometry request, like from the layout manager,
   * the new geometry applies to the frame; however, if the request is
   * external from ConfigureRequest, it is new geometry of the client window,
   * so we need to take care to handle this right.
   *
   * We only allow change of size in the Y axis; the X axis is always full
   * screen. We do not allow change of position, dialogs are always aligned
   * with the bottom of the screen.
   */
  geom = (flags & MBWMClientReqGeomIsViaConfigureReq) ?
    &client->window->geometry : &client->frame_geometry;

  change_size = (geom->height != new_geometry->height);

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

	  client->window->geometry.height = new_geometry->height;
	  client->frame_geometry.height
	    = client->window->geometry.height + (south + north);

	  client->frame_geometry.y = wm->xdpy_height -
	    (client->frame_geometry.height);
	  client->window->geometry.y = client->frame_geometry.y + north;

	  client->frame_geometry.width = wm->xdpy_width;
	  client->window->geometry.width = wm->xdpy_width - (west + east);

	  client->frame_geometry.x = 0;
	  client->window->geometry.x = west;
	}
      else
	{
	  /*
	   * Internal request, e.g., from layout manager; work out client
	   * window size from the provided frame size.
	   */
	  client->frame_geometry.x      = 0;
	  client->frame_geometry.width  = wm->xdpy_width;
	  client->frame_geometry.height = new_geometry->height;
	  client->frame_geometry.y      = wm->xdpy_height-new_geometry->height;

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

  return True; /* Geometry accepted */
}

MBWindowManagerClient*
hd_dialog_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client
    = MB_WM_CLIENT (mb_wm_object_new (HD_TYPE_DIALOG,
				      MBWMObjectPropWm,           wm,
				      MBWMObjectPropClientWindow, win,
				      NULL));

  return client;
}

