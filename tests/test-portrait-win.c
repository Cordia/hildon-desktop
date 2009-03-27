#include <stdlib.h>
#include <hildon/hildon.h>

#include "portrait-common.c"

static GtkWidget *Subverter;

static gboolean fancycycy(GtkWidget *w, GdkEventKey *e, GtkWindow *win)
{
  gboolean wtf;

  g_signal_emit_by_name(win, "key-press-event", e, &wtf);
  return FALSE;
}

static GtkWidget *fancycy(GtkWidget *w, GtkWindow *win)
{
  g_signal_connect(w, "key-press-event", G_CALLBACK(fancycycy), win);
  return w;
}

static GtkWidget *fancy(GtkWidget *w, GtkWindow *win)
{
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Subverter)))
    {
      static guint32 no = { 0 };

      gtk_widget_realize(w);
      gdk_property_change(w->window,
               gdk_atom_intern_static_string ("_HILDON_PORTRAIT_MODE_SUPPORT"),
               gdk_x11_xatom_to_atom(XA_CARDINAL), 32,
               GDK_PROP_MODE_REPLACE, (gpointer)&no, 1);
    }
  return fancycy(w, win);
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

static HildonAppMenu *mkmenu(GtkWindow *win)
{
  static const gchar *const labels[] =
    { "Time", "goes", "by", "so", "slowly", NULL };
  guint i;
  GtkWidget *menu, *button;

  menu = fancycy(hildon_app_menu_new(), win);
  for (i = 0; labels[i]; i++)
    {
      button = hildon_gtk_button_new(
                       HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH);
      gtk_button_set_label(GTK_BUTTON (button), labels[i]);
      hildon_app_menu_append(HILDON_APP_MENU(menu), GTK_BUTTON(button));
    }
  gtk_widget_show_all(menu);

  return HILDON_APP_MENU(menu);
}

static gboolean boo_hit_cb(GtkWidget *w, GdkEventKey *e, GtkWindow *win)
{
  if (e->keyval == 'm')
    {
      static gboolean ismax;
      static GtkAllocation restore_size;

      if (!ismax)
        {
          restore_size = w->allocation;
          gtk_widget_set_size_request(w,
                                      GTK_WIDGET(win)->allocation.width,
                                      GTK_WIDGET(win)->allocation.height);
        }
      else
        gtk_widget_set_size_request(w, restore_size.width, restore_size.height);
      ismax = !ismax;
    }
  else if (e->keyval == 'f')
    {
      static gboolean isfull;

      if (isfull)
        gtk_window_unfullscreen(GTK_WINDOW(w));
      else
        gtk_window_fullscreen(GTK_WINDOW(w));
      isfull = !isfull;
    }

  return FALSE;
}

static void boo_boo_cb(GtkDialog *dlg, gint response)
{
  if (response == GTK_RESPONSE_NO)
    {
      GtkWidget *dada;

      g_signal_stop_emission_by_name (dlg, "response");
      dada = gtk_message_dialog_new(GTK_WINDOW (dlg), GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                    "Boo! Boo!");
      gtk_dialog_run(GTK_DIALOG(dada));
      gtk_widget_destroy(dada);
    }
}

static void boo_cb(GtkWindow *win)
{
  static guint i = 1;
  GtkWidget *dlg;

  dlg = fancy(gtk_message_dialog_new(win, GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_INFO, GTK_BUTTONS_YES_NO,
                                     "Boo!"), win);
  gtk_window_set_title(GTK_WINDOW(dlg), g_strdup_printf("Csapo %u", i++));
  g_signal_connect(dlg, "key-press-event", G_CALLBACK(boo_hit_cb), win);
  g_signal_connect(dlg, "response", G_CALLBACK(boo_boo_cb), NULL);
  gtk_dialog_run(GTK_DIALOG(dlg));
  gtk_widget_destroy(dlg);
}

static void bee_cb(GtkWindow *win)
{
  GtkWidget *dlg;

  dlg = fancy(hildon_note_new_information(win, "Bee!"), win);
  gtk_dialog_run(GTK_DIALOG(dlg));
  gtk_widget_destroy(dlg);
}

static void baa_cb(GtkWindow *win)
{
  hildon_banner_show_information(GTK_WIDGET(win), NULL, "Baa!");
}

static void moo_cb(GtkWindow *win)
{
  GtkWidget *dlg;

  dlg = fancy(hildon_note_new_confirmation_add_buttons(win,
    "Moo?",
    "Moo!", 1,
    "Ooo...", 0,
    NULL, NULL), win);
  gtk_dialog_run(GTK_DIALOG(dlg));
  gtk_widget_destroy(dlg);
}

static void hee_cb(GtkWindow *win)
{
  gulong id;
  GtkWidget *newin, *menu, *menu_item, *vbox, *w;

  newin = hildon_stackable_window_new();
  gtk_window_set_title(GTK_WINDOW(newin),
                       "Hejj ha en egyszer osember lennek");

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(newin), vbox);

  gtk_container_add(GTK_CONTAINER(vbox),
           gtk_label_new("Bunkosbottal bunkoznam le a sok bugris bunkot"));
  w = gtk_entry_new();
  id = g_timeout_add(1000, idiot_cb, w);
  gtk_container_add(GTK_CONTAINER(vbox), w);

  menu = gtk_menu_new();
  menu_item = gtk_menu_item_new_with_label("Die, my darling");
  g_signal_connect_swapped(menu_item, "activate",
                           G_CALLBACK(gtk_object_destroy), newin);
  gtk_widget_show(menu_item);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  hildon_window_set_main_menu(HILDON_WINDOW(newin), GTK_MENU(menu));

  gtk_widget_show_all(fancy(newin, win));
  g_signal_connect_swapped(newin, "destroy",
                           G_CALLBACK(g_source_remove),
                           GINT_TO_POINTER(id));
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
          case HILDON_HARDKEY_MENU:
            HILDON_WINDOW_GET_CLASS(win)->toggle_menu(HILDON_WINDOW(win), 0, 0);
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
  gtk_window_set_title(GTK_WINDOW(win), "Look, a window!");
  g_signal_connect(win, "destroy", G_CALLBACK(exit), NULL);
  hildon_stackable_window_set_main_menu(HILDON_STACKABLE_WINDOW(win),
                                        mkmenu(GTK_WINDOW(win)));
  init_portrait(win, argv);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(win), vbox);

  hbox = portrait();
  w = gtk_check_button_new();
  g_signal_connect(w, "toggled", G_CALLBACK(fos_cb), win);
  gtk_container_add(GTK_CONTAINER(hbox), gtk_label_new("FS"));
  gtk_container_add(GTK_CONTAINER(hbox), w);

  Subverter = gtk_check_button_new();
  gtk_container_add(GTK_CONTAINER(hbox), gtk_label_new("Subvert"));
  gtk_container_add(GTK_CONTAINER(hbox), Subverter);

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
