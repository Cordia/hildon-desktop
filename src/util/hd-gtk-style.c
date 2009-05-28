/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Robert Bragg <bob@o-hand.com>
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

#include <hd-gtk-style.h>

#include <clutter/clutter.h>
#include <gtk/gtk.h>
#include <string.h>

enum {
  HD_GTK_STYLE_FG,
  HD_GTK_STYLE_BG,
  HD_GTK_STYLE_LIGHT,
  HD_GTK_STYLE_DARK,
  HD_GTK_STYLE_MID,
  HD_GTK_STYLE_TEXT,
  HD_GTK_STYLE_BASE
};

static GtkWidget *top_level_window = NULL;
static GtkWidget *gtk_widget_singletons[HD_GTK_WIDGET_SINGLETON_COUNT] = {0};

/* We create some singleton GtkWidgets and realize them so that the
 * gtkrc is read and they have a valid GtkStyle that can then
 * be accessed */
void
hd_gtk_style_init (void)
{
  GtkWidget *button;

  if (gtk_widget_singletons[HD_GTK_BUTTON_SINGLETON] != NULL)
    return;

  /* We create a top level window so we can realize other styled
   * widgets
   */
  top_level_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated (GTK_WINDOW (top_level_window), FALSE);
  gtk_widget_realize (top_level_window);

  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (top_level_window), button);
  gtk_widget_show (button);
  gtk_widget_realize (button);
  gtk_widget_singletons[HD_GTK_BUTTON_SINGLETON] = button;
}

static void
hd_gtk_style_to_clutter_color(ClutterColor          *dst,
                              const GdkColor        *src)
{
  dst->red   = CLAMP (((src->red   / 65535.0) * 255), 0, 255);
  dst->green = CLAMP (((src->green / 65535.0) * 255), 0, 255);
  dst->blue  = CLAMP (((src->blue  / 65535.0) * 255), 0, 255);
  dst->alpha = 255;
}

static void
hd_gtk_style_get_color_component (HDGtkWidgetSingleton   widget_id,
				  GtkRcFlags             component,
				  GtkStateType           state,
				  ClutterColor          *color)
{
  GtkWidget *widget;
  GtkStyle *style;
  GdkColor gtk_color = { 0, };

  g_return_if_fail (widget_id < HD_GTK_WIDGET_SINGLETON_COUNT);
  g_return_if_fail (state < (sizeof (style->fg)/sizeof (GdkColor)));

  widget = gtk_widget_singletons[widget_id];
  style = gtk_widget_get_style (widget);

  if (!style) {
    g_critical("%s: gtk_widget_get_style returned NULL", __FUNCTION__);
    return;
  }

  switch (component)
    {
    case HD_GTK_STYLE_FG:
      gtk_color = style->fg[state];
      break;

    case HD_GTK_STYLE_BG:
      gtk_color = style->bg[state];
      break;

    case HD_GTK_STYLE_LIGHT:
      gtk_color = style->light[state];
      break;

    case HD_GTK_STYLE_DARK:
      gtk_color = style->dark[state];
      break;

    case HD_GTK_STYLE_MID:
      gtk_color = style->mid[state];
      break;

    case HD_GTK_STYLE_TEXT:
      gtk_color = style->text[state];
      break;

    case HD_GTK_STYLE_BASE:
      gtk_color = style->base[state];
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  hd_gtk_style_to_clutter_color(color, &gtk_color);
}

void
hd_gtk_style_get_fg_color (HDGtkWidgetSingleton  widget_id,
			   GtkStateType          state,
			   ClutterColor         *color)
{
  g_return_if_fail (color != NULL);

  hd_gtk_style_get_color_component (widget_id, HD_GTK_STYLE_FG, state, color);
}

void
hd_gtk_style_get_bg_color (HDGtkWidgetSingleton  widget_id,
			   GtkStateType		 state,
			   ClutterColor		*color)
{
  g_return_if_fail (color != NULL);

  hd_gtk_style_get_color_component (widget_id, HD_GTK_STYLE_BG, state, color);
}

void
hd_gtk_style_get_light_color (HDGtkWidgetSingleton   widget_id,
			      GtkStateType	     state,
			      ClutterColor	    *color)
{
  g_return_if_fail (color != NULL);

  hd_gtk_style_get_color_component (widget_id, HD_GTK_STYLE_LIGHT,
				    state, color);
}

void
hd_gtk_style_get_dark_color (HDGtkWidgetSingleton  widget_id,
			     GtkStateType	   state,
			     ClutterColor	  *color)
{
  g_return_if_fail (color != NULL);

  hd_gtk_style_get_color_component (widget_id, HD_GTK_STYLE_DARK,
				    state, color);
}

void
hd_gtk_style_get_mid_color (HDGtkWidgetSingleton   widget_id,
			    GtkStateType	   state,
			    ClutterColor	  *color)
{
  g_return_if_fail (color != NULL);

  hd_gtk_style_get_color_component (widget_id, HD_GTK_STYLE_MID,
				    state, color);
}

void
hd_gtk_style_get_text_color (HDGtkWidgetSingleton  widget_id,
			     GtkStateType	   state,
			     ClutterColor	  *color)
{
  g_return_if_fail (color != NULL);

  hd_gtk_style_get_color_component (widget_id, HD_GTK_STYLE_TEXT, state, color);
}

void
hd_gtk_style_get_base_color (HDGtkWidgetSingleton  widget_id,
			     GtkStateType	   state,
			     ClutterColor	  *color)
{
  g_return_if_fail (color != NULL);

  hd_gtk_style_get_color_component (widget_id, HD_GTK_STYLE_BASE, state, color);
}

/**
 * hd_gtk_style_get_font_string:
 * @widget_id: The index of a particular gtk widget singleton whos
 *             style you want to reference.
 *
 * Gets a string representation of the font description held within
 * the GtkStyle of the specified widget. The string is suitable
 * for passing to pango_font_description_from_string and thus
 * clutter_label_new_full.
 *
 * Return value: a new string that must be freed with g_free().
 */
char *
hd_gtk_style_get_font_string (HDGtkWidgetSingleton  widget_id)
{
  GtkWidget *widget;
  GtkStyle *style;

  g_return_val_if_fail (widget_id < HD_GTK_WIDGET_SINGLETON_COUNT, NULL);

  widget = gtk_widget_singletons[widget_id];
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  style = gtk_widget_get_style (widget);

  return pango_font_description_to_string (style->font_desc);
}

/* Fonts and colors {{{ */
/* Resolves a logical color name to a #ClutterColor. */
void
hd_gtk_style_resolve_logical_color (ClutterColor * color,
                                    const gchar * logical_name)
{
  GtkStyle *style;
  GdkColor gtk_color;

  style = gtk_rc_get_style_by_paths (gtk_settings_get_default (),
                                     NULL, NULL,
                                     GTK_TYPE_WIDGET);
  if (!style || !gtk_style_lookup_color (style, logical_name, &gtk_color))
    { /* Fall back to all-black. */
      g_critical ("%s: unknown color", logical_name);
      memset (&gtk_color, 0, sizeof (gtk_color));
    }

  hd_gtk_style_to_clutter_color(color, &gtk_color);
}

/* Returns a font descrition string for a logical font name you can use
 * to create #ClutterLabel:s.  The returned string is yours. */
gchar *
hd_gtk_style_resolve_logical_font (const gchar * logical_name)
{
  GtkStyle *style;

  style = gtk_rc_get_style_by_paths (gtk_settings_get_default (),
                                     logical_name, NULL, G_TYPE_NONE);
  if (!style)
    { /* Fall back to system font. */
      g_critical("%s: unknown font", logical_name);
      return g_strdup ("Nokia Sans 18");
    }
  else
    return pango_font_description_to_string (style->font_desc);
}
/* Fonts and colors }}} */
