#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "portrait-common.c"

static void boo_cb(GtkWidget *button, GtkWindow *win)
{
  GtkWidget *dlg;

  dlg = gtk_message_dialog_new(GTK_WINDOW(win), GTK_DIALOG_MODAL,
                               GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                               "Edd meg a gyikomat");
  gtk_dialog_run(GTK_DIALOG(dlg));
  gtk_widget_destroy(dlg);
}

static void fos_cb(GtkToggleButton *self, GtkWindow *win)
{
  if (gtk_toggle_button_get_active(self))
    gtk_window_fullscreen(win);
  else
    gtk_window_unfullscreen(win);
}

static gboolean idiot_cb(gpointer entry)
{
  time_t now;

  time(&now);
  gtk_entry_set_text(entry, ctime(&now));
  return TRUE;
}

int main(int argc, char const *argv[])
{
  GtkWidget *win, *vbox, *hbox, *w;

  gtk_init(NULL, NULL);
  win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(win), "Faszvero");
  g_signal_connect(win, "destroy", G_CALLBACK(exit), NULL);
  init_portrait(win, argv);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(win), vbox);

  hbox = portrait();
  w = gtk_check_button_new();
  g_signal_connect(w, "toggled", G_CALLBACK(fos_cb), win);
  gtk_container_add(GTK_CONTAINER(hbox), gtk_label_new("Fos"));
  gtk_container_add(GTK_CONTAINER(hbox), w);
  gtk_container_add(GTK_CONTAINER(vbox), hbox);

  w = gtk_entry_new();
  g_timeout_add(1000, idiot_cb, w);
  gtk_container_add(GTK_CONTAINER(vbox), w);

  w = gtk_button_new_with_label("Boo");
  gtk_container_add(GTK_CONTAINER(vbox), w);
  g_signal_connect(w, "clicked", G_CALLBACK(boo_cb), win);

  gtk_widget_show_all(win);
  gtk_main();

  return 0;
}
