#ifndef __HD_APP_LAUNCHER_H__
#define __HD_APP_LAUNCHER_H__

#include "hd-launcher-item.h"

G_BEGIN_DECLS

#define HD_TYPE_APP_LAUNCHER            (hd_app_launcher_get_type ())
#define HD_APP_LAUNCHER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_APP_LAUNCHER, HdAppLauncher))
#define HD_IS_APP_LAUNCHER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_APP_LAUNCHER))
#define HD_APP_LAUNCHER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_APP_LAUNCHER, HdAppLauncherClass))
#define HD_IS_APP_LAUNCHER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_APP_LAUNCHER))
#define HD_APP_LAUNCHER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_APP_LAUNCHER, HdAppLauncherClass))

typedef struct _HdAppLauncher           HdAppLauncher;
typedef struct _HdAppLauncherPrivate    HdAppLauncherPrivate;
typedef struct _HdAppLauncherClass      HdAppLauncherClass;

struct _HdAppLauncher
{
  HdLauncherItem parent_instance;

  HdAppLauncherPrivate *priv;
};

struct _HdAppLauncherClass
{
  HdLauncherItemClass parent_class;
};

GType           hd_app_launcher_get_type          (void) G_GNUC_CONST;

HdLauncherItem *hd_app_launcher_new_from_keyfile  (GKeyFile       *key_file,
                                                   GError        **error);
HdLauncherItem *hd_app_launcher_new_from_file     (const gchar    *filename,
                                                   GError        **error);

gboolean        hd_app_launcher_load_from_keyfile (HdAppLauncher  *launcher,
                                                   GKeyFile       *key_file,
                                                   GError        **error);

G_CONST_RETURN gchar *hd_app_launcher_get_item_type     (HdAppLauncher *item);
G_CONST_RETURN gchar *hd_app_launcher_get_icon_name     (HdAppLauncher *item);
G_CONST_RETURN gchar *hd_app_launcher_get_name          (HdAppLauncher *item);
G_CONST_RETURN gchar *hd_app_launcher_get_comment       (HdAppLauncher *item);
G_CONST_RETURN gchar *hd_app_launcher_get_exec          (HdAppLauncher *item);
G_CONST_RETURN gchar *hd_app_launcher_get_service       (HdAppLauncher *item);
G_CONST_RETURN gchar *hd_app_launcher_get_text_domain   (HdAppLauncher *item);
G_CONST_RETURN gchar *hd_app_launcher_get_preload_image (HdAppLauncher *item);
G_CONST_RETURN gchar *hd_app_launcher_get_preload_mode  (HdAppLauncher *item);

gsize                 hd_app_launcher_get_n_categories  (HdAppLauncher *item);
gboolean              hd_app_launcher_has_category      (HdAppLauncher *item,
                                                         const gchar   *category);

G_END_DECLS

#endif /* __HD_APP_LAUNCHER_H__ */
