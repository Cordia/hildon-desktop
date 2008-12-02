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

#include "hd-launcher-cat.h"

G_DEFINE_TYPE (HdLauncherCat, hd_launcher_cat, HD_TYPE_LAUNCHER_ITEM);

static void
hd_launcher_cat_finalize (GObject *gobject)
{
  G_OBJECT_CLASS (hd_launcher_cat_parent_class)->finalize (gobject);
}

static gboolean
hd_launcher_cat_parse_keyfile (HdLauncherItem *item,
                               GKeyFile *key_file,
                               GError **error)
{
  return TRUE;
}

static void
hd_launcher_cat_class_init (HdLauncherCatClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  HdLauncherItemClass *launcher_class = HD_LAUNCHER_ITEM_CLASS (klass);

  gobject_class->finalize = hd_launcher_cat_finalize;

  launcher_class->parse_key_file = hd_launcher_cat_parse_keyfile;
}

static void
hd_launcher_cat_init (HdLauncherCat *launcher)
{
}
