/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Johan Bilien <johan.bilien@nokia.com>
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

#include "hd-comp-mgr.h"
#include "hd-window-actor.h"
#include "hd-comp-window.h"
#include "hd-mb-wm-props.h"
#include "hd-stage.h"
#include "hd-window-group.h"

#include <matchbox/core/mb-wm.h>
#include <matchbox/core/mb-window-manager.h>
#include <matchbox/core/mb-wm-client.h>

#include <X11/extensions/Xcomposite.h>

#include <clutter/clutter-container.h>
#include <clutter/clutter-x11.h>

static int hd_comp_mgr_init (MBWMObject *obj, va_list vap);
static void hd_comp_mgr_class_init (MBWMObjectClass *klass);
static void hd_comp_mgr_destroy (MBWMObject *obj);
static void hd_comp_mgr_register_client (MBWMCompMgr *mgr, MBWindowManagerClient *c);
static void hd_comp_mgr_unregister_client (MBWMCompMgr *mgr, MBWindowManagerClient *c);
static void hd_comp_mgr_turn_on (MBWMCompMgr *mgr);
static void hd_comp_mgr_turn_off (MBWMCompMgr *mgr);
static void hd_comp_mgr_render (MBWMCompMgr *mgr);
static Bool hd_comp_mgr_handle_damage (XDamageNotifyEvent * xev, MBWMCompMgr *mgr);
static void hd_comp_mgr_effect (MBWMCompMgr *mgr, MBWindowManagerClient *c, MBWMCompMgrClientEvent event);
static void hd_comp_mgr_restack (MBWMCompMgr    *mgr);

struct HdCompMgrPrivate
{
  GHashTable   *windows;

  ClutterActor *window_group;
  ClutterActor *top_window_group;
};

int
hd_comp_mgr_class_type ()
{
  static int type = 0;

  if (type == 0)
    {
      static MBWMObjectClassInfo info = {
        sizeof (HdCompMgrClass),
        sizeof (HdCompMgr),
        hd_comp_mgr_init,
        hd_comp_mgr_destroy,
        hd_comp_mgr_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_COMP_MGR, 0);
    }

  return type;
}

static void
hd_comp_mgr_class_init (MBWMObjectClass *klass)
{
  MBWMCompMgrClass *cm_klass = MB_WM_COMP_MGR_CLASS (klass);

  cm_klass->register_client   = hd_comp_mgr_register_client;
  cm_klass->unregister_client = hd_comp_mgr_unregister_client;
  cm_klass->turn_on           = hd_comp_mgr_turn_on;
  cm_klass->turn_off          = hd_comp_mgr_turn_off;
  cm_klass->render            = hd_comp_mgr_render;
  cm_klass->handle_damage     = hd_comp_mgr_handle_damage;
  cm_klass->client_event      = hd_comp_mgr_effect;
  cm_klass->restack           = hd_comp_mgr_restack;
}

static int
hd_comp_mgr_init (MBWMObject *obj, va_list vap)
{
  HdCompMgr            *mgr = HD_COMP_MGR (obj);
  HdCompMgrPrivate     *priv;
  HdMbWmProp            prop;

  priv = mgr->priv = g_new0 (HdCompMgrPrivate, 1);

  priv->windows = g_hash_table_new_full (g_direct_hash,
                                         g_direct_equal,
                                         NULL,
                                         NULL
                                         /*(GDestroyNotify)mb_wm_object_unref*/);

  prop = va_arg (vap, HdMbWmProp);

  while (prop)
    {
      switch (prop)
        {
          case HdMbWmPropWindowGroup:
              priv->window_group = va_arg (vap, ClutterActor *);
              break;
          case HdMbWmPropTopWindowGroup:
              priv->top_window_group = va_arg (vap, ClutterActor *);
              break;
          default:
              MBWMO_PROP_EAT (vap, prop);
        }

      prop = va_arg (vap, MBWMObjectProp);
    }

  return 1;
}

static void
hd_comp_mgr_destroy (MBWMObject *obj)
{
}

static void
hd_comp_mgr_register_client (MBWMCompMgr *mgr, MBWindowManagerClient *c)
{
  HdCompMgrPrivate     *priv = HD_COMP_MGR (mgr)->priv;
  static Window         overlay = None;
  static Window         stage_window = None;
  MBWMObject           *client;
  Display              *dpy = c->wmref->xdpy;
  ClutterActor         *actor;

  client = g_hash_table_lookup (priv->windows, c);
  if (client)
    return;

  if (overlay == None)
    overlay = XCompositeGetOverlayWindow (dpy,
                                          RootWindow (dpy, DefaultScreen (dpy)));

  if (stage_window == None)
    stage_window =
        clutter_x11_get_stage_window (CLUTTER_STAGE (hd_get_default_stage ()));

  if (overlay == c->window->xwindow)
    return;

  if (stage_window == c->window->xwindow)
    return;

  actor = g_object_new (HD_TYPE_WINDOW_ACTOR,
                        "opacity", 0,
                        NULL);

  if (c->window->net_type ==
      c->wmref->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_NORMAL] &&
      !c->window->override_redirect)
    clutter_container_add_actor (CLUTTER_CONTAINER (priv->window_group), actor);
  else
    {
      MBGeometry r = {0};

      mb_wm_client_get_coverage (c, &r);
      clutter_container_add_actor (CLUTTER_CONTAINER (priv->top_window_group),
                                   actor);
      clutter_actor_set_position (actor, r.x, r.y);
      clutter_actor_set_size (actor, r.width, r.height);
    }

  client = mb_wm_object_new (HD_TYPE_COMP_WINDOW,
                             MBWMObjectPropClient, c,
                             HdMbWmPropActor, actor,
                             NULL);

  g_object_set (actor,
                "comp-window", client,
                NULL);

  c->cm_client = MB_WM_COMP_MGR_CLIENT (client);

  g_hash_table_insert (priv->windows, c, client);

}

static void
hd_comp_mgr_unregister_client (MBWMCompMgr *mgr, MBWindowManagerClient *c)
{
  HdCompMgrPrivate     *priv = HD_COMP_MGR (mgr)->priv;
  gboolean              removed;

  removed = g_hash_table_remove (priv->windows, c);

}

static void
hd_comp_mgr_turn_on (MBWMCompMgr *mgr)
{
  MBWindowManager             * wm = mgr->wm;
  mgr->disabled = False;

  XCompositeRedirectSubwindows (wm->xdpy,
                                wm->root_win->xwindow,
                                CompositeRedirectManual);
}

static void
hd_comp_mgr_turn_off (MBWMCompMgr *mgr)
{
  MBWindowManager             * wm = mgr->wm;

  XCompositeUnredirectSubwindows (wm->xdpy,
                                  wm->root_win->xwindow,
                                  CompositeRedirectManual);
}

static void
hd_comp_mgr_render (MBWMCompMgr *mgr)
{
}

static Bool
hd_comp_mgr_handle_damage (XDamageNotifyEvent * xev, MBWMCompMgr *mgr)
{
  MBWindowManagerClient * c;

  c = mb_wm_managed_client_from_frame (mgr->wm, xev->drawable);

  if (c && c->cm_client)
    mb_wm_comp_mgr_client_repair (c->cm_client);

  return False;
}

static void
hd_comp_mgr_effect (MBWMCompMgr                *mgr,
                    MBWindowManagerClient      *c,
                    MBWMCompMgrClientEvent      event)
{
  HdCompMgrPrivate     *priv;
  HdCompWindow         *window;

  priv = HD_COMP_MGR (mgr)->priv;
  window = g_hash_table_lookup (priv->windows, c);

  if (window)
    hd_comp_window_effect (window, event);

}

static void
hd_comp_mgr_restack (MBWMCompMgr    *mgr)
{
}
