/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Robert Bragg <bob@o-hand.com>
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

/* This test simply creates a Gtk+ top level window, maps it and installs a 1
 * second timeout. When the timeout callback is called we enter an infinite
 * loop that will hang the process including it's ability to respond to
 * NET_WM_PING messages from the window manager.
 *
 * Use this to test the window managers ability to terminate hung clients.
 */

#include <gtk/gtk.h>


static gboolean
timeout_cb (gpointer data)
{
  for (;;)
    ;
}

int
main (int argc, char **argv)
{
  GtkWidget *window;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_show (window);

  g_timeout_add_seconds (1, timeout_cb, NULL);

  gtk_main();

  return 0;
}

