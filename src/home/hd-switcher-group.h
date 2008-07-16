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

#ifndef __HD_SWITCHER_GROUP_H__
#define __HD_SWITCHER_GROUP_H__

#include <glib.h>
#include <glib-object.h>
#include <clutter/clutter-group.h>

G_BEGIN_DECLS

#define HD_TYPE_SWITCHER_GROUP            (hd_switcher_group_get_type ())
#define HD_SWITCHER_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_SWITCHER_GROUP, HdSwitcherGroup))
#define HD_SWITCHER_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_SWITCHER_GROUP, HdSwitcherGroupClass))
#define IS_HD_SWITCHER_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_SWITCHER_GROUP_TYPE))
#define HD_IS_SWITCHER_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_SWITCHER_GROUP))
#define HD_SWITCHER_GROUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_SWITCHER_GROUP, HdSwitcherGroupClass))

typedef struct _HdSwitcherGroup      HdSwitcherGroup;
typedef struct _HdSwitcherGroupClass HdSwitcherGroupClass;
typedef struct _HdSwitcherGroupPrivate HdSwitcherGroupPrivate;

struct _HdSwitcherGroupClass
{
  ClutterGroupClass     parent_class;

  void                  (*item_selected) (HdSwitcherGroup *group,
                                          ClutterActor    *actor);
  void                  (*background_clicked) (HdSwitcherGroup *group,
					       ClutterButtonEvent *ev);
};

struct _HdSwitcherGroup
{
  ClutterGroup parent;

  HdSwitcherGroupPrivate *priv;
};

GType hd_switcher_group_get_type         (void);

void hd_switcher_group_add_actor         (HdSwitcherGroup       *group,
					  ClutterActor          *actor);
void hd_switcher_group_remove_actor      (HdSwitcherGroup       *group,
					  ClutterActor          *actor);

void hd_switcher_group_replace_actor      (HdSwitcherGroup      *group,
					   ClutterActor         *old,
					   ClutterActor         *new);

gboolean hd_switcher_group_have_children (HdSwitcherGroup       *group);

void hd_switcher_group_hibernate_actor (HdSwitcherGroup *group,
					ClutterActor    *actor);

G_END_DECLS

#endif
