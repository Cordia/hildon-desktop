/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Author:  Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
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

/**
 * SECTION:hda-launcher-tile
 * @short_description: Implementation of the ATK interfaces for a #HdLauncherTile
 * @see_also: #HdLauncherTile
 *
 * #HdaLauncherTile implements the required ATK interfaces of #HdLauncherTile
 */

#include <cail/cail-actor.h>

#include "launcher/hd-launcher-tile.h"
#include "hda-launcher-tile.h"

/* GObject */
static void
hda_launcher_tile_class_init                    (HdaLauncherTileClass *klass);

static void
hda_launcher_tile_init                          (HdaLauncherTile *root);

/* AtkObject.h */

static void
hda_launcher_tile_initialize                    (AtkObject *obj,
                                                 gpointer data);

static const gchar *
hda_launcher_tile_get_name                      (AtkObject *obj);

static const gchar *
hda_launcher_tile_get_description               (AtkObject *obj);

G_DEFINE_TYPE (HdaLauncherTile, hda_launcher_tile,  CAIL_TYPE_ACTOR)

static void
hda_launcher_tile_class_init                            (HdaLauncherTileClass *klass)
{
/*   GObjectClass *gobject_class = G_OBJECT_CLASS (klass); */
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  /* AtkObject */
  class->initialize = hda_launcher_tile_initialize;
  class->get_name = hda_launcher_tile_get_name;
  class->get_description = hda_launcher_tile_get_description;
}

static void
hda_launcher_tile_init                          (HdaLauncherTile *tile)
{
  /* nothing required */
}


AtkObject*
hda_launcher_tile_new                           (ClutterActor *tile)
{
  GObject *object = NULL;
  AtkObject *accessible = NULL;

  object = g_object_new (HDA_TYPE_LAUNCHER_TILE, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, tile);

  return accessible;
}

/* AtkObject */

static void
hda_launcher_tile_initialize                    (AtkObject   *obj,
                                                 gpointer    data)
{
  ATK_OBJECT_CLASS (hda_launcher_tile_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_PUSH_BUTTON;
}

static const gchar *
hda_launcher_tile_get_name                      (AtkObject *obj)
{
  const gchar *name = NULL;

  g_return_val_if_fail (HDA_IS_LAUNCHER_TILE (obj), NULL);

  name = ATK_OBJECT_CLASS (hda_launcher_tile_parent_class)->get_name (obj);
  if (name == NULL)
    {
      ClutterActor *actor = NULL;

      actor = CLUTTER_ACTOR (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (obj)));
      name = hd_launcher_tile_get_icon_name (HD_LAUNCHER_TILE (actor));
    }

  return name;
}

static const gchar *
hda_launcher_tile_get_description               (AtkObject *obj)
{
  const gchar *description = NULL;

  g_return_val_if_fail (HDA_IS_LAUNCHER_TILE (obj), NULL);

  description = ATK_OBJECT_CLASS (hda_launcher_tile_parent_class)->get_description (obj);
  if (description == NULL)
    {
      ClutterActor *actor = NULL;

      actor = CLUTTER_ACTOR (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (obj)));
      description = hd_launcher_tile_get_text (HD_LAUNCHER_TILE (actor));
    }

  return description;
}
