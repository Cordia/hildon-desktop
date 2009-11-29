#include <unistd.h>

#include "hd-dbus.h"
#include "hd-switcher.h"
#include "hd-title-bar.h"
#include "hd-render-manager.h"
#include "hd-task-navigator.h"

#include <glib.h>
#ifdef HAVE_DSME
+/* TODO Convert MCE to DSME */
#include <mce/dbus-names.h>
#include <mce/mode-names.h>
#endif
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

gboolean hd_dbus_display_is_off = FALSE;
gboolean hd_dbus_tklock_on = FALSE;
HDRMStateEnum hd_dbus_state_before_tklock = HDRM_STATE_UNDEFINED;
gboolean hd_dbus_cunt = FALSE;

static DBusConnection *connection, *sysbus_conn;

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
  else if (dbus_message_is_signal(msg,
                                  TASKNAV_SIGNAL_INTERFACE,
                                  "set_state"))
    {
      DBusMessageIter args;
      int sigvalue;
 
      if ((dbus_message_iter_init(msg, &args)) && 
          (DBUS_TYPE_INT32 == dbus_message_iter_get_arg_type(&args))) {
        dbus_message_iter_get_basic(&args, &sigvalue);
	switch(sigvalue) {
	  case HDRM_STATE_HOME :
          case HDRM_STATE_HOME_PORTRAIT:
          case HDRM_STATE_APP:
          case HDRM_STATE_APP_PORTRAIT:
          case HDRM_STATE_TASK_NAV:
	  case HDRM_STATE_LAUNCHER:
	  case HDRM_STATE_NON_COMPOSITED:
          case HDRM_STATE_NON_COMP_PORT:
            hd_render_manager_set_state (sigvalue);
            break;
        }
        return DBUS_HANDLER_RESULT_HANDLED;
      }
    }
  else if (dbus_message_is_signal(msg,
                                  TASKNAV_SIGNAL_INTERFACE,
                                  "activate_window"))
    {
      DBusMessageIter args;
      int sigvalue;
 
      if ((dbus_message_iter_init(msg, &args)) && 
          (DBUS_TYPE_INT32 == dbus_message_iter_get_arg_type(&args))) {
        dbus_message_iter_get_basic(&args, &sigvalue);
	hd_task_navigator_activate(sigvalue, -1, 0);
        return DBUS_HANDLER_RESULT_HANDLED;
      }
    }
  else if (dbus_message_is_signal(msg,
                                  TASKNAV_SIGNAL_INTERFACE,
                                  "close_window"))
    {
      DBusMessageIter args;
      int sigvalue;
 
      if ((dbus_message_iter_init(msg, &args)) && 
          (DBUS_TYPE_INT32 == dbus_message_iter_get_arg_type(&args))) {
        dbus_message_iter_get_basic(&args, &sigvalue);
	hd_task_navigator_activate(sigvalue, -1, 1);
        return DBUS_HANDLER_RESULT_HANDLED;
      }
    }
  else if (dbus_message_is_signal(msg,
                                  TASKNAV_SIGNAL_INTERFACE,
                                  "activate_window_time"))
    {
      DBusMessageIter args;
      int sigvalue;
 
      if ((dbus_message_iter_init(msg, &args)) && 
          (DBUS_TYPE_INT32 == dbus_message_iter_get_arg_type(&args))) {
        dbus_message_iter_get_basic(&args, &sigvalue);
	hd_task_navigator_activate(sigvalue, -2, 0);
        return DBUS_HANDLER_RESULT_HANDLED;
      }
    }
  else if (dbus_message_is_signal(msg,
                                  TASKNAV_SIGNAL_INTERFACE,
                                  "close_window_time"))
    {
      DBusMessageIter args;
      int sigvalue;
 
      if ((dbus_message_iter_init(msg, &args)) && 
          (DBUS_TYPE_INT32 == dbus_message_iter_get_arg_type(&args))) {
        dbus_message_iter_get_basic(&args, &sigvalue);
	hd_task_navigator_activate(sigvalue, -2, 1);
        return DBUS_HANDLER_RESULT_HANDLED;
      }
    }
  else if (dbus_message_is_signal(msg, TASKNAV_SIGNAL_INTERFACE, "launcher_activate"))
    {
	    DBusMessageIter args;
	    int sigvalue;

	    if ((dbus_message_iter_init(msg, &args)) && 
			    (DBUS_TYPE_INT32 == dbus_message_iter_get_arg_type(&args))) {
		    dbus_message_iter_get_basic(&args, &sigvalue);
		    hd_launcher_activate(sigvalue);
		    return DBUS_HANDLER_RESULT_HANDLED;
	    }
    }




  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult
hd_dbus_system_bus_signal_handler (DBusConnection *conn,
                                   DBusMessage *msg, void *data)
{
  HdCompMgr  * hmgr = data;
  extern MBWindowManager *hd_mb_wm;
  static gboolean call_active;

  if (dbus_message_is_signal(msg, DSME_SIGNAL_INTERFACE,
			     DSME_SHUTDOWN_SIGNAL_NAME))
    {
      Window overlay;
      g_warning ("%s: " DSME_SHUTDOWN_SIGNAL_NAME " from DSME", __func__);
      /* send TERM to applications and exit without cleanup */

      hd_comp_mgr_kill_all_apps (hmgr);
      overlay = mb_wm_comp_mgr_clutter_get_overlay_window (
                        MB_WM_COMP_MGR_CLUTTER (hmgr));
      if (overlay != None)
        {
          /* needed because of the non-composite optimisations in X,
           * otherwise we could show garbage if the shutdown screen is
           * a bit slow or missing */
          XClearWindow (hd_mb_wm->xdpy, overlay);
          XFlush (hd_mb_wm->xdpy);
        }
      _exit (0);
    }
#ifdef HAVE_DSME
/* TODO Convert MCE to DSME */
  else if (dbus_message_is_signal (msg, MCE_SIGNAL_IF, "tklock_mode_ind"))
    {
      const char *mode;

      if (dbus_message_get_args (msg, NULL, DBUS_TYPE_STRING, &mode,
                                 DBUS_TYPE_INVALID));
        {
          if (strcmp(mode, MCE_TK_LOCKED))
            {
              hd_dbus_cunt = FALSE;
              if (hd_dbus_tklock_on)
                {
                  hd_dbus_tklock_on = FALSE;
                  /* if we avoided focusing a window during tklock, do it now
                   * (this only has an effect if no window is currently
                   * focused) */
                  mb_wm_unfocus_client (hd_mb_wm, NULL);

                  if (hd_dbus_state_before_tklock != HDRM_STATE_UNDEFINED)
                    /* possibly go back to the state before tklock */
                    hd_render_manager_set_state (HDRM_STATE_AFTER_TKLOCK);
                  else
                    hd_app_mgr_mce_activate_accel_if_needed (FALSE);
                }
            }
          else if (!hd_dbus_tklock_on)
            {
              /*
               * The order of the events is either
               * call_state=ringing, tklock_ind=locked, call_state=active or
               * call_state=ringing, call_state=active, tklock_ind=locked.
               * Handle both cases.
               */
              hd_dbus_state_before_tklock = hd_render_manager_get_state ();
              hd_dbus_tklock_on = TRUE;
              hd_dbus_cunt = call_active
                && (hd_render_manager_get_state()
                    & (HDRM_STATE_HOME|HDRM_STATE_HOME_PORTRAIT));
              hd_app_mgr_mce_activate_accel_if_needed (FALSE);
            }
        }
    }
  else if (dbus_message_is_signal (msg, MCE_SIGNAL_IF, "display_status_ind"))
    {
      DBusMessageIter iter;

      if (dbus_message_iter_init(msg, &iter))
        {
          char *str = NULL;
          dbus_message_iter_get_basic(&iter, &str);
          if (str)
            {
              if (strcmp (str, "on") == 0)
                {
                  ClutterActor *stage = clutter_stage_get_default ();
                  /* Allow redraws again... */
                  clutter_actor_show(
                      CLUTTER_ACTOR(hd_render_manager_get()));
                  clutter_actor_set_allow_redraw(stage, TRUE);
                  /* make a blocking redraw to draw any new window (such as
                   * the "swipe to unlock") first, otherwise just a black
                   * screen will be visible (see below) */
                  hd_dbus_display_is_off = FALSE;
                  clutter_redraw (CLUTTER_STAGE (stage));
                  if (hd_task_navigator_has_notifications ())
                    { /* (Re)start pulsating if we have notifs. */
                      HdTitleBar *tb = HD_TITLE_BAR (hd_render_manager_get_title_bar ());
                      hd_title_bar_set_switcher_pulse (tb, FALSE);
                      hd_title_bar_set_switcher_pulse (tb, TRUE);
                    }
                  hd_app_mgr_check_show_callui ();
                }
              else if (strcmp (str, "off") == 0)
                {
                  ClutterActor *stage = clutter_stage_get_default ();
                  /* Stop redraws from anything. We do this on the stage
                   * because Rotation does it on HDRM, and we don't want to
                   * conflict. */
                  clutter_actor_hide(
                      CLUTTER_ACTOR(hd_render_manager_get()));
                  clutter_actor_set_allow_redraw(stage, FALSE);
                  hd_dbus_display_is_off = TRUE;
                  /* Hiding before set_allow_redraw will queue a redraw,
                   * which will draw a black screen (because hdrm is hidden).
                   * This is needed for bug 139928 so that there is
                   * absolutely no flicker of the previous screen
                   * contents before the lock window appears. */
                  clutter_redraw (CLUTTER_STAGE (stage));
                }

              hd_comp_mgr_update_applets_on_current_desktop_property (hmgr);
            }
        }
    }
  else if (dbus_message_is_signal (msg, MCE_SIGNAL_IF, "sig_call_state_ind"))
    {
      const char *state;

      /* Watch the call state.  If we got an active call tell hdrm to
       * try keeping the call-ui in the foreground after tklock is closed. */
      dbus_message_get_args (msg, NULL, DBUS_TYPE_STRING, &state,
                             DBUS_TYPE_INVALID);
      call_active = !strcmp(state, "active");
      hd_dbus_cunt = call_active && hd_dbus_tklock_on
        && (hd_render_manager_get_state()
            & (HDRM_STATE_HOME|HDRM_STATE_HOME_PORTRAIT));
    }

#endif
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

#ifdef HAVE_DSME
/* TODO Convert MCE to DSME */
static void
hd_dbus_prevent_display_blanking (void)
{
  DBusMessage *msg;
  dbus_bool_t b;

  if (sysbus_conn == NULL) {
    g_warning ("%s: no D-Bus system bus connection", __func__);
    return;
  }
  msg = dbus_message_new_method_call(MCE_SERVICE, MCE_REQUEST_PATH,
                                     MCE_REQUEST_IF, MCE_PREVENT_BLANK_REQ);
  if (msg == NULL) {
    g_warning ("%s: could not create message", __func__);
    return;
  }

  b = dbus_connection_send(sysbus_conn, msg, NULL);
  if (!b) {
    g_warning ("%s: dbus_connection_send() failed", __func__);
  } else {
    dbus_connection_flush(sysbus_conn);
  }
  dbus_message_unref(msg);
}

static gboolean
display_timeout_f (gpointer unused)
{
  /* g_warning ("%s: called", __func__); */
  hd_dbus_prevent_display_blanking ();
  return TRUE;
}

/* keeps the display lit by sending periodical messages to MCE's D-Bus
 * interface */
void
hd_dbus_disable_display_blanking (gboolean setting)
{
  static guint timeout_f = 0;

  if (setting)
    {
      hd_dbus_prevent_display_blanking ();
      if (!timeout_f)
        timeout_f = g_timeout_add (30 * 1000, display_timeout_f, NULL);
    }
  else if (timeout_f)
    {
      g_source_remove (timeout_f);
      timeout_f = 0;
    }
}
#endif

void hd_dbus_send_event (char *value)
{
  DBusMessage *msg;
  dbus_bool_t b;
  DBusMessageIter args;

  if (sysbus_conn == NULL) {
    g_warning ("%s: no D-Bus system bus connection", __func__);
    return;
  }
  msg = dbus_message_new_signal("/com/nokia/hildon_desktop", "com.nokia.hildon_desktop", "KeyEvent");
  if (msg == NULL) {
    g_warning ("%s: could not create message", __func__);
    return;
  }

  dbus_message_iter_init_append(msg, &args);
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &value)) { 
    g_warning("Out Of Memory!"); 
    return;
  }

  b = dbus_connection_send(sysbus_conn, msg, NULL);
  if (!b) {
    g_warning ("%s: dbus_connection_send() failed", __func__);
  } else {
    dbus_connection_flush(sysbus_conn);
  }
  dbus_message_unref(msg);
}

void hd_dbus_send_desktop_orientation_changed (gboolean to_portrait)
{
  DBusMessage *msg;
  dbus_bool_t b;
  DBusMessageIter args;

  if (sysbus_conn == NULL) {
    g_warning ("%s: no D-Bus system bus connection", __func__);
    return;
  }
  msg = dbus_message_new_signal("/com/nokia/hildon_desktop", "com.nokia.hildon_desktop", "DesktopOrientationChanged");
  if (msg == NULL) {
    g_warning ("%s: could not create message", __func__);
    return;
  }

  dbus_message_iter_init_append(msg, &args);
  if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_BOOLEAN, &to_portrait)) { 
    g_warning("Out Of Memory!"); 
    return;
  }

  b = dbus_connection_send(sysbus_conn, msg, NULL);
  if (!b) {
    g_warning ("%s: dbus_connection_send() failed", __func__);
  } else {
    dbus_connection_flush(sysbus_conn);
  }
  dbus_message_unref(msg);
}

DBusConnection *
hd_dbus_init (HdCompMgr * hmgr)
{
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
#ifdef HAVE_DSME
/* TODO Convert MCE to DSME */
      dbus_bus_add_match (sysbus_conn,
                          "type='signal',path='"MCE_SIGNAL_PATH"',"
                          "interface='"MCE_SIGNAL_IF"',"
                          "member='tklock_mode_ind'",
                          NULL);
      dbus_bus_add_match (sysbus_conn,
                          "type='signal',path='" MCE_SIGNAL_PATH "',"
                          "interface='" MCE_SIGNAL_IF "',"
                          "member='" MCE_DISPLAY_SIG "'", NULL);
      dbus_bus_add_match (sysbus_conn,
                          "type='signal',path='" MCE_SIGNAL_PATH "',"
                          "interface='" MCE_SIGNAL_IF "',"
                          "member='" MCE_CALL_STATE_SIG "'", NULL);
#endif

      dbus_connection_add_filter (sysbus_conn,
                                  hd_dbus_system_bus_signal_handler,
                                  hmgr, NULL);
    }

  return connection;
}
