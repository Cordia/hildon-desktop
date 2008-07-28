#ifndef __HD_DUMMY_LAUNCHER_H__
#define __HD_DUMMY_LAUNCHER_H__

#include <clutter/clutter.h>
#include "hd-launcher-item.h"

G_BEGIN_DECLS

#define HD_TYPE_DUMMY_LAUNCHER            (hd_dummy_launcher_get_type ())
#define HD_DUMMY_LAUNCHER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_DUMMY_LAUNCHER, HdDummyLauncher))
#define HD_IS_DUMMY_LAUNCHER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_DUMMY_LAUNCHER))
#define HD_DUMMY_LAUNCHER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_DUMMY_LAUNCHER, HdDummyLauncherClass))
#define HD_IS_DUMMY_LAUNCHER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_DUMMY_LAUNCHER))
#define HD_DUMMY_LAUNCHER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_DUMMY_LAUNCHER, HdDummyLauncherClass))

typedef struct _HdDummyLauncher         HdDummyLauncher;
typedef struct _HdDummyLauncherClass    HdDummyLauncherClass;

struct _HdDummyLauncher
{
  HdLauncherItem parent_instance;

  ClutterEffectTemplate *tmpl;
};

struct _HdDummyLauncherClass
{
  HdLauncherItemClass parent_class;
};

GType           hd_dummy_launcher_get_type (void) G_GNUC_CONST;
HdLauncherItem *hd_dummy_launcher_new      (HdLauncherItemType ltype);

G_END_DECLS

#endif /* __HD_DUMMY_LAUNCHER_H__ */
