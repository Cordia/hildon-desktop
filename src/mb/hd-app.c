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
#include "hd-transition.h"

static Bool
hd_app_request_geometry (MBWindowManagerClient *client,
                         MBGeometry            *new_geometry,
                         MBWMClientReqGeomType  flags);

static void
hd_app_detransitise (MBWindowManagerClient     *client);

static void
hd_app_class_init (MBWMObjectClass *klass)
{
#if MBWM_WANT_DEBUG
  klass->klass_name = "HdApp";
#endif

  MB_WM_CLIENT_CLASS (klass)->geometry = hd_app_request_geometry;
  MB_WM_CLIENT_CLASS (klass)->detransitise = hd_app_detransitise;
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
      if (MB_WM_CLIENT_CLIENT_TYPE (c) == (MBWMClientType) HdWmClientTypeAppMenu ||
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

static Bool
hd_app_request_geometry (MBWindowManagerClient *client,
                         MBGeometry            *new_geometry,
                         MBWMClientReqGeomType  flags)
{
  MBWMDecor *decor;
  MBWMDecorButton *button;

  /*
   * Ignore the layout manager's request if we're in portrait but we don't
   * support it.  The window manager ensures that we are not visible in
   * this case.  If we were it couldn't be in portrait mode because we don't
   * support it.  When the wm exits portrait mode we'll be reconfigured,
   * then we can do a proper geometry change.  Unfortunately we cannot skip
   * reconfiguration if the client is just being mapped (!map_confirmed())
   * because if it's not even realized %MBWMClientBase::realize needs some
   * geometry, otherwise it will try to create a 0x0 frame window and fail
   * in a sneaky way.
   */
  if ((flags & MBWMClientReqGeomIsViaLayoutManager)
      &&  (hd_comp_mgr_is_portrait () ||
           hd_transition_is_rotating_to_portrait())
      &&  mb_wm_client_is_map_confirmed (client)
      && !hd_comp_mgr_client_supports_portrait (client))
    return False;

  /* Resize the close button according to portraitness. */
  if (client->decor
      && (decor = client->decor->data)
      && decor->type == MBWMDecorTypeNorth
      && decor->buttons
      && (button = decor->buttons->data)
      && button->type == MBWMDecorButtonClose)
    {
      MBGeometry geo;
      
      geo = button->geom;
      geo.width = new_geometry->width > new_geometry->height
        ? HD_COMP_MGR_TOP_RIGHT_BTN_WIDTH
        : HD_COMP_MGR_TOP_RIGHT_BTN_WIDTH_SMALL;
      geo.x = decor->geom.width - geo.width;

      if (button->geom.width != geo.width)
        {
          button->geom = geo;
          button->needs_sync = True;
        }
    }

  return MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(client)))
    ->geometry (client, new_geometry, flags);
}

static void
hd_app_detransitise (MBWindowManagerClient     *client)
{
  HdApp                      *app = HD_APP (client);
  MBWindowManagerClientClass *klass;

  app->detransitised_from = client->window ?
	  client->window->xwin_transient_for : None;

  klass = MB_WM_CLIENT_CLASS(
		  MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(client)));
  if (klass->detransitise)
    klass->detransitise (client);
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
