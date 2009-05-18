/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Author:  Alejandro Pinheiro <apinheiro@igalia.com>
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

#include <gtk/gtk.h>

#include "launcher/hda-launcher.h"
#include "home/hda-home-init.h"
#include "tail/tail.h"

void
hda_accessibility_module_init                   (void);

extern void
gnome_accessibility_module_init                 (void);

extern void
gnome_accessibility_module_shutdown             (void);

static int hda_initialized = FALSE;

void
hda_accessibility_module_init(void)
{
  if (hda_initialized)
    {
      return;
    }
  hda_initialized = TRUE;

  /* init individual libraries  */
  hda_launcher_accessibility_init ();
  hda_home_accessibility_init ();
  tail_accessibility_init ();

  g_message ("Hildon Desktop Accessibility Module initialized");
}


/**
 * gnome_accessibility_module_init:
 * @void:
 *
 * Common gnome hook to be used in order to activate the module
 **/
void
gnome_accessibility_module_init                 (void)
{
  hda_accessibility_module_init ();
}

/**
 * gnome_accessibility_module_shutdown:
 * @void:
 *
 * Common gnome hook to be used in order to de-activate the module
 **/
void
gnome_accessibility_module_shutdown             (void)
{
  if (!hda_initialized)
    {
      return;
    }
  hda_initialized = FALSE;

  g_message ("Hildon Desktop Accessibility Module shutdown");
}

