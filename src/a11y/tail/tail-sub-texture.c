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
 * SECTION:tail-sub-texture
 * @short_description: Implementation of the ATK interfaces for a #TidySubTexture
 * @see_also: #TidySubTexture
 *
 * #TailSubTexture implements the required ATK interfaces of #TidySubTexture
 */

#include <cail/cail-actor.h>
#include <cail/cail-texture.h>

#include "tidy/tidy-sub-texture.h"
#include "tail-sub-texture.h"

#define CAIL_SUB_TEXTURE_DEFAULT_DESCRIPTION "A sub texture"

/* GObject */
static void
tail_sub_texture_class_init                     (TailSubTextureClass *klass);

static void
tail_sub_texture_init                           (TailSubTexture *root);

/* AtkObject.h */

static void
tail_sub_texture_initialize                     (AtkObject *obj,
                                                 gpointer data);

static G_CONST_RETURN gchar *
tail_sub_texture_get_description                (AtkObject *obj);

G_DEFINE_TYPE (TailSubTexture, tail_sub_texture,  CAIL_TYPE_ACTOR)

static void
tail_sub_texture_class_init                     (TailSubTextureClass *klass)
{
/*   GObjectClass *gobject_class = G_OBJECT_CLASS (klass); */
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  /* AtkObject */
  class->initialize = tail_sub_texture_initialize;
  class->get_description = tail_sub_texture_get_description;
}

static void
tail_sub_texture_init                           (TailSubTexture *tile)
{
  /* nothing required */
}


AtkObject*
tail_sub_texture_new                            (ClutterActor *tile)
{
  GObject *object = NULL;
  AtkObject *accessible = NULL;

  object = g_object_new (TAIL_TYPE_SUB_TEXTURE, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, tile);

  return accessible;
}

/* AtkObject */

static void
tail_sub_texture_initialize                     (AtkObject   *obj,
                                                 gpointer    data)
{
  guint         signal_id  = 0;
  gulong        handler_id = 0;
  CailActor    *self       = NULL;
  ClutterActor *actor      = NULL;

  ATK_OBJECT_CLASS (tail_sub_texture_parent_class)->initialize (obj, data);

  /*
   * The obvious role for a texture is ATK_ROLE_IMAGE, but in several ocasions
   * the texture is used as a kind of button, by connecting to the texture
   * the ClutterActor press and release events. In this cases the ClutterTexture
   * is used as a button, so ATK_ROLE_PUSH_BUTTON is more proper. So in
   * order to decide the role, we check if the ClutterActor has a handler
   * to the release event
   *
   * FIXME: based on the assumption that the signal handler since the clutter
   * actor creation.
   */
  self = CAIL_ACTOR (obj);
  actor = CLUTTER_ACTOR (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (self)));

  signal_id  = g_signal_lookup ("button-release-event", CLUTTER_TYPE_ACTOR);
  handler_id = g_signal_handler_find (actor, G_SIGNAL_MATCH_ID, signal_id,
                                      0, NULL, NULL, NULL);

  if (handler_id)
    {
      obj->role = ATK_ROLE_PUSH_BUTTON;
    }
  else
    {
      obj->role = ATK_ROLE_IMAGE;
    }
}

static G_CONST_RETURN gchar *
tail_sub_texture_get_description                (AtkObject *obj)
{
  G_CONST_RETURN gchar *description = NULL;

  g_return_val_if_fail (CAIL_IS_TEXTURE (obj), NULL);

  description = ATK_OBJECT_CLASS (tail_sub_texture_parent_class)->get_description (obj);
  if (description == NULL)
    {
      /* It gets the default description */
      description = CAIL_SUB_TEXTURE_DEFAULT_DESCRIPTION;
    }

  return description;
}
