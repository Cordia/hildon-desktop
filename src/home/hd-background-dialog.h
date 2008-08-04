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

#ifndef __HD_BACKGROUND_DIALOG_H__
#define __HD_BACKGROUND_DIALOG_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include "hd-home.h"
#include "hd-home-view.h"

G_BEGIN_DECLS

#define HD_TYPE_BACKGROUND_DIALOG            (hd_background_dialog_get_type ())
#define HD_BACKGROUND_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_BACKGROUND_DIALOG, HdBackgroundDialog))
#define HD_BACKGROUND_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_BACKGROUND_DIALOG, HdBackgroundDialogClass))
#define HD_IS_BACKGROUND_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_BACKGROUND_DIALOG))
#define HD_IS_BACKGROUND_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_BACKGROUND_DIALOG))
#define HD_BACKGROUND_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_BACKGROUND_DIALOG, HdBackgroundDialogClass))

typedef struct _HdBackgroundDialog        HdBackgroundDialog;
typedef struct _HdBackgroundDialogClass   HdBackgroundDialogClass;
typedef struct _HdBackgroundDialogPrivate HdBackgroundDialogPrivate;

struct _HdBackgroundDialogClass
{
  GtkDialogClass parent_class;
};

struct _HdBackgroundDialog
{
  GtkDialog		    parent;

  HdBackgroundDialogPrivate    *priv;
};

GType       hd_background_dialog_get_type (void);

GtkWidget * hd_background_dialog_new      (HdHome     *home,
					   HdHomeView *view);

G_END_DECLS

#endif
