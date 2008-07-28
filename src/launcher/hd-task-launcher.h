#ifndef __HD_TASK_LAUNCHER_H__
#define __HD_TASK_LAUNCHER_H__

#include <clutter/clutter-actor.h>
#include "hd-launcher-item.h"

G_BEGIN_DECLS

#define HD_TYPE_TASK_LAUNCHER                   (hd_task_launcher_get_type ())
#define HD_TASK_LAUNCHER(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_TASK_LAUNCHER, HdTaskLauncher))
#define HD_IS_TASK_LAUNCHER(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_TASK_LAUNCHER))
#define HD_TASK_LAUNCHER_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_TASK_LAUNCHER, HdTaskLauncherClass))
#define HD_IS_TASK_LAUNCHER_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_TASK_LAUNCHER))
#define HD_TASK_LAUNCHER_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_TASK_LAUNCHER, HdTaskLauncherClass))

typedef struct _HdLauncherPadding       HdLauncherPadding;
typedef struct _HdTaskLauncher          HdTaskLauncher;
typedef struct _HdTaskLauncherPrivate   HdTaskLauncherPrivate;
typedef struct _HdTaskLauncherClass     HdTaskLauncherClass;

struct _HdTaskLauncher
{
  ClutterActor parent_instance;

  HdTaskLauncherPrivate *priv;
};

struct _HdTaskLauncherClass
{
  ClutterActorClass parent_class;

  void (* item_clicked) (HdTaskLauncher *launcher,
                         HdLauncherItem *item);
};

struct _HdLauncherPadding
{
  ClutterUnit top;
  ClutterUnit right;
  ClutterUnit bottom;
  ClutterUnit left;
};

GType         hd_task_launcher_get_type (void) G_GNUC_CONST;
ClutterActor *hd_task_launcher_new      (void);

void          hd_task_launcher_add_item (HdTaskLauncher *launcher,
                                         HdLauncherItem *item);
void          hd_task_launcher_clear    (HdTaskLauncher *launcher);

G_END_DECLS

#endif /* __HD_TASK_LAUNCHER_H__ */
