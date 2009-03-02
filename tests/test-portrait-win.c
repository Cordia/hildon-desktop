#include <stdlib.h>
#include <hildon/hildon.h>

#include "portrait-common.c"

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

static void portrait_supported(GtkWidget *w)
{
  if (!Init_portrait_support)
    return;
  gtk_widget_realize(w);
  gdk_property_change(w->window,
           gdk_atom_intern_static_string ("_HILDON_PORTRAIT_MODE_SUPPORT"),
           gdk_x11_xatom_to_atom(XA_CARDINAL), 32,
           GDK_PROP_MODE_REPLACE, (gpointer)&Init_portrait_support, 1);
}

static void boo_cb(GtkWindow *win)
{
  GtkWidget *dlg;

  dlg = gtk_message_dialog_new(win, GTK_DIALOG_MODAL,
                               GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                               "Edd meg a gyikomat");
  portrait_supported(dlg);
  gtk_dialog_run(GTK_DIALOG(dlg));
  gtk_widget_destroy(dlg);
}

static void bee_cb(GtkWindow *win)
{
  GtkWidget *dlg;

  dlg = hildon_note_new_information(win, "Faszkivan");
  portrait_supported(dlg);
  gtk_dialog_run(GTK_DIALOG(dlg));
  gtk_widget_destroy(dlg);
}

static void baa_cb(GtkWindow *win)
{
  hildon_banner_show_information(GTK_WIDGET(win), NULL, "Lehanylak");
}

static void moo_cb(GtkWindow *win)
{
  GtkWidget *dlg;

  dlg = hildon_note_new_confirmation_add_buttons(win,
    "Kivered nekem?",
    "Mint a turmixgep", 1,
    "Nyissz", 0,
    NULL, NULL);
  portrait_supported(dlg);
  gtk_dialog_run(GTK_DIALOG(dlg));
  gtk_widget_destroy(dlg);
}

static void hee_cb(GtkWindow *win)
{
  GtkWidget *newin;

  newin = hildon_stackable_window_new();
  gtk_window_set_title(GTK_WINDOW(newin),
                       "Hejj ha en egyszer osember lennek");
  gtk_container_add(GTK_CONTAINER(newin),
           gtk_label_new("Bunkosbottal bunkoznam le a sok bugris bunkot"));
  gtk_widget_show_all(newin);
}

static gboolean hit_cb(GtkWindow *win, GdkEventKey *e, GtkContainer *bin)
{
  GList *toggles;
  GtkToggleButton *toggle;

  if (e->state & GDK_CONTROL_MASK)
    {
      gint n;

      /* $bin has <label>-<toggle> pairs */
      n = e->keyval - '0';
      n = (n - 1) * 2 + 1;

      toggles = gtk_container_get_children(bin);   
      if ((toggle  = g_list_nth_data(toggles, n)) != NULL)
        gtk_toggle_button_set_active(toggle, !gtk_toggle_button_get_active(toggle));
      g_list_free(toggles);
    }
  else
    {
      switch (e->keyval)
        {
          case '1':
            boo_cb(win);
            break;
          case '2':
            bee_cb(win);
            break;
          case '3':
            baa_cb(win);
            break;
          case '4':
            moo_cb(win);
            break;
          case '5':
            hee_cb(win);
            break;
          case 'x':
            exit(0);
        }
    }
  return TRUE;
}

int main(int argc, char const *argv[])
{
  GtkWidget *win, *vbox, *hbox, *w;

  gtk_init(NULL, NULL);
  win = hildon_stackable_window_new();
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
  g_signal_connect(win, "key-press-event", G_CALLBACK(hit_cb), hbox);

  w = gtk_entry_new();
  g_timeout_add(1000, idiot_cb, w);
  gtk_container_add(GTK_CONTAINER(vbox), w);

  hbox = gtk_hbox_new(TRUE, 0);
  gtk_container_add(GTK_CONTAINER(vbox), hbox);

  w = gtk_button_new_with_label("Boo");
  g_signal_connect_swapped(w, "clicked", G_CALLBACK(boo_cb), win);
  gtk_container_add(GTK_CONTAINER(hbox), w);

  w = gtk_button_new_with_label("Bee");
  g_signal_connect_swapped(w, "clicked", G_CALLBACK(bee_cb), win);
  gtk_container_add(GTK_CONTAINER(hbox), w);

  w = gtk_button_new_with_label("Baa");
  g_signal_connect_swapped(w, "clicked", G_CALLBACK(baa_cb), win);
  gtk_container_add(GTK_CONTAINER(hbox), w);

  w = gtk_button_new_with_label("Moo");
  g_signal_connect_swapped(w, "clicked", G_CALLBACK(moo_cb), win);
  gtk_container_add(GTK_CONTAINER(hbox), w);

  w = gtk_button_new_with_label("Hee");
  g_signal_connect_swapped(w, "clicked", G_CALLBACK(hee_cb), win);
  gtk_container_add(GTK_CONTAINER(hbox), w);

  gtk_widget_show_all(win);
  gtk_main();

  return 0;
}
