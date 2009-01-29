/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Gordon Williams <gordon.williams@collabora.co.uk>
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

/* This class is a singleton that caches textures that may be loaded multiple
 * times - for instance theme textures.
 */

#include "tidy/tidy-sub-texture.h"

#include "hd-clutter-cache.h"
#include "hd-render-manager.h"

struct _HdClutterCachePrivate
{

};

/* ------------------------------------------------------------------------- */

G_DEFINE_TYPE (HdClutterCache, hd_clutter_cache, CLUTTER_TYPE_GROUP);
#define HD_CLUTTER_CACHE_GET_PRIVATE(obj) \
                (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                HD_TYPE_CLUTTER_CACHE, HdClutterCachePrivate))

/* ------------------------------------------------------------------------- */

static HdClutterCache *the_clutter_cache = 0;

/* ------------------------------------------------------------------------- */

static void
hd_clutter_cache_init (HdClutterCache *cache)
{
  ClutterStage *stage;
 /* HdClutterCachePrivate *priv = cache->priv =
    HD_CLUTTER_CACHE_GET_PRIVATE(cache);*/

  clutter_actor_hide(CLUTTER_ACTOR(cache));
  clutter_actor_set_name(CLUTTER_ACTOR(cache), "HdClutterCache");

  stage = CLUTTER_STAGE(clutter_stage_get_default());
  if (stage)
    {
      clutter_container_add_actor(CLUTTER_CONTAINER(stage),
                                  CLUTTER_ACTOR(cache));
    }
}

static void
hd_clutter_cache_dispose (GObject *obj)
{
}

static void
hd_clutter_cache_class_init (HdClutterCacheClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdClutterCachePrivate));

  gobject_class->dispose = hd_clutter_cache_dispose;
}

static HdClutterCache *hd_get_clutter_cache()
{
  if (!the_clutter_cache)
    the_clutter_cache = HD_CLUTTER_CACHE(g_object_ref (
        g_object_new (HD_TYPE_CLUTTER_CACHE, NULL)));
  return the_clutter_cache;
}

static ClutterActor *
hd_clutter_cache_get_real_texture(const char *filename)
{
  HdClutterCache *cache = hd_get_clutter_cache();
  gint n_elements, i;
  ClutterActor *texture;

  if (!cache)
    return 0;

  n_elements = clutter_group_get_n_children(CLUTTER_GROUP(cache));
  for (i=0;i<n_elements;i++)
    {
      ClutterActor *actor =
        clutter_group_get_nth_child(CLUTTER_GROUP(cache), i);
      const char *name = clutter_actor_get_name(actor);
      if (name && g_str_equal(name, filename))
        return actor;
    }

  texture = clutter_texture_new_from_file(filename, 0);
  if (!texture)
    return 0;
  clutter_actor_set_name(texture, filename);
  clutter_container_add_actor(CLUTTER_CONTAINER(cache), texture);
  return texture;
}

/* Returns an actor representing a broken texture.
 * ...Maybe make this Firefox-style broken picture symbol? */
static ClutterActor *
hd_clutter_cache_get_broken_texture()
{
  ClutterColor col = {0xFF, 0x00, 0xFF, 0xFF};
  ClutterActor *actor = clutter_rectangle_new_with_color(&col);
  return actor;
}

ClutterActor *
hd_clutter_cache_get_texture(const char *filename)
{
  ClutterActor *texture = hd_clutter_cache_get_real_texture(filename);
  if (!texture)
    texture = hd_clutter_cache_get_broken_texture();
  else
    texture = clutter_clone_texture_new(CLUTTER_TEXTURE(texture));
  clutter_actor_set_name(texture, filename);
  return texture;
}

ClutterActor *
hd_clutter_cache_get_sub_texture(const char *filename, ClutterGeometry *geo)
{
  ClutterActor *texture;
  TidySubTexture *tex;
  HdClutterCache *cache = hd_get_clutter_cache();
  if (!cache)
    return 0;

  texture = hd_clutter_cache_get_real_texture(filename);
  if (!texture)
    {
      texture = hd_clutter_cache_get_broken_texture(filename);
      clutter_actor_set_name(texture, filename);
      clutter_actor_set_size(texture, geo->width, geo->height);
      return texture;
    }

  tex = tidy_sub_texture_new(CLUTTER_TEXTURE(texture));
  tidy_sub_texture_set_region(tex, geo);
  clutter_actor_set_name(CLUTTER_ACTOR(tex), filename);
  clutter_actor_set_position(CLUTTER_ACTOR(tex), 0, 0);
  clutter_actor_set_size(CLUTTER_ACTOR(tex), geo->width, geo->height);

  return CLUTTER_ACTOR(tex);
}

/* like hd_clutter_cache_get_sub_texture, but divides up the texture
 * and repositions it so it extends to fill the given area */
ClutterActor *
hd_clutter_cache_get_sub_texture_for_area(const char *filename,
                                          ClutterGeometry *geo,
                                          ClutterGeometry *area)
{
  gboolean extend_x = area->width > geo->width;
  gboolean extend_y = area->height > geo->height;
  gint low_x, low_y, high_x, high_y;
  ClutterTexture *texture = 0;
  ClutterGroup *group = 0;

  /* no need to extend */
  if (!extend_x && !extend_y)
    {
      ClutterActor *actor = hd_clutter_cache_get_sub_texture(filename, geo);
      clutter_actor_set_position(actor, area->x, area->y);
      return actor;
    }

  texture = CLUTTER_TEXTURE(hd_clutter_cache_get_real_texture(filename));
  if (!texture)
    {
      ClutterActor *actor = hd_clutter_cache_get_broken_texture();
      clutter_actor_set_name(actor, filename);
      clutter_actor_set_position(actor, area->x, area->y);
      clutter_actor_set_size(actor, area->width, area->height);
      return actor;
    }

  group = CLUTTER_GROUP(clutter_group_new());
  clutter_actor_set_name(CLUTTER_ACTOR(group), filename);
  low_x = geo->x + (geo->width/4);
  low_y = geo->y + (geo->height/4);
  high_x = geo->x + (3*geo->width/4);
  high_y = geo->y + (3*geo->height/4);

  if (extend_x && !extend_y)
    {
      TidySubTexture *texa,*texb,*texc;
      ClutterGeometry geoa, geob, geoc;

      geoa = geob = geoc = *geo;
      geoa.width = low_x-geo->x;
      geob.x = low_x;
      geob.width = high_x-low_x;
      geoc.x = high_x;
      geoc.width = geo->x+geo->width - high_x;

      texa = tidy_sub_texture_new(texture);
      tidy_sub_texture_set_region(texa, &geoa);
      texb = tidy_sub_texture_new(texture);
      tidy_sub_texture_set_region(texb, &geob);
      texc = tidy_sub_texture_new(texture);
      tidy_sub_texture_set_region(texc, &geoc);
      clutter_actor_set_position(CLUTTER_ACTOR(texa),
          area->x, area->y);
      clutter_actor_set_size(CLUTTER_ACTOR(texa),
          geoa.width, area->height);
      clutter_actor_set_position(CLUTTER_ACTOR(texb),
          area->x+geoa.width, area->y);
      clutter_actor_set_size(CLUTTER_ACTOR(texb),
          area->width - (geoa.width+geoc.width), area->height);
      clutter_actor_set_position(CLUTTER_ACTOR(texc),
          area->x+area->width - geoc.width, area->y);
      clutter_actor_set_size(CLUTTER_ACTOR(texc),
          geoc.width, area->height);

      clutter_container_add_actor(
          CLUTTER_CONTAINER(group),
          CLUTTER_ACTOR(texa));
      clutter_container_add_actor(
          CLUTTER_CONTAINER(group),
          CLUTTER_ACTOR(texb));
      clutter_container_add_actor(
          CLUTTER_CONTAINER(group),
          CLUTTER_ACTOR(texc));
      return CLUTTER_ACTOR(group);
    }

  g_warning("%s: Y and XY texture extensions not implemented yet",
      __FUNCTION__);
  /* FIXME: Do other kinds of extension */
  return CLUTTER_ACTOR(group);
}
