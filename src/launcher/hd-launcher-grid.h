/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Unknown
 *          Marc Ordinas i Llopis <marc.ordinasillopis@collabora.co.uk>
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

/*
 * A HdLauncherGrid is a ClutterActor that displays a grid of
 * HdLauncherItems. It implements the interfaces ClutterContainer and
 * TidyScrollable.
 *
 */

#ifndef __HD_LAUNCHER_GRID_H__
#define __HD_LAUNCHER_GRID_H__

#include <clutter/clutter-actor.h>
#include "hd-launcher-tile.h"
#include "hd-launcher-page.h"

G_BEGIN_DECLS

#define HD_TYPE_LAUNCHER_GRID                   (hd_launcher_grid_get_type ())
#define HD_LAUNCHER_GRID(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_LAUNCHER_GRID, HdLauncherGrid))
#define HD_IS_LAUNCHER_GRID(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_LAUNCHER_GRID))
#define HD_LAUNCHER_GRID_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_LAUNCHER_GRID, HdLauncherGridClass))
#define HD_IS_LAUNCHER_GRID_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_LAUNCHER_GRID))
#define HD_LAUNCHER_GRID_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_LAUNCHER_GRID, HdLauncherGridClass))

typedef struct _HdLauncherGrid          HdLauncherGrid;
typedef struct _HdLauncherGridPrivate   HdLauncherGridPrivate;
typedef struct _HdLauncherGridClass     HdLauncherGridClass;

struct _HdLauncherGrid
{
  ClutterGroup parent_instance;

  HdLauncherGridPrivate *priv;
};

struct _HdLauncherGridClass
{
  ClutterGroupClass parent_class;
};

GType         hd_launcher_grid_get_type (void) G_GNUC_CONST;
ClutterActor *hd_launcher_grid_new      (void);

void          hd_launcher_grid_clear    (HdLauncherGrid *grid);
void          hd_launcher_grid_reset_v_adjustment (HdLauncherGrid *grid);

void          hd_launcher_grid_transition_begin(HdLauncherGrid *grid,
                                  HdLauncherPageTransition trans_type);
void          hd_launcher_grid_transition_end(HdLauncherGrid *grid);
void          hd_launcher_grid_transition(HdLauncherGrid *grid,
                                          HdLauncherPage *page,
                                          HdLauncherPageTransition trans_type,
                                          float amount);
void          hd_launcher_grid_reset(HdLauncherGrid *grid, gboolean hard);
void          hd_launcher_grid_layout(HdLauncherGrid *grid);

void hd_launcher_grid_activate(ClutterActor *actor, int p);

/* Fixed sizes */
#define HD_LAUNCHER_GRID_MIN_HEIGHT \
                (HD_LAUNCHER_PAGE_HEIGHT - HD_LAUNCHER_PAGE_YMARGIN)
#define HD_LAUNCHER_GRID_WIDTH  (HD_LAUNCHER_PAGE_WIDTH)

#define HD_LAUNCHER_GRID_ROW_SPACING 40

G_END_DECLS

#endif /* __HD_LAUNCHER_GRID_H__ */
