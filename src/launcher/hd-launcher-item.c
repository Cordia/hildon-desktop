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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hd-launcher.h"
#include "hd-launcher-item.h"
#include "hd-launcher-app.h"
#include "hd-launcher-cat.h"

#define I_(str) (g_intern_static_string ((str)))
#define HD_PARAM_READ (G_PARAM_READABLE    | \
                       G_PARAM_STATIC_NICK | \
                       G_PARAM_STATIC_NAME | \
                       G_PARAM_STATIC_BLURB)

GType
hd_launcher_item_type_get_type (void)
{
  static GType gtype = 0;

  if (G_UNLIKELY (gtype == 0))
    {
      static GEnumValue values[] = {
        { HD_APPLICATION_LAUNCHER, "HdLauncherApp", "Application" },
        { HD_CATEGORY_LAUNCHER, "HdLauncherCat", "Category" },
        { 0, NULL, NULL }
      };

      gtype = g_enum_register_static (I_("HdLauncherItemType"), values);
    }

  return gtype;
}

#define HD_LAUNCHER_ITEM_GET_PRIVATE(obj)       (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HD_TYPE_LAUNCHER_ITEM, HdLauncherItemPrivate))

struct _HdLauncherItemPrivate
{
  HdLauncherItemType item_type;
  gchar *id;
  gchar *name;
  gchar *icon_name;
  gchar *comment;
  gchar *text_domain;
  gboolean nodisplay;

  gchar *category;
  guint position;
};

enum
{
  PROP_0,

  PROP_LAUNCHER_ITEM_TYPE,
  PROP_LAUNCHER_ITEM_ID,
  PROP_LAUNCHER_ITEM_NAME,
  PROP_LAUNCHER_ITEM_ICON_NAME,
};

G_DEFINE_ABSTRACT_TYPE (HdLauncherItem, hd_launcher_item, G_TYPE_OBJECT);

/* Desktop file entries */
#define HD_DESKTOP_ENTRY_TYPE          "Type"
#define HD_DESKTOP_ENTRY_NAME          "Name"
#define HD_DESKTOP_ENTRY_ICON          "Icon"
#define HD_DESKTOP_ENTRY_COMMENT       "Comment"
#define HD_DESKTOP_ENTRY_CATEGORY      "X-Maemo-Category"
#define HD_DESKTOP_ENTRY_TEXT_DOMAIN   "X-Text-Domain"
#define HD_DESKTOP_ENTRY_NO_DISPLAY    "NoDisplay"
#define HD_DESKTOP_ENTRY_USER_POSITION "X-Osso-User-Position"

/* Forward declarations */
gboolean hd_launcher_item_parse_keyfile (HdLauncherItem *item,
                                         GKeyFile *key_file,
                                         GError **error);

static void
hd_launcher_item_finalize (GObject *gobject)
{
  HdLauncherItemPrivate *priv = HD_LAUNCHER_ITEM_GET_PRIVATE (gobject);

  if (priv->name)
    {
      g_free (priv->name);
      priv->name = NULL;
    }
  if (priv->icon_name)
    {
      g_free (priv->icon_name);
      priv->icon_name = NULL;
    }
  if (priv->comment)
    {
      g_free (priv->comment);
      priv->comment = NULL;
    }
  if (priv->text_domain)
    {
      g_free (priv->text_domain);
      priv->text_domain = NULL;
    }

  if (priv->category)
    {
      g_free (priv->category);
      priv->category = NULL;
    }

  G_OBJECT_CLASS (hd_launcher_item_parent_class)->finalize (gobject);
}

static void
hd_launcher_item_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  HdLauncherItem *item = HD_LAUNCHER_ITEM (object);

  switch (property_id)
    {
    case PROP_LAUNCHER_ITEM_TYPE:
      g_value_set_enum (value,
          hd_launcher_item_get_item_type (item));
      break;

    case PROP_LAUNCHER_ITEM_ID:
      g_value_set_string (value,
          hd_launcher_item_get_id (item));
      break;

    case PROP_LAUNCHER_ITEM_NAME:
      g_value_set_string (value,
          hd_launcher_item_get_name (item));
      break;

    case PROP_LAUNCHER_ITEM_ICON_NAME:
      g_value_set_string (value,
          hd_launcher_item_get_icon_name (item));
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
hd_launcher_item_class_init (HdLauncherItemClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (HdLauncherItemPrivate));

  gobject_class->get_property = hd_launcher_item_get_property;
  gobject_class->finalize     = hd_launcher_item_finalize;

  pspec = g_param_spec_enum ("launcher-type",
                             "Launcher Type",
                             "Type of the launcher",
                             HD_TYPE_LAUNCHER_ITEM_TYPE,
                             HD_APPLICATION_LAUNCHER,
                             HD_PARAM_READ);
  g_object_class_install_property (gobject_class, PROP_LAUNCHER_ITEM_TYPE, pspec);
  pspec = g_param_spec_string ("id",
                               "Id",
                               "Unique id of the item",
                               "Unknown",
                               HD_PARAM_READ);
  g_object_class_install_property (gobject_class, PROP_LAUNCHER_ITEM_ID, pspec);
  pspec = g_param_spec_string ("name",
                               "Name",
                               "Name of the item, before i18n",
                               "Unknown",
                               HD_PARAM_READ);
  g_object_class_install_property (gobject_class, PROP_LAUNCHER_ITEM_NAME, pspec);
  pspec = g_param_spec_string ("icon-name",
                               "Icon Name",
                               "Name of the icon to display",
                               HD_LAUNCHER_DEFAULT_ICON,
                               HD_PARAM_READ);
  g_object_class_install_property (gobject_class, PROP_LAUNCHER_ITEM_ICON_NAME, pspec);
}

static void
hd_launcher_item_init (HdLauncherItem *item)
{
  HdLauncherItemPrivate *priv;

  item->priv = priv = HD_LAUNCHER_ITEM_GET_PRIVATE (item);

  priv->item_type = HD_APPLICATION_LAUNCHER;
}

HdLauncherItemType
hd_launcher_item_get_item_type (HdLauncherItem *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_ITEM (item), 0);

  return item->priv->item_type;
}

const gchar *
hd_launcher_item_get_id (HdLauncherItem *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_ITEM (item), NULL);

  return item->priv->id;
}

const gchar *
hd_launcher_item_get_name (HdLauncherItem *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_ITEM (item), NULL);

  return item->priv->name;
}

const gchar *
hd_launcher_item_get_icon_name (HdLauncherItem *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_ITEM (item), 0);

  return item->priv->icon_name;
}

const gchar *
hd_launcher_item_get_comment (HdLauncherItem *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_ITEM (item), 0);

  return item->priv->comment;
}

const gchar *
hd_launcher_item_get_text_domain (HdLauncherItem *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_ITEM (item), 0);

  return item->priv->text_domain;
}

const gchar *
hd_launcher_item_get_category (HdLauncherItem *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_ITEM (item), 0);

  return item->priv->category;
}

guint
hd_launcher_item_get_position (HdLauncherItem *item)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_ITEM (item), 0);

  return item->priv->position;
}

gboolean
hd_launcher_item_parse_keyfile (HdLauncherItem *item,
                                GKeyFile *key_file,
                                GError **error)
{
  HdLauncherItemPrivate *priv = HD_LAUNCHER_ITEM_GET_PRIVATE (item);

  priv->name = g_key_file_get_string (key_file,
                                      HD_DESKTOP_ENTRY_GROUP,
                                      HD_DESKTOP_ENTRY_NAME,
                                      NULL);
  if (!priv->name)
    {
      g_free (priv->name);
      return FALSE;
    }

  priv->icon_name = g_key_file_get_string (key_file,
                                           HD_DESKTOP_ENTRY_GROUP,
                                           HD_DESKTOP_ENTRY_ICON,
                                           NULL);
  priv->comment = g_key_file_get_string (key_file,
                                         HD_DESKTOP_ENTRY_GROUP,
                                         HD_DESKTOP_ENTRY_COMMENT,
                                         NULL);
  priv->text_domain = g_key_file_get_string (key_file,
                                             HD_DESKTOP_ENTRY_GROUP,
                                             HD_DESKTOP_ENTRY_TEXT_DOMAIN,
                                             NULL);
  priv->category = g_key_file_get_string (key_file,
                                          HD_DESKTOP_ENTRY_GROUP,
                                          HD_DESKTOP_ENTRY_CATEGORY,
                                          NULL);
  if (!priv->category)
    priv->category = g_strdup (HD_LAUNCHER_ITEM_DEFAULT_CATEGORY);

  priv->position = g_key_file_get_integer (key_file,
                                           HD_DESKTOP_ENTRY_GROUP,
                                           HD_DESKTOP_ENTRY_USER_POSITION,
                                           NULL);
  if (priv->position == 0)
    priv->position = 1000;

  return TRUE;
}

static guint
_hd_launcher_app_main_position (const gchar *id)
{
  static GData *main_apps = NULL;
  if (!main_apps)
    {
      g_datalist_init (&main_apps);

      g_datalist_set_data (&main_apps, "browser", (gpointer)1);
      g_datalist_set_data (&main_apps, "mediaplayer", (gpointer)2);
      g_datalist_set_data (&main_apps, "calendar", (gpointer)3);
      g_datalist_set_data (&main_apps, "image-viewer", (gpointer)4);
      g_datalist_set_data (&main_apps, "osso-addressbook", (gpointer)5);
      g_datalist_set_data (&main_apps, "rtcom-call-ui", (gpointer)6);
      g_datalist_set_data (&main_apps, "nokia-maps", (gpointer)7);
      g_datalist_set_data (&main_apps, "camera-ui", (gpointer)8);
      g_datalist_set_data (&main_apps, "modest", (gpointer)9);
      g_datalist_set_data (&main_apps, "rtcom-messaging-ui", (gpointer)10);
      g_datalist_set_data (&main_apps, "worldclock", (gpointer)11);
      g_datalist_set_data (&main_apps, "osso_calculator", (gpointer)12);
      g_datalist_set_data (&main_apps, "hildon-application-manager", (gpointer)13);
      g_datalist_set_data (&main_apps, "hildon-control-panel", (gpointer)14);
    }

  return (guint)g_datalist_get_data (&main_apps, id);
}

HdLauncherItem *
hd_launcher_item_new_from_keyfile (const gchar *id,
                                   GKeyFile *key_file,
                                   GError **error)
{
  HdLauncherItem *result;
  GError *parse_error = NULL;
  GType item_type;
  gboolean no_display;

  g_return_val_if_fail (key_file != NULL, NULL);

  if (!g_key_file_has_group (key_file, HD_DESKTOP_ENTRY_GROUP))
    return FALSE;

  if (!g_key_file_has_key (key_file,
                           HD_DESKTOP_ENTRY_GROUP,
                           HD_DESKTOP_ENTRY_TYPE,
                           &parse_error))
    {
      g_propagate_error (error, parse_error);
      return NULL;
    }

  if (!g_key_file_has_key (key_file,
                           HD_DESKTOP_ENTRY_GROUP,
                           HD_DESKTOP_ENTRY_NAME,
                           &parse_error))
    {
      g_propagate_error (error, parse_error);
      return NULL;
    }

  /* skip NoDisplay entries */
  no_display = g_key_file_get_boolean (key_file,
                                       HD_DESKTOP_ENTRY_GROUP,
                                       HD_DESKTOP_ENTRY_NO_DISPLAY,
                                       &parse_error);
  if (parse_error)
    g_clear_error (&parse_error);
  else if (no_display)
    return FALSE;

  gchar *type_name = g_key_file_get_string (key_file,
                                            HD_DESKTOP_ENTRY_GROUP,
                                            HD_DESKTOP_ENTRY_TYPE,
                                            NULL);
  GTypeClass *item_type_class = g_type_class_ref (HD_TYPE_LAUNCHER_ITEM_TYPE);
  GEnumValue *type_value = g_enum_get_value_by_nick (
                             G_ENUM_CLASS (item_type_class),
                             type_name);
  g_free (type_name);
  g_type_class_unref (item_type_class);
  if (!type_value)
    {
      return NULL;
    }
  if (type_value->value == HD_APPLICATION_LAUNCHER)
    item_type = HD_TYPE_LAUNCHER_APP;
  else
    item_type = HD_TYPE_LAUNCHER_CAT;

  result = g_object_new (item_type, NULL);
  if (!result)
    return NULL;

  result->priv->item_type = type_value->value;
  result->priv->id = g_strdup (id);
  if (!hd_launcher_item_parse_keyfile (result, key_file, error))
    {
      g_object_unref (result);
      return NULL;
    }

  if (!(HD_LAUNCHER_ITEM_GET_CLASS (result))->parse_key_file
            (result, key_file, error))
    {
      g_object_unref (result);
      return NULL;
    }

  /* FIXME: The list of apps in the main category is currently hard-coded
   * until we have freedesktop menu support or equivalent.
   */
  guint main_pos = _hd_launcher_app_main_position (id);
  if (main_pos) {
    if (result->priv->category)
      g_free (result->priv->category);
    result->priv->category = g_strdup (HD_LAUNCHER_ITEM_TOP_CATEGORY);
    result->priv->position = main_pos;
  }

  return result;
}
