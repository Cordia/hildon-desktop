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

#ifndef __HD_EDIT_MENU_H__
#define __HD_EDIT_MENU_H__

#include <glib.h>
#include <glib-object.h>

#include <clutter/clutter-group.h>

G_BEGIN_DECLS

#define HD_TYPE_EDIT_MENU            (hd_edit_menu_get_type ())
#define HD_EDIT_MENU(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_EDIT_MENU, HdEditMenu))
#define HD_EDIT_MENU_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_EDIT_MENU, HdEditMenuClass))
#define HD_IS_EDIT_MENU(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_EDIT_MENU))
#define HD_IS_EDIT_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_EDIT_MENU))
#define HD_EDIT_MENU_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_EDIT_MENU, HdEditMenuClass))

typedef struct _HdEditMenu        HdEditMenu;
typedef struct _HdEditMenuClass   HdEditMenuClass;
typedef struct _HdEditMenuPrivate HdEditMenuPrivate;

struct _HdEditMenuClass
{
  ClutterGroupClass parent_class;
};

struct _HdEditMenu
{
  ClutterGroup          parent;

  HdEditMenuPrivate    *priv;
};

GType hd_edit_menu_get_type (void);

G_END_DECLS

#endif
