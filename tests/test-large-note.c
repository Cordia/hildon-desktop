/*
 * This file is part of hildon-desktop tests
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Gordon Williams <gordon.williams@collabora.co.uk>
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
#include <hildon/hildon.h>

/* Just creates a large note that would have caused NB#117673. It'll need
 * killing to remove it. */

int
main (int argc, char **argv)
{
  gtk_init (&argc, &argv);

  HildonNote *note;

  note = HILDON_NOTE(hildon_note_new_confirmation(NULL, "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18"));
  if (argv[1])
    gtk_window_fullscreen(GTK_WINDOW(note));
  gtk_widget_show_all (GTK_WIDGET (note));

  gtk_main ();

  return 0;
}

