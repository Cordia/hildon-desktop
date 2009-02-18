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
 * SECTION:hda-launcher-grid
 * @short_description: Implementation of the ATK interfaces for a #HdLauncherGroup
 * @see_also: #HdLauncherGrid
 *
 * #HdaLauncherGrid implements the required ATK interfaces of #HdLauncherGrid
 * In particular it exposes the #HdLauncherGrid object inside.
 */

#include <cail/cail-actor.h>

#include "launcher/hd-launcher-grid.h"
#include "hda-launcher-grid.h"

#define HDA_LAUNCHER_GRID_DEFAULT_NAME "Launcher Grid"

/* GObject */
static void
hda_launcher_grid_class_init                    (HdaLauncherGridClass *klass);

static void
hda_launcher_grid_init                          (HdaLauncherGrid *root);

/* AtkObject.h */

static void
hda_launcher_grid_initialize                    (AtkObject *obj,
                                                 gpointer data);
static gint
hda_launcher_grid_get_n_children                (AtkObject *obj);

static AtkObject*
hda_launcher_grid_ref_child                     (AtkObject *obj,
                                                 gint i);

static G_CONST_RETURN gchar *
hda_launcher_grid_get_name                      (AtkObject *obj);


G_DEFINE_TYPE (HdaLauncherGrid, hda_launcher_grid,  CAIL_TYPE_ACTOR)

static void
hda_launcher_grid_class_init                            (HdaLauncherGridClass *klass)
{
/*   GObjectClass *gobject_class = G_OBJECT_CLASS (klass); */
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  /* AtkObject */
  class->get_n_children = hda_launcher_grid_get_n_children;
  class->ref_child = hda_launcher_grid_ref_child;
  class->initialize = hda_launcher_grid_initialize;
  class->get_name = hda_launcher_grid_get_name;
}

static void
hda_launcher_grid_init                          (HdaLauncherGrid *grid)
{
  /* nothing required */
}


AtkObject*
hda_launcher_grid_new                           (ClutterActor *grid)
{
  GObject *object = NULL;
  AtkObject *accessible = NULL;

  object = g_object_new (HDA_TYPE_LAUNCHER_GRID, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, grid);

  return accessible;
}

/* AtkObject */

static void
hda_launcher_grid_initialize                    (AtkObject   *obj,
                                                 gpointer    data)
{
  ATK_OBJECT_CLASS (hda_launcher_grid_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_FILLER;
}

static gint
hda_launcher_grid_get_n_children                (AtkObject *obj)
{
  GList *children = NULL;
  ClutterActor *actor = NULL;
  gint num = 0;

  g_return_val_if_fail (HDA_IS_LAUNCHER_GRID (obj), 0);

  actor = CLUTTER_ACTOR (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (obj)));
  if (actor == NULL)
    {
      return 0;
    }

  g_return_val_if_fail (CLUTTER_IS_CONTAINER (actor), 0);

  /* Probably this is not a really efficient method, but as HdLauncherGrid has
     no public methods to get the children, this is the only way to do thay */
  children = clutter_container_get_children (CLUTTER_CONTAINER (actor));
  num = g_list_length (children);

  g_list_free (children);

  return num;
}

static AtkObject*
hda_launcher_grid_ref_child                     (AtkObject *obj,
                                                 gint i)
{
  ClutterActor *grid = NULL;
  ClutterActor *actor = NULL;
  GList *children = NULL;
  AtkObject *result = NULL;

  g_return_val_if_fail (HDA_IS_LAUNCHER_GRID (obj), NULL);

  grid = CLUTTER_ACTOR (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (obj)));

  if (grid == NULL) /* State is defunct */
    {
      return NULL;
    }

  g_return_val_if_fail (HD_IS_LAUNCHER_GRID (grid), NULL);
  g_return_val_if_fail (CLUTTER_IS_CONTAINER (grid), NULL);

  children = clutter_container_get_children (CLUTTER_CONTAINER (grid));
  actor = g_list_nth_data (children, i);

  result = atk_gobject_accessible_for_object (G_OBJECT (actor));

  g_object_ref (result);

  return result;
}

static G_CONST_RETURN gchar *
hda_launcher_grid_get_name                      (AtkObject *obj)
{
  G_CONST_RETURN gchar *name = NULL;

  g_return_val_if_fail (HDA_IS_LAUNCHER_GRID (obj), NULL);

  name = ATK_OBJECT_CLASS (hda_launcher_grid_parent_class)->get_name (obj);
  if (name == NULL)
    {
      name = HDA_LAUNCHER_GRID_DEFAULT_NAME;
    }

  return name;
}
