/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Gordon Williams <gordon.williams@collabora.co.uk>
 *
 * Copyright (C) 2006 Nokia
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

#ifndef _HAVE_TIDY_SUB_TEXTURE_H
#define _HAVE_TIDY_SUB_TEXTURE_H

#include <clutter/clutter-actor.h>
#include <clutter/clutter-texture.h>

G_BEGIN_DECLS

#define TIDY_TYPE_SUB_TEXTURE (tidy_sub_texture_get_type ())

#define TIDY_SUB_TEXTURE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TIDY_TYPE_SUB_TEXTURE, TidySubTexture))

#define TIDY_SUB_TEXTURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TIDY_TYPE_SUB_TEXTURE, TidySubTextureClass))

#define TIDY_IS_SUB_TEXTURE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TIDY_TYPE_SUB_TEXTURE))

#define TIDY_IS_SUB_TEXTURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TIDY_TYPE_SUB_TEXTURE))

#define TIDY_SUB_TEXTURE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TIDY_TYPE_SUB_TEXTURE, TidySubTextureClass))

typedef struct _TidySubTexture        TidySubTexture;
typedef struct _TidySubTexturePrivate TidySubTexturePrivate;
typedef struct _TidySubTextureClass   TidySubTextureClass;

struct _TidySubTexture
{
  ClutterActor                 parent;

  /*< priv >*/
  TidySubTexturePrivate    *priv;
};

struct _TidySubTextureClass
{
  ClutterActorClass parent_class;
};

GType           tidy_sub_texture_get_type           (void) G_GNUC_CONST;

TidySubTexture *tidy_sub_texture_new                (ClutterTexture      *texture);
ClutterTexture *tidy_sub_texture_get_parent_texture (TidySubTexture *sub);
void            tidy_sub_texture_set_parent_texture (TidySubTexture *sub,
                                                     ClutterTexture      *texture);
void            tidy_sub_texture_set_region (TidySubTexture *sub,
                                             ClutterGeometry *region);

G_END_DECLS

#endif
