#ifndef __HD_LAUNCHER_TREE_H__
#define __HD_LAUNCHER_TREE_H__

#include <glib-object.h>
#include "hd-launcher-item.h"

G_BEGIN_DECLS

#define HD_TYPE_LAUNCHER_TREE                   (hd_launcher_tree_get_type ())
#define HD_LAUNCHER_TREE(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_LAUNCHER_TREE, HdLauncherTree))
#define HD_IS_LAUNCHER_TREE(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_LAUNCHER_TREE))
#define HD_LAUNCHER_TREE_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_LAUNCHER_TREE, HdLauncherTreeClass))
#define HD_IS_LAUNCHER_TREE_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_LAUNCHER_TREE))
#define HD_LAUNCHER_TREE_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_LAUNCHER_TREE, HdLauncherTreeClass))

typedef struct _HdLauncherTree          HdLauncherTree;
typedef struct _HdLauncherTreePrivate   HdLauncherTreePrivate;
typedef struct _HdLauncherTreeClass     HdLauncherTreeClass;

struct _HdLauncherTree
{
  GObject parent_instance;

  HdLauncherTreePrivate *priv;
};

struct _HdLauncherTreeClass
{
  GObjectClass parent_class;

  void (* item_added)    (HdLauncherTree *tree,
                          HdLauncherItem *item);
  void (* item_removed)  (HdLauncherTree *tree,
                          HdLauncherItem *item);
  void (* item_changed)  (HdLauncherTree *tree,
                          HdLauncherItem *item);
  void (* finished)      (HdLauncherTree *tree);
};

GType           hd_launcher_tree_get_type (void) G_GNUC_CONST;

HdLauncherTree *hd_launcher_tree_new         (const gchar    *path);

void            hd_launcher_tree_populate    (HdLauncherTree *tree);

GList *         hd_launcher_tree_get_items   (HdLauncherTree *tree,
                                              HdLauncherItem *parent);
guint           hd_launcher_tree_get_n_items (HdLauncherTree *tree,
                                              HdLauncherItem *parent);
guint           hd_launcher_tree_get_size    (HdLauncherTree *tree);

void            hd_launcher_tree_insert_item (HdLauncherTree *tree,
                                              gint            position,
                                              HdLauncherItem *parent,
                                              HdLauncherItem *item);
void            hd_launcher_tree_remove_item (HdLauncherTree *tree,
                                              HdLauncherItem *item);

G_END_DECLS

#endif /* __HD_LAUNCHER_TREE_H__ */
