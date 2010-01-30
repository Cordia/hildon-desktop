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

#include "hd-keyboard.h"
#include "hd-comp-mgr.h"
#include "hd-render-manager.h"
#include "hd-title-bar.h"
#include "hd-wm.h"

#include <matchbox/theme-engines/mb-wm-theme.h>
#include <matchbox/theme-engines/mb-wm-theme-xml.h>

static Bool hd_keyboard_request_geometry (MBWindowManagerClient *client,
                                             MBGeometry            *new_geometry,
                                             MBWMClientReqGeomType  flags);

static void
hd_keyboard_stack (MBWindowManagerClient *client,
                   int                    flags)
{
  mb_wm_stack_move_top(client);

  MBWindowManager *wm = client->wmref;
  Window w;
  int focus;

  XGetInputFocus (wm->xdpy, &w, &focus);

  if (w == client->window->xwindow)
    g_debug ("$$$$$ BINGO");
}

static Bool
hd_keyboard_set_focus (MBWindowManagerClient *client)
{
  MBWindowManager *wm = client->wmref;
  //Window xwin = client->window->xwindow;
  gboolean success = True;

  if (client->window->protos & MBWMClientWindowProtosFocus)
    {
      Time t;
      /*g_printerr ("sending XEvent WM_TAKE_FOCUS to 0x%lx\n", xwin); */

      t = mb_wm_get_server_time (wm);
      success = mb_wm_client_deliver_message (client,
                        wm->atoms[MBWM_ATOM_WM_PROTOCOLS],
                        wm->atoms[MBWM_ATOM_WM_TAKE_FOCUS],
                        t, 0, 0, 0);
    }

  return success;
}

static void
hd_keyboard_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  client->client_type  = HdWmClientTypeKeyboard;
  client->geometry     = hd_keyboard_request_geometry;
  client->stack	       = hd_keyboard_stack;
  client->focus	       = hd_keyboard_set_focus;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdKeyboard";
#endif
}

static void
hd_keyboard_destroy (MBWMObject *this)
{
}

static int
hd_keyboard_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient *client = MB_WM_CLIENT (this);
  MBGeometry             geom;
  MBWindowManager       *wm = client->wmref;

  mb_wm_client_set_layout_hints (client, LayoutPrefOverlaps |
					 LayoutPrefPositionFree | 
					 LayoutPrefVisible);

  client->stacking_layer = MBWMStackLayerTop;

  geom.x = 0;
  geom.width  = wm->xdpy_width;
  geom.height = wm->xdpy_height*0.66;
  geom.y = wm->xdpy_height - geom.height;
g_debug ("Keyboard size w: %d h: %d", geom.width, geom.height);
  //client->frame_geometry.height = 0;

  hd_keyboard_request_geometry (client, &geom, MBWMClientReqGeomForced);

  return 1;
}

int
hd_keyboard_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
          sizeof (HdKeyboardClass),
          sizeof (HdKeyboard),
          hd_keyboard_init,
          hd_keyboard_destroy,
          hd_keyboard_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_BASE, 0);
    }

  return type;
}

static Bool
hd_keyboard_request_geometry (MBWindowManagerClient *client,
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
hd_keyboard_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client
    = MB_WM_CLIENT (mb_wm_object_new (HD_TYPE_KEYBOARD,
                                      MBWMObjectPropWm,           wm,
                                      MBWMObjectPropClientWindow, win,
                                      NULL));

  return client;
}

