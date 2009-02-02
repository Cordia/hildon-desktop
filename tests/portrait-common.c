#include <gdk/gdkx.h>
#include <X11/Xatom.h>

static gboolean Init_portrait_support, Init_portrait_request;

static void set_portrait(GtkWidget *self, char const *prop, guint32 value)
{
  gdk_property_change(gtk_widget_get_toplevel(self)->window,
                      gdk_atom_intern_static_string (prop),
                      gdk_x11_xatom_to_atom(XA_CARDINAL), 32,
                      GDK_PROP_MODE_REPLACE, (gpointer)&value, 1);
}

static void portrait_cb(GtkToggleButton *self, char const *prop)
{
  set_portrait(GTK_WIDGET(self), prop, gtk_toggle_button_get_active(self));
}

static GtkWidget *portrait(void)
{
  GtkWidget *hbox, *chk;

  hbox = gtk_hbox_new(TRUE, 0);
  gtk_container_add(GTK_CONTAINER(hbox), gtk_label_new("Ok"));
  chk = gtk_check_button_new();
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk), 
                               Init_portrait_support);
  g_signal_connect(chk, "toggled", G_CALLBACK(portrait_cb),
                   "_HILDON_PORTRAIT_MODE_SUPPORT");
  gtk_container_add(GTK_CONTAINER(hbox), chk);

  gtk_container_add(GTK_CONTAINER(hbox), gtk_label_new("Req"));
  chk = gtk_check_button_new();
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk), 
                               Init_portrait_request);
  g_signal_connect(chk, "toggled", G_CALLBACK(portrait_cb),
                   "_HILDON_PORTRAIT_MODE_REQUEST");
  gtk_container_add(GTK_CONTAINER(hbox), chk);

  return hbox;
}

static void size_requested(GtkWidget *w, GtkRequisition *geo)
{
  g_warning("SIZE REQUEST %dx%d",
            geo->width, geo->height);
}

static void size_allocated(GtkWidget *w, GtkAllocation *geo)
{
  g_warning("SIZE ALLOCATION %dx%d%+d%+d",
            geo->width, geo->height, geo->x, geo->y);
}

static void init_portrait(GtkWidget *win, char const *argv[])
{
  g_signal_connect(win, "size-request",  G_CALLBACK(size_requested),  NULL);
  g_signal_connect(win, "size-allocate", G_CALLBACK(size_allocated), NULL);

  if (!argv[1])
    return;
  gtk_widget_realize(win);

  set_portrait(win, "_HILDON_PORTRAIT_MODE_SUPPORT", 1);
  Init_portrait_support = TRUE;

  if (argv[1][0] == 'w')
    {
      set_portrait(win, "_HILDON_PORTRAIT_MODE_REQUEST", 1);
      Init_portrait_request = TRUE;
    }
}
