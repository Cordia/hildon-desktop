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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <glib/gi18n.h>
#include <sys/stat.h>

#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gmenu-tree.h>

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "hd-launcher-editor.h"
#include "hd-app-mgr.h"
#include "hd-launcher.h"
#include "hd-launcher-item.h"
#include "hd-launcher-tile.h"

#include "home/hd-render-manager.h"

enum
{
  COL_ICON,
  COL_LABEL,
  COL_DESKTOP_ID,
  NUM_COLS
};

#define CREATE_MODE (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

#define LAYOUT_START    "\t<Layout>\n"
#define LAYOUT_ENTRY    "\t\t<Filename>%s.desktop</Filename>\n"
#define LAYOUT_END      "\t\t<Merge type=\"all\"/>\n" \
                        "\t</Layout>\n\n"

#define DRAG_THRESHOLD (25)

#define I_(str) (g_intern_static_string ((str)))
#define HD_LAUNCHER_EDITOR_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_LAUNCHER_EDITOR, HdLauncherEditorPrivate))

struct _HdLauncherEditorPrivate
{
  GtkTreeModel *model;
  GtkWidget    *icon_view;

  GtkTreePath *selected;

  const gchar *select_label;
  gfloat x_align, y_align;

  gint last_press_x;
  gint last_press_y;

  gboolean dirty;
};

enum
{
  DONE,

  LAST_SIGNAL
};

static guint launcher_editor_signals[LAST_SIGNAL] = {0, };

G_DEFINE_TYPE (HdLauncherEditor, hd_launcher_editor, HILDON_TYPE_WINDOW);

static void
_hd_launcher_editor_load (HdLauncherEditor *editor)
{
  HdLauncherEditorPrivate *priv = HD_LAUNCHER_EDITOR (editor)->priv;
  HdLauncherTree *tree;
  GList *entries;
  GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();

  tree = hd_app_mgr_get_tree();
  entries = hd_launcher_tree_get_items(tree);
  while (entries)
    {
      HdLauncherItem *item = entries->data;
      const gchar *icon_fname = NULL;
      const gchar *icon_name = NULL;
      GtkIconInfo *icon_info = NULL;
      GdkPixbuf *pixbuf = NULL;

      icon_name = hd_launcher_item_get_icon_name (item);
      if (icon_name)
        {
          icon_info = gtk_icon_theme_lookup_icon (icon_theme,
                                      icon_name,
                                      HD_LAUNCHER_TILE_ICON_REAL_SIZE,
                                      GTK_ICON_LOOKUP_NO_SVG);
        }
      if (icon_info == NULL)
        {
          /* Try to load the default icon. */
          icon_info = gtk_icon_theme_lookup_icon(icon_theme,
                                            HD_LAUNCHER_DEFAULT_ICON,
                                            HD_LAUNCHER_TILE_ICON_REAL_SIZE,
                                            GTK_ICON_LOOKUP_NO_SVG);
        }
      if (icon_info)
        {
          icon_fname = gtk_icon_info_get_filename (icon_info);
        }
      if (icon_fname)
        {
          pixbuf = gdk_pixbuf_new_from_file_at_size(icon_fname,
                                          HD_LAUNCHER_TILE_ICON_REAL_SIZE,
                                          HD_LAUNCHER_TILE_ICON_REAL_SIZE, 0);
        }

      gtk_list_store_insert_with_values (GTK_LIST_STORE (priv->model),
                 NULL, -1,
                 COL_ICON, pixbuf,
                 COL_LABEL, hd_launcher_item_get_local_name(item),
                 COL_DESKTOP_ID, hd_launcher_item_get_id(item),
                 -1);

      entries = entries->next;
    }
}

static gboolean
_hd_launcher_editor_ensure_dirs ()
{
  gboolean ret = FALSE;
  gchar *dirname;

  dirname = g_build_filename (g_get_user_config_dir (),
                              "menus", NULL);
  ret = g_file_test (dirname, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR);
  if (!ret)
    {
      ret = !g_mkdir_with_parents (dirname, CREATE_MODE);
    }

  g_free (dirname);
  return ret;
}

static void
_hd_launcher_editor_selection_changed (HdLauncherEditor *editor,
                                         GtkIconView *icon_view)
{
  HdLauncherEditorPrivate *priv = HD_LAUNCHER_EDITOR (editor)->priv;
  GList *sel_list = gtk_icon_view_get_selected_items (icon_view);

  if (priv->selected)
    gtk_tree_path_free (priv->selected);
  if (sel_list)
    priv->selected = gtk_tree_path_copy (sel_list->data);
  else
    priv->selected = NULL;

  g_list_foreach (sel_list, (GFunc)gtk_tree_path_free, NULL);
  g_list_free (sel_list);
}

static gboolean
_hd_launcher_editor_pressed (HdLauncherEditor *editor,
                               GdkEventButton *event,
                               GtkIconView *icon_view)
{
  HdLauncherEditorPrivate *priv = HD_LAUNCHER_EDITOR (editor)->priv;

  /* Keep track of where the press is to check the user is not
   * dragging the widget.
   */
  priv->last_press_x = (gint)event->x;
  priv->last_press_y = (gint)event->y;

  return TRUE;
}

static gboolean
_hd_launcher_editor_released (HdLauncherEditor *editor,
                                GdkEventButton *event,
                                GtkIconView *icon_view)
{
  HdLauncherEditorPrivate *priv = HD_LAUNCHER_EDITOR (editor)->priv;
  gint moved_x, moved_y;
  GtkTreePath *clicked_path;

  /* Check it's the same location as before, if not return as it's a
   * drag event.
   */
  moved_x = abs((gint)event->x - priv->last_press_x);
  moved_y = abs((gint)event->y - priv->last_press_y);
  if (moved_x >= DRAG_THRESHOLD ||
      moved_y >= DRAG_THRESHOLD)
    return FALSE;

  clicked_path = gtk_icon_view_get_path_at_pos (icon_view,
                                              (gint)event->x, (gint)event->y);
  if (!clicked_path)
    {
      /* Clicked somewhere else. */
      return FALSE;
    }

  /* If it's the same path we had already selected, unselect it. */
  if (gtk_icon_view_path_is_selected (icon_view, clicked_path))
    {
      gtk_icon_view_unselect_path (icon_view, clicked_path);
      gtk_widget_queue_draw (GTK_WIDGET (icon_view));
    }
  else
    {
      if (priv->selected == NULL)
        {
            gtk_icon_view_select_path (icon_view, clicked_path);
        }
      else
        {
          GtkTreeIter oldpos, newpos;
          gint comp;

          /* First check if it's the same one. If it's not, move it. */
          comp = gtk_tree_path_compare (priv->selected, clicked_path);
          if (comp)
            {
              gtk_tree_model_get_iter (priv->model, &oldpos, priv->selected);
              gtk_tree_model_get_iter (priv->model, &newpos, clicked_path);
              if (comp < 0)
                gtk_list_store_move_after (GTK_LIST_STORE (priv->model),
                                           &oldpos, &newpos);
              else
                gtk_list_store_move_before (GTK_LIST_STORE (priv->model),
                                            &oldpos, &newpos);
              priv->dirty = TRUE;
            }
          gtk_icon_view_unselect_all (icon_view);
        }
    }

  gtk_tree_path_free (clicked_path);

  return TRUE;
}

static void
_hd_launcher_editor_close (HdLauncherEditor *editor)
{
  g_signal_emit (editor, launcher_editor_signals[DONE], 0, FALSE);
}

static void
_hd_launcher_editor_save (HdLauncherEditor *editor)
{
  HdLauncherEditorPrivate *priv = HD_LAUNCHER_EDITOR (editor)->priv;
  gchar *menu_filename;
  GString *menu;
  GtkTreeIter iter;

  if (!priv->dirty)
    {
      g_signal_emit (editor, launcher_editor_signals[DONE], 0, FALSE);
      return;
    }

  /* Make insensitive while it saves. */
  gtk_widget_set_sensitive (GTK_WIDGET (editor), FALSE);

  if (!gtk_tree_model_get_iter_first (priv->model, &iter))
    {
      g_warning ("%s: App menu ordering model is empty",
                 __FUNCTION__);
      return;
    }

  if (!_hd_launcher_editor_ensure_dirs ())
    {
      g_warning ("%s: Couldn't create directory for applications menu",
                 __FUNCTION__);
      return;
    }

  menu_filename = g_build_filename (g_get_user_config_dir (),
                                    "menus",
                                    HD_LAUNCHER_MENU_FILE,
                                    NULL);
  menu = g_string_new (NULL);
  g_string_append_printf (menu, HD_LAUNCHER_MENU_START,
                          g_get_user_data_dir (),
                          g_get_user_data_dir ());
  g_string_append (menu, LAYOUT_START);

  do
    {
      gchar *name;
      gtk_tree_model_get (priv->model, &iter,
                          COL_DESKTOP_ID, &name,
                          -1);
      g_string_append_printf (menu, LAYOUT_ENTRY, name);
      g_free (name);
    }
  while (gtk_tree_model_iter_next (priv->model, &iter));

  g_string_append (menu, LAYOUT_END);
  g_string_append_printf (menu, HD_LAUNCHER_MENU_END,
                          g_get_user_config_dir ());

  g_file_set_contents (menu_filename,
                       menu->str, -1,
                       NULL);
  g_string_free (menu, TRUE);
  g_free (menu_filename);

  g_signal_emit (editor, launcher_editor_signals[DONE], 0, TRUE);
}

static void
hd_launcher_editor_dispose (GObject *object)
{
  HdLauncherEditorPrivate *priv = HD_LAUNCHER_EDITOR (object)->priv;

  if (priv->model)
    priv->model = (g_object_unref (priv->model), NULL);

  G_OBJECT_CLASS (hd_launcher_editor_parent_class)->dispose (object);
}

static void
hd_launcher_editor_constructed (GObject *object)
{
  HdLauncherEditor *editor = HD_LAUNCHER_EDITOR (object);

  _hd_launcher_editor_load (editor);
}

static void
hd_launcher_editor_class_init (HdLauncherEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdLauncherEditorPrivate));

  object_class->constructed = hd_launcher_editor_constructed;
  object_class->dispose = hd_launcher_editor_dispose;

  launcher_editor_signals[DONE] =
    g_signal_new (I_("done"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1,
                  G_TYPE_BOOLEAN);
}

static void
hd_launcher_editor_init (HdLauncherEditor *editor)
{
  HdLauncherEditorPrivate *priv = HD_LAUNCHER_EDITOR_GET_PRIVATE (editor);
  GtkWidget *toolbar;
  GtkWidget *area;
  GtkCellRenderer *renderer;

  editor->priv = priv;

  /* Set window title and properties*/
  gtk_window_set_title (GTK_WINDOW (editor), HD_LAUNCHER_EDITOR_TITLE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (editor), TRUE);

  /* Create the model */
  priv->model = (GtkTreeModel *) gtk_list_store_new (NUM_COLS,
                                                     GDK_TYPE_PIXBUF,
                                                     G_TYPE_STRING,
                                                     G_TYPE_STRING);

  /* and the icon view. */
  priv->icon_view = hildon_gtk_icon_view_new_with_model (HILDON_UI_MODE_EDIT,
                                                         priv->model);

  renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (G_OBJECT (renderer),
                "xalign", 0.5,
                "yalign", 0.5,
                "width", 64,
                "height", 64,
                NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->icon_view),
                              renderer,
                              FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->icon_view),
                                 renderer,
                                 "pixbuf", COL_ICON);

  /* Add the label renderer */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer),
                "width", 142,
                "height", 96 - (64 + HILDON_MARGIN_HALF),
                "alignment", PANGO_ALIGN_CENTER,
                "ellipsize", PANGO_ELLIPSIZE_END,
                "wrap-width", 142,
                "wrap-mode", PANGO_WRAP_WORD_CHAR,
                "font", "Nokia Sans 15",
                NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->icon_view),
                              renderer,
                              FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->icon_view),
                                 renderer,
                                 "text", COL_LABEL);

  if(STATE_IS_PORTRAIT (hd_render_manager_get_state ())) {
    gtk_icon_view_set_columns (GTK_ICON_VIEW (priv->icon_view), 3);

    /* Force portrait mode */
    hildon_gtk_window_set_portrait_flags(GTK_WINDOW(editor), 
      HILDON_PORTRAIT_MODE_REQUEST);
  }
  else
    gtk_icon_view_set_columns (GTK_ICON_VIEW (priv->icon_view), 5);

  gtk_icon_view_set_item_width (GTK_ICON_VIEW (priv->icon_view), 142);
  gtk_icon_view_set_column_spacing (GTK_ICON_VIEW (priv->icon_view),
                                    HILDON_MARGIN_DEFAULT);
  gtk_icon_view_set_row_spacing (GTK_ICON_VIEW (priv->icon_view), 40);
  gtk_icon_view_set_spacing (GTK_ICON_VIEW (priv->icon_view),
                             HILDON_MARGIN_HALF);

  gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (priv->icon_view),
                                    GTK_SELECTION_SINGLE);
  gtk_icon_view_unselect_all (GTK_ICON_VIEW (priv->icon_view));

  g_signal_connect_swapped (priv->icon_view, "selection-changed",
                            G_CALLBACK (_hd_launcher_editor_selection_changed),
                            editor);
  g_signal_connect_swapped (priv->icon_view, "button-press-event",
                            G_CALLBACK (_hd_launcher_editor_pressed),
                            editor);
  g_signal_connect_swapped (priv->icon_view, "button-release-event",
                            G_CALLBACK (_hd_launcher_editor_released),
                            editor);

  area = hildon_pannable_area_new ();

  toolbar = hildon_edit_toolbar_new_with_text (
      gettext ("tana_fi_reorder"),
      dgettext ("hildon-libs", "wdgt_bd_done"));

  g_signal_connect_swapped (toolbar, "button-clicked",
                            G_CALLBACK (_hd_launcher_editor_save),
                            editor);

  g_signal_connect_swapped (toolbar, "arrow-clicked",
                            G_CALLBACK (_hd_launcher_editor_close),
                            editor);

  hildon_window_set_edit_toolbar (HILDON_WINDOW (editor),
                                  HILDON_EDIT_TOOLBAR (toolbar));
  gtk_container_add (GTK_CONTAINER (area), priv->icon_view);
  gtk_container_add (GTK_CONTAINER (editor), area);
}

GtkWidget *
hd_launcher_editor_new (void)
{
  GtkWidget *editor;

  editor = g_object_new (HD_TYPE_LAUNCHER_EDITOR,
                         NULL);

  return editor;
}

void
hd_launcher_editor_show (GtkWidget *window)
{
  gtk_widget_realize (window);

  GdkWindow *gdk_window = window->window;
  GdkDisplay *gdk_display = gdk_display_get_default ();
  Display *display = GDK_DISPLAY_XDISPLAY (gdk_display);
  Window xwindow = xwindow = GDK_WINDOW_XID (gdk_window);
  Atom no_trans;
  int one = 1;

  /* No transitions for this window. */
  no_trans = gdk_x11_get_xatom_by_name_for_display (gdk_display,
                "_HILDON_WM_ACTION_NO_TRANSITIONS");

  XChangeProperty (display,
                   xwindow,
                   no_trans,
                   XA_CARDINAL, 32, PropModeReplace,
                   (unsigned char *) &one, 1);
  XSync(display, FALSE);

  gtk_widget_show_all (window);
  gtk_window_fullscreen (GTK_WINDOW (window));
}

void
hd_launcher_editor_unselect_all (HdLauncherEditor *editor)
{
  HdLauncherEditorPrivate *priv = editor->priv;

  gtk_icon_view_unselect_all (GTK_ICON_VIEW (priv->icon_view));
}

static gboolean
_hd_launcher_editor_select_label (GtkTreeModel *model,
                                  GtkTreePath *path,
                                  GtkTreeIter *iter,
                                  gpointer data)
{
  HdLauncherEditorPrivate *priv = HD_LAUNCHER_EDITOR (data)->priv;
  gchar *desktop_id;

  gtk_tree_model_get (model, iter, COL_LABEL, &desktop_id, -1);
  if (!g_strcmp0 (desktop_id, priv->select_label))
    {
      gtk_icon_view_select_path (GTK_ICON_VIEW (priv->icon_view), path);
      gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (priv->icon_view), path,
                                    TRUE,
                                    priv->y_align, priv->x_align);
      return TRUE;
    }

  return FALSE;
}

void
hd_launcher_editor_select (HdLauncherEditor *editor,
                           const gchar *desktop_id,
                           gfloat x_align, gfloat y_align)
{
  HdLauncherEditorPrivate *priv = editor->priv;

  gtk_icon_view_unselect_all (GTK_ICON_VIEW (priv->icon_view));

  priv->select_label = desktop_id;
  priv->x_align = x_align;
  priv->y_align = y_align;
  gtk_tree_model_foreach (priv->model,
                          _hd_launcher_editor_select_label,
                          editor);
  priv->select_label = NULL;
}
