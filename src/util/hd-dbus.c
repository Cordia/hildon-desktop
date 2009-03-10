
#include "hd-dbus.h"
#include "hd-switcher.h"
#include "hd-render-manager.h"

#include <glib.h>
/*
 * Application killer DBus interface
 */
#define APPKILLER_SIGNAL_INTERFACE "com.nokia.osso_app_killer"
#define APPKILLER_SIGNAL_PATH      "/com/nokia/osso_app_killer"
#define APPKILLER_SIGNAL_NAME      "exit"

#define TASKNAV_SIGNAL_INTERFACE   "com.nokia.hildon_desktop"
#define TASKNAV_SIGNAL_NAME        "exit_app_view"

static DBusHandlerResult
hd_dbus_signal_handler (DBusConnection *conn, DBusMessage *msg, void *data)
{
  HdCompMgr  * hmgr = data;

  if (dbus_message_is_signal(msg,
			     APPKILLER_SIGNAL_INTERFACE,
			     APPKILLER_SIGNAL_NAME))
    { /* kill -TERM all programs started from the launcher unconditionally */
      hd_comp_mgr_kill_all_apps (hmgr);

      return DBUS_HANDLER_RESULT_HANDLED;
    }
  else if (dbus_message_is_signal(msg,
                                  TASKNAV_SIGNAL_INTERFACE,
                                  TASKNAV_SIGNAL_NAME))
    {
      if (STATE_IS_APP (hd_render_manager_get_state ()))
        hd_render_manager_set_state (HDRM_STATE_TASK_NAV);
      return DBUS_HANDLER_RESULT_HANDLED;
    }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusConnection *
hd_dbus_init (HdCompMgr * hmgr)
{
  DBusConnection *connection;
  DBusError       error;
  gchar          *match_rule = NULL;

  dbus_error_init (&error);

  connection = dbus_bus_get (DBUS_BUS_SESSION, &error);

  if (!connection)
    {
      g_warning ("Could not open Dbus connection.");
      dbus_error_free( &error );
    }
  else
    {
      /* Match rule */
      match_rule = g_strdup_printf("type='signal', interface='%s'",
				   APPKILLER_SIGNAL_INTERFACE);
      dbus_bus_add_match (connection, match_rule, NULL);
      g_free (match_rule);

      match_rule = g_strdup_printf("type='signal', interface='%s'",
				   TASKNAV_SIGNAL_INTERFACE);
      dbus_bus_add_match (connection, match_rule, NULL);
      g_free (match_rule);

      dbus_connection_add_filter (connection, hd_dbus_signal_handler,
				  hmgr, NULL);

      dbus_connection_flush (connection);
    }

  return connection;
}
