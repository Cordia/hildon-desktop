#ifndef __HD_LAUNCHER_UTILS_H__
#define __HD_LAUNCHER_UTILS_H__

#include <glib.h>
#include <clutter/clutter-actor.h>
#include "../home/hd-switcher.h"

G_BEGIN_DECLS

typedef void (*HdSwitcherCb)(HdSwitcher *switcher);

ClutterActor *hd_get_application_launcher (HdSwitcher *s, HdSwitcherCb s_cb);
void hd_launcher_group_set_back_button_cb (ClutterActor *group, GCallback cb,
                                           gpointer data);

G_END_DECLS

#endif /* __HD_LAUNCHER_UTILS_H__ */
