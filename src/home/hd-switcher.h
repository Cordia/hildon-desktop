/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Johan Bilien <johan.bilien@nokia.com>
 *          Kimmo Hämäläinen <kimmo.hamalainen@nokia.com>
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

#ifndef __HD_SWITCHER_H__
#define __HD_SWITCHER_H__

#include <glib.h>
#include <glib-object.h>

#include <clutter/clutter-group.h>

#include "mb/hd-note.h"

G_BEGIN_DECLS

#define HD_TYPE_SWITCHER            (hd_switcher_get_type ())
#define HD_SWITCHER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_SWITCHER, HdSwitcher))
#define HD_SWITCHER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_SWITCHER, HdSwitcherClass))
#define HD_IS_SWITCHER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_SWITCHER))
#define HD_IS_SWITCHER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_SWITCHER))
#define HD_SWITCHER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_SWITCHER, HdSwitcherClass))

typedef struct _HdSwitcher        HdSwitcher;
typedef struct _HdSwitcherClass   HdSwitcherClass;
typedef struct _HdSwitcherPrivate HdSwitcherPrivate;

struct _HdSwitcherClass
{
  ClutterGroupClass parent_class;
};

struct _HdSwitcher
{
  ClutterGroup          parent;

  HdSwitcherPrivate    *priv;
};

GType hd_switcher_get_type (void);

void hd_switcher_add_window_actor    (HdSwitcher   * switcher,
				      ClutterActor * actor);
void hd_switcher_add_status_area (HdSwitcher *switcher, ClutterActor *sa);
void hd_switcher_add_status_menu (HdSwitcher *switcher, ClutterActor *sa);
void hd_switcher_add_notification (HdSwitcher *switcher, HdNote *note);

void hd_switcher_remove_window_actor (HdSwitcher   * switcher,
				      ClutterActor * actor);
void hd_switcher_remove_status_area (HdSwitcher   * switcher,
				     ClutterActor * sa);
void hd_switcher_remove_status_menu (HdSwitcher   * switcher,
				     ClutterActor * sa);
void hd_switcher_remove_notification (HdSwitcher * switcher,
                                      HdNote *note);

void hd_switcher_replace_window_actor (HdSwitcher   * switcher,
				       ClutterActor * old,
				       ClutterActor * new);
void
hd_switcher_hibernate_window_actor (HdSwitcher   * switcher,
				    ClutterActor * actor);

void hd_switcher_get_button_geometry (HdSwitcher      * switcher,
				      ClutterGeometry * geom);

gboolean hd_switcher_showing_switcher (HdSwitcher * switcher);

void hd_switcher_deactivate (HdSwitcher * switcher);

void hd_switcher_get_control_area_size (HdSwitcher *switcher,
					guint *control_width,
					guint *control_height);

G_END_DECLS

#endif
