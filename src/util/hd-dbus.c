
#include "hd-dbus.h"
#include "hd-switcher.h"
#include "hd-render-manager.h"
#include "hd-volume-profile.h"

#include <glib.h>
/*
 * Application killer DBus interface
 */
#define APPKILLER_SIGNAL_INTERFACE "com.nokia.osso_app_killer"
#define APPKILLER_SIGNAL_PATH      "/com/nokia/osso_app_killer"
#define APPKILLER_SIGNAL_NAME      "exit"

#define TASKNAV_SIGNAL_INTERFACE   "com.nokia.hildon_desktop"
#define TASKNAV_SIGNAL_NAME        "exit_app_view"

#define DSME_SIGNAL_INTERFACE "com.nokia.dsme.signal"
#define DSME_SHUTDOWN_SIGNAL_NAME "shutdown_ind"

static DBusHandlerResult
hd_dbus_signal_handler (DBusConnection *conn, DBusMessage *msg, void *data)
{
  HdCompMgr  * hmgr = data;

  if (dbus_message_is_signal(msg,
			     APPKILLER_SIGNAL_INTERFACE,
			     APPKILLER_SIGNAL_NAME))
    {
      /* kill -TERM all programs started from the launcher unconditionally,
       * this signal is used by Backup application */
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

static DBusHandlerResult
hd_dbus_system_bus_signal_handler (DBusConnection *conn,
                                   DBusMessage *msg, void *data)
{
  HdCompMgr  * hmgr = data;
  if (dbus_message_is_signal(msg, DSME_SIGNAL_INTERFACE,
			     DSME_SHUTDOWN_SIGNAL_NAME))
    {
      g_warning ("%s: " DSME_SHUTDOWN_SIGNAL_NAME " from DSME", __func__);
      /* send TERM to applications and exit */
      hd_volume_profile_set_silent (TRUE);
      hd_comp_mgr_kill_all_apps (hmgr);
      exit (0);
    }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusConnection *
hd_dbus_init (HdCompMgr * hmgr)
{
  DBusConnection *connection, *sysbus_conn;
  DBusError       error;

  dbus_error_init (&error);

  connection = dbus_bus_get (DBUS_BUS_SESSION, &error);
  if (dbus_error_is_set (&error))
    {
      g_warning ("Could not open D-Bus session bus connection.");
      dbus_error_free (&error);
    }
  sysbus_conn = dbus_bus_get (DBUS_BUS_SYSTEM, &error);

  if (!connection || !sysbus_conn)
    {
      g_warning ("Could not open D-Bus session/system bus connection.");
      dbus_error_free (&error);
    }
  else
    {
      /* session bus */
      dbus_bus_add_match (connection, "type='signal', interface='"
                          APPKILLER_SIGNAL_INTERFACE "'", NULL);

      dbus_bus_add_match (connection, "type='signal', interface='"
                          TASKNAV_SIGNAL_INTERFACE "'", NULL);

      dbus_connection_add_filter (connection, hd_dbus_signal_handler,
				  hmgr, NULL);

      /* system bus */
      dbus_bus_add_match (sysbus_conn, "type='signal', interface='"
                          DSME_SIGNAL_INTERFACE "'", NULL);

      dbus_connection_add_filter (sysbus_conn,
                                  hd_dbus_system_bus_signal_handler,
                                  hmgr, NULL);
    }

  return connection;
}
