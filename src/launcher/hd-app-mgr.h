/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Marc Ordinas i Llopis <marc.ordinasillopis@collabora.co.uk>
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

/*
 * The HdAppMgr controls launched applications.
 *
 */

#ifndef __HD_APP_MGR_H__
#define __HD_APP_MGR_H__

#include <glib-object.h>

#include "launcher/hd-launcher-app.h"

G_BEGIN_DECLS

#define HD_TYPE_APP_MGR            (hd_app_mgr_get_type ())
#define HD_APP_MGR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_APP_MGR, HdAppMgr))
#define HD_IS_APP_MGR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_APP_MGR))
#define HD_APP_MGR_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_APP_MGR, HdAppMgrClass))
#define HD_IS_APP_MGR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_APP_MGR))
#define HD_APP_MGR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_APP_MGR, HdAppMgrClass))

typedef struct _HdAppMgr        HdAppMgr;
typedef struct _HdAppMgrPrivate HdAppMgrPrivate;
typedef struct _HdAppMgrClass   HdAppMgrClass;

struct _HdAppMgr
{
  GObject parent_instance;

  HdAppMgrPrivate *priv;
};

struct _HdAppMgrClass
{
  GObjectClass parent_class;
};

GType hd_app_mgr_get_type (void) G_GNUC_CONST;

HdAppMgr   *hd_app_mgr_get (void);

gboolean hd_app_mgr_launch       (HdLauncherApp *app);
gboolean hd_app_mgr_relaunch     (HdLauncherApp *app);
gboolean hd_app_mgr_wakeup       (HdLauncherApp *app);
gboolean hd_app_mgr_kill         (HdLauncherApp *app);

void hd_app_mgr_prestartable     (HdLauncherApp *app);
void hd_app_mgr_not_prestartable (HdLauncherApp *app);
void hd_app_mgr_hibernatable     (HdLauncherApp *app);
void hd_app_mgr_not_hibernatable (HdLauncherApp *app);

/* Window matching */
HdLauncherApp *hd_app_mgr_match_window (const char *res_name,
                                        const char *res_class);

void hd_app_mgr_dump_app_list (gboolean only_running);

G_END_DECLS

#endif /* __HD_APP_MGR_H__ */
