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
};

struct _HdHomeView
{
  ClutterGroup          parent;

  HdHomeViewPrivate    *priv;
};

GType hd_home_view_get_type (void);

void hd_home_view_set_background_image (HdHomeView *view, const gchar * path);

void hd_home_view_set_thumbnail_mode (HdHomeView * view, gboolean on);

guint hd_home_view_get_view_id (HdHomeView *view);

void hd_home_view_add_applet (HdHomeView *view, ClutterActor *applet);
GSList *hd_home_view_get_all_applets (HdHomeView *view);

void hd_home_view_unregister_applet (HdHomeView *view, ClutterActor *applet);
void hd_home_view_remove_applet (HdHomeView *view, ClutterActor *applet);

void hd_home_view_move_applet (HdHomeView   *old_view, HdHomeView   *new_view,
			       ClutterActor *applet);

void hd_home_view_close_all_applets (HdHomeView *view);

ClutterActor * hd_home_view_get_background (HdHomeView *view);
ClutterActor * hd_home_view_get_applets_container (HdHomeView *view);

gboolean hd_home_view_get_active (HdHomeView *view);
void     hd_home_view_set_active (HdHomeView *view,
                                  gboolean    active);
void hd_home_view_load_background (HdHomeView *view);

G_END_DECLS

#endif
