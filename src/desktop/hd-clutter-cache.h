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

#ifndef _HAVE_HD_CLUTTER_CACHE_H
#define _HAVE_HD_CLUTTER_CACHE_H

#include <clutter/clutter.h>
#include <matchbox/theme-engines/mb-wm-theme.h>
#include <matchbox/core/mb-wm-decor.h>

typedef struct _HdClutterCache        HdClutterCache;
typedef struct _HdClutterCacheClass   HdClutterCacheClass;
typedef struct _HdClutterCachePrivate HdClutterCachePrivate;

#define HD_CLUTTER_CACHE(c)       ((HdClutterCache*)(c))
#define HD_CLUTTER_CACHE_CLASS(c) ((HdClutterCacheClass*)(c))
#define HD_TYPE_CLUTTER_CACHE     (hd_clutter_cache_get_type ())
#define HD_IS_CLUTTER_CACHE(c)    (MB_WM_OBJECT_TYPE(c)==HD_TYPE_CLUTTER_CACHE)

struct _HdClutterCache
{
  ClutterGroup     parent;
  HdClutterCachePrivate *priv;
};

struct _HdClutterCacheClass
{
  ClutterGroupClass parent;
};

GType hd_clutter_cache_get_type (void) G_GNUC_CONST;

/* Called when the theme has changes, this causes a reload of
 * all textures. */
void
hd_clutter_cache_theme_changed(void);

/* Create a clutter clone texture from a texture in our cache.
 * This is created specially and is not owned by the cache.
 * If from_theme is true, the filename will be appended to the current
 * theme's path.
 */
ClutterActor *
hd_clutter_cache_get_texture(
    const char *filename,
    gboolean from_theme);

/* Create a smaller texture from a master texture, supply the geometry
 * in the master texture to use for this texture.
 * This is created specially and is not owned by the cache */
ClutterActor *
hd_clutter_cache_get_sub_texture(
    const char *filename,
    gboolean from_theme,
    ClutterGeometry *geo);

/* like hd_clutter_cache_get_texture, but divides up the texture
 * and repositions it so it extends to fill the given area */
ClutterActor *
hd_clutter_cache_get_texture_for_area(const char *filename,
                                          gboolean from_theme,
                                          ClutterGeometry *area);

/* like hd_clutter_cache_get_sub_texture, but divides up the texture
 * and repositions it so it extends to fill the given area.
 * This is created specially and is not owned by the cache. */
ClutterActor *
hd_clutter_cache_get_sub_texture_for_area(const char *filename,
                                          gboolean from_theme,
                                          ClutterGeometry *geo,
                                          ClutterGeometry *area);

#endif
