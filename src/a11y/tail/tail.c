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

#include "tidy/tidy-sub-texture.h"

#include "hda-factory.h"
#include "tail-sub-texture.h"

#include "tail.h"

/* factories initialization*/
HDA_ACCESSIBLE_FACTORY (TAIL_TYPE_SUB_TEXTURE, tail_sub_texture, tail_sub_texture_new)

void
tail_accessibility_init (void)
{
  HDA_ACTOR_SET_FACTORY (TIDY_TYPE_SUB_TEXTURE, tail_sub_texture);
}

