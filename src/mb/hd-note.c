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

#include "hd-note.h"
#include "hd-comp-mgr.h"

#include <matchbox/theme-engines/mb-wm-theme.h>
#include <matchbox/theme-engines/mb-wm-theme-xml.h>

static Bool hd_note_request_geometry (MBWindowManagerClient *client,
				      MBGeometry            *new_geometry,
				      MBWMClientReqGeomType  flags);

static void
hd_note_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  client->client_type  = MBWMClientTypeNote;
  client->geometry     = hd_note_request_geometry;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdNote";
#endif
}

static void
hd_note_destroy (MBWMObject *this)
{
}

static int
hd_note_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient *client = MB_WM_CLIENT (this);
  MBWindowManager       *wm = client->wmref;
  MBWMClientWindow      *win = client->window;
  HdCompMgr             *hmgr = HD_COMP_MGR (wm->comp_mgr);
  HdNote                *note = HD_NOTE (this);
  MBGeometry             geom;
  unsigned char         *prop;
  unsigned long          items, left;
  int                    format;
  Atom                   note_type;
  Atom                   type;
  int                    n, s, w, e;

  note_type = hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_NOTIFICATION_TYPE);

  XGetWindowProperty (wm->xdpy, win->xwindow,
		      note_type, 0, 1, False,
		      XA_ATOM, &type, &format,
		      &items, &left,
		      &prop);

  if (prop)
    {
      Atom *a = (Atom*)prop;
      Atom  note_type_banner;
      Atom  note_type_info;
      Atom  note_type_confirmation;

      note_type_banner =
	hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_NOTIFICATION_TYPE_BANNER);

      note_type_info =
	hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_NOTIFICATION_TYPE_INFO);

      note_type_confirmation =
	hd_comp_mgr_get_atom (hmgr,
			      HD_ATOM_HILDON_NOTIFICATION_TYPE_CONFIRMATION);

      if (*a == note_type_banner)
	note->note_type = HdNoteTypeBanner;
      else if (*a == note_type_info)
	note->note_type = HdNoteTypeInfo;
      else if (*a == note_type_confirmation)
	note->note_type = HdNoteTypeConfirmation;
      else
	{
	  g_warning ("Unknown hildon notification type.");
	}

      XFree (prop);
    }

  mb_wm_theme_get_decor_dimensions (wm->theme, client, &n, &s, &w, &e);

  geom.x      = 0;
  geom.width  = wm->xdpy_width;
  geom.height = client->window->geometry.height + n + s;

  if (note->note_type == HdNoteTypeInfo)
    {
      geom.y = (wm->xdpy_height - (client->window->geometry.height + n + s))/2;
    }
  else if (note->note_type == HdNoteTypeBanner)
    {
      /* FIXME -- need to get decor size from theme */
      MBWMXmlClient *c;
      MBWMXmlDecor  *d;
      int            north = 0;

      if (wm->theme &&
	  (c = mb_wm_xml_client_find_by_type (wm->theme->xml_clients,
					      MBWMClientTypeApp)))
	{
	  if ((d = mb_wm_xml_decor_find_by_type (c->decors,MBWMDecorTypeNorth)))
	    north = d->height;
	}

      if (!north)
	north = 40; /* Fallback value */

      geom.y = north;
    }
  else
    {
      geom.y = wm->xdpy_height - (client->window->geometry.height + n + s);
    }

  hd_note_request_geometry (client, &geom, MBWMClientReqGeomForced);

  return 1;
}

int
hd_note_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMClientNoteClass),
	sizeof (MBWMClientNote),
	hd_note_init,
	hd_note_destroy,
	hd_note_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_NOTE, 0);
    }

  return type;
}

static Bool
hd_note_request_geometry (MBWindowManagerClient *client,
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

  /*
   * We only allow resizing and moving in the y axis, since notes are
   * fullscreen.
   */
  change_pos = (geom->y != new_geometry->y);
  change_size = (geom->height != new_geometry->height);

  if (change_size || (flags & MBWMClientReqGeomForced))
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

MBWindowManagerClient*
hd_note_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client
    = MB_WM_CLIENT (mb_wm_object_new (HD_TYPE_NOTE,
				      MBWMObjectPropWm,           wm,
				      MBWMObjectPropClientWindow, win,
				      NULL));

  return client;
}

