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
#include <gio/gio.h>

#include "hd-desktop-config.h"

#include "hd-comp-mgr.h"

#define HD_DEFAULT_COLOR "#FFFFFFFF"
#define HD_DEFAULT_FONT "CorisandeBold Bold 9"

#define HD_DESKTOP_CONFIG_PATH HD_CONF_DIR"/hildon-desktop.conf"

struct _HDDesktopConfigPrivate
{
  gchar *path,
	*system_font,
	*large_font,
	*small_font,
	*title_font,
	*tn_lib_path;

  ClutterColor  txt_color;
  ClutterColor  notif_txt_color;
  ClutterColor  notif_2txt_color;
  ClutterColor  ntxt_color;
  ClutterColor  bg_color;

  GFileMonitor *conf_monitor;
  GFile	       *path_file;
};

#define HD_DESKTOP_CONFIG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                HD_TYPE_DESKTOP_CONFIG, HDDesktopConfigPrivate))

G_DEFINE_TYPE (HDDesktopConfig, hd_desktop_config, G_TYPE_OBJECT);

static GObject *
hd_desktop_config_constructor (GType                  gtype,
                            	guint                  n_properties,
                            	GObjectConstructParam *properties);

static void hd_desktop_config_finalize (GObject *gobject);

static void hd_desktop_config_read_keys (HDDesktopConfigPrivate *priv);

static HDDesktopConfig *desktop_config = NULL;

static void
configuration_changed (HDDesktopConfig *self)
{
  HDDesktopConfigPrivate *priv;

  priv = HD_DESKTOP_CONFIG_GET_PRIVATE (self);

  hd_desktop_config_read_keys (priv);

  g_signal_emit_by_name (self, "configuration-changed", NULL);
}

static void
hd_desktop_config_class_init (HDDesktopConfigClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HDDesktopConfigPrivate));

  gobject_class->constructor = hd_desktop_config_constructor;
  gobject_class->finalize    = hd_desktop_config_finalize;

  g_signal_new ("configuration-changed", 
		G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                0, NULL, NULL, 
		g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE, 0);
}

static void
hd_desktop_config_init (HDDesktopConfig *self)
{
  HDDesktopConfigPrivate *priv;

  priv = HD_DESKTOP_CONFIG_GET_PRIVATE (self);

  priv->path = HD_DESKTOP_CONFIG_PATH;

  clutter_color_parse (HD_DEFAULT_COLOR,&(priv->txt_color));
  clutter_color_parse (HD_DEFAULT_COLOR,&(priv->ntxt_color));
  clutter_color_parse (HD_DEFAULT_COLOR,&(priv->bg_color));
  clutter_color_parse (HD_DEFAULT_COLOR,&(priv->notif_txt_color));
  clutter_color_parse (HD_DEFAULT_COLOR,&(priv->notif_2txt_color));

  priv->system_font = g_strdup (HD_DEFAULT_FONT); 
  priv->large_font  = g_strdup (HD_DEFAULT_FONT);
  priv->small_font  = g_strdup (HD_DEFAULT_FONT);
  priv->title_font  = g_strdup (HD_DEFAULT_FONT);
  priv->tn_lib_path = NULL;

  priv->path_file = g_file_new_for_path (priv->path);

  priv->conf_monitor =
    g_file_monitor_file (priv->path_file,
                         G_FILE_MONITOR_NONE,
                         NULL,NULL);

  g_signal_connect_swapped (G_OBJECT (priv->conf_monitor),
                    	    "changed",
                    	    G_CALLBACK (configuration_changed),
                    	    (gpointer)self);
}

static void
hd_desktop_config_hex_key (GKeyFile *keyfile,
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
hd_desktop_config_str_key (GKeyFile *keyfile,
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
    return tmp;
  else
    {
      g_error_free (*error);
      *error = NULL;
    }

  return NULL;
}			 

static void
hd_desktop_config_read_keys (HDDesktopConfigPrivate *priv)
{
  GKeyFile *keyfile;

  keyfile = g_key_file_new ();

  if (!g_key_file_load_from_file (keyfile,priv->path,0,NULL))
    goto finalize;
  
  GError *error = NULL;
  gchar *tmp;

  hd_desktop_config_hex_key (keyfile,
			   HDTC_KEY_DEFAULT_TXT_COLOR,
			   &(priv->txt_color),
			   &error);
	
  hd_desktop_config_hex_key (keyfile,
			   HDTC_KEY_SECONDARY_TXT_COLOR,
			   &(priv->ntxt_color),
			   &error);

  hd_desktop_config_hex_key (keyfile,
			   HDTC_KEY_DEFAULT_BG_COLOR,
			   &(priv->bg_color),
			   &error);
	
  hd_desktop_config_hex_key (keyfile,
			   HDTC_KEY_NOTIFICATION_TXT_COLOR,
			   &(priv->notif_txt_color),
			   &error);
	
  hd_desktop_config_hex_key (keyfile,
			     HDTC_KEY_NOTIFICATION_2TXT_COLOR,
			     &(priv->notif_2txt_color),
			     &error);

  tmp = 
    hd_desktop_config_str_key (keyfile,
			       HDTC_KEY_SYSTEM_FONT,
			       &error);

  if (tmp != NULL)
    priv->system_font = tmp;

  tmp = 
    hd_desktop_config_str_key (keyfile,
			       HDTC_KEY_LARGE_SYSTEM_FONT,
			       &error);

  if (tmp != NULL)
    priv->large_font = tmp;

  tmp = 
    hd_desktop_config_str_key (keyfile,
			       HDTC_KEY_SMALL_SYSTEM_FONT,
			       &error);

  if (tmp != NULL)
    priv->small_font = tmp;

  tmp = 
    hd_desktop_config_str_key (keyfile,
			       HDTC_KEY_TITLEBAR_FONT,
			       &error);

  if (tmp != NULL)
    priv->title_font = tmp;

  priv->tn_lib_path = 
    hd_desktop_config_str_key (keyfile,
			       HDTC_KEY_TN_LAYOUT,
			       &error);

  if (priv->tn_lib_path != NULL && error == NULL)
    {
      gchar *to_be_freed = priv->tn_lib_path;

      priv->tn_lib_path =
        g_strdup_printf ("%s/%s",
			 "/usr/lib/hildon-desktop",
			 priv->tn_lib_path);

      g_free (to_be_freed);
    }

finalize:
  g_key_file_free (keyfile);
}

static void
hd_desktop_config_finalize (GObject *object)
{
  HDDesktopConfigPrivate *priv;

  priv = HD_DESKTOP_CONFIG_GET_PRIVATE (desktop_config);

  g_free (priv->system_font); 
  g_free (priv->large_font);
  g_free (priv->small_font);
  g_free (priv->title_font);

  if (priv->tn_lib_path != NULL)
    g_free (priv->tn_lib_path);
 
  if (priv->path_file != NULL)
    g_object_unref (priv->path_file);

  G_OBJECT_CLASS (hd_desktop_config_parent_class)->finalize (object);  
}

static GObject *
hd_desktop_config_constructor (GType                  gtype,
                            	guint                  n_properties,
                            	GObjectConstructParam *properties)
{
  GObject *obj;
  HDDesktopConfigPrivate *priv;
  
  obj = 
    G_OBJECT_CLASS(hd_desktop_config_parent_class)->constructor (gtype,
								  n_properties,
								  properties);
  priv = 
    HD_DESKTOP_CONFIG_GET_PRIVATE (HD_DESKTOP_CONFIG (obj));

  hd_desktop_config_read_keys (priv);

  return obj;
}

HDDesktopConfig *
hd_desktop_config_get (void)
{
  if (G_UNLIKELY (!desktop_config))
    desktop_config = g_object_new (HD_TYPE_DESKTOP_CONFIG, NULL);

  return desktop_config;
}

void 
hd_desktop_config_get_color (HDConfigColor type, ClutterColor *color)
{
  HDDesktopConfigPrivate *priv;

  g_assert (desktop_config != NULL);

  priv = HD_DESKTOP_CONFIG_GET_PRIVATE (desktop_config);

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
hd_desktop_config_get_tn_layout (void)
{
  HDDesktopConfigPrivate *priv;

  g_return_val_if_fail (desktop_config != NULL, NULL);

  priv = HD_DESKTOP_CONFIG_GET_PRIVATE (desktop_config);

  return priv->tn_lib_path;
}

const gchar *
hd_desktop_config_get_font (HDConfigFont type)
{
  HDDesktopConfigPrivate *priv;

  g_assert (desktop_config != NULL);

  priv = HD_DESKTOP_CONFIG_GET_PRIVATE (desktop_config);

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

