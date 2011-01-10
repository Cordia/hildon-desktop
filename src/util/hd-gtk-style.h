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
#ifndef _HD_GTK_STYLE_H_
#define _HD_GTK_STYLE_H_

#include <gtk/gtk.h>

#include <clutter/clutter.h>

/* Since the hildon desktop isn't itself a gtk application but we
 * want to integrate with GTK theming, then we maintain a small
 * number of common GtkWidget singletons for the sake of their
 * GtkStyles. These enums let you choose which widgets style you
 * are interested in referencing. */
typedef enum
{
    HD_GTK_BUTTON_SINGLETON,

    HD_GTK_WIDGET_SINGLETON_COUNT
} HDGtkWidgetSingleton;

/**
 * hd_gtk_style_init:
 *
 * Initialises the internal widget singletons that are needed
 * for their GtkStyles.
 */
void hd_gtk_style_init (void);

void hd_gtk_style_get_fg_color (HDGtkWidgetSingleton  widget_id,
				GtkStateType          state,
				CoglColor            *color);

void hd_gtk_style_get_bg_color (HDGtkWidgetSingleton   widget_id,
				GtkStateType	       state,
				CoglColor	          *color);

void hd_gtk_style_get_light_color (HDGtkWidgetSingleton  widget_id,
				   GtkStateType		 state,
				   CoglColor		*color);

void hd_gtk_style_get_dark_color (HDGtkWidgetSingleton	 widget_id,
				  GtkStateType		 state,
				  CoglColor		    *color);

void hd_gtk_style_get_mid_color (HDGtkWidgetSingleton  widget_id,
				 GtkStateType	       state,
				 CoglColor	          *color);

void hd_gtk_style_get_text_color (HDGtkWidgetSingleton   widget_id,
				  GtkStateType		 state,
				  CoglColor		    *color);

void hd_gtk_style_get_base_color (HDGtkWidgetSingleton	 widget,
				  GtkStateType		 state,
				  CoglColor		    *color);

char *hd_gtk_style_get_font_string (HDGtkWidgetSingleton  widget_id);

void
hd_gtk_style_resolve_logical_color (CoglColor * color,
                                    const gchar * logical_name);

gchar *
hd_gtk_style_resolve_logical_font (const gchar * logical_name);

#endif /* _HD_GTK_STYLE_H_ */

