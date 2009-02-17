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
#ifndef                                         __HDA_LAUNCHER_GRID_H__
#define                                         __HDA_LAUNCHER_GRID_H__

#include                                        <atk/atk.h>
#include                                        "cail/cail-actor.h"

G_BEGIN_DECLS

#define                                         HDA_TYPE_LAUNCHER_GRID \
                                                (hda_launcher_grid_get_type ())
#define                                         HDA_LAUNCHER_GRID(obj) \
                                                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                                HDA_TYPE_LAUNCHER_GRID, HdaLauncherGrid))
#define                                         HDA_LAUNCHER_GRID_CLASS(klass) \
                                                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                                HDA_TYPE_LAUNCHER_GRID, HdaLauncherGridClass))
#define                                         HDA_IS_LAUNCHER_GRID(obj) \
                                                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                                HDA_TYPE_LAUNCHER_GRID))
#define                                         HDA_IS_LAUNCHER_GRID_CLASS(klass)\
                                                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                                HDA_TYPE_LAUNCHER_GRID))
#define                                         HDA_LAUNCHER_GRID_GET_CLASS(obj) \
                                                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                                HDA_TYPE_LAUNCHER_GRID, HdaLauncherGridClass))

typedef struct                                  _HdaLauncherGrid        HdaLauncherGrid;
typedef struct                                  _HdaLauncherGridClass   HdaLauncherGridClass;

struct                                          _HdaLauncherGrid
{
  CailActor parent;
};

struct                                          _HdaLauncherGridClass
{
  CailActorClass parent_class;
};


GType
hda_launcher_grid_get_type                      (void);

AtkObject*
hda_launcher_grid_new                           (ClutterActor *grid);


G_END_DECLS

#endif
