/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Tomas Frydrych <tf@o-hand.com>
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

#ifndef __HD_HOME_H__
#define __HD_HOME_H__

#include <glib.h>
#include <glib-object.h>

#include <clutter/clutter-group.h>

#include "hd-home-view.h"

/* FIXME -- match spec */
#define HDH_SWITCH_WIDTH 20

#define HDH_GCONF_PREFIX "/apps/hildon_home"

G_BEGIN_DECLS

#define HD_TYPE_HOME            (hd_home_get_type ())
#define HD_HOME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_HOME, HdHome))
#define HD_HOME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_HOME, HdHomeClass))
#define HD_IS_HOME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_HOME))
#define HD_IS_HOME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_HOME))
#define HD_HOME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_HOME, HdHomeClass))

typedef enum _HdHomeMode
{
  HD_HOME_MODE_NORMAL = 0,
  HD_HOME_MODE_LAYOUT,
  HD_HOME_MODE_EDIT,
} HdHomeMode;

typedef struct _HdHome        HdHome;
typedef struct _HdHomeClass   HdHomeClass;
typedef struct _HdHomePrivate HdHomePrivate;

struct _HdHomeClass
{
  ClutterGroupClass parent_class;

  void (*background_clicked) (HdHome *home, ClutterButtonEvent *ev);
  void (*mode_changed)       (HdHome *home, HdHomeMode mode);
};

struct _HdHome
{
  ClutterGroup      parent;

  HdHomePrivate    *priv;
};

GType hd_home_get_type (void);

void hd_home_show_view (HdHome * home, guint view_index);

HdHomeMode hd_home_get_mode (HdHome *home);
void hd_home_set_mode (HdHome* home, HdHomeMode mode);
void hd_home_show_activate_views_dialog (HdHome *home);

void hd_home_add_applet (HdHome *home, ClutterActor *applet);
void hd_home_add_status_area (HdHome *home, ClutterActor *sa);
void hd_home_add_status_menu (HdHome *home, ClutterActor *sa);

void hd_home_remove_applet (HdHome *home, ClutterActor *applet);
void hd_home_remove_status_area (HdHome *home, ClutterActor *sa);
void hd_home_remove_status_menu (HdHome *home, ClutterActor *sa);

void hd_home_grab_pointer (HdHome *home);

void hd_home_ungrab_pointer (HdHome *home);

void hd_home_show_applet_buttons (HdHome *home, ClutterActor *applet);

void hd_home_hide_applet_buttons (HdHome *home);

void hd_home_move_applet_buttons (HdHome *home, gint x_by, gint y_by);

guint hd_home_get_current_view_id (HdHome *home);

HdHomeView * hd_home_get_current_view (HdHome *home);

void hd_home_set_operator_label (HdHome *home, const char *text);

void hd_home_set_operator_icon (HdHome *home, const char *file);

void hd_home_fixup_operator_position (HdHome *home);

void hd_home_show_switches (HdHome *home);

void hd_home_hide_switches (HdHome *home);

void hd_home_highlight_switch (HdHome *home, gboolean left);

void hd_home_unhighlight_switches (HdHome *home);

void hd_home_pan_and_move_applet (HdHome *home, gboolean left, ClutterActor *applet);

GList* hd_home_get_all_views (HdHome *home);

GList* hd_home_get_active_views (HdHome *home);

void hd_home_set_view_status (HdHome * home, guint id, gboolean active);

void hd_home_hide_edit_button (HdHome *home);

G_END_DECLS

#endif
