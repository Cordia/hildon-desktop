
#include "hd-dbus.h"

#include <glib.h>
/*
 * Application killer DBus interface
 */
#define APPKILLER_SIGNAL_INTERFACE "com.nokia.osso_app_killer"
#define APPKILLER_SIGNAL_PATH      "/com/nokia/osso_app_killer"
#define APPKILLER_SIGNAL_NAME      "exit"

#define LOWMEM_OFF_SIGNAL_PATH      "/com/nokia/ke_recv/user_lowmem_off"
#define LOWMEM_OFF_SIGNAL_INTERFACE "com.nokia.ke_recv.user_lowmem_off"
#define LOWMEM_OFF_SIGNAL_NAME      "user_lowmem_off"
#define LOWMEM_ON_SIGNAL_PATH       "/com/nokia/ke_recv/user_lowmem_on"
#define LOWMEM_ON_SIGNAL_INTERFACE  "com.nokia.ke_recv.user_lowmem_on"
#define LOWMEM_ON_SIGNAL_NAME       "user_lowmem_on"

static DBusHandlerResult
hd_dbus_signal_handler (DBusConnection *conn, DBusMessage *msg, void *data)
{
  HdCompMgr  * hmgr = data;

  if (dbus_message_is_signal(msg,
			     APPKILLER_SIGNAL_INTERFACE,
			     APPKILLER_SIGNAL_NAME))
    { /* kill -TERM all programs started from the launcher unconditionally */
      hd_comp_mgr_hibernate_all (hmgr, TRUE);

      return DBUS_HANDLER_RESULT_HANDLED;
    }
  else if (dbus_message_is_signal(msg,
				  LOWMEM_ON_SIGNAL_INTERFACE,
				  LOWMEM_ON_SIGNAL_NAME))
    {
      hd_comp_mgr_set_low_memory_state (hmgr, TRUE);
    }
  else if (dbus_message_is_signal(msg,
				  LOWMEM_OFF_SIGNAL_INTERFACE,
				  LOWMEM_OFF_SIGNAL_NAME))
    {
      hd_comp_mgr_set_low_memory_state (hmgr, FALSE);
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
				   LOWMEM_ON_SIGNAL_INTERFACE);
      dbus_bus_add_match (connection, match_rule, NULL);
      g_free (match_rule);

      match_rule = g_strdup_printf("type='signal', interface='%s'",
				   LOWMEM_OFF_SIGNAL_INTERFACE);
      dbus_bus_add_match (connection, match_rule, NULL);
      g_free (match_rule);

      dbus_connection_add_filter (connection, hd_dbus_signal_handler,
				  hmgr, NULL);

      dbus_connection_flush (connection);
    }

  return connection;
}

gboolean
hd_dbus_launch_service (DBusConnection *connection,
			const gchar    *service,
			const gchar    *path,
			const gchar    *interface,
			const gchar    *method,
			const gchar    *launch_param)
{
  DBusMessage *msg;
  dbus_bool_t  retval;

  msg = dbus_message_new_method_call (service, path, interface, method);

  if (!msg)
    {
      g_warning ("dbus_message_new_method_call for service %s failed",
		 service);

      return FALSE;
    }

  dbus_message_set_auto_start (msg, TRUE);
  dbus_message_set_no_reply (msg, TRUE);

  if (launch_param)
    dbus_message_append_args (msg, DBUS_TYPE_STRING, launch_param,
			      DBUS_TYPE_INVALID);

  retval = dbus_connection_send (connection, msg, NULL);

  dbus_message_unref(msg);

  if (!retval)
    {
      g_warning ("dbus_connection_send failed");
      return FALSE;
    }

  return TRUE;
}
