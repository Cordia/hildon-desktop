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

#include <cally/cally.h>

#include "launcher/hd-launcher-page.h"
#include "launcher/hd-launcher-grid.h"
#include "launcher/hd-launcher-tile.h"

#include "hda-factory.h"
#include "hda-launcher-page.h"
#include "hda-launcher-grid.h"
#include "hda-launcher-tile.h"

#include "hda-launcher.h"

/* factories initialization*/
HDA_ACCESSIBLE_FACTORY (HDA_TYPE_LAUNCHER_PAGE, hda_launcher_page, hda_launcher_page_new)
HDA_ACCESSIBLE_FACTORY (HDA_TYPE_LAUNCHER_GRID, hda_launcher_grid, hda_launcher_grid_new)
HDA_ACCESSIBLE_FACTORY (HDA_TYPE_LAUNCHER_TILE, hda_launcher_tile, hda_launcher_tile_new)

void
hda_launcher_accessibility_init (void)
{
  HDA_ACTOR_SET_FACTORY (HD_TYPE_LAUNCHER_PAGE, hda_launcher_page);
  HDA_ACTOR_SET_FACTORY (HD_TYPE_LAUNCHER_GRID, hda_launcher_grid);
  HDA_ACTOR_SET_FACTORY (HD_TYPE_LAUNCHER_TILE, hda_launcher_tile);
}

