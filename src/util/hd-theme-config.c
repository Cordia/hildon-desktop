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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <string.h>

#include "hd-theme-config.h"

#include "hd-comp-mgr.h"

#define HD_DEFAULT_COLOR "FFFFFFFF"
#define HD_DEFAULT_FONT "CorisandeBold"

#define HD_THEME_CONFIG_PATH "/usr/share/themes/default/hildon-desktop.desktop"

struct _HDThemeConfigPrivate
{
  gchar *path,
	*system_font,
	*large_font,
	*small_font,
	*title_font;

  ClutterColor  txt_color;
  ClutterColor  notif_txt_color;
  ClutterColor  notif_2txt_color;
  ClutterColor  ntxt_color;
  ClutterColor  bg_color;

};

#define HD_THEME_CONFIG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                HD_TYPE_THEME_CONFIG, HDThemeConfigPrivate))

G_DEFINE_TYPE (HDThemeConfig, hd_theme_config, G_TYPE_OBJECT);

static GObject *
hd_theme_config_constructor (GType                  gtype,
                            	guint                  n_properties,
                            	GObjectConstructParam *properties);

static void hd_theme_config_finalize (GObject *gobject);

static HDThemeConfig *theme_config = NULL;

static void
hd_theme_config_class_init (HDThemeConfigClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HDThemeConfigPrivate));

  gobject_class->constructor = hd_theme_config_constructor;
  gobject_class->finalize    = hd_theme_config_finalize;
}

static void
hd_theme_config_init (HDThemeConfig *self)
{
  HDThemeConfigPrivate *priv;

  priv = HD_THEME_CONFIG_GET_PRIVATE (self);

  priv->path = HD_THEME_CONFIG_PATH;

  clutter_color_parse (HD_DEFAULT_COLOR,&(priv->txt_color));
  clutter_color_parse (HD_DEFAULT_COLOR,&(priv->ntxt_color));
  clutter_color_parse (HD_DEFAULT_COLOR,&(priv->bg_color));
  clutter_color_parse (HD_DEFAULT_COLOR,&(priv->notif_txt_color));
  clutter_color_parse (HD_DEFAULT_COLOR,&(priv->notif_2txt_color));

  priv->system_font = g_strdup (HD_DEFAULT_FONT); 
  priv->large_font  = g_strdup (HD_DEFAULT_FONT);
  priv->small_font  = g_strdup (HD_DEFAULT_FONT);
  priv->title_font  = g_strdup (HD_DEFAULT_FONT);
}

static void
hd_theme_config_hex_key (GKeyFile *keyfile,
			    gchar *key, 
			    ClutterColor *real, 
			    GError **error)
{
  gchar *tmp;

  tmp = 
    g_key_file_get_string (keyfile,
			   HDTC_KEY_GROUP,
			   key,
			   &(*error));
  
  if (!*error)
  { 
    if (!clutter_color_parse (tmp, real))
      g_debug ("Impossible to parse color %s", tmp);
    g_free (tmp);
  }
  else
  {
    g_error_free (*error);
    *error = NULL;
  }
}

static gchar *
hd_theme_config_str_key (GKeyFile *keyfile,
			 gchar *key,
			 GError **error)
{
  gchar *tmp;

  tmp = 
    g_key_file_get_string (keyfile,
			   HDTC_KEY_GROUP,
			   key,
			   &(*error));
  
  if (!*error)
  { 
    return tmp;
  }
  else
  {
    g_error_free (*error);
    *error = NULL;
  }

  return NULL;
}			 

static void
hd_theme_config_read_keys (HDThemeConfigPrivate *priv)
{
  GKeyFile *keyfile;

  keyfile = g_key_file_new ();

  if (!g_key_file_load_from_file (keyfile,priv->path,0,NULL))
    goto finalize;
  
  GError *error = NULL;
  gchar *tmp;

  hd_theme_config_hex_key (keyfile,
			   HDTC_KEY_DEFAULT_TXT_COLOR,
			   &(priv->txt_color),
			   &error);
	
  hd_theme_config_hex_key (keyfile,
			   HDTC_KEY_SECONDARY_TXT_COLOR,
			   &(priv->ntxt_color),
			   &error);

  hd_theme_config_hex_key (keyfile,
			   HDTC_KEY_DEFAULT_BG_COLOR,
			   &(priv->bg_color),
			   &error);
	
  hd_theme_config_hex_key (keyfile,
			   HDTC_KEY_NOTIFICATION_TXT_COLOR,
			   &(priv->notif_txt_color),
			   &error);
	
  hd_theme_config_hex_key (keyfile,
			   HDTC_KEY_NOTIFICATION_2TXT_COLOR,
			   &(priv->notif_2txt_color),
			   &error);

  tmp = 
    hd_theme_config_str_key (keyfile,
			     HDTC_KEY_SYSTEM_FONT,
			     &error);

  if (tmp != NULL)
    priv->system_font = tmp;

  tmp = 
    hd_theme_config_str_key (keyfile,
			     HDTC_KEY_LARGE_SYSTEM_FONT,
			     &error);

  if (tmp != NULL)
    priv->large_font = tmp;

  tmp = 
    hd_theme_config_str_key (keyfile,
			     HDTC_KEY_SMALL_SYSTEM_FONT,
			     &error);

  if (tmp != NULL)
    priv->small_font = tmp;

  tmp = 
    hd_theme_config_str_key (keyfile,
			     HDTC_KEY_TITLEBAR_FONT,
			     &error);

  if (tmp != NULL)
    priv->title_font = tmp;

finalize:
  g_key_file_free (keyfile);
}

static void
hd_theme_config_finalize (GObject *object)
{
  HDThemeConfigPrivate *priv;

  priv = HD_THEME_CONFIG_GET_PRIVATE (theme_config);

  g_free (priv->system_font); 
  g_free (priv->large_font);
  g_free (priv->small_font);
  g_free (priv->title_font);

  G_OBJECT_CLASS(hd_theme_config_parent_class)->finalize (object);  
}

static GObject *
hd_theme_config_constructor (GType                  gtype,
                            	guint                  n_properties,
                            	GObjectConstructParam *properties)
{
  GObject *obj;
  HDThemeConfigPrivate *priv;
  
  obj = 
    G_OBJECT_CLASS(hd_theme_config_parent_class)->constructor (gtype,
								  n_properties,
								  properties);
  priv = 
    HD_THEME_CONFIG_GET_PRIVATE (HD_THEME_CONFIG (obj));

  hd_theme_config_read_keys (priv);

  return obj;
}

HDThemeConfig *
hd_theme_config_get (void)
{
  if (G_UNLIKELY (!theme_config))
    theme_config = g_object_new (HD_TYPE_THEME_CONFIG, NULL);

  return theme_config;
}

void 
hd_theme_config_get_color (HDConfigColor type, ClutterColor *color)
{
  HDThemeConfigPrivate *priv;

  g_assert (theme_config != NULL);

  priv = HD_THEME_CONFIG_GET_PRIVATE (theme_config);

  ClutterColor *tmp = NULL;

  switch (type)
  {
    case HD_TXT_COLOR:
      tmp = &(priv->txt_color);
    case HD_2TXT_COLOR:
      tmp = &(priv->ntxt_color);
    case HD_NOTIFICATION_TXT_COLOR:
      tmp = &(priv->bg_color);
    case HD_NOTIFICATION_2TXT_COLOR:
      tmp = &(priv->notif_txt_color);
    case HD_BG_COLOR:
      tmp = &(priv->notif_2txt_color);
  }

  memcpy ((void *)color,(const void*)tmp, sizeof(ClutterColor));
}

const gchar *
hd_theme_config_get_font (HDConfigFont type)
{
  HDThemeConfigPrivate *priv;

  g_assert (theme_config != NULL);

  priv = HD_THEME_CONFIG_GET_PRIVATE (theme_config);

  gchar *tmp = NULL;

  switch (type)
  {
    case HD_SYSTEM_FONT:
      tmp = priv->system_font;
    case HD_LARGE_SYSTEM_FONT:
      tmp = priv->large_font;
    case HD_SMALL_SYSTEM_FONT:
      tmp = priv->small_font;
    case HD_TITLEBAR_FONT:
      tmp = priv->title_font; 
  }
 
  return tmp;
}

