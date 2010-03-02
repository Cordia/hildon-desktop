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
 * An HDThemeConfig is a singleton GObject that manages the size configuration
 * of all the elements in the theme_config.
 */

#ifndef __HD_THEME_CONFIG_H__
#define __HD_THEME_CONFIG_H__

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

/* Defines hildon-desktop.desktop */

#define HDTC_KEY_GROUP			 "Hildon Desktop Config"
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

#define HD_TYPE_THEME_CONFIG            (hd_theme_config_get_type ())
#define HD_THEME_CONFIG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_THEME_CONFIG, HDThemeConfig))
#define HD_IS_THEME_CONFIG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_THEME_CONFIG))
#define HD_THEME_CONFIG_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_THEME_CONFIG, HDThemeConfigClass))
#define HD_IS_THEME_CONFIG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_THEME_CONFIG))
#define HD_THEME_CONFIG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_THEME_CONFIG, HDThemeConfigClass))

typedef struct _HDThemeConfig        HDThemeConfig;
typedef struct _HDThemeConfigPrivate HDThemeConfigPrivate;
typedef struct _HDThemeConfigClass   HDThemeConfigClass;

struct _HDThemeConfig
{
  GObject parent_instance;

  HDThemeConfigPrivate *priv;
};

struct _HDThemeConfigClass
{
  GObjectClass parent_class;
};

GType hd_theme_config_get_type (void) G_GNUC_CONST;

HDThemeConfig   *hd_theme_config_get (void);

void hd_theme_config_get_color (HDConfigColor type, ClutterColor *color);

const gchar *hd_theme_config_get_font (HDConfigFont type);

G_END_DECLS

#endif /* __HD_THEME_CONFIG_H__ */
