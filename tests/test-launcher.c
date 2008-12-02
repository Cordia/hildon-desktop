/*
 * This file is part of hildon-desktop tests
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Emmanuele Bassi <ebassi@o-hand.com>
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

/* A unit test for the launcher that can be used outside of the
 * hildon-desktop
 */

#include <stdlib.h>
#include <glib-object.h>
#include <clutter/clutter.h>

#include "tidy/tidy-finger-scroll.h"

#include "hd-launcher-grid.h"

static gboolean
on_key_press (ClutterActor    *actor,
              ClutterKeyEvent *event,
              gpointer         user_data)
{
  guint keyval;

  keyval = clutter_key_event_symbol (event);

  if (keyval == CLUTTER_q || keyval == CLUTTER_Escape)
    {
      clutter_main_quit ();
      return TRUE;
    }

  return FALSE;
}

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  ClutterActor *launcher;
  ClutterColor stage_color = { 0, 0, 0, 255 };

  g_thread_init (NULL);

  clutter_threads_init ();
  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_actor_set_size (stage, 800, 480);
  g_signal_connect (stage,
                    "key-press-event", G_CALLBACK (on_key_press),
                    NULL);

  launcher = hd_launcher_grid_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), launcher);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
