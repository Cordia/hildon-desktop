
#include "hd-dbus.h"

#include <dbus/dbus.h>

#include <glib.h>
/*
 * Application killer DBus interface
 */
#define APPKILLER_SIGNAL_INTERFACE "com.nokia.osso_app_killer"
#define APPKILLER_SIGNAL_PATH      "/com/nokia/osso_app_killer"
#define APPKILLER_SIGNAL_NAME      "exit"


static DBusHandlerResult
hd_dbus_signal_handler (DBusConnection *conn, DBusMessage *msg, void *data)
{
  HdCompMgr  * hmgr = data;
  
  if (dbus_message_is_signal(msg, APPKILLER_SIGNAL_INTERFACE,
				  APPKILLER_SIGNAL_NAME))
  {
    hd_comp_mgr_hibernate_all (hmgr);
    
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void
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
      dbus_connection_add_filter (connection, hd_dbus_signal_handler,
				  hmgr, NULL);
      g_free(match_rule);

      dbus_connection_flush (connection);
    }
}
