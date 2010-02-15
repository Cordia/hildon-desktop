/*
 * This file is part of hildon-desktop tests
 *
 * Copyright (C) 2008-2010 Nokia Corporation.
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

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>          /* for XA_ATOM etc */

static void draw_rect (GtkWidget *widget, int x, int y, int w, int h)
{
  cairo_t *cr;
  cr = gdk_cairo_create (GDK_DRAWABLE (widget->window));
  cairo_set_source_rgba (cr, 1.0, 0.0, 0.0, 1.0);
  cairo_move_to (cr, x, y);
  cairo_line_to (cr, x + w, y);
  cairo_line_to (cr, x + w, y + h);
  cairo_line_to (cr, x, y + h);
  cairo_fill (cr);
  cairo_destroy (cr);
}

static gint
on_button_event (GtkWidget *widget, GdkEventButton *event)
{
        /*
  g_printerr ("Got button %s event [%f,%f]\n",
	     event->type == GDK_BUTTON_RELEASE ? "RELEASE" : "PRESS",
	     event->x, event->y);
             */
  draw_rect (widget, event->x, event->y, 10, 10);

  return FALSE;
}

static gboolean timeout_func (gpointer data)
{
  GtkWidget *window = data;
  draw_rect (window, rand() * 1000 % 800 + 1, rand() * 1000 % 480 + 1,
             10, 10);
  return TRUE;
}


static gboolean
on_expose_event (GtkWidget      *widget,
                 GdkEventExpose *event)
{
  cairo_t *cr;

  g_printerr("%s: %p\n", __func__, widget);
  /* Create cairo context */
  cr = gdk_cairo_create (GDK_DRAWABLE (widget->window));
  gdk_cairo_region (cr, event->region);
  cairo_clip (cr);

  /* Draw alpha background */
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.0);
  cairo_paint (cr);

  /* Free context */
  cairo_destroy (cr);

  g_timeout_add (2000, timeout_func, widget);
  return FALSE;
}

static void set_live_bg (Display *display, Window xwindow, int mode)
{
        Atom atom;

        atom = XInternAtom (display, "_HILDON_LIVE_DESKTOP_BACKGROUND", False);
        fprintf (stderr, "XID: 0x%x\n", (unsigned)xwindow);

        XChangeProperty (display,
                       xwindow,
                       atom,
                       XA_INTEGER, 32, PropModeReplace,
                       (unsigned char *) &mode, 1);
}

int main (int argc, char *argv[])
{
  GtkWidget *window;
  GdkScreen *screen;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  g_signal_connect (G_OBJECT (window), "button-release-event",
		    G_CALLBACK (on_button_event), NULL);

  g_signal_connect (G_OBJECT (window), "button-press-event",
		    G_CALLBACK (on_button_event), NULL);

  screen = gtk_widget_get_screen (window);
  gtk_widget_set_colormap (window,
                           gdk_screen_get_rgba_colormap (screen));
  gtk_widget_set_app_paintable (window, TRUE);

  g_signal_connect (window, "expose-event",
                    G_CALLBACK (on_expose_event), window);

  gtk_window_resize (GTK_WINDOW (window), 800, 480);
  gtk_widget_realize (window);

  gdk_window_set_events (window->window,
			 gdk_window_get_events (window->window)|
			 GDK_EXPOSURE_MASK  |
			 GDK_BUTTON_PRESS_MASK   |
			 GDK_BUTTON_RELEASE_MASK
                         /*
                         |
			 GDK_POINTER_MOTION_MASK
                         */
                         );

  set_live_bg (GDK_DISPLAY (), GDK_WINDOW_XID (window->window), 101);

  gtk_widget_show_all (window);
  gdk_window_fullscreen (window->window);
  g_printerr("going to main loop\n");

  gtk_main();

  return 0;
}
