/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2009 Nokia Corporation.
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

#ifndef __HD_HOME_VIEW_CONTAINER_H__
#define __HD_HOME_VIEW_CONTAINER_H__

#include <clutter/clutter.h>
#include "hd-home.h"
#include "hd-comp-mgr.h"

G_BEGIN_DECLS

#define HD_TYPE_HOME_VIEW_CONTAINER            (hd_home_view_container_get_type ())
#define HD_HOME_VIEW_CONTAINER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_HOME_VIEW_CONTAINER, HdHomeViewContainer))
#define HD_HOME_VIEW_CONTAINER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_HOME_VIEW_CONTAINER, HdHomeViewContainerClass))
#define HD_IS_HOME_VIEW_CONTAINER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_HOME_VIEW_CONTAINER))
#define HD_IS_HOME_VIEW_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_HOME_VIEW_CONTAINER))
#define HD_HOME_VIEW_CONTAINER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_HOME_VIEW_CONTAINER, HdHomeViewContainerClass))

typedef struct _HdHomeViewContainer        HdHomeViewContainer;
typedef struct _HdHomeViewContainerClass   HdHomeViewContainerClass;
typedef struct _HdHomeViewContainerPrivate HdHomeViewContainerPrivate;

struct _HdHomeViewContainer
{
  ClutterGroup            parent;

  HdHomeViewContainerPrivate *priv;
};

struct _HdHomeViewContainerClass
{
  ClutterGroupClass parent_class;
};

GType            hd_home_view_container_get_type           (void);

ClutterActor    *hd_home_view_container_new                (HdCompMgr           *comp_mgr,
                                                            ClutterActor        *home);

guint            hd_home_view_container_get_current_view   (HdHomeViewContainer *container);
void             hd_home_view_container_set_current_view   (HdHomeViewContainer *container,
                                                            guint                current_view);

ClutterActor    *hd_home_view_container_get_view           (HdHomeViewContainer *container,
                                                            guint                view_id);
gboolean         hd_home_view_container_get_active         (HdHomeViewContainer *container,
                                                            guint                view_id);

void             hd_home_view_container_set_offset         (HdHomeViewContainer *container,
                                                            ClutterUnit          offset);

ClutterActor    *hd_home_view_container_get_previous_view  (HdHomeViewContainer *container);
ClutterActor    *hd_home_view_container_get_next_view      (HdHomeViewContainer *container);

void             hd_home_view_container_scroll_back        (HdHomeViewContainer *container,
                                                            gint velocity);
void             hd_home_view_container_scroll_to_previous (HdHomeViewContainer *container,
                                                            gint velocity);
ClutterTimeline *hd_home_view_container_scroll_to_next     (HdHomeViewContainer *container,
                                                            gint velocity);
void             hd_home_view_container_set_reactive       (HdHomeViewContainer *container,
                                                            gboolean             reactive);

void 
hd_home_view_container_set_live_bg (HdHomeViewContainer *container,
                                    MBWindowManagerClient *client);
MBWindowManagerClient *
hd_home_view_container_get_live_bg (HdHomeViewContainer *container);

HdHomeViewContainer *hd_home_get_view_container(HdHome *home);

G_END_DECLS

#endif
