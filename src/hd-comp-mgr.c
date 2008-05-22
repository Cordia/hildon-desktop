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
#include "hd-switcher.h"

#include <matchbox/core/mb-wm.h>
#include <matchbox/core/mb-window-manager.h>
#include <matchbox/core/mb-wm-client.h>

#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/shape.h>

#include <clutter/clutter-container.h>
#include <clutter/x11/clutter-x11.h>

static int hd_comp_mgr_init (MBWMObject *obj, va_list vap);
static void hd_comp_mgr_class_init (MBWMObjectClass *klass);
static void hd_comp_mgr_destroy (MBWMObject *obj);
static void hd_comp_mgr_register_client (MBWMCompMgr *mgr, MBWindowManagerClient *c);
static void hd_comp_mgr_unregister_client (MBWMCompMgr *mgr, MBWindowManagerClient *c);
static void hd_comp_mgr_map_notify (MBWMCompMgr *mgr, MBWindowManagerClient *c);

static void hd_comp_mgr_turn_on (MBWMCompMgr *mgr);
static void hd_comp_mgr_effect (MBWMCompMgr *mgr, MBWindowManagerClient *c, MBWMCompMgrClientEvent event);

static void hd_comp_mgr_restack (MBWMCompMgr * mgr);

struct HdCompMgrPrivate
{
  ClutterActor *switcher_group;

  gboolean      stack_sync : 1;
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

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_COMP_MGR_CLUTTER, 0);
    }

  return type;
}

static void
hd_comp_mgr_class_init (MBWMObjectClass *klass)
{
  MBWMCompMgrClass *cm_klass = MB_WM_COMP_MGR_CLASS (klass);

  cm_klass->register_client   = hd_comp_mgr_register_client;
  cm_klass->unregister_client = hd_comp_mgr_unregister_client;
  cm_klass->client_event      = hd_comp_mgr_effect;
  cm_klass->turn_on           = hd_comp_mgr_turn_on;
  cm_klass->map_notify        = hd_comp_mgr_map_notify;
  cm_klass->restack           = hd_comp_mgr_restack;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HDCompMgr";
#endif
}

static int
hd_comp_mgr_init (MBWMObject *obj, va_list vap)
{
  MBWMCompMgr          *cmgr = MB_WM_COMP_MGR (obj);
  MBWindowManager      *wm = cmgr->wm;
  HdCompMgr            *hmgr = HD_COMP_MGR (obj);
  HdCompMgrPrivate     *priv;
  ClutterActor         *stage, *switcher;

  priv = hmgr->priv = g_new0 (HdCompMgrPrivate, 1);

  priv->switcher_group = switcher = g_object_new (HD_TYPE_SWITCHER,
						  "comp-mgr", cmgr,
						  NULL);

  clutter_actor_set_size (switcher, wm->xdpy_width, wm->xdpy_height);

  clutter_actor_show (switcher);

  stage = clutter_stage_get_default ();

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), switcher);

  return 1;
}

static void
hd_comp_mgr_destroy (MBWMObject *obj)
{
}

static void
hd_comp_mgr_setup_input_viewport (HdCompMgr *hmgr, ClutterGeometry * geom)
{
  XserverRegion      region;
  Window             overlay;
  Window             clutter_window;
  XRectangle         rectangle;
  MBWMCompMgr      * mgr = MB_WM_COMP_MGR (hmgr);
  MBWindowManager  * wm = mgr->wm;
  Display          * xdpy = wm->xdpy;

  overlay = XCompositeGetOverlayWindow (xdpy, wm->root_win->xwindow);

  XSelectInput (xdpy,
                overlay,
                FocusChangeMask |
                ExposureMask |
                PropertyChangeMask |
                ButtonPressMask | ButtonReleaseMask |
                KeyPressMask | KeyReleaseMask);

  rectangle.x      = geom->x;
  rectangle.y      = geom->y;
  rectangle.width  = geom->width;
  rectangle.height = geom->height;

  region = XFixesCreateRegion (wm->xdpy, &rectangle, 1);

  XFixesSetWindowShapeRegion (xdpy,
                              overlay,
                              ShapeBounding,
                              0, 0,
                              None);

  XFixesSetWindowShapeRegion (xdpy,
                              overlay,
                              ShapeInput,
                              0, 0,
                              region);

  clutter_window =
    clutter_x11_get_stage_window (CLUTTER_STAGE (clutter_stage_get_default()));

  XSelectInput (xdpy,
                clutter_window,
                FocusChangeMask |
                ExposureMask |
                PropertyChangeMask |
                ButtonPressMask | ButtonReleaseMask |
                KeyPressMask | KeyReleaseMask);

  XFixesSetWindowShapeRegion (xdpy,
                              clutter_window,
                              ShapeBounding,
                              0, 0,
                              None);

  XFixesSetWindowShapeRegion (xdpy,
                              clutter_window,
                              ShapeInput,
                              0, 0,
                              region);

  XFixesDestroyRegion (xdpy, region);
}

static void
hd_comp_mgr_turn_on (MBWMCompMgr *mgr)
{
  ClutterGeometry    geom;
  HdCompMgrPrivate * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClass * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));

  if (parent_klass->turn_on)
    parent_klass->turn_on (mgr);

  hd_switcher_get_button_geometry (HD_SWITCHER (priv->switcher_group), &geom);

  hd_comp_mgr_setup_input_viewport (HD_COMP_MGR (mgr), &geom);
}

static void
hd_comp_mgr_register_client (MBWMCompMgr *mgr, MBWindowManagerClient *c)
{
  MBWMCompMgrClass * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));

  if (parent_klass->register_client)
    parent_klass->register_client (mgr, c);
}

static void
hd_comp_mgr_unregister_client (MBWMCompMgr *mgr, MBWindowManagerClient *c)
{
  HdCompMgrPrivate         * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClass         * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));

  /*
   * If the actor is an appliation, remove it also to the switcher
   *
   * FIXME: will need to do this for notifications as well.
   */
  if (MB_WM_CLIENT_CLIENT_TYPE (c) == MBWMClientTypeApp)
    {
      MBWMCompMgrClutterClient * cclient;
      ClutterActor             * actor;

      cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);

      actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);

      hd_switcher_remove_window_actor (HD_SWITCHER (priv->switcher_group),
				       actor);

      g_object_set_data (G_OBJECT (actor),
			 "HD-MBWindowManagerClient", NULL);
    }

  if (parent_klass->unregister_client)
    parent_klass->unregister_client (mgr, c);
}

static void
hd_comp_mgr_map_notify (MBWMCompMgr *mgr, MBWindowManagerClient *c)
{
  ClutterActor             * actor;
  HdCompMgrPrivate         * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClutterClient * cclient;
  MBWMCompMgrClass         * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));

  /*
   * This results in the actual actor being created for the client
   * by our parent class
   */
  if (parent_klass->map_notify)
    parent_klass->map_notify (mgr, c);

  /*
   * If the actor is an appliation, add it also to the switcher
   *
   * FIXME: will need to do this for notifications as well.
   */
  if (MB_WM_CLIENT_CLIENT_TYPE (c) != MBWMClientTypeApp)
    return;

  cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);

  actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);

  g_object_set_data (G_OBJECT (actor),
		     "HD-MBWindowManagerClient", c);

  hd_switcher_add_window_actor (HD_SWITCHER (priv->switcher_group), actor);
}

static void
hd_comp_mgr_effect (MBWMCompMgr                *mgr,
                    MBWindowManagerClient      *c,
                    MBWMCompMgrClientEvent      event)
{
}

static void
hd_comp_mgr_restack (MBWMCompMgr * mgr)
{
  HdCompMgrPrivate         * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClass         * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));

  /*
   * We use the parent class restack() method to do the stacking, but as our
   * switcher shares actors with the CM, we cannot run this when the switcher
   * is showing; instead we set a flag, and let the switcher request stack
   * sync when it closes.
   */
  if (hd_switcher_showing_switcher (HD_SWITCHER (priv->switcher_group)))
    {
      priv->stack_sync = TRUE;
    }
  else
    {
      if (parent_klass->restack)
	parent_klass->restack (mgr);
    }
}

void
hd_comp_mgr_sync_stacking (HdCompMgr * hmgr)
{
  HdCompMgrPrivate * priv = hmgr->priv;

  if (priv->stack_sync)
    {
      priv->stack_sync = FALSE;
      hd_comp_mgr_restack (MB_WM_COMP_MGR (hmgr));
    }
}

