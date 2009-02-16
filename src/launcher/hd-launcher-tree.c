#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hd-launcher-tree.h"

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ftw.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <clutter/clutter.h>

/* the amount of desktop files we parse in the loader thread
 * before we consume them inside the ::item-added signal
 */
#define CHUNK_SIZE      25

/* where the menu XML resides */
#define HILDON_DESKTOP_MENU_DIR                 "xdg" G_DIR_SEPARATOR_S "menus"
#define HILDON_DESKTOP_APPLICATIONS_MENU        "applications.menu"

/* where to find the desktop files */
#define HILDON_DESKTOP_APPLICATIONS_DIR         "applications"

#define HD_LAUNCHER_TREE_GET_PRIVATE(obj)       (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HD_TYPE_LAUNCHER_TREE, HdLauncherTreePrivate))

typedef struct
{
  gchar *id;
  GKeyFile *key_file;
} WalkItem;

typedef struct
{
  HdLauncherTree *tree;

  gchar *path;
  GList *files_list;

  gint n_processed_files;

  /* accessed by both threads */
  volatile guint cancelled : 1;
} WalkThreadData;

typedef struct
{
  GList *items;
  gint n_items;
  gint current_pos;

  WalkThreadData *thread_data;
} WalkItems;

struct _HdLauncherTreePrivate
{
  /* the base path of the desktop files */
  gchar *path;

  /* the path of the menu file */
  gchar *menu_path;

  /* keep a shortcut to the top-level items */
  GList *top_levels;

  /* we keep the items inside a list because
   * it's easier to iterate than a tree
   */
  GList *items_list;

  /* this is the actual tree of launchers, as
   * built by parsing the applications.menu file
   */
  GNode *tree;

  guint n_top_levels;
  guint size;

  WalkThreadData *active_walk;

  guint has_finished : 1;
};

enum
{
  PROP_0,

  PROP_MENU_PATH
};

enum
{
  ITEM_ADDED,
  ITEM_REMOVED,
  ITEM_CHANGED,
  FINISHED,

  LAST_SIGNAL
};

static gulong tree_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (HdLauncherTree, hd_launcher_tree, G_TYPE_OBJECT);

static void
walk_item_free (WalkItem *item)
{
  g_free (item->id);
  g_key_file_free (item->key_file);
}

static WalkThreadData *
walk_thread_data_new (HdLauncherTree *tree)
{
  WalkThreadData *data;

  data = g_new0 (WalkThreadData, 1);
  data->tree = g_object_ref (tree);
  data->path = g_strdup (tree->priv->path);

  return data;
}

static void
walk_thread_data_free (WalkThreadData *data)
{
  g_list_foreach (data->files_list, (GFunc) g_key_file_free, NULL);
  g_list_free (data->files_list);

  g_object_unref (data->tree);
  g_free (data->path);

  g_free (data);
}

static gint
walk_thread_compare_items (HdLauncherItem *a, HdLauncherItem *b)
{
  guint apos = hd_launcher_item_get_position (a);
  guint bpos = hd_launcher_item_get_position (b);

  return apos - bpos;
}

static gboolean
walk_thread_done_idle (gpointer user_data)
{
  WalkThreadData *data = user_data;

  /* FIXME: This is a nasty hack. */
  data->tree->priv->items_list = g_list_sort (data->tree->priv->items_list,
      (GCompareFunc) walk_thread_compare_items);

  if (!data->cancelled)
    g_signal_emit (data->tree, tree_signals[FINISHED], 0);

  data->tree->priv->active_walk = NULL;
  walk_thread_data_free (data);

  return FALSE;
}

static gboolean
walk_thread_add_items_idle (gpointer user_data)
{
  WalkItems *items = user_data;

  if (!items->thread_data->cancelled)
    {
      for (GList *l = items->items; l != NULL; l = l->next)
        {
          WalkItem *witem = l->data;
          HdLauncherItem *item;

          item = hd_launcher_item_new_from_keyfile (witem->id,
                                                    witem->key_file,
                                                    NULL);
          if (item)
            {
              g_signal_emit (items->thread_data->tree,
                             tree_signals[ITEM_ADDED], 0,
                             item);

              /* the class signal handler will keep a reference
               * on the object for us; see hd_launcher_tree_real_item_added().
               */
              g_object_unref (item);
            }
        }
    }

   g_list_foreach (items->items, (GFunc) walk_item_free, NULL);
   g_list_free (items->items);
   g_free (items);

  return FALSE;
}

static void
send_chunk (WalkThreadData *data)
{
  WalkItems *items;

  data->n_processed_files = 0;

  if (data->files_list)
    {
      items = g_new (WalkItems, 1);
      items->items = g_list_reverse (data->files_list);
      items->n_items = g_list_length (data->files_list);
      items->current_pos = 0;
      items->thread_data = data;

      clutter_threads_add_idle_full (CLUTTER_PRIORITY_REDRAW + 30,
                                     walk_thread_add_items_idle,
                                     items,
                                     NULL);
    }

  data->files_list = NULL;
}

static GStaticPrivate walk_thread_data = G_STATIC_PRIVATE_INIT;

static int
walk_visit_func (const char        *f_path,
                 const struct stat *sb,
                 int                type_flag,
                 struct FTW        *ftw_buf)
{
  WalkThreadData *data;
  const gchar *name;
  GError *error = NULL;
  gchar *full_path = NULL;
  gboolean is_hidden;
  GKeyFile *key_file = NULL;

  data = (WalkThreadData *) g_static_private_get (&walk_thread_data);

  if (data->cancelled)
#ifdef HAVE_GNU_FTW
    return FTW_STOP;
#else
    return 1;
#endif /* HAVE_GNU_FTW */

  name = strrchr (f_path, '/');
  if (name)
    name++;
  else
    name = f_path;

  is_hidden = (*name == '.') ? TRUE : FALSE;
  if (is_hidden)
#ifdef HAVE_GNU_FTW
    return FTW_SKIP_SUBTREE;
#else
  return 0;
#endif /* HAVE_GNU_FTW */

  if (!S_ISREG (sb->st_mode))
#ifdef HAVE_GNU_FTW
    return FTW_CONTINUE;
#else
  return 0;
#endif /* HAVE_GNU_FTW */

  if (g_str_has_suffix (name, ".desktop"))
    full_path = g_build_filename (data->tree->priv->path, name, NULL);

  if (full_path)
    {
      key_file = g_key_file_new ();
      g_key_file_load_from_file (key_file, full_path, 0, &error);
      if (error)
        {
          g_warning ("Unable to parse `%s' in %s: %s",
                     name,
                     data->tree->priv->path,
                     error->message);

          g_error_free (error);
          g_key_file_free (key_file);
          key_file = NULL;
        }

      g_free (full_path);
    }

  if (key_file) {
    WalkItem *item = g_new0 (WalkItem, 1);
    item->key_file = key_file;
    item->id = g_strndup (name, strlen (name) - strlen (".desktop"));
    data->files_list = g_list_prepend (data->files_list, item);
  }

  data->n_processed_files++;

  if (data->n_processed_files > CHUNK_SIZE)
    send_chunk (data);

#ifdef HAVE_GNU_FTW
  if (is_hidden)
    return FTW_SKIP_SUBTREE;
  else
    return FTW_CONTINUE;
#else
  return 0;
#endif /* HAVE_GNU_FTW */
}

static gpointer
walk_thread_func (gpointer user_data)
{
  WalkThreadData *data = user_data;

  g_static_private_set (&walk_thread_data, data, NULL);

  nftw (data->path, walk_visit_func, 20,
#ifdef HAVE_GNU_FTW
        FTW_ACTIONRETVAL |
#endif
        FTW_PHYS);

  send_chunk (data);

  clutter_threads_add_idle (walk_thread_done_idle, data);

  return NULL;
}

static void
hd_launcher_tree_finalize (GObject *gobject)
{
  HdLauncherTreePrivate *priv = HD_LAUNCHER_TREE_GET_PRIVATE (gobject);

  g_free (priv->menu_path);
  g_free (priv->path);

  if (priv->active_walk)
    {
      priv->active_walk->cancelled = TRUE;
      priv->active_walk = NULL;
    }

  g_list_foreach (priv->items_list, (GFunc) g_object_unref, NULL);
  g_list_free (priv->items_list);
  priv->items_list = NULL;

  G_OBJECT_CLASS (hd_launcher_tree_parent_class)->finalize (gobject);
}

static void
hd_launcher_tree_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  HdLauncherTreePrivate *priv = HD_LAUNCHER_TREE_GET_PRIVATE (gobject);

  switch (prop_id)
    {
    case PROP_MENU_PATH:
      g_free (priv->menu_path);
      priv->menu_path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
hd_launcher_tree_get_property (GObject    *gobject,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  HdLauncherTreePrivate *priv = HD_LAUNCHER_TREE_GET_PRIVATE (gobject);

  switch (prop_id)
    {
    case PROP_MENU_PATH:
      g_value_set_string (value, priv->menu_path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static GObject *
hd_launcher_tree_constructor (GType                  gtype,
                              guint                  n_params,
                              GObjectConstructParam *params)
{
  GObjectClass *parent_class;
  HdLauncherTree *tree;
  GObject *retval;

  parent_class = G_OBJECT_CLASS (hd_launcher_tree_parent_class);
  retval = parent_class->constructor (gtype, n_params, params);

  tree = HD_LAUNCHER_TREE (retval);

  /* KIMMO hack
  tree->priv->path = g_build_filename (DATADIR,
                                       HILDON_DESKTOP_APPLICATIONS_DIR,
                                       NULL);
                                       */
  tree->priv->path = g_strdup("/usr/share/applications/hildon");

  if (!tree->priv->menu_path)
          /* KIMMO hack
    tree->priv->menu_path = g_build_filename (SYSCONFDIR,
                                              HILDON_DESKTOP_MENU_DIR,
                                              HILDON_DESKTOP_APPLICATIONS_MENU,
                                              NULL);
                                              */
    tree->priv->menu_path = g_strdup("/etc/xdg/menus/applications.menu");

  return retval;
}

static void
hd_launcher_tree_real_item_added (HdLauncherTree *tree,
                                  HdLauncherItem *item)
{
  HdLauncherTreePrivate *priv = tree->priv;

  if (!item)
    return;

  /* keep a reference inside the items list, since the functions
   * that emit the ::item-added signal are all going to remove a
   * reference
   */
  priv->items_list = g_list_prepend (priv->items_list,
                                     g_object_ref (item));
}

static void
hd_launcher_tree_real_finished (HdLauncherTree *tree)
{

}

static void
hd_launcher_tree_class_init (HdLauncherTreeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdLauncherTreePrivate));

  klass->item_added = hd_launcher_tree_real_item_added;
  klass->finished = hd_launcher_tree_real_finished;

  gobject_class->constructor = hd_launcher_tree_constructor;
  gobject_class->set_property = hd_launcher_tree_set_property;
  gobject_class->get_property = hd_launcher_tree_get_property;
  gobject_class->finalize = hd_launcher_tree_finalize;

  g_object_class_install_property (gobject_class,
                                   PROP_MENU_PATH,
                                   g_param_spec_string ("menu-path",
                                                        "Menu Path",
                                                        "Path of the applications.menu file",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  tree_signals[ITEM_ADDED] =
    g_signal_new ("item-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (HdLauncherTreeClass, item_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  HD_TYPE_LAUNCHER_ITEM);
  tree_signals[ITEM_CHANGED] =
    g_signal_new ("item-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (HdLauncherTreeClass, item_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  HD_TYPE_LAUNCHER_ITEM);
  tree_signals[ITEM_REMOVED] =
    g_signal_new ("item-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (HdLauncherTreeClass, item_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  HD_TYPE_LAUNCHER_ITEM);
  tree_signals[FINISHED] =
    g_signal_new ("finished",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (HdLauncherTreeClass, finished),
                  NULL, NULL,
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
hd_launcher_tree_new (const gchar *path)
{
  return g_object_new (HD_TYPE_LAUNCHER_TREE,
                       "menu-path", path,
                       NULL);
}

/**
 * hd_launcher_tree_populate:
 * @tree: a #HdLauncherTree
 *
 * Populates the @tree with the launchers by walking
 * the applications directory using an helper thread
 * to avoid blocking.
 *
 * Emits the #HdLauncherTree::item-added for each launcher
 * read, parsed and added; emits the #HdLauncherTree::finished
 * when done.
 */
void
hd_launcher_tree_populate (HdLauncherTree *tree)
{
  WalkThreadData *data;

  g_return_if_fail (HD_IS_LAUNCHER_TREE (tree));

  if (tree->priv->active_walk)
    return;

  data = walk_thread_data_new (tree);

  g_thread_create (walk_thread_func, data, FALSE, NULL);
  tree->priv->active_walk = data;
}

GList *
hd_launcher_tree_get_items (HdLauncherTree *tree,
                            HdLauncherItem *parent)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_TREE (tree), NULL);
  g_return_val_if_fail (parent == NULL || HD_IS_LAUNCHER_ITEM (parent), NULL);

  if (G_LIKELY (parent == NULL))
    return tree->priv->items_list;

  return NULL;
}

guint
hd_launcher_tree_get_n_items (HdLauncherTree *tree,
                              HdLauncherItem *parent)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_TREE (tree), 0);
  g_return_val_if_fail (parent == NULL || HD_IS_LAUNCHER_ITEM (parent), 0);

  if (G_LIKELY (parent == NULL))
    return tree->priv->n_top_levels;

  return 0;
}

guint
hd_launcher_tree_get_size (HdLauncherTree *tree)
{
  g_return_val_if_fail (HD_IS_LAUNCHER_TREE (tree), 0);

  return tree->priv->size;
}

void
hd_launcher_tree_insert_item (HdLauncherTree *tree,
                              gint            position,
                              HdLauncherItem *parent,
                              HdLauncherItem *item)
{
}

void
hd_launcher_tree_remove_item (HdLauncherTree *tree,
                              HdLauncherItem *item)
{
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
