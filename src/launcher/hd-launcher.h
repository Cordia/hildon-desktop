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
 * An HdLauncher is a singleton GObject that manages all the
 * launching-related things, like the HdLauncherGrids, and application
 * management.
 *
 */

#ifndef __HD_LAUNCHER_H__
#define __HD_LAUNCHER_H__

#include <glib-object.h>
#include <clutter/clutter-actor.h>

#include "hd-app-mgr.h"
#include "hd-launcher-app.h"
#include "hd-launcher-tile.h"

G_BEGIN_DECLS

/* Common defines */
#define HD_LAUNCHER_DEFAULT_ICON  "tasklaunch_default_application"
#define HD_LAUNCHER_NO_TRANSITION "none"

#define HD_TYPE_LAUNCHER            (hd_launcher_get_type ())
#define HD_LAUNCHER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_LAUNCHER, HdLauncher))
#define HD_IS_LAUNCHER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_LAUNCHER))
#define HD_LAUNCHER_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_LAUNCHER, HdLauncherClass))
#define HD_IS_LAUNCHER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_LAUNCHER))
#define HD_LAUNCHER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_LAUNCHER, HdLauncherClass))

typedef struct _HdLauncher        HdLauncher;
typedef struct _HdLauncherPrivate HdLauncherPrivate;
typedef struct _HdLauncherClass   HdLauncherClass;

struct _HdLauncher
{
  ClutterGroup parent_instance;

  HdLauncherPrivate *priv;
};

struct _HdLauncherClass
{
  ClutterGroupClass parent_class;
};

GType hd_launcher_get_type (void) G_GNUC_CONST;

HdLauncher   *hd_launcher_get (void);

void          hd_launcher_show (void);
void          hd_launcher_hide (void);
void          hd_launcher_hide_final (void);
void          hd_launcher_transition_stop(void);
gboolean      hd_launcher_transition_is_playing(void);

/* Called when an app window has been created */
void          hd_launcher_window_created (void);

HdLauncherTree *hd_launcher_get_tree (void);

/* Called from HdTitleBar */
gboolean
hd_launcher_back_button_clicked(void);

/* Exported so it can be called when something is launched from outside,
 * like with a dbus call. */
gboolean
hd_launcher_transition_app_start (HdLauncherApp *item);

void hd_launcher_stop_loading_transition (void);

/* left/right/top/bottom margin that is clicked on to go back */
#define HD_LAUNCHER_LEFT_MARGIN (68) /* layout guide F */
#define HD_LAUNCHER_RIGHT_MARGIN (68) /* layout guide F */
#define HD_LAUNCHER_TOP_MARGIN (72) /* layout guide A */
#define HD_LAUNCHER_BOTTOM_MARGIN (40) /* layout guide G */


G_END_DECLS

#endif /* __HD_LAUNCHER_H__ */
