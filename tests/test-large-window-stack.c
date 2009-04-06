#include <stdlib.h>
#include <hildon/hildon.h>

static gboolean Add_windows = TRUE;

static gboolean tapi(GtkWindow *win)
{
  Add_windows = !Add_windows;
  return TRUE;
}

static GtkWidget *newin(void)
{
  static guint counter;
  char str[5];
  GtkWidget *win, *label;

  win = hildon_stackable_window_new();
  gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK);
  g_signal_connect(win, "button-press-event", G_CALLBACK(tapi), NULL);

  sprintf(str, "%u", counter++);
  label = gtk_label_new(str);
  gtk_widget_modify_font(label, pango_font_description_from_string("300"));
  gtk_container_add(GTK_CONTAINER(win), label);

  gtk_widget_show_all(win);
  return win;
}

static gboolean wakeup(gpointer unused)
{
  if (Add_windows)
    newin();
  else
    {
      HildonWindowStack *stack;

      stack = hildon_window_stack_get_default();
      if (hildon_window_stack_size(stack) > 1)
        gtk_widget_destroy(hildon_window_stack_pop_1(stack));
    }
  return TRUE;
}

int main(void)
{
  GtkWidget *win;

  gtk_init(NULL, NULL);
  win = newin();
  g_signal_connect(win, "delete-event", G_CALLBACK(exit), NULL);

  g_timeout_add(250, wakeup, NULL);
  gtk_main();
  return 0;
}
