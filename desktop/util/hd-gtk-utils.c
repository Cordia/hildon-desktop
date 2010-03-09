
#include "hd-gtk-utils.h"

#include <gtk/gtk.h>
#include <clutter/clutter.h>

/**
 * hd_gtk_icon_theme_load_icon:
 * @icon_theme: a #GtkIconTheme
 * @icon_name: the name of the icon to lookup
 * @size: the desired icon size. The resulting icon may not be
 *        exactly this size; see gtk_icon_info_load_icon().
 * @flags: flags modifying the behavior of the icon lookup
 *
 * Looks up an icon in an icon theme according to gtk_icon_theme_load_icon()
 * and uploads the pixmap data into a ClutterTexture.
 *
 * Return value: The newly created ClutterTexture. If a pixmap can't be loaded
 * or the data can't be uploaded to a texture then a place holder
 * ClutterRectangle is created instead.
 */
ClutterActor *
hd_gtk_icon_theme_load_icon (GtkIconTheme         *icon_theme,
			     const gchar          *icon_name,
			     gint                  size,
			     GtkIconLookupFlags    flags)
{
  GError *tmp_error = NULL;
  GdkPixbuf *icon_pixbuf;
  ClutterActor *texture;
  ClutterActor *fake_icon;

  icon_pixbuf =
    gtk_icon_theme_load_icon (icon_theme, icon_name, size, flags, &tmp_error);
  if (tmp_error != NULL)
    goto error;

  texture = clutter_texture_new();
  clutter_texture_set_from_rgb_data (
      CLUTTER_TEXTURE (texture),
      gdk_pixbuf_get_pixels (icon_pixbuf),
      TRUE,
      gdk_pixbuf_get_width (icon_pixbuf),
      gdk_pixbuf_get_height (icon_pixbuf),
      gdk_pixbuf_get_rowstride (icon_pixbuf),
      4,
#ifdef MAEMO_CHANGES
      CLUTTER_TEXTURE_FLAG_16_BIT,
#else
      0,
#endif
      &tmp_error);

  g_object_unref (icon_pixbuf);

  if (tmp_error != NULL)
    goto error;

  return texture;

error:
  g_error ("Failure loading icon %s: %s", icon_name, tmp_error->message);
  fake_icon = clutter_rectangle_new ();
  clutter_actor_set_size (fake_icon, size, size);
  clutter_actor_set_name (fake_icon, "LOAD FAIL ICON");
  return fake_icon;
}

