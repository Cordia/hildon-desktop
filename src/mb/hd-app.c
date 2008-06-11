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

#include "hd-app.h"

static void
hd_app_class_init (MBWMObjectClass *klass)
{
#if MBWM_WANT_DEBUG
  klass->klass_name = "HdApp";
#endif
}

static void
hd_app_destroy (MBWMObject *this)
{
}

static int
hd_app_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient *client = MB_WM_CLIENT (this);
  MBWindowManagerClient *c_tmp;
  MBWindowManager       *wm = client->wmref;
  MBWMClientWindow      *win = client->window;
  Window                 win_group;
  HdApp                 *app = HD_APP (this);

  /*
   * Check if there is another app window with the same group leader; if
   * there is not, then this is the primary group window.
   */
  win_group = win->xwin_group;
  app->primary_window = TRUE;

  mb_wm_stack_enumerate (wm, c_tmp)
    {
      if (c_tmp != client &&
	  (MB_WM_CLIENT_CLIENT_TYPE (c_tmp) == MBWMClientTypeApp) &&
	  c_tmp->window->xwin_group == win_group)
	{
	  app->primary_window = FALSE;
	  break;
	}
    }

  /*
   * This forces the decors to be redone, taking into account the
   * primary_window flag.
   */
  mb_wm_client_theme_change (client);

  return 1;
}

int
hd_app_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdAppClass),
	sizeof (HdApp),
	hd_app_init,
	hd_app_destroy,
	hd_app_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_APP, 0);
    }

  return type;
}

MBWindowManagerClient*
hd_app_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client
    = MB_WM_CLIENT(mb_wm_object_new (HD_TYPE_APP,
				     MBWMObjectPropWm,           wm,
				     MBWMObjectPropClientWindow, win,
				     NULL));

  return client;
}

