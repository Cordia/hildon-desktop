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

G_BEGIN_DECLS

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
  GObject parent_instance;

  HdLauncherPrivate *priv;
};

struct _HdLauncherClass
{
  GObjectClass parent_class;
};

GType hd_launcher_get_type (void) G_GNUC_CONST;

HdLauncher   *hd_launcher_get (void);
ClutterActor *hd_launcher_get_group (void);

void          hd_launcher_show (void);
void          hd_launcher_hide (void);
void          hd_launcher_hide_final (void);

/* Called when an app window has been created */
void          hd_launcher_window_created (void);
/* to be used by hd-launcher-page only */
void          hd_launcher_set_top_blur (float amount, float opacity);
void          hd_launcher_set_back_arrow_opacity (float amount);


G_END_DECLS

#endif /* __HD_LAUNCHER_H__ */
