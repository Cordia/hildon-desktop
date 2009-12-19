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

#include "hd-launcher-config.h"

#include "hd-comp-mgr.h"
/* Default values */

/* Fixed size */
#define HD_LAUNCHER_TILE_HEIGHT (100)
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

#define HD_LAUNCHER_LEFT_MARGIN (68)
#define HD_LAUNCHER_RIGHT_MARGIN (68)

#define HD_LAUNCHER_DEFAULT_ICON  "tasklaunch_default_application"

#define HILDON_MARGIN_DEFAULT 8

#define HD_LAUNCHER_TILE_DEFAULT_FONT "CorisandeBold Bold 12"

#define HD_LAUNCHER_CONFIG_PATH "/usr/share/hildon-desktop/launcher.desktop"

struct _HdLauncherConfigPrivate
{
  gchar *path,
        *tile_font,
        *default_icon;

  gint   margin_left,
         margin_right,
         default_margin,
         tile_width,
         tile_height,
         real_icon_size,
         icon_size,
         glow_size,
         max_drag,
         max_columns;
};

#define HD_LAUNCHER_CONFIG_GET_PRIVATE(obj) \
            (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                          HD_TYPE_LAUNCHER_CONFIG, \
                                          HdLauncherConfigPrivate))

G_DEFINE_TYPE (HdLauncherConfig, hd_launcher_config, G_TYPE_OBJECT);

static GObject *
hd_launcher_config_constructor (GType                  gtype,
                                guint                  n_properties,
                                GObjectConstructParam *properties);

static void hd_launcher_config_finalize (GObject *gobject);

static HdLauncherConfig *launcher_config = NULL;

static void
hd_launcher_config_class_init (HdLauncherConfigClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdLauncherConfigPrivate));

  gobject_class->constructor = hd_launcher_config_constructor;
  gobject_class->finalize    = hd_launcher_config_finalize;
}

static void
hd_launcher_config_init (HdLauncherConfig *self)
{
  HdLauncherConfigPrivate *priv;

  priv = HD_LAUNCHER_CONFIG_GET_PRIVATE (self);

  priv->tile_width  = HD_LAUNCHER_TILE_WIDTH;
  priv->tile_height = HD_LAUNCHER_TILE_HEIGHT;

  priv->margin_left  = HD_LAUNCHER_LEFT_MARGIN;
  priv->margin_right = HD_LAUNCHER_RIGHT_MARGIN;

  priv->default_margin = HILDON_MARGIN_DEFAULT;

  priv->real_icon_size = HD_LAUNCHER_TILE_ICON_REAL_SIZE;
  priv->icon_size = priv->real_icon_size + 2;

  priv->glow_size = HD_LAUNCHER_TILE_GLOW_SIZE;
  priv->max_drag  = HD_LAUNCHER_TILE_MAX_DRAG;

  priv->tile_font = HD_LAUNCHER_TILE_DEFAULT_FONT;

  priv->default_icon = HD_LAUNCHER_DEFAULT_ICON;

  priv->path = HD_LAUNCHER_CONFIG_PATH;
}

static void
hd_launcher_config_int_key (GKeyFile *keyfile,
                            gchar *key,
                            gint *tmp,
                            gint *real,
                            GError **error)
{
  *tmp = -1;

  *tmp = g_key_file_get_integer (keyfile,
                                 HDLC_KEY_GROUP,
                                 key,
                                 &(*error));

  if (!*error)
  {
    *real = *tmp;
  }
  else
  {
    g_error_free (*error);
    *error = NULL;
  }
}

static void
hd_launcher_config_read_keys (HdLauncherConfigPrivate *priv)
{
  GKeyFile *keyfile;

  keyfile = g_key_file_new ();

  if (!g_key_file_load_from_file (keyfile, priv->path, 0, NULL))
    goto finalize;

  GError *error = NULL;
  gint  tmp_int;
  gchar *tmp_str;

  tmp_str = g_key_file_get_string (keyfile,
                                   HDLC_KEY_GROUP,
                                   HDLC_KEY_TILE_FONT,
                                   &error);

  if(!error)
  {
    priv->path = tmp_str;
  }
  else
  {
    g_error_free (error);
    error = NULL;
  }

  hd_launcher_config_int_key (keyfile,
                              HDLC_KEY_LEFT_MARGIN,
                              &tmp_int,
                              &(priv->margin_left),
                              &error);
  hd_launcher_config_int_key (keyfile,
                              HDLC_KEY_RIGHT_MARGIN,
                              &tmp_int,
                              &(priv->margin_right),
                              &error);
  hd_launcher_config_int_key (keyfile,
                              HDLC_KEY_DEFAULT_MARGIN,
                              &tmp_int,
                              &(priv->default_margin),
                              &error);
  hd_launcher_config_int_key (keyfile,
                              HDLC_KEY_TILE_WIDTH,
                              &tmp_int,
                              &(priv->tile_width),
                              &error);
  hd_launcher_config_int_key (keyfile,
                              HDLC_KEY_TILE_HEIGHT,
                              &tmp_int,
                              &(priv->tile_height),
                              &error);
  hd_launcher_config_int_key (keyfile,
                              HDLC_KEY_REAL_ICON_SIZE,
                              &tmp_int,
                              &(priv->real_icon_size),
                              &error);
   hd_launcher_config_int_key (keyfile,
                              HDLC_KEY_ICON_SIZE,
                              &tmp_int,
                              &(priv->icon_size),
                              &error);
   hd_launcher_config_int_key (keyfile,
                              HDLC_KEY_ICON_GLOW,
                              &tmp_int,
                              &(priv->glow_size),
                              &error);
   hd_launcher_config_int_key (keyfile,
                              HDLC_KEY_MAX_DRAG,
                              &tmp_int,
                              &(priv->max_drag),
                              &error);

finalize:
  g_key_file_free (keyfile);
}

static void
hd_launcher_config_finalize (GObject *object)
{
  HdLauncherConfigPrivate *priv;

  priv = HD_LAUNCHER_CONFIG_GET_PRIVATE (launcher_config);

  if (!g_str_equal (priv->path,HD_LAUNCHER_CONFIG_PATH))
    g_free (priv->path);

  G_OBJECT_CLASS(hd_launcher_config_parent_class)->finalize (object);  
}

static GObject *
hd_launcher_config_constructor (GType                  gtype,
                                guint                  n_properties,
                                GObjectConstructParam *properties)
{
  GObject *obj;
  HdLauncherConfigPrivate *priv;

  obj = G_OBJECT_CLASS(hd_launcher_config_parent_class)
                       ->constructor (gtype,
                                      n_properties,
                                      properties);
  priv = HD_LAUNCHER_CONFIG_GET_PRIVATE (HD_LAUNCHER_CONFIG (obj));

  hd_launcher_config_read_keys (priv);

  priv->max_columns = (gint) (HD_COMP_MGR_LANDSCAPE_WIDTH/priv->tile_width);

  return obj;
}

HdLauncherConfig *
hd_launcher_config_get (void)
{
  if (G_UNLIKELY (!launcher_config))
    launcher_config = g_object_new (HD_TYPE_LAUNCHER_CONFIG, NULL);

  return launcher_config;
}

void
hd_launcher_config_get_tile_size (gint *width, gint *height)
{
  HdLauncherConfigPrivate *priv;

  priv = HD_LAUNCHER_CONFIG_GET_PRIVATE (launcher_config);

  if (width != NULL)
    *width = priv->tile_width;

  if (height != NULL)
    *height = priv->tile_height;
}

void 
hd_launcher_config_get_margins_size (gint *left, gint *right)
{
  HdLauncherConfigPrivate *priv;

  priv = HD_LAUNCHER_CONFIG_GET_PRIVATE (launcher_config);

  if (left != NULL)
    *left = priv->margin_left;

  if (right != NULL)
    *right = priv->margin_right;
}

void 
hd_launcher_config_get_icons_size (gint *real, gint *size, gint *glow)
{
  HdLauncherConfigPrivate *priv;

  priv = HD_LAUNCHER_CONFIG_GET_PRIVATE (launcher_config);

  if (real != NULL)
    *real = priv->real_icon_size;

  if (size != NULL)
    *size = priv->icon_size;

  if (glow != NULL)
    *glow = priv->glow_size;
}

const gchar *
hd_launcher_config_get_tile_font (void)
{
  HdLauncherConfigPrivate *priv;

  priv = HD_LAUNCHER_CONFIG_GET_PRIVATE (launcher_config);

  return priv->tile_font;
}

const gchar *
hd_launcher_config_get_default_icon (void)
{
  HdLauncherConfigPrivate *priv;

  priv = HD_LAUNCHER_CONFIG_GET_PRIVATE (launcher_config);

  return priv->default_icon;
}

gint
hd_launcher_config_get_max_drag (void)
{
  HdLauncherConfigPrivate *priv;

  priv = HD_LAUNCHER_CONFIG_GET_PRIVATE (launcher_config);

  return priv->max_drag;
}

gint
hd_launcher_config_get_columns (void)
{
  HdLauncherConfigPrivate *priv;

  priv = HD_LAUNCHER_CONFIG_GET_PRIVATE (launcher_config);

  return priv->max_columns;
}

gint
hd_launcher_config_get_default_margin (void)
{
  HdLauncherConfigPrivate *priv;

  priv = HD_LAUNCHER_CONFIG_GET_PRIVATE (launcher_config);

  return priv->default_margin;
}
