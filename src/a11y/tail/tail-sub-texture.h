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
#ifndef                                         __TAIL_SUB_TEXTURE_H__
#define                                         __TAIL_SUB_TEXTURE_H__

#include                                        <atk/atk.h>
#include                                        <cail/cail-actor.h>

G_BEGIN_DECLS

#define                                         TAIL_TYPE_SUB_TEXTURE \
                                                (tail_sub_texture_get_type ())
#define                                         TAIL_SUB_TEXTURE(obj) \
                                                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                                TAIL_TYPE_SUB_TEXTURE, TailSubTexture))
#define                                         TAIL_SUB_TEXTURE_CLASS(klass) \
                                                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                                TAIL_TYPE_SUB_TEXTURE, TailSubTextureClass))
#define                                         HDA_IS_LAUNCHER_TILE(obj) \
                                                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                                TAIL_TYPE_SUB_TEXTURE))
#define                                         HDA_IS_LAUNCHER_TILE_CLASS(klass)\
                                                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                                TAIL_TYPE_SUB_TEXTURE))
#define                                         TAIL_SUB_TEXTURE_GET_CLASS(obj) \
                                                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                                TAIL_TYPE_SUB_TEXTURE, TailSubTextureClass))

typedef struct                                  _TailSubTexture        TailSubTexture;
typedef struct                                  _TailSubTextureClass   TailSubTextureClass;

struct                                          _TailSubTexture
{
  CailActor parent;
};

struct                                          _TailSubTextureClass
{
  CailActorClass parent_class;
};


GType
tail_sub_texture_get_type                       (void);

AtkObject*
tail_sub_texture_new                            (ClutterActor *tile);


G_END_DECLS

#endif
