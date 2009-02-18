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
 * SECTION:hda-launcher-page
 * @short_description: Implementation of the ATK interfaces for a #HdLauncherGroup
 * @see_also: #HdLauncherPage
 *
 * #HdaLauncherPage implements the required ATK interfaces of #HdLauncherPage
 * In particular it exposes the #HdLauncherGrid object inside.
 */

#include <cail/cail-actor.h>

#include "launcher/hd-launcher-page.h"
#include "hda-launcher-page.h"

#define HDA_LAUNCHER_PAGE_DEFAULT_NAME "Launcher Page"

/* GObject */
static void
hda_launcher_page_class_init                    (HdaLauncherPageClass *klass);

static void
hda_launcher_page_init                          (HdaLauncherPage *root);

/* AtkObject.h */

static void
hda_launcher_page_initialize                    (AtkObject *obj,
                                                 gpointer data);
static gint
hda_launcher_page_get_n_children                (AtkObject *obj);

static AtkObject*
hda_launcher_page_ref_child                     (AtkObject *obj,
                                                 gint i);

static G_CONST_RETURN gchar *
hda_launcher_page_get_name                      (AtkObject *obj);

G_DEFINE_TYPE (HdaLauncherPage, hda_launcher_page,  CAIL_TYPE_ACTOR)

static void
hda_launcher_page_class_init                            (HdaLauncherPageClass *klass)
{
/*   GObjectClass *gobject_class = G_OBJECT_CLASS (klass); */
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  /* AtkObject */
  class->get_n_children = hda_launcher_page_get_n_children;
  class->ref_child = hda_launcher_page_ref_child;
  class->initialize = hda_launcher_page_initialize;
  class->get_name = hda_launcher_page_get_name;
}

static void
hda_launcher_page_init                          (HdaLauncherPage *page)
{
  /* nothing required */
}


AtkObject*
hda_launcher_page_new                           (ClutterActor *page)
{
  GObject *object = NULL;
  AtkObject *accessible = NULL;

  object = g_object_new (HDA_TYPE_LAUNCHER_PAGE, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, page);

  return accessible;
}

/* AtkObject */

static void
hda_launcher_page_initialize                    (AtkObject   *obj,
                                                 gpointer    data)
{
  ATK_OBJECT_CLASS (hda_launcher_page_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_FILLER;
}

static gint
hda_launcher_page_get_n_children                (AtkObject *obj)
{
  g_return_val_if_fail (HDA_IS_LAUNCHER_PAGE (obj), 0);

  return 1;
}

static AtkObject*
hda_launcher_page_ref_child                     (AtkObject *obj,
                                                 gint i)
{
  ClutterActor *page = NULL;
  ClutterActor *grid = NULL;
  AtkObject *result = NULL;

  g_return_val_if_fail (HDA_IS_LAUNCHER_PAGE (obj), NULL);

  page = CLUTTER_ACTOR (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (obj)));
  if (page == NULL) /* State is defunct */
    {
      return NULL;
    }
  g_return_val_if_fail (HD_IS_LAUNCHER_PAGE (page), NULL);

  grid = hd_launcher_page_get_grid (HD_LAUNCHER_PAGE (page));

  result = atk_gobject_accessible_for_object (G_OBJECT (grid));

  g_object_ref (result);

  return result;
}

static G_CONST_RETURN gchar *
hda_launcher_page_get_name                      (AtkObject *obj)
{
  G_CONST_RETURN gchar *name = NULL;

  g_return_val_if_fail (HDA_IS_LAUNCHER_PAGE (obj), NULL);

  name = ATK_OBJECT_CLASS (hda_launcher_page_parent_class)->get_name (obj);
  if (name == NULL)
    {
      name = HDA_LAUNCHER_PAGE_DEFAULT_NAME;
    }

  return name;
}
