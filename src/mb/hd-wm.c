/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Johan Bilien <johan.bilien@nokia.com>
 *          Tomas Frydrych <tf@o-hand.com>
 *          Kimmo Hämäläinen <kimmo.hamalainen@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hd-wm.h"
#include "hd-comp-mgr.h"
#include "hd-render-manager.h"
#include "hd-desktop.h"
#include "hd-app.h"
#include "hd-switcher.h"

#include <matchbox/core/mb-wm-object.h>
#include <matchbox/core/mb-wm.h>
#include <matchbox/client-types/mb-wm-client-desktop.h>

#include <clutter/clutter-main.h>
#include <clutter/x11/clutter-x11.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <glib/gi18n.h>

#include <hildon/hildon.h>

#include "hd-comp-mgr.h"
#include "hd-home-applet.h"
#include "hd-note.h"
#include "hd-status-area.h"
#include "hd-status-menu.h"
#include "hd-app-menu.h"
#include "hd-dialog.h"
#include "hd-animation-actor.h"
#include "hd-remote-texture.h"
#include "hd-util.h"

static int  hd_wm_init       (MBWMObject *object, va_list vap);
static void hd_wm_destroy    (MBWMObject *object);
static void hd_wm_class_init (MBWMObjectClass *klass);
static MBWindowManagerClient* hd_wm_client_new (MBWindowManager *,
						MBWMClientWindow *);
static MBWMCompMgr * hd_wm_comp_mgr_new (MBWindowManager *wm);
static void hd_wm_client_responding (MBWindowManager *wm,
				     MBWindowManagerClient *c);
static Bool hd_wm_client_hang (MBWindowManager *wm,
			       MBWindowManagerClient *c);

static Bool hd_wm_client_activate (
		MBWindowManager * wm, 
		MBWindowManagerClient *c);

struct HdWmPrivate
{
  GtkWidget *hung_client_dialog;
  Window hung_client_dialog_xid;
};

int
hd_wm_class_type ()
{
  static int type = 0;

  if (type == 0)
    {
      static MBWMObjectClassInfo info = {
        sizeof (HdWmClass),
        sizeof (HdWm),
        hd_wm_init,
        hd_wm_destroy,
        hd_wm_class_init
      };

      type = mb_wm_object_register_class (&info, MB_TYPE_WINDOW_MANAGER, 0);
    }

  return type;
}

static int
hd_wm_init (MBWMObject *object, va_list vap)
{
  MBWindowManager      *wm = MB_WINDOW_MANAGER (object);
  HdWm		       *hdwm = HD_WM (wm);

  wm->modality_type = MBWMModalitySystem;

  hdwm->priv = mb_wm_util_malloc0 (sizeof (struct HdWmPrivate));

  return 1;
}

static void
hd_wm_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClass *wm_class = MB_WINDOW_MANAGER_CLASS (klass);

  wm_class->comp_mgr_new = hd_wm_comp_mgr_new;
  wm_class->client_new   = hd_wm_client_new;

  wm_class->client_responding = hd_wm_client_responding;
  wm_class->client_hang	      = hd_wm_client_hang;
  wm_class->client_activate   = hd_wm_client_activate;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdWm";
#endif
}

static void
hd_wm_destroy (MBWMObject *object)
{
  MBWindowManager   *wm = MB_WINDOW_MANAGER (object);
  HdWm		    *hdwm = HD_WM (wm);

  free (hdwm->priv);
}

static MBWMCompMgr *
hd_wm_comp_mgr_new (MBWindowManager *wm)
{
  MBWMCompMgr  *mgr;

  mgr = (MBWMCompMgr *)mb_wm_object_new (HD_TYPE_COMP_MGR,
                                         MBWMObjectPropWm, wm,
                                         NULL);

  return mgr;
}

static MBWindowManagerClient*
hd_wm_client_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  HdCompMgr            *hmgr = HD_COMP_MGR (wm->comp_mgr);
  MBWindowManagerClass *wm_class =
    MB_WINDOW_MANAGER_CLASS(MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(wm)));

  if (win->override_redirect                                                  ||
      win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_DOCK]           ||
      win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_MENU]           ||
      win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_POPUP_MENU]     ||
      win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_DROPDOWN_MENU]  ||
      win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_TOOLBAR]        ||
      win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_INPUT])
    /* Pass to libmatchbox the types we don't want to handle.
     * We'll handle the unknowns. */
    return wm_class ?  wm_class->client_new (wm, win) : NULL;
  else if (win->net_type ==
      hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_WM_WINDOW_TYPE_HOME_APPLET))
    {
      g_debug ("### is home applet ###");
      return hd_home_applet_new (wm, win);
    }
  else if (win->net_type ==
      hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_WM_WINDOW_TYPE_APP_MENU))
    {
      g_debug ("### is application menu ###");
      return hd_app_menu_new (wm, win);
    }
  else if (win->net_type ==
      hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_WM_WINDOW_TYPE_STATUS_AREA))
    {
      g_debug ("### is status area ###");
      return hd_status_area_new (wm, win);
    }
  else if (win->net_type ==
      hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_WM_WINDOW_TYPE_STATUS_MENU))
    {
      g_debug ("### is status menu ###");
      return hd_status_menu_new (wm, win);
    }
  else if (win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_DESKTOP])
    {
      g_debug ("### is desktop ###");
      /* Only one desktop allowed */
      if (wm->desktop)
        return NULL;

      return hd_desktop_new (wm, win);
    }
  else if (win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_NORMAL])
    {
      g_debug ("### is application ###");
      return hd_app_new (wm, win);
    }
  else if (win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_DIALOG])
    {
      g_debug ("### is dialog ###");
      return hd_dialog_new (wm, win);
    }
  else if (win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION])
    {
      g_debug ("### is notification ###");
      return hd_note_new (wm, win);
    }
  else if (win->net_type == hd_comp_mgr_get_atom (hmgr,
                          HD_ATOM_HILDON_WM_WINDOW_TYPE_ANIMATION_ACTOR))
    {
      g_debug ("### is animation actor ###");
      return hd_animation_actor_new (wm, win);
    }
  else if (win->net_type == hd_comp_mgr_get_atom (hmgr,
                          HD_ATOM_HILDON_WM_WINDOW_TYPE_REMOTE_TEXTURE))
    {
      g_debug ("### is remote texture ###");
      return hd_remote_texture_new (wm, win);
    }
  else
    {
      /* All of hildon-desktop relies on %MBWMClientTypeApp:s being
       * represented by %HdApp:s.  Don't let them down. */
      char * name = XGetAtomName (wm->xdpy, win->net_type);
      if (name)
        {
          g_warning ("### unhandled window type %s (%lx) ###",
                     name, win->xwindow);
          XFree (name);
        }
      else
        g_warning ("### unhandled window type [no net_type] (%lx) ###",
                   win->xwindow);
      return hd_app_new (wm, win);
    }
}

#if 0
static gboolean
show_info_note (gpointer data)
{
  gchar *s = data;
  hildon_banner_show_information (NULL, NULL, s);
  g_free (s);
  return FALSE;
}
#endif

static void
hd_wm_client_responding (MBWindowManager *wm,
			 MBWindowManagerClient *client)
{
  HdWm *hdwm = HD_WM (wm);

  g_debug ("%s: entered", __FUNCTION__);

  /* If we are currently telling the user that the client is not responding
   * then we force a cancelation of that dialog.
   */
  /* FIXME: show the banner only if the dialog is really visible */
  if (hdwm->priv->hung_client_dialog)
    {
#if 0  /* removed as of NB#140674 */
      char buf[200];
      const char *name;

      /* TODO: get the localised name for application */
      name = mb_wm_client_get_name (client);
      snprintf (buf, 200, _("tana_ib_apkil_responded"),
	        name ? name : "NO NAME");

      /* have to show the banner in idle, otherwise this can cause a lock-up
       * in libxcb (see NB#106919) */
      g_idle_add (show_info_note, g_strdup (buf));
#endif

      gtk_dialog_response (GTK_DIALOG (hdwm->priv->hung_client_dialog),
			   GTK_RESPONSE_REJECT);
    }
}

Window
hd_wm_get_hung_client_dialog_xid (MBWindowManager *wm)
{
  HdWm *hdwm = HD_WM (wm);
  return hdwm->priv->hung_client_dialog_xid;
}

static GtkWidget*
hd_wm_make_dialog (MBWindowManagerClient *client)
{
    char buf[200];
    const char *name = NULL;
    HildonNote *note;

    HdCompMgrClient *hclient = HD_COMP_MGR_CLIENT (client->cm_client);
    if (hclient)
      name = hd_comp_mgr_client_get_app_local_name (hclient);
    if (!name)
      name = mb_wm_client_get_name (client);
    snprintf (buf, 200, _("tana_nc_apkil_notresponding"),
        name? name : "NO NAME");

    note = HILDON_NOTE (hildon_note_new_confirmation (NULL, buf));
    /*
    gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("qgn_bd_apkil_ok"),
                          GTK_RESPONSE_ACCEPT,
                          _("qgn_bd_apkil_cancel"),
                          GTK_RESPONSE_REJECT,
                          NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT);
			  */

    return (GtkWidget*)note;
}

static Bool hd_wm_client_hang (MBWindowManager *wm,
			       MBWindowManagerClient *c)
{
  HdWm	    *hdwm = HD_WM (wm);
  GtkWidget *dialog;
  GdkWindow *gdk_win;
  gint	     response;

  g_debug ("%s: entered", __FUNCTION__);
  dialog = hd_wm_make_dialog (c);

  /* NB: Setting hdwm->priv->hung_client_dialog is an indication to
   * hd_wm_client_responding that the user has been presented the dialog
   * so it may be canceled if the client starts responding again.
   */
  hdwm->priv->hung_client_dialog = dialog;

  gtk_widget_realize (dialog);
  gdk_win = gtk_widget_get_window (dialog);
  if (gdk_win)
    hdwm->priv->hung_client_dialog_xid = gdk_x11_drawable_get_xid (gdk_win);

  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  hdwm->priv->hung_client_dialog = NULL;

  if (response == GTK_RESPONSE_OK)
    return False;
  else
    return True;
}

/* This is like hd_wm_client_activate() but designed specifically
 * for the switcher.  The focal difference is that this function
 * doesn't try to zoom in. */
Bool
hd_wm_activate_zoomed_client (MBWindowManager *wm,
                                     MBWindowManagerClient *c)
{
  MBWindowManagerClass *wm_class = 
    MB_WINDOW_MANAGER_CLASS(MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(wm)));
  gboolean ret = wm_class->client_activate (wm, c);

  hd_render_manager_set_state (HDRM_STATE_APP);

  hd_render_manager_stop_transition ();
  return ret;
}

static Bool 
hd_wm_client_activate (MBWindowManager * wm, 
                       MBWindowManagerClient *c)
{
  MBWindowManagerClass *wm_class = 
    MB_WINDOW_MANAGER_CLASS(MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(wm)));
  Bool ret = True;

  /* If we're in switcher when the client is activated try to zoom in.
   * Otherwisw just go to APP state. */
  if (c == wm->desktop)
    {
      ret = wm_class->client_activate (wm, c);
      if (!STATE_NEED_DESKTOP(hd_render_manager_get_state () ))
        hd_render_manager_set_state (HDRM_STATE_HOME);
    }
  else if (HD_IS_APP (c))
    {
      HdSwitcher *sw;
      ClutterActor *a;
      HDRMStateEnum state;
      extern HdTaskNavigator *hd_task_navigator;

      /*
       * We need to verify the thing is in the switcher already, otherwise
       * zoom_in() may complain about missing apwins.  This can arise when
       * an application is started while you're in the switcher.  This is
       * because when a new %MBWindowManagerClient is created it is activated
       * but its window is not added to the switcher yet.
       */
      a = mb_wm_comp_mgr_clutter_client_get_actor (
                      MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client));
      sw = HD_SWITCHER (hd_comp_mgr_get_switcher (HD_COMP_MGR (wm->comp_mgr)));
      state = hd_render_manager_get_state();
      if (state == HDRM_STATE_TASK_NAV
          && hd_task_navigator_has_window (hd_task_navigator, a))
        {
          hd_switcher_item_selected (sw, a);
          ret = True;
        }
      else if (!STATE_IS_APP (state) && !c->window->live_background)
        {
          /* This will restack, which is necessary for us before going to
           * APP state, because it makes decisions based on the topmost
           * application on the stack. */
          ret = wm_class->client_activate (wm, c);
          hd_render_manager_set_state (HDRM_STATE_APP);

          /*
           * Roughly for the same reason as in hd-comp-mgr.c,
           * see "let's stop the transition and problem solved".
           * You could trigger the problem by increasing the blur
           * timeline to 2 secs, have a program that gtk_present()s
           * itself in 1 second then tap out of the switcher.
           * At the end the client will be active but it will be
           * invisible.
           */
          hd_render_manager_stop_transition ();
        }
      else
        ret = wm_class->client_activate (wm, c);
    }
  else
    ret = wm_class->client_activate (wm, c);

  return ret;
}


/*
 * If @wm is %NULL return the XID of the foreground %HdApp client
 * in application view.  If we are not in application view this
 * should be 0x00000000, or the XID of the desktop window in home
 * view.  Itherwise set the set the _MB_CURRENT_APP_WINDOW
 * property of the wm root window to @xid if it's changing.
 */
Window
hd_wm_current_app_is (MBWindowManager *wm, Window xid)
{
  static Window last;

  if (!wm || xid == last)
    return last;

  XChangeProperty(wm->xdpy, wm->root_win->xwindow,
                  wm->atoms[MBWM_ATOM_MB_CURRENT_APP_WINDOW],
                  XA_WINDOW, 32, PropModeReplace,
                  (unsigned char *)&xid, 1);

  mb_wm_util_async_trap_x_errors (wm->xdpy);
  /* Remove the mirrored property from the previous window */
  if (last != 0 && last != ~0)
    {
      Window none = None;
      XChangeProperty(wm->xdpy, last,
                      wm->atoms[MBWM_ATOM_MB_CURRENT_APP_WINDOW],
                      XA_WINDOW, 32, PropModeReplace,
                      (unsigned char *)&none, 1);
    }

  /* Add the mirrored property on the new window */
  if (xid != 0 && xid != ~0)
    {
      XChangeProperty(wm->xdpy, xid,
                      wm->atoms[MBWM_ATOM_MB_CURRENT_APP_WINDOW],
                      XA_WINDOW, 32, PropModeReplace,
                      (unsigned char *)&xid, 1);
    }
  mb_wm_util_async_untrap_x_errors ();

  last = xid;

  g_debug ("CURRENT_APP_WINDOW => 0x%lx", xid);
  return last;
}

static gboolean
hd_wm_is_fullscreen_vkb (MBWindowManagerClient *client)
{
  MBWindowManager *wm = client->wmref;
  const char *name = mb_wm_client_get_name (client);
  if (name && strcmp("hildon-input-method", name) == 0 &&
      client->window->geometry.height >= wm->xdpy_height &&
      client->window->geometry.width >= wm->xdpy_width &&
      client->window->geometry.x <= 0 &&
      client->window->geometry.y <= 0)
    return TRUE;
  return FALSE;
}

/*
 * Closes all the modal blockers that can be safely closed (currently all the
 * menus). Returns TRUE if there are no modal blockers left. If there is at
 * least one modal blocker that can not be closed this function will not close
 * any of the menus either.
 */
gboolean
hd_wm_close_modal_blockers (const MBWindowManager *wm)
{
  MBWindowManagerClient *client;

  for (client = wm->stack_top; client && client != wm->desktop;
       client = client->stacked_below)
    {
      if (hd_util_client_has_modal_blocker (client))
        {
	  MBWMClientType c_type = MB_WM_CLIENT_CLIENT_TYPE (client);
          if (!(c_type == MBWMClientTypeMenu ||
                c_type == HdWmClientTypeAppMenu ||
                c_type == HdWmClientTypeStatusMenu)
              && !hd_wm_is_fullscreen_vkb (client)) 
            /* a real blocker that cannot be deleted */
	    return FALSE;
	}
    }

  /* the remaining blockers should be deleteable menus */
  for (client = wm->stack_top; client && client != wm->desktop;
       client = client->stacked_below) 
    if (hd_util_client_has_modal_blocker (client)) 
      mb_wm_client_deliver_delete (client);

  return TRUE;
}


gboolean
hd_wm_has_modal_blockers (const MBWindowManager *wm)
{
  MBWindowManagerClient *client;

  for (client = wm->stack_top; client && client != wm->desktop;
       client = client->stacked_below)
    if (hd_util_client_has_modal_blocker(client))
      return TRUE;
  return FALSE;
}

/* Delete the topmost menu. */
void
hd_wm_delete_temporaries (MBWindowManager *wm)
{
  MBWindowManagerClient *c;

  for (c = wm->stack_top; c; c = c->stacked_below)
    if (MB_WM_CLIENT_CLIENT_TYPE (c) & HdWmClientTypeAppMenu)
      { /* HdAppMenu understands WM_DELETE. */
        mb_wm_client_deliver_delete (c);
        return;
      }
    else if (MB_WM_CLIENT_CLIENT_TYPE (c) & MBWMClientTypeMenu)
      { /* GtkMenu wants DELETE_TEMPORARIES. */
        mb_wm_client_deliver_message (c,
                       hd_comp_mgr_get_atom (HD_COMP_MGR (wm->comp_mgr),
                                             HD_ATOM_DELETE_TEMPORARIES),
                       None, None, None, None, None);
        return;
      }
}
