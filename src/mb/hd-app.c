/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008-2009 Nokia Corporation.
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
#include "hd-comp-mgr.h"
#include "hd-wm.h"

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

static void
delete_open_menus (MBWindowManager *wm)
{
  MBWindowManagerClient *c;

  for (c = wm->stack_top; c; c = c->stacked_below)
    {
      if (MB_WM_CLIENT_CLIENT_TYPE (c) == HdWmClientTypeAppMenu ||
          MB_WM_CLIENT_CLIENT_TYPE (c) == MBWMClientTypeMenu)
        {
          mb_wm_client_deliver_delete (c);
        }
    }
}

static int
hd_app_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient *client = MB_WM_CLIENT (this);
  MBWindowManager       *wm = client->wmref;
  MBWMClientWindow      *win = client->window;

  /*
   * The HdApp can be a transient window in case of using stackable windows.
   */
  if (win->xwin_transient_for
      && win->xwin_transient_for != win->xwindow
      && win->xwin_transient_for != wm->root_win->xwindow)
    {
      MBWindowManagerClient *trans_parent;

      trans_parent = mb_wm_managed_client_from_xwindow (wm, 
		      win->xwin_transient_for);

      if (trans_parent) 
        {
          mb_wm_client_add_transient (trans_parent, client);
	  /*
	   * The secondary window has te same stacking layer as the transient
	   * parent.
	   *
	   * TODO: to set the stacking layer if the window has no transient
	   * parent.
	   */
	  /* FIXME: this is not the right place to set stacking layer,
	   * hd_app_stacking_layer() should contain this. */
	  client->stacking_layer = trans_parent->stacking_layer;
	}
    }

  /* close open menus */
  delete_open_menus (wm);

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

MBWindowManagerClient*
hd_app_get_next_group_member (HdApp *app)
{
  HdApp *next = NULL;

  if (app->stack_index > 0)
    {
      HdApp *leader = app->leader;
      GList *l      = leader->followers;

      while (l)
	{
	  HdApp *a = l->data;

	  if (a == app && l->next)
	    {
	      next = l->next->data;
	      break;
	    }

	  l = l->next;
	}

      if (!next)
	next = leader;
    }
  else
    {
      next = app->followers->data;
    }

  return MB_WM_CLIENT (next);
}

MBWindowManagerClient*
hd_app_get_prev_group_member (HdApp *app)
{
  HdApp *prev = NULL;

  if (app->stack_index > 0)
    {
      HdApp *leader = app->leader;
      GList *l      = leader->followers;

      while (l)
	{
	  HdApp *a = l->data;

	  if (a == app && l->prev)
	    {
	      prev = l->prev->data;
	      break;
	    }

	  l = l->next;
	}

      if (!prev)
	prev = leader;
    }
  else
    {
      GList *last = g_list_last (app->followers);

      if (last)
	prev = last->data;
      else
	prev = app;
    }

  return MB_WM_CLIENT (prev);
}

MBWindowManagerClient*
hd_app_close_followers (HdApp *app)
{
  GList *l;
  HdApp *leader;

  g_debug ("%s: entered", __FUNCTION__);
  leader = app->leader;

  for (l = g_list_last (leader->followers); l; l = l->prev)
    {
      MBWindowManagerClient *f = l->data;

      mb_wm_client_deliver_delete (f);
    }

  return MB_WM_CLIENT (leader);
}

