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

#include "home/hd-home.h"

#include "hda-factory.h"

#include "hda-home-init.h"
#include "hda-home.h"

/* factories initialization*/
HDA_ACCESSIBLE_FACTORY (HDA_TYPE_HOME, hda_home, hda_home_new)

void
hda_home_accessibility_init (void)
{
  HDA_ACTOR_SET_FACTORY (HD_TYPE_HOME, hda_home);
}

