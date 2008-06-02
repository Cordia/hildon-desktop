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

#ifndef __HD_HOME_VIEW_H__
#define __HD_HOME_VIEW_H__

#include <glib.h>
#include <glib-object.h>

#include <clutter/clutter-group.h>

G_BEGIN_DECLS

#define HD_TYPE_HOME_VIEW            (hd_home_view_get_type ())
#define HD_HOME_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_HOME_VIEW, HdHomeView))
#define HD_HOME_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_HOME_VIEW, HdHomeViewClass))
#define HD_IS_HOME_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_HOME_VIEW))
#define HD_IS_HOME_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_HOME_VIEW))
#define HD_HOME_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_HOME_VIEW, HdHomeViewClass))

typedef struct _HdHomeView        HdHomeView;
typedef struct _HdHomeViewClass   HdHomeViewClass;
typedef struct _HdHomeViewPrivate HdHomeViewPrivate;

struct _HdHomeViewClass
{
  ClutterGroupClass parent_class;

  void (*thumbnail_clicked)  (HdHomeView * view, ClutterButtonEvent * ev);
  void (*background_clicked) (HdHomeView * view, ClutterButtonEvent * ev);
};

struct _HdHomeView
{
  ClutterGroup          parent;

  HdHomeViewPrivate    *priv;
};

GType hd_home_view_get_type (void);

void hd_home_view_set_background_color (HdHomeView *view, ClutterColor *color);

void hd_home_view_set_background_image (HdHomeView *view, const gchar * path);

void hd_home_view_set_thumbnail_mode (HdHomeView * view, gboolean on);

void hd_home_view_set_input_mode (HdHomeView *view, gboolean active);

G_END_DECLS

#endif
