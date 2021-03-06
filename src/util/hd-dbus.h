
#ifndef __HD_DBUS__
#define __HD_DBUS__

#include "hd-comp-mgr.h"

#include <dbus/dbus.h>

DBusConnection * hd_dbus_init (HdCompMgr *hmgr);

void hd_dbus_disable_display_blanking (gboolean setting);

gboolean hd_dbus_launch_service (DBusConnection *connection,
				 const gchar    *service,
				 const gchar    *path,
				 const gchar    *interface,
				 const gchar    *method,
				 const gchar    *launch_param);
void hd_dbus_send_event (char *value);
void hd_dbus_send_desktop_orientation_changed (gboolean to_portrait);
#endif
