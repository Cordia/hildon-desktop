/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Gordon Williams <gordon.williams@collabora.co.uk>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _HAVE_TIDY_MEM_TEXTURE_H
#define _HAVE_TIDY_MEM_TEXTURE_H

#include <clutter/clutter.h>
#include <cogl/cogl.h>

G_BEGIN_DECLS

#define TIDY_TYPE_MEM_TEXTURE (tidy_mem_texture_get_type ())

#define TIDY_MEM_TEXTURE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TIDY_TYPE_MEM_TEXTURE, TidyMemTexture))

#define TIDY_MEM_TEXTURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TIDY_TYPE_MEM_TEXTURE, TidyMemTextureClass))

#define TIDY_IS_MEM_TEXTURE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TIDY_TYPE_MEM_TEXTURE))

#define TIDY_IS_MEM_TEXTURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TIDY_TYPE_MEM_TEXTURE))

#define TIDY_MEM_TEXTURE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TIDY_TYPE_MEM_TEXTURE, TidyMemTextureClass))

typedef struct _TidyMemTexture        TidyMemTexture;
typedef struct _TidyMemTexturePrivate TidyMemTexturePrivate;
typedef struct _TidyMemTextureClass   TidyMemTextureClass;

struct _TidyMemTexture
{
  ClutterActor                 parent;

  /*< priv >*/
  TidyMemTexturePrivate    *priv;
};

struct _TidyMemTextureClass
{
  ClutterActorClass parent_class;
};

GType           tidy_mem_texture_get_type           (void) G_GNUC_CONST;

TidyMemTexture *tidy_mem_texture_new(void);

void tidy_mem_texture_set_data(TidyMemTexture *texture,
                               const guchar *data,
                               gint width, gint height,
                               gint bytes_per_pixel);
void tidy_mem_texture_damage(TidyMemTexture *texture,
                             gint x, gint y,
                             gint width, gint height);
void tidy_mem_texture_set_offset(TidyMemTexture *texture,
                                 CoglFixed x, CoglFixed y);
void tidy_mem_texture_set_scale(TidyMemTexture *texture,
                                CoglFixed scale_x, CoglFixed scale_Y);

G_END_DECLS

#endif
