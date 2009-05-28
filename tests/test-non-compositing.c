/*
 * Copyright (C) 2009 Nokia Corporation, all rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; version 2.1 of
 * the License, or (at your option) any later version.
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
#define MAEMO_CHANGES 1

#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkwindow.h>

void set_stacking_layer (GtkWidget *win, int layer)
{
	Window xwindow;
	Atom atom;
	GdkWindow *gdk_window = GTK_WIDGET (win)->window;
	GdkDisplay *gdk_display = gdk_display_get_default ();
	Display *display = GDK_DISPLAY_XDISPLAY (gdk_display);

        atom = gdk_x11_get_xatom_by_name_for_display (gdk_display,
			"_HILDON_STACKING_LAYER");
	xwindow = GDK_WINDOW_XID (gdk_window);

        XChangeProperty (display,
                       xwindow,
                       atom,
                       XA_CARDINAL, 32, PropModeReplace,
                       (unsigned char *) &layer, 1);
}

void set_non_compositing (GtkWidget *win)
{
	Window xwindow;
	Atom atom;
	int one = 1;
	GdkWindow *gdk_window = GTK_WIDGET (win)->window;
	GdkDisplay *gdk_display = gdk_display_get_default ();
	Display *display = GDK_DISPLAY_XDISPLAY (gdk_display);

        atom = gdk_x11_get_xatom_by_name_for_display (gdk_display,
			"_HILDON_NON_COMPOSITED_WINDOW");
	xwindow = GDK_WINDOW_XID (gdk_window);

        XChangeProperty (display,
                       xwindow,
                       atom,
                       XA_INTEGER, 32, PropModeReplace,
                       (unsigned char *) &one, 1);
}

void set_menu_type (GtkWidget *win)
{
	Window xwindow;
	Atom atom, menu_atom;
	GdkWindow *gdk_window = GTK_WIDGET (win)->window;
	GdkDisplay *gdk_display = gdk_display_get_default ();
	Display *display = GDK_DISPLAY_XDISPLAY (gdk_display);

        atom = gdk_x11_get_xatom_by_name_for_display (gdk_display,
			"_NET_WM_WINDOW_TYPE");
        menu_atom = gdk_x11_get_xatom_by_name_for_display (gdk_display,
			"_NET_WM_WINDOW_TYPE_MENU");
	xwindow = GDK_WINDOW_XID (gdk_window);

        XChangeProperty (display,
                       xwindow,
                       atom,
                       XA_ATOM, 32, PropModeReplace,
                       (unsigned char *) &menu_atom, 1);
}

int layer = 1;

/*
gboolean do_something (gpointer w)
{
	GtkWidget *window = GTK_WIDGET(w);

	set_stacking_layer (window, layer);

	return False;
}
*/

void create_dialog (int layer)
{
	GtkWidget *dialog, *label;
	char buf[100];

	snprintf (buf, 100, "Hildon Layer %d", layer);
	label = gtk_label_new (buf);

	dialog = gtk_dialog_new ();
	gtk_widget_realize (dialog);
	set_stacking_layer (dialog, layer);
	/*
	gtk_window_set_title (GTK_WINDOW(dialog), buf);
	gtk_window_set_decorated (GTK_WINDOW(dialog), FALSE);

	*/
	gtk_container_add (GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);

	gtk_widget_show_all (dialog);
}

void create_menu (int layer)
{
	GtkWidget *menu, *label;
	char buf[100];

	snprintf (buf, 100, "Hildon Layer %d", layer);
	label = gtk_label_new (buf);

	menu = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize (menu);
	set_stacking_layer (menu, layer);
        set_menu_type (menu);
	/*
	gtk_window_fullscreen (GTK_WINDOW(menu));
	*/

	gtk_container_add (GTK_CONTAINER(menu), label);

	gtk_widget_show_all (menu);
}

void create_normal_toplevel (int layer)
{
	GtkWidget *menu, *label;
	char buf[100];

	snprintf (buf, 100, "Hildon Layer %d", layer);
	label = gtk_label_new (buf);

	menu = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize (menu);

	if (layer)
		set_non_compositing (menu);
	/*

	*/
	gtk_window_fullscreen (GTK_WINDOW(menu));

	gtk_container_add (GTK_CONTAINER(menu), label);

	gtk_widget_show_all (menu);
}

void create_override_window (int layer)
{
	GtkWidget *menu, *label;
	char buf[100];

	snprintf (buf, 100, "Hildon Layer %d", layer);
	label = gtk_label_new (buf);

	menu = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_widget_realize (menu);
	set_stacking_layer (menu, layer);
        gtk_window_set_position (GTK_WINDOW(menu), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size (GTK_WINDOW(menu), 200, 200);

	/*
	gtk_window_fullscreen (GTK_WINDOW(menu));
	*/

	gtk_container_add (GTK_CONTAINER(menu), label);

	gtk_widget_show_all (menu);
}

int
main (int argc, char **argv)
{
	gtk_init (&argc, &argv);

	/*
	create_dialog (3);
	create_menu (1);
	create_normal_toplevel (1);
	create_dialog (2);
	*/
	if (argc == 1)
		create_normal_toplevel (1);
	else
		create_normal_toplevel (0);

	gtk_main ();

	return 0;
}
