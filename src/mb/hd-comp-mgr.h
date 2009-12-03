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
#include <matchbox/core/mb-window-manager.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr-clutter.h>

#include "hd-atoms.h"
#include "launcher/hd-running-app.h"

G_BEGIN_DECLS

/* Hardware display dimensions */
#define HD_COMP_MGR_LANDSCAPE_WIDTH   800
#define HD_COMP_MGR_LANDSCAPE_HEIGHT  480

/* The title bar height + HALF_MARGIN border. */
#define HD_COMP_MGR_TOP_MARGIN         56
/* task button width */
#define HD_COMP_MGR_TOP_LEFT_BTN_WIDTH          112
#define HD_COMP_MGR_TOP_LEFT_BTN_HEIGHT         HD_COMP_MGR_TOP_MARGIN
#define HD_COMP_MGR_TOP_RIGHT_BTN_WIDTH         112
#define HD_COMP_MGR_TOP_RIGHT_BTN_HEIGHT        HD_COMP_MGR_TOP_MARGIN
#define HD_COMP_MGR_STATUS_MENU_WIDTH           688
#define HD_COMP_MGR_OPERATOR_PADDING            16 /* specs v2.2 */

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

  /*
   * hd-transition.c may choose to store context information of the currently
   * running effect involving this client.  If not this field is %NULL,
   * but that doesn't mean the client is not participating in an effect.
   */
  struct _HDEffectData *effect;
};

struct HdCompMgrClientClass
{
    MBWMCompMgrClutterClientClass parent;
};


int hd_comp_mgr_client_class_type (void);

gboolean hd_comp_mgr_client_is_hibernating (HdCompMgrClient *hclient);
gboolean hd_comp_mgr_client_can_hibernate (HdCompMgrClient *hclient);

HdRunningApp  *hd_comp_mgr_client_get_app (HdCompMgrClient *hclient);
HdLauncherApp *hd_comp_mgr_client_get_launcher (HdCompMgrClient *hclient);
const gchar   *hd_comp_mgr_client_get_app_local_name (HdCompMgrClient *hclient);

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
void hd_comp_mgr_close_app           (HdCompMgr                *hmgr,
                                      MBWMCompMgrClutterClient *cc,
                                      gboolean                  close_all);
void hd_comp_mgr_close_client        (HdCompMgr *hmgr,
				      MBWMCompMgrClutterClient *c);
void hd_comp_mgr_hibernate_client    (HdCompMgr                *hmgr,
				      MBWMCompMgrClutterClient *c,
				      gboolean                  force);

void hd_comp_mgr_kill_all_apps       (HdCompMgr *hmgr);

void hd_comp_mgr_wakeup_client       (HdCompMgr       *hmgr,
				      HdCompMgrClient *hclient);

gboolean hd_comp_mgr_should_be_portrait (HdCompMgr *hmgr);
gboolean hd_comp_mgr_can_be_portrait (HdCompMgr *hmgr);
gboolean hd_comp_mgr_client_supports_portrait (MBWindowManagerClient *mbwmc);

Atom hd_comp_mgr_get_atom (HdCompMgr *hmgr, HdAtoms id);

ClutterActor * hd_comp_mgr_get_home (HdCompMgr *hmgr);
GObject* hd_comp_mgr_get_switcher (HdCompMgr *hmgr);

gint hd_comp_mgr_get_current_home_view_id (HdCompMgr *hmgr);

MBWindowManagerClient * hd_comp_mgr_get_desktop_client (HdCompMgr *hmgr);

void hd_comp_mgr_dump_debug_info (const gchar *tag);

gboolean hd_comp_mgr_restack (MBWMCompMgr * mgr);
void hd_comp_mgr_set_effect_running(HdCompMgr *hmgr, gboolean running);

void hd_comp_mgr_reset_overlay_shape (HdCompMgr *hmgr);

static inline guint
hd_comp_mgr_get_current_screen_width(void);
static inline guint
hd_comp_mgr_get_current_screen_width (void)
{
  extern MBWindowManager *hd_mb_wm;
  return hd_mb_wm->xdpy_width;
}

static inline guint
hd_comp_mgr_get_current_screen_height(void);
static inline guint
hd_comp_mgr_get_current_screen_height(void)
{
  extern MBWindowManager *hd_mb_wm;
  return hd_mb_wm->xdpy_height;
}

static inline
HdCompMgr *hd_comp_mgr_get(void);
static inline
HdCompMgr *hd_comp_mgr_get(void) {
  extern MBWindowManager *hd_mb_wm;
  return HD_COMP_MGR(hd_mb_wm->comp_mgr);
}

static inline
gboolean hd_comp_mgr_is_portrait(void);
static inline
gboolean hd_comp_mgr_is_portrait(void)
{ /* This is a very typesafe macro. */
  extern MBWindowManager *hd_mb_wm;
  return hd_mb_wm->xdpy_width < hd_mb_wm->xdpy_height;
}

static inline gboolean
hd_comp_mgr_client_is_maximized (MBGeometry geom);
static inline gboolean
hd_comp_mgr_client_is_maximized (MBGeometry geom)
{
  extern MBWindowManager *hd_mb_wm;

  if (geom.x != 0 || geom.y != 0)
    return FALSE;
  if (geom.width >= hd_mb_wm->xdpy_width && geom.height >= hd_mb_wm->xdpy_height)
    return TRUE;
  if (geom.width >= hd_mb_wm->xdpy_height && geom.height >= hd_mb_wm->xdpy_width)
    /* Client covers the rotated screen.  If we select it as the CURRENT_APP,
     * we'll rotate [back] and everything will make sense. */
    return TRUE;

  return FALSE;
}

gint hd_comp_mgr_time_since_last_map(HdCompMgr *hmgr);

void hd_comp_mgr_update_applets_on_current_desktop_property (HdCompMgr *hmgr);
void hd_comp_mgr_unredirect_topmost_client (MBWindowManager *wm,
                                            gboolean force);
void hd_comp_mgr_reconsider_compositing (MBWMCompMgr *mgr);
HdCompMgrClient * hd_comp_mgr_get_current_client (HdCompMgr *hmgr);
void hd_comp_mgr_set_portrait_if_possible(HdCompMgr *hmgr,
                                          gboolean enabled,
                                          gboolean portrait);

G_END_DECLS

#endif /* __HD_COMP_MGR_H__ */
