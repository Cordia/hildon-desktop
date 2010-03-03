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
 * An HDDesktopConfig is a singleton GObject that manages the size configuration
 * of all the elements in the desktop_config.
 */

#ifndef __HD_DESKTOP_CONFIG_H__
#define __HD_DESKTOP_CONFIG_H__

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

/* Defines hildon-desktop.desktop */

#define HDTC_KEY_GROUP			 "Hildon Desktop Config"
#define HDTC_KEY_TN_LAYOUT		 "TnLayout"
#define HDTC_KEY_DEFAULT_TXT_COLOR	 "DefaultTextColor"
#define HDTC_KEY_NOTIFICATION_TXT_COLOR  "NotificationTextColor"
#define HDTC_KEY_NOTIFICATION_2TXT_COLOR "NotificationSecondaryTextColor"
#define HDTC_KEY_SECONDARY_TXT_COLOR     "SecondaryTextColor"
#define HDTC_KEY_DEFAULT_BG_COLOR	 "DefaultBackgroundColor"
#define HDTC_KEY_SYSTEM_FONT	         "SystemFont"
#define HDTC_KEY_LARGE_SYSTEM_FONT       "LargeSystemFont"
#define HDTC_KEY_SMALL_SYSTEM_FONT       "SmallSystemFont"
#define HDTC_KEY_TITLEBAR_FONT		 "TitlebarFont"

typedef enum
{
  HD_TXT_COLOR,
  HD_2TXT_COLOR,
  HD_NOTIFICATION_TXT_COLOR,
  HD_NOTIFICATION_2TXT_COLOR,
  HD_BG_COLOR
}
HDConfigColor; 

typedef enum
{
  HD_SYSTEM_FONT,
  HD_LARGE_SYSTEM_FONT,
  HD_SMALL_SYSTEM_FONT,
  HD_TITLEBAR_FONT
}
HDConfigFont;

#define HD_TYPE_DESKTOP_CONFIG            (hd_desktop_config_get_type ())
#define HD_DESKTOP_CONFIG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_DESKTOP_CONFIG, HDDesktopConfig))
#define HD_IS_DESKTOP_CONFIG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_DESKTOP_CONFIG))
#define HD_DESKTOP_CONFIG_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_DESKTOP_CONFIG, HDDesktopConfigClass))
#define HD_IS_DESKTOP_CONFIG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_DESKTOP_CONFIG))
#define HD_DESKTOP_CONFIG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_DESKTOP_CONFIG, HDDesktopConfigClass))

typedef struct _HDDesktopConfig        HDDesktopConfig;
typedef struct _HDDesktopConfigPrivate HDDesktopConfigPrivate;
typedef struct _HDDesktopConfigClass   HDDesktopConfigClass;

struct _HDDesktopConfig
{
  GObject parent_instance;

  HDDesktopConfigPrivate *priv;
};

struct _HDDesktopConfigClass
{
  GObjectClass parent_class;
};

GType hd_desktop_config_get_type (void) G_GNUC_CONST;

HDDesktopConfig   *hd_desktop_config_get (void);

const gchar *hd_desktop_config_get_tn_layout (void);

void hd_desktop_config_get_color (HDConfigColor type, ClutterColor *color);

const gchar *hd_desktop_config_get_font (HDConfigFont type);

G_END_DECLS

#endif /* __HD_DESKTOP_CONFIG_H__ */
