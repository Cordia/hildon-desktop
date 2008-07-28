#include <clutter/clutter.h>

#define I_(str) (g_intern_static_string ((str)))

#define HD_PARAM_READWRITE      (G_PARAM_READWRITE | \
                                 G_PARAM_STATIC_NICK | \
                                 G_PARAM_STATIC_NAME | \
                                 G_PARAM_STATIC_BLURB)
#define HD_PARAM_READABLE       (G_PARAM_READABLE | \
                                 G_PARAM_STATIC_NICK | \
                                 G_PARAM_STATIC_NAME | \
                                 G_PARAM_STATIC_BLURB)
#define HD_PARAM_WRITABLE       (G_PARAM_WRITABLE | \
                                 G_PARAM_STATIC_NICK | \
                                 G_PARAM_STATIC_NAME | \
                                 G_PARAM_STATIC_BLURB)

#define HD_TASK_LAUNCHER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_TASK_LAUNCHER, HdTaskLauncherClass))
#define HD_IS_TASK_LAUNCHER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_TASK_LAUNCHER))
#define HD_TASK_LAUNCHER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_TASK_LAUNCHER, HdTaskLauncherClass))

typedef struct _HdTaskLauncherClass     HdTaskLauncherClass;

typedef struct _HdTaskLauncherMargin    HdTaskLauncherMargin;
typedef struct _HdTaskLauncherSpacing   HdTaskLauncherSpacing;

struct _HdTaskLauncher
{
  ClutterActor parent_instance;

  /* margins of the group */
  HdTaskLauncherMargin margin;

  /* spacing between the items */
  HdTaskLauncherSpacing spacing;
};

struct _HdTaskLauncherClass
{
  ClutterActorClass parent_class;
};

struct _HdTaskLauncherMargin
{
  ClutterUnit top;
  ClutterUnit right;
  ClutterUnit bottom;
  ClutterUnit left;
};

struct _HdTaskLauncherSpacing
{
  ClutterUnit horizontal;
  ClutterUnit vertical;
};
