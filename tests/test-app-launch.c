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
#include <dbus/dbus-glib.h>

static void
launch_app(const gchar *app)
{
  DBusGProxy *app_mgr_proxy;
  DBusGConnection *connection;
  GError *error = NULL;

  /* Connect to D-Bus */
  connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  if (error)
    {
      g_warning ("%s. Could not connect to session bus. %s",
                 __FUNCTION__,
                 error->message);
      g_error_free (error);
      return;
    }

  app_mgr_proxy = dbus_g_proxy_new_for_name (connection,
      "com.nokia.HildonDesktop.AppMgr",
      "/com/nokia/HildonDesktop/AppMgr",
      "com.nokia.HildonDesktop.AppMgr");
  dbus_g_proxy_call_no_reply (app_mgr_proxy,
                              "LaunchApplication",
                              G_TYPE_STRING,
                              app,
                              G_TYPE_INVALID,
                              G_TYPE_INVALID);
}

static void
launch_calculator (GtkButton *button)
{
  launch_app("osso_calculator");
}

static gboolean create_new_window() {
  GtkWidget *window, *label;
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  label = gtk_label_new("Hello World!");
  gtk_container_add (GTK_CONTAINER (window), label);
  gtk_widget_show_all (window);

  return FALSE;
}

static void
new_window_immediate (GtkButton *button)
{
  launch_app("");
  create_new_window();
}

static void
new_window_delay (GtkButton *button)
{
  launch_app("");
  g_timeout_add(500, create_new_window, 0);
}

static void
new_window_never (GtkButton *button)
{
  launch_app("");
}

int
main (int argc, char **argv)
{
  GtkWidget *window, *hbox, *vbox, *button;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 12);
  g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_container_add (GTK_CONTAINER (window), hbox);

  vbox = gtk_vbox_new (TRUE, 6);

  button = gtk_button_new_with_label ("Launch calculator");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (launch_calculator),
		    0);
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("New Window (immediate)");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (new_window_immediate),
		    0);
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("New Window (delayed)");
  g_signal_connect (button,
                    "clicked",
                    G_CALLBACK (new_window_delay),
                    0);
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("New Window (not appearing)");
  g_signal_connect (button,
                    "clicked",
                    G_CALLBACK (new_window_never),
                    0);
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, TRUE, 0);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}

