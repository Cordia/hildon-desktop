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
#include "hd-comp-mgr.h"
#include "hd-switcher.h"

#include "launcher/hd-launcher.h"

static void
hd_app_show (MBWindowManagerClient *client)
{
  MBWindowManagerClientClass* parent_klass =
    MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(client)));
  HdCompMgr             *hmgr = HD_COMP_MGR (client->wmref->comp_mgr);

  if (parent_klass->show)
    parent_klass->show(client);

  /* Hide/show the switcher (the Tasks button in particular) depending on
   * whether we're fullscreen or not. */
  if (client->window->ewmh_state & MBWMClientWindowEWMHStateFullscreen)
  {
    hd_comp_mgr_set_show_home (hmgr, FALSE);
  }
    
  /* We're now showing this app, so remove our app 
   * starting screen if we had one */
  hd_launcher_window_created();
}

static void
hd_app_hide (MBWindowManagerClient *client)
{
  MBWindowManagerClientClass* parent_klass =
    MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(client)));
  HdCompMgr             *hmgr = HD_COMP_MGR (client->wmref->comp_mgr);

  if (parent_klass->hide)
    parent_klass->hide(client);

  if (client->window->ewmh_state & MBWMClientWindowEWMHStateFullscreen)
  {
    hd_comp_mgr_set_show_home (hmgr, TRUE);
  }
}

static Bool
hd_app_focus (MBWindowManagerClient *client)
{
  ClutterActor *actor;
  MBWMCompMgrClutterClient *cmgrcc;
  MBWindowManagerClientClass* parent_klass =
    MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(client)));

  if (!parent_klass->focus (client))
    return False;

  /* Show our actor (hidden by the switcher) if we accept focus. */
  cmgrcc = MB_WM_COMP_MGR_CLUTTER_CLIENT (client->cm_client);
  actor = mb_wm_comp_mgr_clutter_client_get_actor (cmgrcc);
  if (actor && !CLUTTER_ACTOR_IS_VISIBLE (actor))
    clutter_actor_show (actor);
  return True;
}

static void
hd_app_class_init (MBWMObjectClass *klass)
{
#if MBWM_WANT_DEBUG
  klass->klass_name = "HdApp";
#endif
  MB_WM_CLIENT_CLASS (klass)->show = hd_app_show;
  MB_WM_CLIENT_CLASS (klass)->hide = hd_app_hide;
  MB_WM_CLIENT_CLASS (klass)->focus = hd_app_focus;
}

static void
hd_app_destroy (MBWMObject *this)
{
  HdApp *app = HD_APP (this);

  if (app->secondary_window && app->leader)
    {
      HdApp *leader = app->leader;

      leader->followers = g_list_remove (leader->followers, app);
    }
  else
    {
      GList *l = app->followers;

      while (l)
	{
	  HdApp *a = HD_APP (l->data);
	  a->leader = NULL;

	  l = l->next;
	}

      g_list_free (app->followers);
    }
}

static int
hd_app_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient *client = MB_WM_CLIENT (this);
  MBWindowManager       *wm = client->wmref;
  MBWMClientWindow      *win = client->window;
  HdCompMgr             *hmgr = HD_COMP_MGR (wm->comp_mgr);
  Window                 win_group;
  HdApp                 *app = HD_APP (this);
  unsigned char         *prop = NULL;
  unsigned long          items, left;
  int                    format;
  Atom                   stackable_atom;
  Atom                   type;

  stackable_atom = hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_STACKABLE_WINDOW);

  XGetWindowProperty (wm->xdpy, win->xwindow,
		      stackable_atom, 0, 1, False,
		      XA_STRING, &type, &format,
		      &items, &left,
		      &prop);

  if (prop)
    {
      MBWindowManagerClient *c_tmp;

      /*
       * Stackable hildon window; check if there is another app window with the
       * same group leader; if there is not, then this is the primary group
       * window.
       */
      win_group = win->xwin_group;

      mb_wm_stack_enumerate (wm, c_tmp)
        {
          if (c_tmp != client &&
              (MB_WM_CLIENT_CLIENT_TYPE (c_tmp) == MBWMClientTypeApp) &&
              c_tmp->window->xwin_group == win_group)
            {
              HdApp *h_tmp = HD_APP (c_tmp);

              app->secondary_window = TRUE;
              app->leader = h_tmp->leader;

              /*
               * This forces the decors to be redone, taking into account the
               * secondary_window flag.
               */
              mb_wm_client_theme_change (client);
              break;
            }
        }

      XFree (prop);

      if (app->secondary_window)
        {
          HdApp *leader = app->leader;
          
          leader->followers = g_list_append (leader->followers, this);
        }
      else
        {
          /*
           * We set the leader field to ourselves.
           */
          app->leader = app;
        }
    }

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

  if (app->secondary_window)
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

  if (app->secondary_window)
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
  HdApp *leader = app->leader;
  GList *l = leader->followers;

  l = g_list_last (l);
  while (l)
    {
      MBWindowManagerClient *f = l->data;

      //g_debug ("%s: Closing App client %s", __func__, mb_wm_client_get_name(f));
      mb_wm_client_deliver_delete (f);
      l = l->prev;
    }

  return MB_WM_CLIENT (leader);
}

