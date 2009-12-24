/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2009 Moises Martinez
 *
 * Author:  Moises Martinez <moimart@gmail.com>
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
 * An HdLauncherConfig is a singleton GObject that manages the size configuration
 * of all the elements in the launcher_config.
 */

#ifndef __HD_LAUNCHER_CONFIG_H__
#define __HD_LAUNCHER_CONFIG_H__

#include <glib-object.h>


G_BEGIN_DECLS

/* Defines launcher.desktop */

#define HDLC_KEY_GROUP		"Launcher Config"
#define HDLC_KEY_LEFT_MARGIN	"LeftMargin"
#define HDLC_KEY_RIGHT_MARGIN   "RightMargin"
#define HDLC_KEY_TOP_MARGIN	"TopMargin"
#define HDLC_KEY_BOTTOM_MARGIN	"BottomMargin"
#define HDLC_KEY_TILE_WIDTH     "TileWidth"
#define HDLC_KEY_TILE_HEIGHT    "TileHeight"
#define HDLC_KEY_REAL_ICON_SIZE "IconSize"
#define HDLC_KEY_ICON_SIZE      "TileIconSize"
#define HDLC_KEY_ICON_GLOW      "IconGlow"
#define HDLC_KEY_TILE_FONT      "TileFont"
#define HDLC_KEY_MAX_DRAG       "MaxDrag"
#define HDLC_KEY_DEFAULT_MARGIN "DefaultMargin"
#define HDLC_KEY_DEFAULT_ICON   "DefaultIcon"

#define HD_TYPE_LAUNCHER_CONFIG            (hd_launcher_config_get_type ())
#define HD_LAUNCHER_CONFIG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_LAUNCHER_CONFIG, HdLauncherConfig))
#define HD_IS_LAUNCHER_CONFIG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_LAUNCHER_CONFIG))
#define HD_LAUNCHER_CONFIG_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_LAUNCHER_CONFIG, HdLauncherConfigClass))
#define HD_IS_LAUNCHER_CONFIG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_LAUNCHER_CONFIG))
#define HD_LAUNCHER_CONFIG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_LAUNCHER_CONFIG, HdLauncherConfigClass))

typedef struct _HdLauncherConfig        HdLauncherConfig;
typedef struct _HdLauncherConfigPrivate HdLauncherConfigPrivate;
typedef struct _HdLauncherConfigClass   HdLauncherConfigClass;

struct _HdLauncherConfig
{
  GObject parent_instance;

  HdLauncherConfigPrivate *priv;
};

struct _HdLauncherConfigClass
{
  GObjectClass parent_class;
};

GType hd_launcher_config_get_type (void) G_GNUC_CONST;

HdLauncherConfig   *hd_launcher_config_get (void);

void hd_launcher_config_get_tile_size (gint *width, gint *height);

void hd_launcher_config_get_margins_size (gint *left, gint *right, gint *top, gint *bottom);

void hd_launcher_config_get_icons_size (gint *real, gint *size, gint *glow);

const gchar *hd_launcher_config_get_tile_font (void);

const gchar *hd_launcher_config_get_default_icon (void);

gint hd_launcher_config_get_max_drag (void);

gint hd_launcher_config_get_columns (void);

gint hd_launcher_config_get_default_margin (void);

/* left/right/top/bottom margin that is clicked on to go back */
#define HD_LAUNCHER_CONFIG_LEFT_MARGIN (68) /* layout guide F */
#define HD_LAUNCHER_CONFIG_RIGHT_MARGIN (68) /* layout guide F */
#define HD_LAUNCHER_CONFIG_TOP_MARGIN (70) /* layout guide A */
#define HD_LAUNCHER_CONFIG_BOTTOM_MARGIN (46) /* layout guide G */


G_END_DECLS

#endif /* __HD_LAUNCHER_CONFIG_H__ */
