#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

#include "hd-launcher-tree.h"
#include "hd-app-launcher.h"

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ftw.h>

#include <clutter/clutter.h>

#define CHUNK_SIZE      100

#define HD_LAUNCHER_TREE_GET_PRIVATE(obj)       (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HD_TYPE_LAUNCHER_TREE, HdLauncherTreePrivate))

typedef struct
{
  HdLauncherTree *tree;

  gchar *path;
  GList *items_list;

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
  gchar *path;

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

  PROP_PATHS
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
  g_object_unref (data->tree);
  g_free (data->path);
  g_free (data);
}

static gboolean
walk_thread_done_idle (gpointer user_data)
{
  WalkThreadData *data = user_data;

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

  /* keep emitting the ::item-added signal inside the idle
   * handler until we hit the bottom
   */

  if (!items->thread_data->cancelled)
    {
      GList *l;

      for (l = items->items; l != NULL; l = l->next)
        {
          GKeyFile *key_file = l->data;
          HdLauncherItem *item;

          item = hd_app_launcher_new_from_keyfile (key_file, NULL);
          if (item)
            {
              if (hd_app_launcher_get_item_type (HD_APP_LAUNCHER (item)) &&
                  hd_app_launcher_get_name (HD_APP_LAUNCHER (item)))
                {
                  g_signal_emit (items->thread_data->tree,
                                 tree_signals[ITEM_ADDED], 0,
                                 item);
                }

              g_object_unref (item);
            }
        }
    }

   g_list_foreach (items->items, (GFunc) g_key_file_free, NULL);
   g_list_free (items->items);
   g_free (items);

  return FALSE;
}

static void
send_chunk (WalkThreadData *data)
{
  WalkItems *items;

  data->n_processed_files = 0;

  if (data->items_list)
    {
      items = g_new (WalkItems, 1);
      items->items = data->items_list;
      items->n_items = g_list_length (data->items_list);
      items->current_pos = 0;
      items->thread_data = data;

      clutter_threads_add_idle_full (CLUTTER_PRIORITY_REDRAW + 30,
                                     walk_thread_add_items_idle,
                                     items,
                                     NULL);
    }

  data->items_list = NULL;
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

  if (S_ISREG (sb->st_mode) &&
      g_str_has_suffix (name, ".desktop") &&
      !is_hidden)
    {
      GError *error = NULL;
      gchar *full_path;
      
      full_path = g_build_filename (data->tree->priv->path, name, NULL);

      key_file = g_key_file_new ();
      g_key_file_load_from_file (key_file, full_path, 0, &error);
      if (error)
        {
          g_error_free (error);
          g_key_file_free (key_file);
          key_file = NULL;
        }
    }

  if (key_file)
    data->items_list = g_list_prepend (data->items_list, key_file);

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
    case PROP_PATHS:
      g_free (priv->path);
      priv->path = g_value_dup_string (value);
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
    case PROP_PATHS:
      g_value_set_string (value, priv->path);
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

#define HILDON_DESKTOP_MENU_DIR                 "/etc/xdg/menus"
#define HILDON_DESKTOP_APPLICATIONS_MENU        "applications.menu"

#define HILDON_DESKTOP_APPLICATIONS_DIR         "/usr/share/applications"

  if (!tree->priv->path)
    tree->priv->path = g_build_filename (HILDON_DESKTOP_APPLICATIONS_DIR,
                                         NULL);

  return retval;
}

static void
hd_launcher_tree_real_item_added (HdLauncherTree *tree,
                                  HdLauncherItem *item)
{
  if (!item)
    return;

  /* keep a reference inside the items list */
  tree->priv->items_list = g_list_prepend (tree->priv->items_list,
                                           g_object_ref (item));

  g_debug ("Added `%s' (type: %s)",
           hd_app_launcher_get_name (HD_APP_LAUNCHER (item)),
           hd_app_launcher_get_item_type (HD_APP_LAUNCHER (item)));
}

static void
hd_launcher_tree_class_init (HdLauncherTreeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdLauncherTreePrivate));

  klass->item_added = hd_launcher_tree_real_item_added;

  gobject_class->constructor = hd_launcher_tree_constructor;
  gobject_class->set_property = hd_launcher_tree_set_property;
  gobject_class->get_property = hd_launcher_tree_get_property;
  gobject_class->finalize = hd_launcher_tree_finalize;

  g_object_class_install_property (gobject_class,
                                   PROP_PATHS,
                                   g_param_spec_string ("path",
                                                        "Path",
                                                        "Search path for desktop files",
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
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (HdLauncherTreeClass, item_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  HD_TYPE_LAUNCHER_ITEM);
  tree_signals[ITEM_REMOVED] =
    g_signal_new ("item-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (HdLauncherTreeClass, item_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  HD_TYPE_LAUNCHER_ITEM);
  tree_signals[FINISHED] =
    g_signal_new ("finished",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
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
                       "path", path,
                       NULL);
}

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

