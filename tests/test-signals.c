/*
 * This file is part of hildon-desktop tests
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Chris Lord <chris@o-hand.com>
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
#include <dbus/dbus.h>

typedef struct {
  GtkWidget      *text_view;
  DBusConnection *connection;
} TestData;

static void
append_message (TestData *data, const gchar *message)
{
  GtkTextBuffer *buffer;
  GtkTextIter    iter;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (data->text_view));
  gtk_text_buffer_get_end_iter (buffer, &iter);
  gtk_text_buffer_insert (buffer, &iter, message, -1);
}

static void
send_dbus_signal (TestData *data,
                  const gchar *interface,
                  const gchar *path,
                  const gchar *member)
{
  gchar         *log_message;
  gboolean       success;
  
  DBusMessage *message = dbus_message_new (DBUS_MESSAGE_TYPE_SIGNAL);
  dbus_message_set_interface (message, interface);
  dbus_message_set_path (message, path);
  dbus_message_set_member (message, member);
  success = dbus_connection_send (data->connection, message, NULL);
  dbus_message_unref (message);
  
  log_message = g_strdup_printf ("%s '%s' message.\n",
                                 success ? "Sent" : "Failed to send",
                                 member);
  append_message (data, log_message);
  g_free (log_message);
}

static void
bgkill_on_clicked_cb (GtkButton *button, TestData *data)
{
  send_dbus_signal (data,
                    "com.nokia.ke_recv.bgkill_on",
                    "/com/nokia/ke_recv/bgkill_on",
                    "bgkill_on");
}

static void
bgkill_off_clicked_cb (GtkButton *button, TestData *data)
{
  send_dbus_signal (data,
                    "com.nokia.ke_recv.bgkill_off",
                    "/com/nokia/ke_recv/bgkill_off",
                    "bgkill_off");
}

static void
lowmem_on_clicked_cb (GtkButton *button, TestData *data)
{
  send_dbus_signal (data,
                    "com.nokia.ke_recv.lowmem_on",
                    "/com/nokia/ke_recv/lowmem_on",
                    "lowmem_on");
}

static void
lowmem_off_clicked_cb (GtkButton *button, TestData *data)
{
  send_dbus_signal (data,
                    "com.nokia.ke_recv.lowmem_off",
                    "/com/nokia/ke_recv/lowmem_off",
                    "lowmem_off");
}

int
main (int argc, char **argv)
{
  GtkWidget *window, *hbox, *vbox, *button, *scroll, *log;
  DBusError error;
  TestData data;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 12);
  g_signal_connect (window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_container_add (GTK_CONTAINER (window), hbox);

  vbox = gtk_vbox_new (TRUE, 6);

  button = gtk_button_new_with_label ("Send bgkill_on signal");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (bgkill_on_clicked_cb),
		    &data);
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Send bgkill_off signal");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (bgkill_off_clicked_cb),
		    &data);
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Send lowmem_on signal");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (lowmem_on_clicked_cb),
		    &data);
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_with_label ("Send lowmem_off signal");
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (lowmem_off_clicked_cb),
		    &data);
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, TRUE, 0);
  
  data.text_view = log = gtk_text_view_new ();
  g_object_set (G_OBJECT (log),
                "editable", FALSE,
                "wrap-mode", GTK_WRAP_WORD_CHAR,
                "cursor-visible", FALSE,
                NULL);
  
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
                                       GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (scroll), log);
  
  gtk_box_pack_end (GTK_BOX (hbox), scroll, TRUE, TRUE, 0);

  dbus_error_init (&error);
  data.connection = dbus_bus_get (DBUS_BUS_SESSION, &error);
  if (!data.connection)
    {
      gchar *message = g_strdup_printf ("Error connection to dbus: %s",
                                        error.message);
      append_message (&data, message);
      g_free (message);
      dbus_error_free (&error);
    }

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}

