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

/* Override client->stacking_layer if and only if we're fullscreen. */
static MBWMStackLayerType
hd_app_stacking_layer(MBWindowManagerClient *client)
{
  MBWMList *li;

  if (client->window->ewmh_state & MBWMClientWindowEWMHStateFullscreen)
    return MBWMStackLayerTop;
  for (li = client->transients; li; li = li->next)
    if (hd_app_stacking_layer(li->data) == MBWMStackLayerTop)
      return MBWMStackLayerTop;
  return client->stacking_layer;
}

static void
hd_app_class_init (MBWMObjectClass *klass)
{
#if MBWM_WANT_DEBUG
  klass->klass_name = "HdApp";
#endif
  MB_WM_CLIENT_CLASS (klass)->stacking_layer = hd_app_stacking_layer;
}

static void
hd_app_destroy (MBWMObject *this)
{
  HdApp *app = HD_APP (this);

  if (app->stack_index > 0 && app->leader)
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
  Atom                   actual_type;

  app->stack_index = -1;

  stackable_atom = hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_STACKABLE_WINDOW);

  XGetWindowProperty (wm->xdpy, win->xwindow,
		      stackable_atom, 0, 1, False,
		      XA_INTEGER, &actual_type, &format,
		      &items, &left,
		      &prop);
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

  if (actual_type == XA_INTEGER)
    {
      MBWindowManagerClient *c_tmp;
      HdApp *old_leader = NULL;

      win_group = win->xwin_group;
      app->stack_index = (int)*prop;
      g_debug ("%s: STACK INDEX %d", __func__, app->stack_index);

      mb_wm_stack_enumerate (wm, c_tmp)
        if (c_tmp != client &&
            MB_WM_CLIENT_CLIENT_TYPE (c_tmp) == MBWMClientTypeApp &&
            HD_APP (c_tmp)->stack_index >= 0 /* == stackable window */ &&
            c_tmp->window->xwin_group == win_group)
          {
            old_leader = HD_APP (c_tmp)->leader;
            break;
          }

      if (app->stack_index > 0 && old_leader)
        {
          guint32 one = 1;

	  app->leader = old_leader;
          app->leader->followers = g_list_append (app->leader->followers,
                                                  this);

          /* Flag it with an X property. TODO: is this still used? */
          XChangeProperty (wm->xdpy, win->xwindow,
		           wm->atoms[MBWM_ATOM_MB_SECONDARY],
			   XA_CARDINAL, 32, PropModeReplace,
			   (unsigned char *) &one, 1);

          /* This forces the decors to be redone, taking into account the
           * stack index. */
          mb_wm_client_theme_change (client);
        }
      else  /* we are the first window in the stack */
        {
          /* We set the leader field to ourselves. */
          app->leader = app;
        }
    }

  if (prop)
    XFree (prop);

  /* all stackables have stack_index <= length of the followers list */
  g_assert (!app->leader || !app->leader->followers ||
            app->stack_index <= g_list_length (app->leader->followers));
  /* all stackables have stack_index >= 0 */
  g_assert (!app->leader || (app->leader && app->stack_index >= 0));

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

  g_return_val_if_fail (app != NULL, NULL);
  leader = app->leader;
  g_return_val_if_fail (leader != NULL, MB_WM_CLIENT (app));

  l = g_list_last (leader->followers);
  while (l)
    {
      MBWindowManagerClient *f = l->data;

      mb_wm_client_deliver_delete (f);
      l = l->prev;
    }

  return MB_WM_CLIENT (leader);
}

