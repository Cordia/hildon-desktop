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
 * A HdLauncherItem contains the info on a launcher entry.
 *
 * This code is based on the old hd-launcher-item.
 */

#ifndef __HD_LAUNCHER_ITEM_H__
#define __HD_LAUNCHER_ITEM_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define HD_TYPE_LAUNCHER_ITEM_TYPE       (hd_launcher_item_type_get_type ())
#define HD_TYPE_LAUNCHER_ITEM            (hd_launcher_item_get_type ())
#define HD_LAUNCHER_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_LAUNCHER_ITEM, HdLauncherItem))
#define HD_IS_LAUNCHER_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_LAUNCHER_ITEM))
#define HD_LAUNCHER_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_LAUNCHER_ITEM, HdLauncherItemClass))
#define HD_IS_LAUNCHER_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_LAUNCHER_ITEM))
#define HD_LAUNCHER_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_LAUNCHER_ITEM, HdLauncherItemClass))

/* Desktop file entries */
#define HD_DESKTOP_ENTRY_GROUP         "Desktop Entry"
/* Standard categories */
#define HD_LAUNCHER_ITEM_DEFAULT_CATEGORY "Applications"
#define HD_LAUNCHER_ITEM_TOP_CATEGORY     "Main"

typedef enum {
  HD_APPLICATION_LAUNCHER,
  HD_CATEGORY_LAUNCHER
} HdLauncherItemType;

typedef struct _HdLauncherItem          HdLauncherItem;
typedef struct _HdLauncherItemPrivate   HdLauncherItemPrivate;
typedef struct _HdLauncherItemClass     HdLauncherItemClass;

struct _HdLauncherItem
{
  GObject parent_instance;

  HdLauncherItemPrivate *priv;
};

struct _HdLauncherItemClass
{
  GObjectClass parent_class;

  /* Virtual methods */
  gboolean (* parse_key_file) (HdLauncherItem *item,
                               GKeyFile *key_file,
                               GError **error);
};

GType              hd_launcher_item_type_get_type (void) G_GNUC_CONST;
GType              hd_launcher_item_get_type      (void) G_GNUC_CONST;

HdLauncherItem *   hd_launcher_item_new_from_keyfile (const gchar *id,
                                                      const gchar *category,
                                                      GKeyFile *key_file,
                                                      GError   **error);
const gchar *      hd_launcher_item_get_id           (HdLauncherItem *item);
GQuark             hd_launcher_item_get_id_quark     (HdLauncherItem *item);
HdLauncherItemType hd_launcher_item_get_item_type    (HdLauncherItem *item);
const gchar *      hd_launcher_item_get_name         (HdLauncherItem *item);
const gchar *      hd_launcher_item_get_local_name   (HdLauncherItem *item);
const gchar *      hd_launcher_item_get_icon_name    (HdLauncherItem *item);
const gchar *      hd_launcher_item_get_comment      (HdLauncherItem *item);
const gchar *      hd_launcher_item_get_text_domain  (HdLauncherItem *item);
const gchar *      hd_launcher_item_get_category     (HdLauncherItem *item);

G_END_DECLS

#endif /* __HD_LAUNCHER_ITEM_H__ */
