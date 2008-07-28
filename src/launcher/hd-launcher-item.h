#ifndef __HD_LAUNCHER_ITEM_H__
#define __HD_LAUNCHER_ITEM_H__

#include <clutter/clutter-actor.h>

G_BEGIN_DECLS

#define HD_TYPE_LAUNCHER_ITEM_TYPE       (hd_launcher_item_type_get_type ())
#define HD_TYPE_LAUNCHER_ITEM            (hd_launcher_item_get_type ())
#define HD_LAUNCHER_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_LAUNCHER_ITEM, HdLauncherItem))
#define HD_IS_LAUNCHER_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_LAUNCHER_ITEM))
#define HD_LAUNCHER_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_LAUNCHER_ITEM, HdLauncherItemClass))
#define HD_IS_LAUNCHER_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_LAUNCHER_ITEM))
#define HD_LAUNCHER_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_LAUNCHER_ITEM, HdLauncherItemClass))

typedef enum {
  HD_APPLICATION_LAUNCHER,
  HD_SECTION_LAUNCHER
} HdLauncherItemType;

typedef struct _HdLauncherItem          HdLauncherItem;
typedef struct _HdLauncherItemPrivate   HdLauncherItemPrivate;
typedef struct _HdLauncherItemClass     HdLauncherItemClass;

struct _HdLauncherItem
{
  ClutterActor parent_instance;

  HdLauncherItemPrivate *priv;
};

struct _HdLauncherItemClass
{
  ClutterActorClass parent_class;

  /* vfuncs, not signals */
  ClutterActor *(* get_icon)  (HdLauncherItem *item);
  ClutterActor *(* get_label) (HdLauncherItem *item);

  void          (* pressed)   (HdLauncherItem *item);
  void          (* released)  (HdLauncherItem *item);

  /* signals */
  void (* clicked) (HdLauncherItem *item);
};

GType              hd_launcher_item_type_get_type (void) G_GNUC_CONST;
GType              hd_launcher_item_get_type      (void) G_GNUC_CONST;

HdLauncherItemType hd_launcher_item_get_item_type (HdLauncherItem *item);

ClutterActor *     hd_launcher_item_get_icon      (HdLauncherItem *item);
ClutterActor *     hd_launcher_item_get_label     (HdLauncherItem *item);

const gchar *      hd_launcher_item_get_text      (HdLauncherItem *item);

G_END_DECLS

#endif /* __HD_LAUNCHER_ITEM_H__ */
