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
 * A HdLauncherTile is a ClutterGroup that displays an icon in a
 * HdLauncherGrid for launching an app or switching to a different
 * grid.
 *
 * This code is based on the old hd-launcher-item.
 */

#ifndef __HD_LAUNCHER_TILE_H__
#define __HD_LAUNCHER_TILE_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define HD_TYPE_LAUNCHER_TILE            (hd_launcher_tile_get_type ())
#define HD_LAUNCHER_TILE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_LAUNCHER_TILE, HdLauncherTile))
#define HD_IS_LAUNCHER_TILE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_LAUNCHER_TILE))
#define HD_LAUNCHER_TILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_LAUNCHER_TILE, HdLauncherTileClass))
#define HD_IS_LAUNCHER_TILE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_LAUNCHER_TILE))
#define HD_LAUNCHER_TILE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_LAUNCHER_TILE, HdLauncherTileClass))

typedef struct _HdLauncherTile          HdLauncherTile;
typedef struct _HdLauncherTilePrivate   HdLauncherTilePrivate;
typedef struct _HdLauncherTileClass     HdLauncherTileClass;

struct _HdLauncherTile
{
  ClutterGroup parent_instance;

  HdLauncherTilePrivate *priv;
};

struct _HdLauncherTileClass
{
  ClutterGroupClass parent_class;
};

GType              hd_launcher_tile_get_type      (void) G_GNUC_CONST;

HdLauncherTile *hd_launcher_tile_new (const gchar *icon_name,
                                      const gchar *text);
const gchar *hd_launcher_tile_get_icon_name (HdLauncherTile *tile);
const gchar *hd_launcher_tile_get_text      (HdLauncherTile *tile);

void hd_launcher_tile_set_icon_name (HdLauncherTile *tile,
                                     const gchar *icon_name);
void hd_launcher_tile_set_text      (HdLauncherTile *tile,
                                     const gchar *text);

ClutterActor *hd_launcher_tile_get_icon (HdLauncherTile *tile);
ClutterActor *hd_launcher_tile_get_label (HdLauncherTile *tile);

void hd_launcher_tile_reset(HdLauncherTile *tile, gboolean hard);

void hd_launcher_tile_activate(ClutterActor       *actor);

/* Fixed size */
#define HD_LAUNCHER_TILE_HEIGHT (96)
#define HD_LAUNCHER_TILE_WIDTH  (142)
/* For the glow, we unfortunately have to have a 1px transparent border
 * around the icons. */
#define HD_LAUNCHER_TILE_ICON_REAL_SIZE (64)
#define HD_LAUNCHER_TILE_ICON_SIZE (HD_LAUNCHER_TILE_ICON_REAL_SIZE+2)
/* The glow is a little bigger than the icon, so we don't get clipped edges*/
#define HD_LAUNCHER_TILE_GLOW_SIZE (80)
/* Maximum amount we can drag without deselecting the currently
 * pressed tile */
#define HD_LAUNCHER_TILE_MAX_DRAG (40)

G_END_DECLS

#endif /* __HD_LAUNCHER_TILE_H__ */
