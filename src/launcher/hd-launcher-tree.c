#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hildon-desktop.h"
#include "hd-launcher-tree.h"

#include "hd-gtk-style.h"

#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <clutter/clutter.h>

#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gmenu-tree.h>

#define HD_LAUNCHER_TREE_GET_PRIVATE(obj)       (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HD_TYPE_LAUNCHER_TREE, HdLauncherTreePrivate))

typedef struct
{
  HdLauncherTree *tree;
  GMenuTreeDirectory *root;
  guint level;

  /* The items we have created so far. */
  GList *items;

  /* accessed by both threads */
  volatile gboolean cancelled : 1;
} WalkThreadData;

struct _HdLauncherTreePrivate
{
  /* we keep the items inside a list because
   * it's easier to iterate than a tree
   */
  GList *items_list;

  /* this is the actual tree of launchers, as
   * built by parsing the applications.menu file
   */
  GMenuTree *tree;
  GMenuTreeDirectory *root;

  WalkThreadData *active_walk;

  gboolean theme_changed_signal_connected : 1;
};

enum
{
  STARTING,
  FINISHED,

  LAST_SIGNAL
};

static gulong tree_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (HdLauncherTree, hd_launcher_tree, G_TYPE_OBJECT);

static void hd_launcher_tree_handle_tree_changed (GMenuTree *menu_tree,
                                                  gpointer user_data);

static void hd_launcher_tree_handle_theme_changed (HdLauncherTree *tree);

static WalkThreadData *
walk_thread_data_new (HdLauncherTree *tree)
{
  WalkThreadData *data;

  data = g_new0 (WalkThreadData, 1);
  data->tree = g_object_ref (tree);
  data->level = 0;
  data->cancelled = FALSE;
  data->items = NULL;

  return data;
}

static WalkThreadData *
walk_thread_data_new_level (WalkThreadData *parent, GMenuTreeDirectory *dir)
{
  WalkThreadData *result = walk_thread_data_new (parent->tree);
  result->level = parent->level + 1;
  result->root = dir;
  return result;
}

static void
walk_thread_data_free (WalkThreadData *data)
{
  g_object_unref (data->tree);

  g_free (data);
}

/**
 * TODO: When we get here, we have two lists of items, the old one
 * and the new one.
 * Using idle times, we replace the old one with the new information
 * but, instead of replacing old items, copying the new values as the
 * old items contain run-time information we can't discard.
 * We also send signals for new and removed items.
 */
static gboolean
walk_thread_done_idle (gpointer user_data)
{
  WalkThreadData *data = user_data;
  HdLauncherTreePrivate *priv = HD_LAUNCHER_TREE_GET_PRIVATE (data->tree);

  if ((priv->active_walk == data) && !data->cancelled)
    {
      /* This is the correct walking. */
      g_list_foreach (priv->items_list, (GFunc) g_object_unref, NULL);
      g_list_free (priv->items_list);
      priv->items_list = data->items;
      priv->active_walk = NULL;
      gmenu_tree_item_unref (data->root);
      g_signal_emit (data->tree, tree_signals[FINISHED], 0);

      /* Once the first walk is done, connect to the theme change signal. */
      if (!priv->theme_changed_signal_connected)
        {
          g_signal_connect_swapped (gtk_icon_theme_get_default (),
                                    "changed",
                                    G_CALLBACK (hd_launcher_tree_handle_theme_changed),
                                    data->tree);
          priv->theme_changed_signal_connected = TRUE;
        }

      walk_thread_data_free (data);

      hd_mutex_enable (FALSE);
    }
  else
    {
      /* This is the result of an obsolete walking, get rid of it. */
      g_list_foreach (data->items, (GFunc) g_object_unref, NULL);\
      gmenu_tree_item_unref (data->root);
      walk_thread_data_free (data);
    }

  return FALSE;
}

/**
 * This function, in a separate thread, builds up a list of items
 * reading their .desktop files.
 */
static gpointer
walk_thread_func (gpointer user_data)
{
  WalkThreadData *data = user_data;
  GSList *entries;
  GSList *tmp;

  entries = gmenu_tree_directory_get_contents (data->root);
  tmp = entries;
  while (tmp)
    {
      GMenuTreeItem *tmp_entry = tmp->data;
      HdLauncherItem *item = NULL;
      gchar *id;
      const gchar *key_file_path;
      GKeyFile *key_file = NULL;
      struct stat key_file_stat;
      GError *error = NULL;

      switch (gmenu_tree_item_get_type (tmp_entry))
      {
      case GMENU_TREE_ITEM_ENTRY:
        {
          GMenuTreeEntry *entry = GMENU_TREE_ENTRY (tmp_entry);
          const gchar *id_desktop = NULL;
          /* We want the id without the .desktop suffix. */
          id_desktop = gmenu_tree_entry_get_desktop_file_id (entry);
          if (g_str_has_suffix (id_desktop, ".desktop"))
            id = g_strndup (id_desktop, strlen (id_desktop) - strlen (".desktop"));
          else
            id = g_strdup (id_desktop);
          key_file_path = gmenu_tree_entry_get_desktop_file_path (entry);
          break;
        }
      case GMENU_TREE_ITEM_DIRECTORY:
        {
          GMenuTreeDirectory *entry_dir = GMENU_TREE_DIRECTORY (tmp_entry);
          id = g_strdup (gmenu_tree_directory_get_menu_id (entry_dir));
          key_file_path = gmenu_tree_directory_get_desktop_file_path (entry_dir);

          /* Iterate. */
          WalkThreadData *subdata = walk_thread_data_new_level (data, entry_dir);
          subdata->root = entry_dir;
          walk_thread_func ((gpointer)subdata);
          data->items = g_list_concat (data->items, subdata->items);
          walk_thread_data_free (subdata);

          break;
        }
      default:
        tmp = tmp->next;
        continue;
      }

      if (stat(key_file_path, &key_file_stat))
        {
          g_warning ("%s: Unable to stat %s", __FUNCTION__,
                               key_file_path);
        }
      else
        {
          key_file = g_key_file_new ();
          g_key_file_load_from_file (key_file, key_file_path, 0, &error);
          if (error)
            {
              g_warning ("%s: Unable to parse %s: %s", __FUNCTION__,
                         key_file_path,
                         error->message);

              g_error_free (error);
              g_key_file_free (key_file);
              key_file = NULL;
            }
        }

      if (key_file) {
        item = hd_launcher_item_new_from_keyfile (id,
                  gmenu_tree_directory_get_menu_id (data->root),
                  key_file, NULL);
	g_key_file_free (key_file);
      }
      if (item)
        data->items = g_list_prepend (data->items, (gpointer)item);

      g_free (id);
      gmenu_tree_item_unref (tmp->data);
      tmp = tmp->next;
    }

  g_slist_free (entries);

  if (data->level == 0)
    {
      data->items = g_list_reverse (data->items);

      clutter_threads_add_idle (walk_thread_done_idle, data);
    }

  return NULL;
}

static void
hd_launcher_tree_finalize (GObject *gobject)
{
  HdLauncherTreePrivate *priv = HD_LAUNCHER_TREE_GET_PRIVATE (gobject);

  if (priv->active_walk)
    {
      priv->active_walk->cancelled = TRUE;
      walk_thread_data_free (priv->active_walk);
      priv->active_walk = NULL;
    }

  g_list_foreach (priv->items_list, (GFunc) g_object_unref, NULL);
  g_list_free (priv->items_list);
  priv->items_list = NULL;

  if (priv->root)
    {
      gmenu_tree_item_unref (priv->root);
      priv->root = NULL;
    }

  if (priv->tree)
    {
      gmenu_tree_remove_monitor (priv->tree,
                                 (GMenuTreeChangedFunc) hd_launcher_tree_handle_tree_changed,
                                 gobject);
      gmenu_tree_unref (priv->tree);
      priv->tree = NULL;
    }

  G_OBJECT_CLASS (hd_launcher_tree_parent_class)->finalize (gobject);
}

static void
hd_launcher_tree_class_init (HdLauncherTreeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdLauncherTreePrivate));

  gobject_class->finalize = hd_launcher_tree_finalize;

  tree_signals[STARTING] =
    g_signal_new ("starting",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  tree_signals[FINISHED] =
    g_signal_new ("finished",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hd_launcher_tree_init (HdLauncherTree *tree)
{
  tree->priv = HD_LAUNCHER_TREE_GET_PRIVATE (tree);

  tree->priv->active_walk = NULL;
}

HdLauncherTree *
hd_launcher_tree_new ()
{
  HdLauncherTree  *tree;

  tree = g_object_new (HD_TYPE_LAUNCHER_TREE, NULL);

  return tree;
}

static void
hd_launcher_tree_handle_tree_changed (GMenuTree *menu_tree,
                                      gpointer user_data)
{
  HdLauncherTree *self = HD_LAUNCHER_TREE (user_data);
  HdLauncherTreePrivate *priv = HD_LAUNCHER_TREE_GET_PRIVATE (self);
  WalkThreadData *data;
  GMenuTreeDirectory *root;

  /* We need to do this everytime or, for some reason, change notifications
   * stop working.
   */
  root = gmenu_tree_get_root_directory (priv->tree);
  if (!root)
    return;

  if (priv->active_walk)
    {
      /* We already have an active walk, cancel it. */
      priv->active_walk->cancelled = TRUE;
      priv->active_walk = NULL;
    }
  else
    {
      /* Only signal starting for the first walking. */
      g_signal_emit (self, tree_signals[STARTING], 0);
    }

  data = walk_thread_data_new (self);
  data->root = root;

  priv->active_walk = data;
  if (hd_disable_threads ())
    walk_thread_func (data);
  else
    {
      hd_mutex_enable (TRUE);
      g_thread_create (walk_thread_func, data, FALSE, NULL);
    }
}

/* When there's a theme change, tell clients to completely rebuild the
 * tree.
 */
static void
hd_launcher_tree_handle_theme_changed (HdLauncherTree *tree)
{
  HdLauncherTreePrivate *priv = tree->priv;
  if (priv->active_walk)
    return;

  g_signal_emit (tree, tree_signals[STARTING], 0);
  g_signal_emit (tree, tree_signals[FINISHED], 0);
}

/**
 * hd_launcher_tree_populate:
 * @tree: a #HdLauncherTree
 *
 * Populates the @tree with the launchers by walking
 * the applications directory using an helper thread
 * to avoid blocking.
 *
 * Emits the #HdLauncherTree::finished
 * when done.
 */
void
hd_launcher_tree_populate (HdLauncherTree *tree)
{
  g_return_if_fail (HD_IS_LAUNCHER_TREE (tree));
  HdLauncherTreePrivate *priv = HD_LAUNCHER_TREE_GET_PRIVATE (tree);

  priv->tree = gmenu_tree_lookup (HD_LAUNCHER_MENU_FILE,
                                  GMENU_TREE_FLAGS_SHOW_EMPTY);
  if (!priv->tree)
    {
      g_warning ("%s: Couldn't load menu.", __FUNCTION__);
      return;
    }

  /* We need to do this here or the monitor won't work. */
  priv->root = gmenu_tree_get_root_directory (priv->tree);
  if (!priv->root)
    {
      g_warning ("%s: Menu is empty", __FUNCTION__);
      return;
    }

  hd_launcher_tree_handle_tree_changed (priv->tree, tree);

  gmenu_tree_add_monitor (priv->tree,
                          (GMenuTreeChangedFunc) hd_launcher_tree_handle_tree_changed,
                          (gpointer)tree);
}

GList *
hd_launcher_tree_get_items (HdLauncherTree *tree)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_TREE (tree), NULL);

  return tree->priv->items_list;
}

guint
hd_launcher_tree_get_size (HdLauncherTree *tree)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_TREE (tree), 0);

  return g_list_length (tree->priv->items_list);
}

static gint
_compare_item_id (gconstpointer a, gconstpointer b)
{
  return g_strcmp0(hd_launcher_item_get_id(HD_LAUNCHER_ITEM (a)),
                   (const gchar *)b);
}

HdLauncherItem *
hd_launcher_tree_find_item (HdLauncherTree *tree, const gchar *id)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_TREE (tree), NULL);
  HdLauncherTreePrivate *priv = HD_LAUNCHER_TREE_GET_PRIVATE (tree);

  GList *res = g_list_find_custom (priv->items_list, id,
                                   (GCompareFunc)_compare_item_id);
  if (res)
    return res->data;
  return NULL;
}

static gint
_compare_service (gconstpointer a, gconstpointer b)
{
  if (!HD_IS_LAUNCHER_APP(a))
    return !0;
  return g_strcmp0(hd_launcher_app_get_service(HD_LAUNCHER_APP (a)),
                   (const gchar *)b);
}

HdLauncherApp *
hd_launcher_tree_find_app_by_service (HdLauncherTree *tree, const gchar *service)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_TREE (tree), NULL);
  HdLauncherTreePrivate *priv = HD_LAUNCHER_TREE_GET_PRIVATE (tree);

  GList *res = g_list_find_custom (priv->items_list, service,
                                   (GCompareFunc)_compare_service);
  if (res)
    return res->data;
  return NULL;
}

#define CREATE_MODE (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

void
hd_launcher_tree_ensure_user_menu (void)
{
  gchar *menu_filename, *menu_dirname;

  /* The menu directory. */
  menu_dirname = g_build_filename (g_get_user_config_dir (),
                                   "menus/hildon", NULL);
  if (!g_file_test (menu_dirname, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
    {
      if (g_mkdir_with_parents (menu_dirname, CREATE_MODE))
        {
          g_warning ("%s: Couldn't create dir %s", __FUNCTION__, menu_dirname);
          g_free (menu_dirname);
          return;
        }
    }

  menu_filename = g_build_filename (g_get_user_config_dir (),
                                    "menus", HD_LAUNCHER_MENU_FILE,
                                    NULL);
  if (!g_file_test (menu_filename, G_FILE_TEST_EXISTS))
    {
      GString *menu = g_string_new (NULL);
      g_string_printf (menu, HD_LAUNCHER_MENU_START,
                       g_get_user_data_dir (),
                       g_get_user_data_dir ());
      g_string_append_printf (menu, HD_LAUNCHER_MENU_END,
                              g_get_user_config_dir ());
      g_file_set_contents (menu_filename, menu->str, -1, NULL);
    }
  g_free (menu_filename);

  /* The desktop files directory. */
  menu_dirname = g_build_filename (g_get_user_data_dir (),
                                   "applications/hildon", NULL);
  if (!g_file_test (menu_dirname, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
    {
      if (g_mkdir_with_parents (menu_dirname, CREATE_MODE))
        {
          g_warning ("%s: Couldn't create dir %s", __FUNCTION__, menu_dirname);
        }
    }
  g_free (menu_dirname);
}
