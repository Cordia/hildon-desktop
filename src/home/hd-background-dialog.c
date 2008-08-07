/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Chris Lord <chris@openedhand.com>
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

#include <clutter/x11/clutter-x11.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>

#include "hd-background-dialog.h"

enum
{
  PROP_HOME = 1,
  PROP_HOME_VIEW,
};

struct _HdBackgroundDialogPrivate
{
  HdHome       *home;
  HdHomeView   *view;
  ClutterColor *color;
  GtkWidget    *image_button;
  GtkWidget    *style_button;
};

G_DEFINE_TYPE (HdBackgroundDialog, hd_background_dialog, GTK_TYPE_DIALOG);

/* Return the text representation of an HdHomeViewBackgroundMode */
static const gchar *
background_mode_to_string (HdHomeViewBackgroundMode mode)
{
  switch (mode)
    {
    case HDHV_BACKGROUND_STRETCHED:
      return _("home_va_set_backgr_stretched");
    case HDHV_BACKGROUND_CENTERED:
      return _("home_va_set_backgr_centered");
    case HDHV_BACKGROUND_SCALED:
      return _("home_va_set_backgr_scaled");
    case HDHV_BACKGROUND_TILED:
      return _("home_va_set_backgr_tiled");
    case HDHV_BACKGROUND_CROPPED:
      return _("home_va_set_backgr_cropped");
    default :
      return "Unknown";
    }
}

static void
hd_background_dialog_set_property (GObject       *object,
				   guint         prop_id,
				   const GValue *value,
				   GParamSpec   *pspec)
{
  HdBackgroundDialogPrivate *priv = HD_BACKGROUND_DIALOG (object)->priv;

  switch (prop_id)
    {
    case PROP_HOME:
      if (priv->home)
	  g_object_unref (priv->home);
      priv->home = g_value_dup_object (value);
      break;

    case PROP_HOME_VIEW:
      if (priv->view)
	  g_object_unref (priv->view);
      priv->view = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hd_background_dialog_get_property (GObject      *object,
				   guint         prop_id,
				   GValue       *value,
				   GParamSpec   *pspec)
{
  HdBackgroundDialogPrivate *priv = HD_BACKGROUND_DIALOG (object)->priv;

  switch (prop_id)
    {
    case PROP_HOME:
      g_value_set_object (value, priv->home);
      break;

    case PROP_HOME_VIEW:
      g_value_set_object (value, priv->view);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hd_background_dialog_dispose (GObject *object)
{
  HdBackgroundDialogPrivate *priv = HD_BACKGROUND_DIALOG (object)->priv;

  if (priv->view)
    {
      g_object_unref (priv->view);
      priv->view = NULL;
    }

  if (priv->home)
    {
      g_object_unref (priv->home);
      priv->home = NULL;
    }

  G_OBJECT_CLASS (hd_background_dialog_parent_class)->dispose (object);
}

static void
hd_background_dialog_finalize (GObject *object)
{
  G_OBJECT_CLASS (hd_background_dialog_parent_class)->finalize (object);
}

/* Update the subtitle label on a button (see button_new_with_labels()) */
static void
button_set_detail (GtkWidget *button, const gchar *detail)
{
  GList     *children;
  gchar     *string;
  GtkWidget *label;

  children = gtk_container_get_children (
    GTK_CONTAINER (gtk_bin_get_child (GTK_BIN (button))));
  label = GTK_WIDGET (g_list_last (children)->data);
  g_list_free (children);

  string = g_strconcat ("<small>", detail, "</small>", NULL);
  gtk_label_set_markup (GTK_LABEL (label), string);
  g_free (string);
}

/* Creates a button that has a label and a second, smaller label underneath
 * it. This second label can also be updated using button_set_detail().
 */
static GtkWidget *
button_new_with_labels (const gchar *title, const gchar *detail)
{
  gchar     *string;
  GtkWidget *button, *vbox, *label;

  vbox = gtk_vbox_new (FALSE, 6);

  label = gtk_label_new (title);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);

  string = g_strconcat ("<small>", detail, "</small>", NULL);
  label = gtk_label_new (string);
  g_free (string);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_box_pack_end (GTK_BOX (vbox), label, FALSE, TRUE, 0);

  gtk_widget_show_all (vbox);

  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (button), vbox);

  return button;
}

static gboolean
color_expose_cb (GtkWidget      *widget,
		 GdkEventExpose *event,
		 gpointer        data)
{
  cairo_t		    *cr;
  HdBackgroundDialogPrivate *priv = HD_BACKGROUND_DIALOG (data)->priv;

  /* Fill the drawing area with the background colour. */

  cr = gdk_cairo_create (widget->window);
  cairo_set_source_rgb (cr,
			priv->color->red/255.0,
			priv->color->green/255.0,
			priv->color->blue/255.0);
  cairo_paint (cr);
  cairo_destroy (cr);

  return TRUE;
}

static void
color_size_request_cb (GtkWidget      *widget,
		       GtkRequisition *requisition,
		       GtkWidget      *label)
{
  /* Make the colour preview square have the same height as the text and
   * a 16:9 aspect ratio (widescreen)
   */
  gtk_widget_size_request (label, requisition);
  requisition->width = (requisition->height * 16) / 9;
}

static void
color_dialog_response_cb (GtkWidget	     *dialog,
			  gint		      response,
			  HdBackgroundDialog *parent)
{
  HdBackgroundDialogPrivate *priv = parent->priv;

  /* If the user chose 'OK', set the background colour of the current view
   * to whatever colour is currently chosen in the colour selection dialog.
   * Also update our cache of the view's background colour and tell the
   * parent dialog to redraw so the colour-preview square is updated.
   */

  if (response == GTK_RESPONSE_OK)
    {
      GdkColor		 gdk_color;
      ClutterColor       color;

      GtkColorSelection *color_sel =
	GTK_COLOR_SELECTION (GTK_COLOR_SELECTION_DIALOG (dialog)->colorsel);

      gtk_color_selection_get_current_color (color_sel, &gdk_color);
      color.red   = gdk_color.red >> 8;
      color.green = gdk_color.green >> 8;
      color.blue  = gdk_color.blue >> 8;
      color.alpha = 0xff;

      hd_home_view_set_background_color (priv->view, &color);
      clutter_color_free (priv->color);
      g_object_get (priv->view, "background-color", &priv->color, NULL);
      gtk_widget_queue_draw (GTK_WIDGET (parent));
    }

  gtk_widget_destroy (dialog);
}

static void
color_button_clicked_cb (GtkWidget	    *button,
			 HdBackgroundDialog *parent)
{
  const gchar	    *title;
  GtkWidget	    *dialog;
  GdkColor	     gdk_color;
  GtkColorSelection *color_sel;

  HdBackgroundDialogPrivate *priv = parent->priv;

  /* Create the colour selection dialog and set its colour to the current
   * view's background colour.
   */

  title = gtk_label_get_text (
    GTK_LABEL (g_object_get_data (G_OBJECT (button), "label")));

  dialog = gtk_color_selection_dialog_new (title);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));

  gdk_color.red = priv->color->red << 8;
  gdk_color.green = priv->color->green << 8;
  gdk_color.blue = priv->color->blue << 8;

  color_sel =
    GTK_COLOR_SELECTION (GTK_COLOR_SELECTION_DIALOG (dialog)->colorsel);
  gtk_color_selection_set_current_color (color_sel, &gdk_color);

  g_signal_connect (dialog,
		    "response",
		    G_CALLBACK (color_dialog_response_cb),
		    parent);

  gtk_widget_show (dialog);
}

/* Creates a new colour-chooser button that sets the background colour of
 * the view held in the private structure of HdBackgroundDialog.
 */
static GtkWidget *
color_label_button_new (HdBackgroundDialog *parent, const gchar *title)
{
  GtkWidget *align, *color, *label, *hbox, *button;

  hbox = gtk_hbox_new (FALSE, 6);

  label = gtk_label_new (title);
  gtk_box_pack_end (GTK_BOX (hbox), label, TRUE, TRUE, 0);

  color = gtk_drawing_area_new ();
  g_signal_connect (color,
		    "expose-event",
		    G_CALLBACK (color_expose_cb),
		    parent);
  g_signal_connect (color,
		    "size-request",
		    G_CALLBACK (color_size_request_cb),
		    label);

  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), color);
  gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);

  gtk_widget_show_all (hbox);

  button = gtk_button_new ();
  g_object_set_data (G_OBJECT (button), "label", label);
  gtk_container_add (GTK_CONTAINER (button), hbox);

  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (color_button_clicked_cb),
		    parent);

  return button;
}

/* Takes a file name, '/a/long/path/filename.blah' and strips off the path
 * and extension, giving 'filename'.
 */
static gchar *
pretty_filename (const gchar *filename)
{
  gchar *pretty;
  if (filename)
    {
      gchar *last_dot;
      pretty = g_filename_display_basename (filename);

      /* Remove extension */
      last_dot = g_utf8_strrchr (pretty, -1, '.');
      last_dot[0] = '\0';
    }
  else
    pretty = g_strdup (_("home_va_set_backgr_none"));

  return pretty;
}

/* Updates the subtitle text on the background image chooser button */
static void
update_image_button (HdBackgroundDialog *parent)
{
  HdBackgroundDialogPrivate *priv;
  gchar                     *image, *basename;

  priv = parent->priv;

  g_object_get (priv->view,
		"background-image", &image,
		NULL);

  basename = pretty_filename (image);
  button_set_detail (priv->image_button, basename);
  g_free (image);
  g_free (basename);
}

static void
background_chooser_file_dialog_response_cb (GtkWidget          *dialog,
					    gint                response,
					    HdBackgroundDialog *parent)
{
  HdBackgroundDialogPrivate *priv;

  priv = parent->priv;

  /* If OK was clicked, set the background to the currently selected file */
  if (response == GTK_RESPONSE_OK)
    {
      gchar          *file;
      GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);

      file = gtk_file_chooser_get_filename (chooser);
      hd_home_view_set_background_image (priv->view, file);
      g_free (file);
    }

  gtk_widget_destroy (dialog);
}

static void
background_chooser_file_button_clicked_cb (GtkWidget	      *button,
					   HdBackgroundDialog *parent)
{
  GtkWidget     *dialog;
  GtkFileFilter *filter;

  /* Create a file-chooser dialog, add the formats supported by
   * GdkPixbuf and add a handler for when it gets closed to set the
   * background image.
   */

  dialog = gtk_file_chooser_dialog_new (_("home_ti_set_backgr"),
					GTK_WINDOW (
					  gtk_widget_get_toplevel (button)),
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_OK,
					GTK_RESPONSE_OK,
					GTK_STOCK_CANCEL,
					GTK_RESPONSE_CANCEL,
					NULL);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pixbuf_formats (filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

  g_signal_connect (dialog,
		    "response",
		    G_CALLBACK (background_chooser_file_dialog_response_cb),
		    parent);

  gtk_widget_show (dialog);
}

static void
current_button_clicked_cb (GtkWidget	      *button,
			   HdBackgroundDialog *parent)
{
  const gchar		    *image;
  HdBackgroundDialogPrivate *priv = parent->priv;

  /* Set the background image to the former background image */
  image = g_object_get_data (G_OBJECT (button), "image");
  hd_home_view_set_background_image (priv->view, image);
}

/* Creates a new background image chooser dialog that selects the background
 * for the view held in the private structure of HdBackgroundDialog.
 */
static GtkWidget *
background_chooser_new (HdBackgroundDialog *parent)
{
  HdBackgroundDialogPrivate *priv;
  GtkWidget		    *window, *button_box, *button;
  gchar			    *image;

  priv = parent->priv;

  /* TODO: How is this dialog meant to be closed? */

  /* Create button box */
  button_box = gtk_vbutton_box_new ();

  /* TODO: Add pre-set wallpaper items */

  /* Add current wallpaper (TODO: Check if it's in the pre-set list) */
  g_object_get (priv->view,
		"background-image", &image,
		NULL);
  if (image)
    {
      gchar *basename = pretty_filename (image);
      button = gtk_button_new_with_label (basename);
      g_object_set_data (G_OBJECT (button), "image", image);
      g_object_weak_ref (G_OBJECT (button), (GWeakNotify)g_free, image);
      g_signal_connect (button,
			"clicked",
			G_CALLBACK (current_button_clicked_cb),
			parent);
      gtk_container_add (GTK_CONTAINER (button_box), button);
      g_free (basename);
    }

  /* Add file-chooser button */
  button = gtk_button_new_with_label (_("home_bd_set_backgr_image"));
  g_signal_connect (button,
		    "clicked",
		    G_CALLBACK (background_chooser_file_button_clicked_cb),
		    parent);
  gtk_container_add (GTK_CONTAINER (button_box), button);

  /* Create a window and pack button-box into it */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (parent));
  gtk_widget_show_all (button_box);
  gtk_container_add (GTK_CONTAINER (window), button_box);

  return window;
}

static void
image_button_clicked_cb (GtkWidget	    *button,
			 HdBackgroundDialog *parent)
{
  HdBackgroundDialogPrivate *priv;
  GtkWidget		    *window;

  priv = parent->priv;

  window = background_chooser_new (parent);
  gtk_widget_show (window);
}

/* Updates the subtitle text on the display style chooser button */
static void
update_style_button (HdBackgroundDialog *parent)
{
  HdBackgroundDialogPrivate *priv;
  HdHomeViewBackgroundMode   mode;

  priv = parent->priv;

  g_object_get (priv->view,
		"background-mode", &mode,
		NULL);

  button_set_detail (priv->style_button, background_mode_to_string (mode));
}

static void
mode_button_clicked_cb (GtkWidget	   *button,
			HdBackgroundDialog *parent)
{
  HdHomeViewBackgroundMode   mode;
  HdBackgroundDialogPrivate *priv = parent->priv;

  /* Set the background display mode using the stored mode of this button */
  mode = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "mode"));

  hd_home_view_set_background_mode (priv->view, mode);

  update_style_button (parent);
}

/* Creates a new background display style chooser dialog that selects the style
 * for the background display in the view held in the private structure of
 * HdBackgroundDialog.
 */
static GtkWidget *
style_chooser_new (HdBackgroundDialog *parent)
{
  HdBackgroundDialogPrivate *priv;
  GtkWidget		    *window, *button_box, *button;
  HdHomeViewBackgroundMode   mode;

  priv = parent->priv;

  /* TODO: How is this dialog meant to be closed? */

  /* Create button box */
  button_box = gtk_vbutton_box_new ();

  /* FIXME: Would be better to use FIRST/LAST defines here */
  for (mode = HDHV_BACKGROUND_STRETCHED;
       mode <= HDHV_BACKGROUND_CROPPED; mode++)
    {
      button = gtk_button_new_with_label (background_mode_to_string (mode));

      /* Store the mode as object data on the button so we can reuse the
       * callback.
       */
      g_object_set_data (G_OBJECT (button), "mode", GINT_TO_POINTER (mode));
      g_signal_connect (button,
			"clicked",
			G_CALLBACK (mode_button_clicked_cb),
			parent);
      gtk_container_add (GTK_CONTAINER (button_box), button);
    }

  /* Create a window and pack button-box into it */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (parent));
  gtk_widget_show_all (button_box);
  gtk_container_add (GTK_CONTAINER (window), button_box);

  return window;
}

static void
style_button_clicked_cb (GtkWidget	    *button,
			 HdBackgroundDialog *parent)
{
  HdBackgroundDialogPrivate *priv;
  GtkWidget		    *window;

  priv = parent->priv;

  window = style_chooser_new (parent);
  gtk_widget_show (window);
}

static GObject*
hd_background_dialog_constructor (GType                  type,
				  guint                  n_properties,
				  GObjectConstructParam *properties)
{
  GObjectClass 		    *gobject_class;
  GObject      		    *obj;
  HdBackgroundDialog        *self;
  HdBackgroundDialogPrivate *priv;
  GtkWidget		    *button, *vbox;

  gobject_class = G_OBJECT_CLASS (hd_background_dialog_parent_class);
  obj = gobject_class->constructor (type, n_properties, properties);

  self = HD_BACKGROUND_DIALOG (obj);
  priv = self->priv;

  g_object_get (priv->view,
		"background-color", &priv->color,
		NULL);

  /* Destroy the dialog when the user presses OK (should this be 'Close'?) */
  gtk_dialog_add_button (GTK_DIALOG (self), GTK_STOCK_OK, GTK_RESPONSE_OK);
  g_signal_connect (self, "response", G_CALLBACK (gtk_widget_destroy), NULL);

  vbox = gtk_vbox_new (TRUE, 6);

  /* Add background image chooser button */
  priv->image_button = button_new_with_labels (_("home_fi_set_backgr_image"),
					       "");
  update_image_button (self);
  g_signal_connect (priv->image_button,
		    "clicked",
		    G_CALLBACK (image_button_clicked_cb),
		    self);
  gtk_box_pack_start (GTK_BOX (vbox), priv->image_button, TRUE, TRUE, 0);

  /* Add background image display style chooser button */
  priv->style_button = button_new_with_labels (_("home_fi_set_backgr_mode"),
					       "");
  update_style_button (self);
  g_signal_connect (priv->style_button,
		    "clicked",
		    G_CALLBACK (style_button_clicked_cb),
		    self);
  gtk_box_pack_start (GTK_BOX (vbox), priv->style_button, TRUE, TRUE, 0);

  /* Add background colour chooser button */
  button = color_label_button_new (self, _("home_fi_set_backgr_color"));
  gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

  gtk_widget_show_all (vbox);

  /* Pack into dialog */
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (self)->vbox), vbox, TRUE, TRUE, 0);

  /* Shouldn't be necessary, but will work nicer when the theme isn't set */
  gtk_window_set_position (GTK_WINDOW (self), GTK_WIN_POS_CENTER_ALWAYS);

  /* Set dialog modal */
  gtk_window_set_modal (GTK_WINDOW (self), TRUE);

  return obj;
}

static void
hd_background_dialog_class_init (HdBackgroundDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec   *pspec;

  g_type_class_add_private (klass, sizeof (HdBackgroundDialogPrivate));

  object_class->constructor  = hd_background_dialog_constructor;
  object_class->dispose      = hd_background_dialog_dispose;
  object_class->finalize     = hd_background_dialog_finalize;
  object_class->set_property = hd_background_dialog_set_property;
  object_class->get_property = hd_background_dialog_get_property;

  pspec = g_param_spec_object ("home",
			       "Home",
			       "Parent HdHome object",
			       HD_TYPE_HOME,
			       G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_HOME, pspec);

  pspec = g_param_spec_object ("view",
			       "View",
			       "HdHomeView whose background we're editing",
			       HD_TYPE_HOME_VIEW,
			       G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_HOME_VIEW, pspec);
}

static void
hd_background_dialog_init (HdBackgroundDialog *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
					    HD_TYPE_BACKGROUND_DIALOG,
					    HdBackgroundDialogPrivate);
}

GtkWidget *
hd_background_dialog_new (HdHome *home, HdHomeView *view)
{
  return GTK_WIDGET (g_object_new (HD_TYPE_BACKGROUND_DIALOG,
				   "home", home,
				   "view", view,
				   NULL));
}
