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
#ifndef                                         __HDA_LAUNCHER_TILE_H__
#define                                         __HDA_LAUNCHER_TILE_H__

#include                                        <atk/atk.h>
#include                                        "cail/cail-actor.h"

G_BEGIN_DECLS

#define                                         HDA_TYPE_LAUNCHER_TILE \
                                                (hda_launcher_tile_get_type ())
#define                                         HDA_LAUNCHER_TILE(obj) \
                                                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                                HDA_TYPE_LAUNCHER_TILE, HdaLauncherTile))
#define                                         HDA_LAUNCHER_TILE_CLASS(klass) \
                                                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                                HDA_TYPE_LAUNCHER_TILE, HdaLauncherTileClass))
#define                                         HDA_IS_LAUNCHER_TILE(obj) \
                                                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                                HDA_TYPE_LAUNCHER_TILE))
#define                                         HDA_IS_LAUNCHER_TILE_CLASS(klass)\
                                                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                                HDA_TYPE_LAUNCHER_TILE))
#define                                         HDA_LAUNCHER_TILE_GET_CLASS(obj) \
                                                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                                HDA_TYPE_LAUNCHER_TILE, HdaLauncherTileClass))

typedef struct                                  _HdaLauncherTile        HdaLauncherTile;
typedef struct                                  _HdaLauncherTileClass   HdaLauncherTileClass;

struct                                          _HdaLauncherTile
{
  CailActor parent;
};

struct                                          _HdaLauncherTileClass
{
  CailActorClass parent_class;
};


GType
hda_launcher_tile_get_type                      (void);

AtkObject*
hda_launcher_tile_new                           (ClutterActor *tile);


G_END_DECLS

#endif
