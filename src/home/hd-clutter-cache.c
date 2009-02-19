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

#define HD_CLUTTER_CACHE_THEME_PATH "/etc/hildon/theme/images/"

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
hd_clutter_cache_get_real_texture(const char *filename, gboolean from_theme)
{
  HdClutterCache *cache = hd_get_clutter_cache();
  gint n_elements, i;
  ClutterActor *texture;
  const char *filename_real = filename;
  char *filename_alloc = 0;

  if (!cache)
    return 0;

  if (from_theme)
    {
      filename_alloc = g_malloc(strlen(HD_CLUTTER_CACHE_THEME_PATH) +
                               strlen(filename) + 1);
      strcpy(filename_alloc, HD_CLUTTER_CACHE_THEME_PATH);
      strcat(filename_alloc, filename);
      filename_real = filename_alloc;
    }

  n_elements = clutter_group_get_n_children(CLUTTER_GROUP(cache));
  for (i = 0; i < n_elements; i++)
    {
      ClutterActor *actor =
        clutter_group_get_nth_child(CLUTTER_GROUP(cache), i);
      const char *name = clutter_actor_get_name(actor);
      if (name && g_str_equal(name, filename_real))
        {
          if (filename_alloc)
	    g_free(filename_alloc);
          return actor;
        }
    }

  texture = clutter_texture_new_from_file(filename_real, 0);
  if (!texture)
    {
      if (filename_alloc)
        g_free(filename_alloc);
      return 0;
    }

  clutter_actor_set_name(texture, filename_real);
  clutter_container_add_actor(CLUTTER_CONTAINER(cache), texture);

  if (filename_alloc)
    g_free(filename_alloc);

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
hd_clutter_cache_get_texture(const char *filename, gboolean from_theme)
{
  ClutterActor *texture = hd_clutter_cache_get_real_texture(filename,
                                                            from_theme);
  if (!texture)
    texture = hd_clutter_cache_get_broken_texture();
  else
    texture = clutter_clone_texture_new(CLUTTER_TEXTURE(texture));
  clutter_actor_set_name(texture, filename);
  return texture;
}

ClutterActor *
hd_clutter_cache_get_sub_texture(const char *filename,
                                 gboolean from_theme,
                                 ClutterGeometry *geo)
{
  ClutterActor *texture;
  TidySubTexture *tex;
  HdClutterCache *cache = hd_get_clutter_cache();
  if (!cache)
    return 0;

  texture = hd_clutter_cache_get_real_texture(filename, from_theme);
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

/* like hd_clutter_cache_get_texture, but divides up the texture
 * and repositions it so it extends to fill the given area */
ClutterActor *
hd_clutter_cache_get_texture_for_area(const char *filename,
                                          gboolean from_theme,
                                          ClutterGeometry *area)
{
  ClutterGeometry geo = {0,0,0,0};
  return hd_clutter_cache_get_sub_texture_for_area(
      filename,
      from_theme,
      &geo,
      area);
}

/* like hd_clutter_cache_get_sub_texture, but divides up the texture
 * and repositions it so it extends to fill the given area */
ClutterActor *
hd_clutter_cache_get_sub_texture_for_area(const char *filename,
                                          gboolean from_theme,
                                          ClutterGeometry *geo_,
                                          ClutterGeometry *area)
{
  gboolean extend_x, extend_y;
  gint low_x, low_y, high_x, high_y;
  ClutterTexture *texture = 0;
  ClutterGroup *group = 0;
  ClutterGeometry geo = *geo_;

  texture = CLUTTER_TEXTURE(hd_clutter_cache_get_real_texture(filename,
                                                              from_theme));
  if (!texture)
    {
      ClutterActor *actor = hd_clutter_cache_get_broken_texture();
      clutter_actor_set_name(actor, filename);
      clutter_actor_set_position(actor, area->x, area->y);
      clutter_actor_set_size(actor, area->width, area->height);
      return actor;
    }

  if (geo.width==0 || geo.height==0)
    {
      geo.x = 0;
      geo.y = 0;
      clutter_actor_get_size(CLUTTER_ACTOR(texture), &geo.width, &geo.height);
    }

  extend_x = area->width > geo.width;
  extend_y = area->height > geo.height;

  /* no need to extend */
  if (!extend_x && !extend_y)
    {
      /* ignore the contents of 'texture' as it will be in our cache */
      ClutterActor *actor =
          hd_clutter_cache_get_sub_texture(filename, from_theme, &geo);
      clutter_actor_set_position(actor, area->x, area->y);
      return actor;
    }


  group = CLUTTER_GROUP(clutter_group_new());
  clutter_actor_set_name(CLUTTER_ACTOR(group), filename);
  low_x = geo.x + (geo.width/4);
  low_y = geo.y + (geo.height/4);
  high_x = geo.x + (3*geo.width/4);
  high_y = geo.y + (3*geo.height/4);

  if (extend_y)
    g_warning("%s: Y and XY texture extensions not implemented yet",
        __FUNCTION__);

  if (extend_y)
    {
      /* Extend X and Y - so a 3x3 set of textures */
      gint x,y;

      for (y=0;y<3;y++)
        for (x=0;x<3;x++)
          {
            TidySubTexture *tex;
            ClutterGeometry geot, pos;
            if (x==0)
              {
                geot.x = 0;
                geot.width = low_x;
                pos.x = area->x;
                pos.width = geot.width;
              }
            else if (x==1)
              {
                geot.x = low_x;
                geot.width = high_x - low_x;
                pos.x = area->x+low_x;
                pos.width = (high_x-low_x) + area->width - geo.width;
              }
            else
              {
                geot.x = high_x;
                geot.width = geo.width - high_x;
                pos.x = area->x + area->width - geot.width;
                pos.width = geot.width;
              }
            if (y==0)
              {
                geot.y = 0;
                geot.height = low_y;
                pos.y = area->y;
                pos.height = geot.height;
              }
            else if (y==1)
              {
                geot.y = low_y;
                geot.height = high_y - low_y;
                pos.y = area->y+low_y;
                pos.height = (high_y-low_y) + area->height - geo.height;
              }
            else
              {
                geot.y = high_y;
                geot.height = geo.height - high_y;
                pos.y = area->y + area->height - geot.height;
                pos.height = geot.height;
              }

            tex = tidy_sub_texture_new(texture);
            tidy_sub_texture_set_region(tex, &geot);
            clutter_actor_set_position(CLUTTER_ACTOR(tex), pos.x, pos.y);
            clutter_actor_set_size(CLUTTER_ACTOR(tex), pos.width, pos.height);
            clutter_container_add_actor(
                      CLUTTER_CONTAINER(group),
                      CLUTTER_ACTOR(tex));
          }
    }
  else
    {
      /* extend just X - so a 3x1 row of texture elements */
      TidySubTexture *texa,*texb,*texc;
      ClutterGeometry geoa, geob, geoc;

      geoa = geob = geoc = geo;
      geoa.width = low_x - geo.x;
      geob.x = low_x;
      geob.width = high_x-low_x;
      geoc.x = high_x;
      geoc.width = geo.x+geo.width - high_x;

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

  /* FIXME: Do other kinds of extension */
  return CLUTTER_ACTOR(group);
}
