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

#ifndef _HD_APPLET_LAYOUT_MANAGER_H_
#define _HD_APPLET_LAYOUT_MANAGER_H_

#include <matchbox/core/mb-wm.h>

typedef struct _HdAppletLayoutManager HdAppletLayoutManager;

HdAppletLayoutManager * hd_applet_layout_manager_new (void);

gint hd_applet_layout_manager_request_geometry (HdAppletLayoutManager *mgr,
						MBGeometry            *geom);

void hd_applet_layout_manager_reclaim_geometry (HdAppletLayoutManager *mgr,
					        gint                  layer_id,
						MBGeometry            *geom);

gint hd_applet_layout_manager_get_layer_count (HdAppletLayoutManager *mgr);

#endif
