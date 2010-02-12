/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2010 Nokia Corporation.
 *
 * Author:  Marc Ordinas i Llopis <marc.ordinasillopis@collabora.co.uk>
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

#ifndef __HD_LAUNCHER_EDITOR_H__
#define __HD_LAUNCHER_EDITOR_H__

#include <hildon/hildon.h>

G_BEGIN_DECLS

#define HD_TYPE_LAUNCHER_EDITOR             (hd_launcher_editor_get_type ())
#define HD_LAUNCHER_EDITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_LAUNCHER_EDITOR, HdLauncherEditor))
#define HD_LAUNCHER_EDITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_LAUNCHER_EDITOR, HdLauncherEditorClass))
#define HD_IS_LAUNCHER_EDITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_LAUNCHER_EDITOR))
#define HD_IS_LAUNCHER_EDITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_LAUNCHER_EDITOR))
#define HD_LAUNCHER_EDITOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_LAUNCHER_EDITOR, HdLauncherEditorClass))

#define HD_LAUNCHER_EDITOR_TITLE "HdLauncherEditor"

typedef struct _HdLauncherEditor        HdLauncherEditor;
typedef struct _HdLauncherEditorClass   HdLauncherEditorClass;
typedef struct _HdLauncherEditorPrivate HdLauncherEditorPrivate;

/** HdLauncherEditor:
 *
 * A dialog for ordering the applications in the task launcher.
 */
struct _HdLauncherEditor
{
  HildonWindow parent;

  HdLauncherEditorPrivate *priv;
};

struct _HdLauncherEditorClass
{
  HildonWindowClass parent;
};

GType      hd_launcher_editor_get_type (void);

GtkWidget *hd_launcher_editor_new      (void);

void       hd_launcher_editor_show (GtkWidget *window);
void       hd_launcher_editor_unselect_all (HdLauncherEditor *window);
void       hd_launcher_editor_select (HdLauncherEditor *window,
                                      const gchar *text,
                                      gfloat x_align, gfloat y_align);

G_END_DECLS

#endif /* __HD_LAUNCHER_EDITOR_H__ */
