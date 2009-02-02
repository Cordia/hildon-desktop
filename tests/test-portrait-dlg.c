#include <stdlib.h>
#include <hildon/hildon.h>

#include "portrait-common.c"

int main(int argc, char const *argv[])
{
  GtkWidget *dlg;

  gtk_init(NULL, NULL);
  dlg = hildon_get_password_dialog_new(NULL, FALSE);
  init_portrait(GTK_WIDGET(dlg), argv);

  hildon_get_password_dialog_set_message(HILDON_GET_PASSWORD_DIALOG(dlg),
                                         "Edd meg a gyikomat");
  gtk_window_set_title(GTK_WINDOW(dlg), "Faszrazo");
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dlg)->vbox), portrait());
  gtk_widget_show_all(dlg);
  gtk_dialog_run(GTK_DIALOG(dlg));

  return 0;
}
