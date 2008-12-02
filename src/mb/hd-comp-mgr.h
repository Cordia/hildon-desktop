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

#ifndef __HD_COMP_MGR_H__
#define __HD_COMP_MGR_H__

#include <glib/gmacros.h>
#include <matchbox/core/mb-wm-object.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr-clutter.h>

#include "hd-atoms.h"

G_BEGIN_DECLS


typedef struct HdCompMgrClientClass   HdCompMgrClientClass;
typedef struct HdCompMgrClient        HdCompMgrClient;
typedef struct HdCompMgrClientPrivate HdCompMgrClientPrivate;

#define HD_COMP_MGR_CLIENT(c)       ((HdCompMgrClient*)(c))
#define HD_COMP_MGR_CLIENT_CLASS(c) ((HdCompMgrClientClass*)(c))
#define HD_TYPE_COMP_MGR_CLIENT     (hd_comp_mgr_client_class_type ())

struct HdCompMgrClient
{
  MBWMCompMgrClutterClient    parent;

  HdCompMgrClientPrivate     *priv;
};

struct HdCompMgrClientClass
{
    MBWMCompMgrClutterClientClass parent;
};

int hd_comp_mgr_client_class_type (void);

gboolean hd_comp_mgr_client_is_hibernating (HdCompMgrClient *hclient);

typedef struct HdCompMgrClass   HdCompMgrClass;
typedef struct HdCompMgr        HdCompMgr;
typedef struct HdCompMgrPrivate HdCompMgrPrivate;

#define HD_COMP_MGR(c)       ((HdCompMgr*)(c))
#define HD_COMP_MGR_CLASS(c) ((HdCompMgrClass*)(c))
#define HD_TYPE_COMP_MGR     (hd_comp_mgr_class_type ())
#define HD_IS_COMP_MGR(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_COMP_MGR))

struct HdCompMgr
{
  MBWMCompMgrClutter    parent;

  HdCompMgrPrivate     *priv;
};

struct HdCompMgrClass
{
    MBWMCompMgrClutterClass parent;
};

int hd_comp_mgr_class_type (void);

void hd_comp_mgr_sync_stacking       (HdCompMgr *hmgr);
void hd_comp_mgr_raise_home_actor    (HdCompMgr *hmgr);
void hd_comp_mgr_lower_home_actor    (HdCompMgr *hmgr);
void hd_comp_mgr_top_home            (HdCompMgr *hmgr);
void hd_comp_mgr_close_app           (HdCompMgr                *hmgr,
                                      MBWMCompMgrClutterClient *cc,
                                      gboolean                  close_all);
void hd_comp_mgr_close_client        (HdCompMgr *hmgr,
				      MBWMCompMgrClutterClient *c);
void hd_comp_mgr_hibernate_client    (HdCompMgr                *hmgr,
				      MBWMCompMgrClutterClient *c,
				      gboolean                  force);

void hd_comp_mgr_hibernate_all       (HdCompMgr *hmgr, gboolean force);

void hd_comp_mgr_wakeup_client       (HdCompMgr       *hmgr,
				      HdCompMgrClient *hclient);

void hd_comp_mgr_set_low_memory_state (HdCompMgr * hmgr, gboolean on);

gboolean hd_comp_mgr_get_low_memory_state (HdCompMgr * hmgr);

void hd_comp_mgr_launch_application (HdCompMgr   *hmgr,
				     const gchar *app_service,
				     const gchar *launch_param);

Atom hd_comp_mgr_get_atom (HdCompMgr *hmgr, HdAtoms id);

ClutterActor * hd_comp_mgr_get_home (HdCompMgr *hmgr);
ClutterActor * hd_comp_mgr_get_switcher (HdCompMgr *hmgr);

gint hd_comp_mgr_get_current_home_view_id (HdCompMgr *hmgr);

MBWindowManagerClient * hd_comp_mgr_get_desktop_client (HdCompMgr *hmgr);

gint hd_comp_mgr_request_home_applet_geometry (HdCompMgr  *hmgr,
					       gint        view_id,
					       MBGeometry *geom);

gint hd_comp_mgr_get_home_applet_layer_count (HdCompMgr *hmgr, gint view_id);
void hd_comp_mgr_blur_home(HdCompMgr *hmgr, gboolean blur);
void hd_comp_mgr_unblur(HdCompMgr *hmgr);

void hd_comp_mgr_dump_debug_info (const gchar *tag);

void hd_comp_mgr_setup_input_viewport (HdCompMgr       *hmgr,
                                       ClutterGeometry *geom,
                                       int              count);

G_END_DECLS

#endif /* __HD_COMP_MGR_H__ */
