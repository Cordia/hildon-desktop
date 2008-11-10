/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>

#include <gconf/gconf-client.h>

#include <string.h>

#define _XOPEN_SOURCE 500
#include <ftw.h>

#include "hd-task-manager.h"

#define HD_TASK_MANAGER_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_TASK_MANAGER, HDTaskManagerPrivate))

#define TASK_SHORTCUTS_GCONF_KEY "/apps/osso/hildon-home/task-shortcuts"

struct _HDTaskManagerPrivate
{
  HDPluginConfiguration *plugin_configuration;

  GtkTreeModel *model;

  GHashTable *available_tasks;
};

typedef struct
{
  gchar *name;
  gchar *icon;
} HDTaskInfo;

G_DEFINE_TYPE (HDTaskManager, hd_task_manager, G_TYPE_OBJECT);

static void
hd_task_manager_load_desktop_file (HDTaskManager *manager,
                                   const gchar   *filename)
{
  HDTaskManagerPrivate *priv = manager->priv;
  GKeyFile *desktop_file;
  GError *error = NULL;
  HDTaskInfo *info = NULL;
  gchar *basename = NULL;

  g_debug ("hd_task_manager_load_desktop_file (%s)", filename);

  desktop_file = g_key_file_new ();
  if (!g_key_file_load_from_file (desktop_file,
                                  filename,
                                  G_KEY_FILE_NONE,
                                  &error))
    {
      g_debug ("Could not read .desktop file `%s'. %s",
               filename,
               error->message);
      g_error_free (error);
      goto cleanup;
    }

  info = g_slice_new0 (HDTaskInfo);

  info->name = g_key_file_get_string (desktop_file,
                                      G_KEY_FILE_DESKTOP_GROUP,
                                      G_KEY_FILE_DESKTOP_KEY_NAME,
                                      &error);
  if (!info->name)
    {
      g_debug ("Could not read .desktop file `%s'. %s",
               filename,
               error->message);
      g_error_free (error);
      goto cleanup;
    }

  info->name = gettext (info->name);

  info->icon = g_key_file_get_string (desktop_file,
                                      G_KEY_FILE_DESKTOP_GROUP,
                                      G_KEY_FILE_DESKTOP_KEY_ICON,
                                      &error);
  if (!info->icon)
    {
      g_debug ("Could not read Icon entry in .desktop file `%s'. %s",
               filename,
               error->message);
      g_error_free (error);
    }

  basename = g_path_get_basename (filename);

  g_hash_table_insert (priv->available_tasks,
                       basename,
                       info);

  gtk_list_store_insert_with_values (GTK_LIST_STORE (priv->model),
                                     NULL, -1,
                                     0, info->name,
                                     1, info->icon,
                                     2, basename,
                                     -1);
 
cleanup:
  if (info)
    g_slice_free (HDTaskInfo, info);
  g_key_file_free (desktop_file);
}

static int
visit_func (const char        *f_path,
            const struct stat *sb,
            int                type_flag,
            struct FTW        *ftw_buf)
{
  g_debug ("visit_func %s, %d", f_path, type_flag);

  /* Directory */
  switch (type_flag)
    {
      case FTW_D:
          {
/*            GnomeVFSMonitorHandle* handle;

            gnome_vfs_monitor_add (&handle,
                                   f_path,
                                   GNOME_VFS_MONITOR_DIRECTORY,
                                   (GnomeVFSMonitorCallback) applications_dir_changed,
                                   NULL);*/
          }
        break;
      case FTW_F:
        hd_task_manager_load_desktop_file (hd_task_manager_get (),
                                           f_path);
        break;
      default:
        g_debug ("%s, %d", f_path, type_flag);
    }

  return 0;
}

static gboolean
hd_task_manager_scan_for_desktop_files (const gchar   *directory)
{
  g_debug ("hd_task_manager_scan_for_desktop_files: %s", directory);

  nftw (directory, visit_func, 20, FTW_PHYS); 

  return FALSE;
}

static void
hd_task_manager_init (HDTaskManager *manager)
{
  HDTaskManagerPrivate *priv;
  manager->priv = HD_TASK_MANAGER_GET_PRIVATE (manager);
  priv = manager->priv;

  priv->available_tasks = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 g_free, NULL);

  priv->model = GTK_TREE_MODEL (gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (priv->model),
                                        0,
                                        GTK_SORT_ASCENDING);

}

static void
hd_task_manager_class_init (HDTaskManagerClass *klass)
{
  g_type_class_add_private (klass, sizeof (HDTaskManagerPrivate));
}

HDTaskManager *
hd_task_manager_get (void)
{
  static HDTaskManager *manager = NULL;

  if (G_UNLIKELY (!manager))
    {
      manager = g_object_new (HD_TYPE_TASK_MANAGER, NULL);

      g_idle_add ((GSourceFunc) hd_task_manager_scan_for_desktop_files,
                  HD_APPLICATIONS_DIR);
    }

  return manager;
}

GtkTreeModel *
hd_task_manager_get_model (HDTaskManager *manager)
{
  HDTaskManagerPrivate *priv = manager->priv;

  return g_object_ref (priv->model);
}

void
hd_task_manager_install_task (HDTaskManager *manager,
                              GtkTreeIter     *iter)
{
  HDTaskManagerPrivate *priv = manager->priv;
  GConfClient *client;
  gchar *desktop_id;
  GSList *list;
  GError *error = NULL;

  client = gconf_client_get_default ();

  gtk_tree_model_get (priv->model, iter,
                      2, &desktop_id,
                      -1);
  
  /* Get the current list of task shortcuts from GConf */
  list = gconf_client_get_list (client,
                                TASK_SHORTCUTS_GCONF_KEY,
                                GCONF_VALUE_STRING,
                                &error);

  if (error)
    {
      g_warning ("Could not get string list from GConf (%s): %s.",
                 TASK_SHORTCUTS_GCONF_KEY,
                 error->message);
      g_error_free (error);
      error = NULL;
    }

  /* Append the new task */
  list = g_slist_append (list, desktop_id);

  /* Set the new list to GConf */
  gconf_client_set_list (client,
                         TASK_SHORTCUTS_GCONF_KEY,
                         GCONF_VALUE_STRING,
                         list,
                         &error);

  if (error)
    {
      g_warning ("Could not write string list from GConf (%s): %s.",
                 TASK_SHORTCUTS_GCONF_KEY,
                 error->message);
      g_error_free (error);
      error = NULL;
    }

  /* Free */
  g_slist_foreach (list, (GFunc) g_free, NULL);
  g_slist_free (list);

  g_object_unref (client);
}
