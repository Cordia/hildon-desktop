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

#ifndef __HD_HOME_VIEW_LAYOUT_H__
#define __HD_HOME_VIEW_LAYOUT_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define HD_TYPE_HOME_VIEW_LAYOUT            (hd_home_view_layout_get_type ())
#define HD_HOME_VIEW_LAYOUT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_HOME_VIEW_LAYOUT, HdHomeViewLayout))
#define HD_HOME_VIEW_LAYOUT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_HOME_VIEW_LAYOUT, HdHomeViewLayoutClass))
#define HD_IS_HOME_VIEW_LAYOUT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_HOME_VIEW_LAYOUT))
#define HD_IS_HOME_VIEW_LAYOUT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_HOME_VIEW_LAYOUT))
#define HD_HOME_VIEW_LAYOUT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_HOME_VIEW_LAYOUT, HdHomeViewLayoutClass))

typedef struct _HdHomeViewLayout        HdHomeViewLayout;
typedef struct _HdHomeViewLayoutClass   HdHomeViewLayoutClass;
typedef struct _HdHomeViewLayoutPrivate HdHomeViewLayoutPrivate;

struct _HdHomeViewLayout
{
  GObject parent;

  HdHomeViewLayoutPrivate *priv;
};

struct _HdHomeViewLayoutClass
{
  GObjectClass parent_class;
};

GType             hd_home_view_layout_get_type       (void);

HdHomeViewLayout *hd_home_view_layout_new            (void);

void              hd_home_view_layout_reset          (HdHomeViewLayout *layout);
void              hd_home_view_layout_arrange_applet (HdHomeViewLayout *layout,
                                                      GSList           *applets,
                                                      ClutterActor     *new_applet);

G_END_DECLS

#endif
