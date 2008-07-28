#ifndef __HD_LAUNCHER_UTILS_H__
#define __HD_LAUNCHER_UTILS_H__

#include <glib.h>

G_BEGIN_DECLS

GNode *hd_build_launcher_items (void);
GList *hd_get_top_level_items  (GNode *root);

G_END_DECLS

#endif /* __HD_LAUNCHER_UTILS_H__ */
