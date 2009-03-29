/*
 * This file is part of hildon-desktop tests
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Tomas Frydrych <tf@o-hand.com>
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
 * Sample Home applet.
 */

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>          /* for XA_ATOM etc */

static void
button_toggled (GtkToggleButton *button,
                GtkWidget       *widget)
{
  if (gtk_toggle_button_get_active (button))
    {
      guint32 set = 1;

      gdk_property_change (widget->window,
                           gdk_atom_intern_static_string ("_HILDON_DO_NOT_DISTURB"),
                           gdk_x11_xatom_to_atom (XA_INTEGER),
                           32,
                           GDK_PROP_MODE_REPLACE,
                           (const guchar *) &set,
                           1);
    }
  else
    {
      gdk_property_delete (widget->window,
                           gdk_atom_intern_static_string ("_HILDON_DO_NOT_DISTURB"));
    }
}

int main (int argc, char *argv[])
{
  GtkWidget *window, *b;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "delete-event",
                    G_CALLBACK (gtk_main_quit), NULL);

  b = gtk_toggle_button_new_with_label ("Do not disturb");
  g_signal_connect (b, "toggled",
		    G_CALLBACK (button_toggled), window);
  gtk_container_add (GTK_CONTAINER (window), b);

  gtk_widget_show_all (window);

  gtk_main();

  return 0;
}
