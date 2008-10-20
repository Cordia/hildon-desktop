#ifndef _HD_GTK_UTILS_H_
#define _HD_GTK_UTILS_H_

#include <gtk/gtk.h>
#include <clutter/clutter.h>

ClutterActor *
hd_gtk_icon_theme_load_icon (GtkIconTheme         *icon_theme,
			     const gchar          *icon_name,
			     gint                  size,
			     GtkIconLookupFlags    flags);


#endif /* _HD_GTK_UTILS_H_ */

