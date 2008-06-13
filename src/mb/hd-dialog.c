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

static void
hd_dialog_realize (MBWindowManagerClient *client);

static void
hd_dialog_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  client->realize = hd_dialog_realize;

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
    }

  dialog->release_cb_id =
    mb_wm_main_context_x_event_handler_add (wm->main_ctx,
				client->xwin_modal_blocker,
				ButtonRelease,
			        (MBWMXEventFunc)hd_dialog_release_handler,
			        client);
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

