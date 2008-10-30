/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
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

#ifndef __HD_ADD_APPLET_DIALOG_H__
#define __HD_ADD_APPLET_DIALOG_H__

#include <hildon/hildon.h>

G_BEGIN_DECLS

#define HD_TYPE_ADD_APPLET_DIALOG             (hd_add_applet_dialog_get_type ())
#define HD_ADD_APPLET_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_ADD_APPLET_DIALOG, HDAddAppletDialog))
#define HD_ADD_APPLET_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_ADD_APPLET_DIALOG, HDAddAppletDialogClass))
#define HD_IS_ADD_APPLET_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_ADD_APPLET_DIALOG))
#define HD_IS_ADD_APPLET_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_ADD_APPLET_DIALOG))
#define HD_ADD_APPLET_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_ADD_APPLET_DIALOG, HDAddAppletDialogClass))

typedef struct _HDAddAppletDialog        HDAddAppletDialog;
typedef struct _HDAddAppletDialogClass   HDAddAppletDialogClass;
typedef struct _HDAddAppletDialogPrivate HDAddAppletDialogPrivate;

/** HDAddAppletDialog:
 *
 * A picker dialog for applets
 */
struct _HDAddAppletDialog
{
  HildonPickerDialog parent;

  HDAddAppletDialogPrivate *priv;
};

struct _HDAddAppletDialogClass
{
  HildonPickerDialogClass parent;
};

GType      hd_add_applet_dialog_get_type (void);

GtkWidget *hd_add_applet_dialog_new      (void);

G_END_DECLS

#endif

