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
 * A HdLauncherCat contains the info on a launcher category.
 *
 * This code is based on the old hd-dummy-launcher.
 */

#include "hildon-desktop.h"
#include "hd-launcher-app.h"

#include <string.h>

/* desktop entry keys */
#define HD_DESKTOP_ENTRY_EXEC           "Exec"
#define HD_DESKTOP_ENTRY_SERVICE        "X-Osso-Service"
#define HD_DESKTOP_ENTRY_LOADING_IMAGE  "X-App-Loading-Image"
#define HD_DESKTOP_ENTRY_PRESTART_MODE  "X-Maemo-Prestarted"
#define HD_DESKTOP_ENTRY_WM_CLASS       "X-Maemo-Wm-Class"
#define HD_DESKTOP_ENTRY_PRIORITY       "X-Maemo-Prestarted-Priority"
#define HD_DESKTOP_ENTRY_SWITCHER_ICON  "X-Maemo-Switcher-Icon"
#define HD_DESKTOP_ENTRY_IGNORE_LOWMEM  "X-Maemo-Ignore-Lowmem"

/* DBus names */
#define OSSO_BUS_ROOT          "com.nokia"
#define OSSO_BUS_TOP           "top_application"

#define HD_LAUNCHER_APP_GET_PRIVATE(obj)        (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HD_TYPE_LAUNCHER_APP, HdLauncherAppPrivate))

struct _HdLauncherAppPrivate
{
  gchar *exec;
  gchar *service;
  gchar *loading_image;
  gchar *switcher_icon;
  gchar *wm_class;

  HdLauncherAppPrestartMode prestart_mode;

  gint priority;
  gboolean ignore_lowmem;
};

G_DEFINE_TYPE (HdLauncherApp, hd_launcher_app, HD_TYPE_LAUNCHER_ITEM);

gboolean hd_launcher_app_parse_keyfile (HdLauncherItem  *item,
                                        GKeyFile        *key_file,
                                        GError          **error);

static void
hd_launcher_app_finalize (GObject *gobject)
{
  HdLauncherAppPrivate *priv = HD_LAUNCHER_APP (gobject)->priv;

  g_free (priv->exec);
  g_free (priv->service);
  g_free (priv->loading_image);
  g_free (priv->switcher_icon);
  g_free (priv->wm_class);

  G_OBJECT_CLASS (hd_launcher_app_parent_class)->finalize (gobject);
}

static void
hd_launcher_app_class_init (HdLauncherAppClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  HdLauncherItemClass *launcher_class = HD_LAUNCHER_ITEM_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdLauncherAppPrivate));

  gobject_class->finalize = hd_launcher_app_finalize;
  launcher_class->parse_key_file = hd_launcher_app_parse_keyfile;
}

static void
hd_launcher_app_init (HdLauncherApp *launcher)
{
  launcher->priv = HD_LAUNCHER_APP_GET_PRIVATE (launcher);
}

static HdLauncherAppPrestartMode
hd_launcher_app_parse_prestart_mode (gchar *mode)
{
  HdLauncherAppPrestartMode res = HD_APP_PRESTART_USAGE;

  if (!mode)
    return HD_APP_PRESTART_NONE;

  if (g_ascii_strcasecmp (mode, HD_APP_PRESTART_USAGE_STRING) == 0)
    res = HD_APP_PRESTART_USAGE;

  if (g_ascii_strcasecmp (mode, HD_APP_PRESTART_ALWAYS_STRING) == 0)
    res = HD_APP_PRESTART_ALWAYS;

  g_free (mode);
  return res;
}

static gchar*
hd_launcher_app_parse_service_name (gchar *name)
{
  if (name == NULL)
    return name;

  gchar *res;

  g_strchomp (name);
  /* Check it's a complete service name */
  if (g_strrstr(name, "."))
    res = name;
  else /* use com.nokia prefix */
  {
    res = g_strdup_printf("%s.%s", OSSO_BUS_ROOT, name);
    g_free (name);
  }

  return res;
}

gboolean
hd_launcher_app_parse_keyfile (HdLauncherItem *item,
                               GKeyFile       *key_file,
                               GError         **error)
{
  HdLauncherAppPrivate *priv;

  g_return_val_if_fail (HD_IS_LAUNCHER_APP (item), FALSE);
  g_return_val_if_fail (key_file != NULL, FALSE);

  priv = HD_LAUNCHER_APP_GET_PRIVATE (item);

  priv->service =
    hd_launcher_app_parse_service_name (g_key_file_get_string (key_file,
                                          HD_DESKTOP_ENTRY_GROUP,
                                          HD_DESKTOP_ENTRY_SERVICE,
                                          NULL));

  priv->exec = g_key_file_get_string (key_file,
                                      HD_DESKTOP_ENTRY_GROUP,
                                      HD_DESKTOP_ENTRY_EXEC,
                                      NULL);
  if (priv->exec)
    g_strchomp (priv->exec);

  priv->loading_image = g_key_file_get_string (key_file,
                                               HD_DESKTOP_ENTRY_GROUP,
                                               HD_DESKTOP_ENTRY_LOADING_IMAGE,
                                               NULL);

  priv->switcher_icon = g_key_file_get_string (key_file,
                                               HD_DESKTOP_ENTRY_GROUP,
                                               HD_DESKTOP_ENTRY_SWITCHER_ICON,
                                               NULL);

  priv->prestart_mode =
    hd_launcher_app_parse_prestart_mode (g_key_file_get_string (key_file,
                                           HD_DESKTOP_ENTRY_GROUP,
                                           HD_DESKTOP_ENTRY_PRESTART_MODE,
                                           NULL));

  priv->wm_class = g_key_file_get_string (key_file,
                                          HD_DESKTOP_ENTRY_GROUP,
                                          HD_DESKTOP_ENTRY_WM_CLASS,
                                          NULL);

  priv->priority = g_key_file_get_integer (key_file,
                                           HD_DESKTOP_ENTRY_GROUP,
                                           HD_DESKTOP_ENTRY_PRIORITY,
                                           NULL);

  priv->ignore_lowmem = g_key_file_get_boolean (key_file,
                                                HD_DESKTOP_ENTRY_GROUP,
                                                HD_DESKTOP_ENTRY_IGNORE_LOWMEM,
                                                NULL);

  return TRUE;
}

G_CONST_RETURN gchar *
hd_launcher_app_get_exec (HdLauncherApp *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_APP (item), NULL);

  return item->priv->exec;
}

G_CONST_RETURN gchar *
hd_launcher_app_get_service (HdLauncherApp *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_APP (item), NULL);

  return item->priv->service;
}

G_CONST_RETURN gchar *
hd_launcher_app_get_loading_image (HdLauncherApp *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_APP (item), NULL);

  return item->priv->loading_image;
}

G_CONST_RETURN gchar *
hd_launcher_app_get_switcher_icon (HdLauncherApp *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_APP (item), NULL);

  return item->priv->switcher_icon;
}

G_CONST_RETURN gchar *
hd_launcher_app_get_wm_class (HdLauncherApp *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_APP (item), NULL);

  return item->priv->wm_class;
}

HdLauncherAppPrestartMode
hd_launcher_app_get_prestart_mode (HdLauncherApp *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_APP (item), HD_APP_PRESTART_NONE);

  return item->priv->prestart_mode;
}

gint
hd_launcher_app_get_priority (HdLauncherApp *app)
{
  HdLauncherAppPrivate *priv = HD_LAUNCHER_APP_GET_PRIVATE (app);
  return priv->priority;
}

gboolean
hd_launcher_app_get_ignore_lowmem (HdLauncherApp *app)
{
  HdLauncherAppPrivate *priv = HD_LAUNCHER_APP_GET_PRIVATE (app);
  return priv->ignore_lowmem;
}

gboolean hd_launcher_app_match_window (HdLauncherApp *app,
                                       const gchar *res_name,
                                       const gchar *res_class)
{
  g_return_val_if_fail(app, FALSE);

  HdLauncherAppPrivate *priv = HD_LAUNCHER_APP_GET_PRIVATE (app);

  if (!res_name && !res_class)
    return FALSE;

  if (res_class &&
      priv->wm_class &&
      g_strcmp0 (priv->wm_class, res_class) == 0)
    return TRUE;

  /* Now try the app's id with the class name, ignoring case. */
  if (res_class &&
      g_ascii_strncasecmp (res_class,
          hd_launcher_item_get_id (HD_LAUNCHER_ITEM (app)),
          strlen (res_class)) == 0)
    return TRUE;

  /* Try the executable as a last resource. */
  if (res_name &&
      g_strcmp0 (res_name, priv->exec) == 0)
    return TRUE;

  return FALSE;
}
