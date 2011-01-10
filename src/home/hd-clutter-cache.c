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
#define HD_CLUTTER_CACHE_FALLBACK_THEME_PATH "/usr/share/themes/default/images/"

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
  G_OBJECT_CLASS (hd_clutter_cache_parent_class)->dispose (obj);
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
  gint i;
  ClutterActor *texture, *actor;
  const char *filename_real = filename;
  char *filename_alloc = 0;

  if (!cache)
    return 0;

  if (from_theme)
    {
      const char *theme_path;

      /*
       * If the theme is broken we have to use the fallback theme path.
       */
      theme_path = mb_wm_theme_is_broken () ?
	      HD_CLUTTER_CACHE_FALLBACK_THEME_PATH :
	      HD_CLUTTER_CACHE_THEME_PATH;

      filename_alloc = g_malloc(strlen(theme_path) + strlen(filename) + 1);
      if (!filename_alloc)
        return 0;
      strcpy(filename_alloc, theme_path);
      strcat(filename_alloc, filename);
      filename_real = filename_alloc;
    }

  for (i = 0, actor = clutter_group_get_nth_child(CLUTTER_GROUP(cache), 0);
       actor; actor = clutter_group_get_nth_child(CLUTTER_GROUP(cache), ++i))
    {
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

      /*
       * If this was the fallback theme path we can not anything else,
       * othwerwise we still can try to load from the fallback path.
       */
      if (mb_wm_theme_is_broken())
        return 0;

      filename_alloc = g_malloc (strlen(HD_CLUTTER_CACHE_FALLBACK_THEME_PATH) +
                                 strlen(filename) + 1);
      if (!filename_alloc)
        return 0;

      strcpy(filename_alloc, HD_CLUTTER_CACHE_FALLBACK_THEME_PATH);
      strcat(filename_alloc, filename);
      filename_real = filename_alloc;

      texture = clutter_texture_new_from_file(filename_real, 0);
      if (!texture)
        {
          if (filename_alloc)
            g_free(filename_alloc);
          return 0;
        }
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
    texture = clutter_clone_new(texture);
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
      texture = hd_clutter_cache_get_broken_texture();
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
  gint x,y;

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
      gfloat gw, gh;
      clutter_actor_get_size(CLUTTER_ACTOR(texture), &gw, &gh);
      geo.x = 0;
      geo.y = 0;
      geo.width = gw;
      geo.height = gh;
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
  if (extend_x)
    {
      low_x = geo.x + (geo.width/4);
      high_x = geo.x + (3*geo.width/4);
    }
  else
    {
      low_x = geo.x;
      high_x = geo.x+geo.width;
    }
  if (extend_y)
    {
      low_y = geo.y + (geo.height/4);
      high_y = geo.y + (3*geo.height/4);
    }
  else
    {
      low_y = geo.y;
      high_y = geo.y + geo.height;
    }

  for (y=0;y<3;y++)
    for (x=0;x<3;x++)
      {
        TidySubTexture *tex;
        ClutterGeometry geot, pos;
        if (x==0)
          {
            geot.x = geo.x;
            geot.width = low_x - geo.x;
            pos.x = area->x;
            pos.width = geot.width;
          }
        else if (x==1)
          {
            geot.x = low_x;
            geot.width = high_x - low_x;
            pos.x = area->x + low_x - geo.x;
            pos.width = (high_x-low_x) + area->width - geo.width;
          }
        else
          {
            geot.x = high_x;
            geot.width = geo.x + geo.width - high_x;
            pos.x = area->x + area->width - geot.width;
            pos.width = geot.width;
          }
        if (y==0)
          {
            geot.y = geo.y;
            geot.height = low_y - geo.y;
            pos.y = area->y;
            pos.height = geot.height;
          }
        else if (y==1)
          {
            geot.y = low_y;
            geot.height = high_y - low_y;
            pos.y = area->y + low_y - geo.y;
            pos.height = (high_y-low_y) + area->height - geo.height;
          }
        else
          {
            geot.y = high_y;
            geot.height = geo.y + geo.height - high_y;
            pos.y = area->y + area->height - geot.height;
            pos.height = geot.height;
          }

        if (geot.width>0 && geot.height>0)
          {
            tex = tidy_sub_texture_new(texture);
            tidy_sub_texture_set_region(tex, &geot);
            if (x==1 || y==1)
              tidy_sub_texture_set_tiled(tex, TRUE);
            clutter_actor_set_position(CLUTTER_ACTOR(tex), pos.x, pos.y);
            clutter_actor_set_size(CLUTTER_ACTOR(tex), pos.width, pos.height);
            clutter_container_add_actor(
                      CLUTTER_CONTAINER(group),
                      CLUTTER_ACTOR(tex));
          }
      }

  return CLUTTER_ACTOR(group);
}

static void
reload_texture_cb (ClutterActor *child,
                   gpointer      data)
{
  gchar *filename;
  if (!CLUTTER_IS_TEXTURE(child))
    return;

  /* filename is set in the child's name. clutter_texture_set_from_file sets
   * the anme to this string, but the string is from the actor in the first
   * place and it just breaks... */
  filename = g_strdup(clutter_actor_get_name(child));
  clutter_texture_set_from_file(CLUTTER_TEXTURE(child), filename, 0);
  g_free(filename);
}

void hd_clutter_cache_theme_changed(void) {
  /* If there is no clutter cache yet then we definitely
   * don't care about reloading stuff */
  if (!the_clutter_cache)
    return;

  clutter_container_foreach (CLUTTER_CONTAINER(the_clutter_cache),
                             reload_texture_cb, 0);
}
