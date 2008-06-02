/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Johan Bilien <johan.bilien@nokia.com>
 *          Tomas Frydrych <tf@o-hand.com>
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

#include "hd-comp-mgr.h"
#include "hd-switcher.h"
#include "hd-home.h"
#include "hd-dbus.h"
#include "hd-atoms.h"
#include "hd-util.h"

#include <matchbox/core/mb-wm.h>
#include <matchbox/core/mb-window-manager.h>
#include <matchbox/core/mb-wm-client.h>

#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/shape.h>

#include <clutter/clutter-container.h>
#include <clutter/x11/clutter-x11.h>

#include <sys/types.h>
#include <signal.h>

#define HDCM_UNMAP_DURATION 200

#define HIBERNATION_TIMEMOUT 3000 /* as suggested by 31410#10 */

#define OSSO_BUS_ROOT          "com.nokia"
#define OSSO_BUS_ROOT_PATH     "/com/nokia"
#define OSSO_BUS_TOP           "top_application"

#define LOWMEM_PROC_ALLOWED    "/proc/sys/vm/lowmem_allowed_pages"
#define LOWMEM_PROC_USED       "/proc/sys/vm/lowmem_used_pages"
#define LOWMEM_LAUNCH_THRESHOLD_DISTANCE 2500

static gchar * hd_comp_mgr_service_from_xwindow (HdCompMgr *hmgr, Window xid);

static gboolean hd_comp_mgr_memory_limits (guint *pages_used,
					   guint *pages_available);

struct HdCompMgrPrivate
{
  ClutterActor   *switcher_group;
  ClutterActor   *home;

  GHashTable     *hibernating_apps;

  Atom            atoms[_HD_ATOM_LAST];

  DBusConnection *dbus_connection;

  gboolean        showing_home    : 1;
  gboolean        stack_sync      : 1;
  gboolean        low_mem         : 1;
};

/*
 * A helper object to store manager's per-client data
 */

typedef enum
{
  HdCompMgrClientFlagHibernating  = (1 << 0),
  HdCompMgrClientFlagCanHibernate = (1 << 1),
} HdCompMgrClientFlags;

struct HdCompMgrClientPrivate
{
  guint                 hibernation_key;
  gchar                *service;
  HdCompMgrClientFlags  flags;
};

static MBWMCompMgrClient *
hd_comp_mgr_client_new (MBWindowManagerClient * client)
{
  MBWMObject *c;

  c = mb_wm_object_new (HD_TYPE_COMP_MGR_CLIENT,
			MBWMObjectPropClient, client,
			NULL);

  return MB_WM_COMP_MGR_CLIENT (c);
}

static void
hd_comp_mgr_client_class_init (MBWMObjectClass *klass)
{
#if MBWM_WANT_DEBUG
  klass->klass_name = "HdCompMgrClient";
#endif
}

static void
hd_comp_mgr_client_process_hibernation_prop (HdCompMgrClient * hc)
{
  HdCompMgrClientPrivate * priv = hc->priv;
  MBWindowManagerClient  * wm_client = MB_WM_COMP_MGR_CLIENT (hc)->wm_client;
  HdCompMgr              * hmgr = HD_COMP_MGR (wm_client->wmref->comp_mgr);
  Atom                   * hibernable = NULL;

  /* NOTE:
   *       the prop has no 'value'; if set the app is killable (hibernatable),
   *       deletes to unset.
   */
  hibernable = hd_util_get_win_prop_data_and_validate
                     (wm_client->wmref->xdpy,
		      wm_client->window->xwindow,
                      hmgr->priv->atoms[HD_ATOM_HILDON_APP_KILLABLE],
                      XA_STRING,
                      8,
                      0,
                      NULL);

  if (!hibernable)
    {
      /*try the alias*/
      hibernable = hd_util_get_win_prop_data_and_validate
	            (wm_client->wmref->xdpy,
		     wm_client->window->xwindow,
                     hmgr->priv->atoms[HD_ATOM_HILDON_ABLE_TO_HIBERNATE],
                     XA_STRING,
                     8,
                     0,
                     NULL);
    }

  if (hibernable)
    priv->flags |= HdCompMgrClientFlagCanHibernate;
  else
    priv->flags &= ~HdCompMgrClientFlagCanHibernate;

  if (hibernable)
    XFree (hibernable);
}

static char *
hd_comp_mgr_client_get_window_role (HdCompMgrClient * hc)
{
  MBWindowManagerClient  * wm_client = MB_WM_COMP_MGR_CLIENT (hc)->wm_client;
  HdCompMgr              * hmgr = HD_COMP_MGR (wm_client->wmref->comp_mgr);
  char                   * role = NULL;

  role = hd_util_get_win_prop_data_and_validate
                     (wm_client->wmref->xdpy,
		      wm_client->window->xwindow,
                      hmgr->priv->atoms[HD_ATOM_WM_WINDOW_ROLE],
                      XA_STRING,
                      8,
                      0,
                      NULL);

  return role;
}

static gchar *
hd_comp_mgr_client_get_window_class (HdCompMgrClient * hc)
{
  gchar * klass = NULL;
  return klass;
}

static int
hd_comp_mgr_client_init (MBWMObject *obj, va_list vap)
{
  HdCompMgrClient        *client = HD_COMP_MGR_CLIENT (obj);
  HdCompMgrClientPrivate *priv;
  HdCompMgr              *hmgr;
  char                   *role;
  gchar                  *klass;
  gchar                  *key = NULL;
  MBWindowManagerClient  *wm_client = MB_WM_COMP_MGR_CLIENT (obj)->wm_client;

  hmgr = HD_COMP_MGR (wm_client->wmref->comp_mgr);

  priv = client->priv = g_new0 (HdCompMgrClientPrivate, 1);

  /*
   * TODO -- if we need to query any more props, we should do that
   * asynchronously.
   */
  hd_comp_mgr_client_process_hibernation_prop (client);

  klass = hd_comp_mgr_client_get_window_class (client);
  role  = hd_comp_mgr_client_get_window_role (client);

  if (role && klass)
    key = g_strconcat (klass, "/", role, NULL);
  else if (klass)
    key = g_strdup (klass);
  else if (role)
    key = g_strdup (role);

  if (key)
    {
      priv->hibernation_key = g_str_hash (key);
      g_free (key);
    }

  g_free (klass);

  if (role)
    XFree (role);

  priv->service =
    hd_comp_mgr_service_from_xwindow (hmgr, wm_client->window->xwindow);

  return 1;
}

static void
hd_comp_mgr_client_destroy (MBWMObject* obj)
{
  HdCompMgrClientPrivate *priv = HD_COMP_MGR_CLIENT (obj)->priv;

  g_free (priv->service);

  g_free (priv);
}

int
hd_comp_mgr_client_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdCompMgrClientClass),
	sizeof (HdCompMgrClient),
	hd_comp_mgr_client_init,
	hd_comp_mgr_client_destroy,
	hd_comp_mgr_client_class_init
      };

      type =
	mb_wm_object_register_class (&info,
				     MB_WM_TYPE_COMP_MGR_CLUTTER_CLIENT, 0);
    }

  return type;
}

gboolean
hd_comp_mgr_client_is_hibernating (HdCompMgrClient *hclient)
{
  HdCompMgrClientPrivate * priv = hclient->priv;

  return ((priv->flags & HdCompMgrClientFlagHibernating) != FALSE);
}

static int  hd_comp_mgr_init (MBWMObject *obj, va_list vap);
static void hd_comp_mgr_class_init (MBWMObjectClass *klass);
static void hd_comp_mgr_destroy (MBWMObject *obj);
static void hd_comp_mgr_unregister_client (MBWMCompMgr *mgr, MBWindowManagerClient *c);
static void hd_comp_mgr_map_notify (MBWMCompMgr *mgr, MBWindowManagerClient *c);
static void hd_comp_mgr_turn_on (MBWMCompMgr *mgr);
static void hd_comp_mgr_effect (MBWMCompMgr *mgr, MBWindowManagerClient *c, MBWMCompMgrClientEvent event);
static void hd_comp_mgr_restack (MBWMCompMgr * mgr);
static void hd_comp_mgr_home_clicked (HdCompMgr *hmgr, ClutterActor *actor);

int
hd_comp_mgr_class_type ()
{
  static int type = 0;

  if (type == 0)
    {
      static MBWMObjectClassInfo info = {
        sizeof (HdCompMgrClass),
        sizeof (HdCompMgr),
        hd_comp_mgr_init,
        hd_comp_mgr_destroy,
        hd_comp_mgr_class_init
      };

      type = mb_wm_object_register_class (&info,
					  MB_WM_TYPE_COMP_MGR_CLUTTER, 0);
    }

  return type;
}

static void
hd_comp_mgr_class_init (MBWMObjectClass *klass)
{
  MBWMCompMgrClass *cm_klass = MB_WM_COMP_MGR_CLASS (klass);
  MBWMCompMgrClutterClass * clutter_klass =
    MB_WM_COMP_MGR_CLUTTER_CLASS (klass);

  cm_klass->unregister_client = hd_comp_mgr_unregister_client;
  cm_klass->client_event      = hd_comp_mgr_effect;
  cm_klass->turn_on           = hd_comp_mgr_turn_on;
  cm_klass->map_notify        = hd_comp_mgr_map_notify;
  cm_klass->restack           = hd_comp_mgr_restack;

  clutter_klass->client_new   = hd_comp_mgr_client_new;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HDCompMgr";
#endif
}

static int
hd_comp_mgr_init (MBWMObject *obj, va_list vap)
{
  MBWMCompMgr          *cmgr = MB_WM_COMP_MGR (obj);
  MBWindowManager      *wm = cmgr->wm;
  HdCompMgr            *hmgr = HD_COMP_MGR (obj);
  HdCompMgrPrivate     *priv;
  ClutterActor         *stage, *switcher, *home;

  priv = hmgr->priv = g_new0 (HdCompMgrPrivate, 1);

  hd_atoms_init (wm->xdpy, priv->atoms);

  priv->dbus_connection = hd_dbus_init (hmgr);

  stage = clutter_stage_get_default ();

  priv->switcher_group = switcher = g_object_new (HD_TYPE_SWITCHER,
						  "comp-mgr", cmgr,
						  NULL);

  clutter_actor_set_size (switcher, wm->xdpy_width, wm->xdpy_height);
  clutter_actor_show (switcher);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), switcher);

  /*
   * Add the home group to stage and push it to the bottom of the actor
   * stack.
   */
  home = priv->home =
    g_object_new (HD_TYPE_HOME, "comp-mgr", cmgr, NULL);

  clutter_actor_set_reactive (home, TRUE);

  g_signal_connect_swapped (home, "background-clicked",
                            G_CALLBACK (hd_comp_mgr_home_clicked),
                            cmgr);

  clutter_actor_show (home);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), home);
  clutter_actor_lower_bottom (home);

  /*
   * Create a hash table for hibernating windows.
   */
  priv->hibernating_apps =
    g_hash_table_new_full (g_int_hash,
			   g_int_equal,
			   NULL,
			   (GDestroyNotify)mb_wm_object_unref);

  return 1;
}

static void
hd_comp_mgr_destroy (MBWMObject *obj)
{
  HdCompMgrPrivate * priv = HD_COMP_MGR (obj)->priv;

  if (priv->hibernating_apps)
    g_hash_table_destroy (priv->hibernating_apps);
}

static void
hd_comp_mgr_setup_input_viewport (HdCompMgr *hmgr, ClutterGeometry * geom)
{
  XserverRegion      region;
  Window             overlay;
  Window             clutter_window;
  XRectangle         rectangle;
  MBWMCompMgr       *mgr = MB_WM_COMP_MGR (hmgr);
  MBWindowManager   *wm = mgr->wm;
  Display           *xdpy = wm->xdpy;
  ClutterActor      *stage;

  overlay = XCompositeGetOverlayWindow (xdpy, wm->root_win->xwindow);

  XSelectInput (xdpy,
                overlay,
                FocusChangeMask |
                ExposureMask |
                PropertyChangeMask |
                ButtonPressMask | ButtonReleaseMask |
                KeyPressMask | KeyReleaseMask);

  rectangle.x      = geom->x;
  rectangle.y      = geom->y;
  rectangle.width  = geom->width;
  rectangle.height = geom->height;

  region = XFixesCreateRegion (wm->xdpy, &rectangle, 1);

  XFixesSetWindowShapeRegion (xdpy,
                              overlay,
                              ShapeBounding,
                              0, 0,
                              None);

  XFixesSetWindowShapeRegion (xdpy,
                              overlay,
                              ShapeInput,
                              0, 0,
                              region);

  stage = clutter_stage_get_default();

  clutter_window = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));

  XSelectInput (xdpy,
                clutter_window,
                FocusChangeMask |
                ExposureMask |
                PropertyChangeMask |
                ButtonPressMask | ButtonReleaseMask |
                KeyPressMask | KeyReleaseMask);

  XFixesSetWindowShapeRegion (xdpy,
                              clutter_window,
                              ShapeBounding,
                              0, 0,
                              None);

  XFixesSetWindowShapeRegion (xdpy,
                              clutter_window,
                              ShapeInput,
                              0, 0,
                              region);

  XFixesDestroyRegion (xdpy, region);
}

static void
hd_comp_mgr_turn_on (MBWMCompMgr *mgr)
{
  ClutterGeometry    geom;
  HdCompMgrPrivate * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClass * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));

  /*
   * The parent class turn_on method deals with setting up the input shape on
   * the overlay window; so we first call it, and then change the shape to
   * suit our custom needs.
   */
  if (parent_klass->turn_on)
    parent_klass->turn_on (mgr);

  hd_switcher_get_button_geometry (HD_SWITCHER (priv->switcher_group), &geom);
  hd_comp_mgr_setup_input_viewport (HD_COMP_MGR (mgr), &geom);
}

static void
hd_comp_mgr_unregister_client (MBWMCompMgr *mgr, MBWindowManagerClient *c)
{
  ClutterActor                  * actor;
  HdCompMgrClientFlags            h_flags;
  HdCompMgrPrivate              * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClass              * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));
  MBWMCompMgrClutterClient      * cclient =
    MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);
  HdCompMgrClient               * hclient = HD_COMP_MGR_CLIENT (c->cm_client);

  h_flags = hclient->priv->flags;

  actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);

  if (h_flags & HdCompMgrClientFlagHibernating)
    {
      /*
       * We want to hold onto the CM client object, so we can continue using
       * the actor.
       */
      mb_wm_object_ref (MB_WM_OBJECT (cclient));

      g_hash_table_insert (priv->hibernating_apps,
			   & hclient->priv->hibernation_key,
			   hclient);

      hd_switcher_hibernate_window_actor (HD_SWITCHER (priv->switcher_group),
					  actor);
    }
  /*
   * If the actor is an application, remove it also to the switcher
   *
   * FIXME: will need to do this for notifications as well.
   */
  else if (MB_WM_CLIENT_CLIENT_TYPE (c) == MBWMClientTypeApp)
    {
      hd_switcher_remove_window_actor (HD_SWITCHER (priv->switcher_group),
				       actor);

      g_object_set_data (G_OBJECT (actor),
			 "HD-MBWMCompMgrClutterClient", NULL);
    }

  if (parent_klass->unregister_client)
    parent_klass->unregister_client (mgr, c);
}

static void
hd_comp_mgr_map_notify (MBWMCompMgr *mgr, MBWindowManagerClient *c)
{
  ClutterActor             * actor;
  HdCompMgrPrivate         * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClutterClient * cclient;
  MBWMCompMgrClass         * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));
  HdCompMgrClient          * hclient;
  HdCompMgrClient          * hclient_h;
  guint                      hkey;

  /*
   * Parent class map_notify creates the actor representing the client.
   */
  if (parent_klass->map_notify)
    parent_klass->map_notify (mgr, c);

  /*
   * If the actor is an appliation, add it also to the switcher
   *
   * FIXME: will need to do this for notifications as well.
   */
  if (MB_WM_CLIENT_CLIENT_TYPE (c) != MBWMClientTypeApp)
    return;

  cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);
  actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);

  g_object_set_data (G_OBJECT (actor),
		     "HD-MBWMCompMgrClutterClient", cclient);

  hclient = HD_COMP_MGR_CLIENT (cclient);
  hkey = hclient->priv->hibernation_key;

  hclient_h = g_hash_table_lookup (priv->hibernating_apps, &hkey);

  if (hclient_h)
    {
      ClutterActor             * actor_h;
      MBWMCompMgrClutterClient * cclient_h;

      cclient_h = MB_WM_COMP_MGR_CLUTTER_CLIENT (hclient_h);

      actor_h = mb_wm_comp_mgr_clutter_client_get_actor (cclient_h);

      hd_switcher_replace_window_actor (HD_SWITCHER (priv->switcher_group),
					actor_h, actor);

      g_hash_table_remove (priv->hibernating_apps, &hkey);
    }
  else
    hd_switcher_add_window_actor (HD_SWITCHER (priv->switcher_group), actor);
}

typedef struct _HDEffectData
{
  MBWMCompMgrClutterClient * cclient;
} HDEffectData;

static void
hd_comp_mgr_effect_completed (ClutterActor * actor, HDEffectData *data)
{
  mb_wm_comp_mgr_clutter_client_unset_flags (data->cclient,
					MBWMCompMgrClutterClientDontUpdate |
                                        MBWMCompMgrClutterClientEffectRunning);

  mb_wm_object_unref (MB_WM_OBJECT (data->cclient));

  g_object_unref (actor);

  g_free (data);
};

static void
hd_comp_mgr_effect (MBWMCompMgr                *mgr,
                    MBWindowManagerClient      *c,
                    MBWMCompMgrClientEvent      event)
{
  MBWMClientType c_type = MB_WM_CLIENT_CLIENT_TYPE (c);

  if (c_type == MBWMClientTypeApp)
    {
      switch (event)
	{
	case MBWMCompMgrClientEventUnmap:
	  {
	    MBWMCompMgrClutterClient * cclient;
	    ClutterActor             * actor;
	    ClutterTimeline          * timeline;
	    ClutterEffectTemplate    * tmpl;
	    gdouble                    scale_x, scale_y;
	    HDEffectData             * data;

	    cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);
	    actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);

	    tmpl =
	      clutter_effect_template_new_for_duration (HDCM_UNMAP_DURATION,
						   CLUTTER_ALPHA_RAMP_INC);

	    clutter_actor_get_scale (actor, &scale_x, &scale_y);

	    data = g_new0 (HDEffectData, 1);
	    data->cclient = mb_wm_object_ref (MB_WM_OBJECT (cclient));

	    clutter_actor_move_anchor_point_from_gravity (actor,
						     CLUTTER_GRAVITY_CENTER);

	    timeline = clutter_effect_scale (tmpl, actor,
					     scale_x, 0.1,
					     (ClutterEffectCompleteFunc)
					     hd_comp_mgr_effect_completed,
					     data);

	    mb_wm_comp_mgr_clutter_client_set_flags (cclient,
					MBWMCompMgrClutterClientDontUpdate |
                                        MBWMCompMgrClutterClientEffectRunning);

	    clutter_timeline_start (timeline);
	  }
	  break;

	default:
	  break;
	}
    }
}

static void
hd_comp_mgr_restack (MBWMCompMgr * mgr)
{
  HdCompMgrPrivate         * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClass         * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));

  /*
   * We use the parent class restack() method to do the stacking, but as our
   * switcher shares actors with the CM, we cannot run this when the switcher
   * is showing; instead we set a flag, and let the switcher request stack
   * sync when it closes.
   */
  if (hd_switcher_showing_switcher (HD_SWITCHER (priv->switcher_group)))
    priv->stack_sync = TRUE;
  else
    {
      /*
       * Check if home is to be the top level application (i.e., there are no
       * application clients); if home is top level, put it into active input
       * mode so we can do panning, etc.
       */
      MBWindowManager       *wm = mgr->wm;
      MBWindowManagerClient *c  = mb_wm_get_visible_main_client (wm);

      if (c)
	{
	  priv->showing_home = FALSE;
	  hd_home_set_input_mode (HD_HOME (priv->home), FALSE);
	}
      else
	{
	  priv->showing_home = TRUE;
	  hd_home_set_input_mode (HD_HOME (priv->home), TRUE);
	}

      if (parent_klass->restack)
	parent_klass->restack (mgr);
    }
}

void
hd_comp_mgr_sync_stacking (HdCompMgr * hmgr)
{
  HdCompMgrPrivate * priv = hmgr->priv;

  /*
   * If the stack_sync flag is set, force restacking of the CM actors
   */
  if (priv->stack_sync)
    {
      priv->stack_sync = FALSE;
      hd_comp_mgr_restack (MB_WM_COMP_MGR (hmgr));
    }
}

void
hd_comp_mgr_raise_home_actor (HdCompMgr *hmgr)
{
  HdCompMgrPrivate * priv = hmgr->priv;

  clutter_actor_lower (priv->home, priv->switcher_group);
}

void
hd_comp_mgr_lower_home_actor (HdCompMgr *hmgr)
{
  HdCompMgrPrivate * priv = hmgr->priv;

  clutter_actor_lower_bottom (priv->home);
}

static void
hd_comp_mgr_home_clicked (HdCompMgr *hmgr, ClutterActor *actor)
{
  g_debug ("Mgr: home clicked.");
  hd_comp_mgr_top_home (hmgr);
}

void
hd_comp_mgr_top_home (HdCompMgr *hmgr)
{
  /* TODO */
  g_print ("topping home\n");
}

static gboolean
hd_comp_mgr_client_shutdown_timeout_cb (gpointer data)
{
  pid_t pid = GPOINTER_TO_INT (data);

  if (pid && !kill (pid, 0))
    {
      /* app did not exit in response to delete protocol, kill it */

      if (kill (pid, SIGKILL))
        {
	  /* Something went wrong, perhaps we do not have sufficient
	   * permissions to kill this process
	   */
	  g_warning ("SIGKILL failed on pid %d.", pid);
        }
    }

  return FALSE;
}

/*
 * Shuts down a client, handling hibernated applications correctly.
 */
void
hd_comp_mgr_close_client (HdCompMgr *hmgr, MBWMCompMgrClutterClient *cc)
{
  HdCompMgrPrivate      * priv = hmgr->priv;
  HdCompMgrClient       * h_client = HD_COMP_MGR_CLIENT (cc);
  HdCompMgrClientFlags    h_flags = h_client->priv->flags;


  if (h_flags & HdCompMgrClientFlagHibernating)
    {
      ClutterActor * actor;

      actor = mb_wm_comp_mgr_clutter_client_get_actor (cc);

      hd_switcher_remove_window_actor (HD_SWITCHER (priv->switcher_group),
				       actor);

      mb_wm_object_unref (MB_WM_OBJECT (cc));
    }
  else
    {
      MBWindowManagerClient * c = MB_WM_COMP_MGR_CLIENT (cc)->wm_client;

      g_timeout_add (HIBERNATION_TIMEMOUT,
                     (GSourceFunc) hd_comp_mgr_client_shutdown_timeout_cb,
                     GINT_TO_POINTER (c->window->pid));

      mb_wm_client_deliver_delete (c);
    }
}

void
hd_comp_mgr_hibernate_client (HdCompMgr *hmgr,
			      MBWMCompMgrClutterClient *cc,
			      gboolean force)
{
  MBWMCompMgrClient * c  = MB_WM_COMP_MGR_CLIENT (cc);
  HdCompMgrClient   * hc = HD_COMP_MGR_CLIENT (cc);

  if (!force && !(hc->priv->flags & HdCompMgrClientFlagCanHibernate))
    return;

  mb_wm_comp_mgr_clutter_client_set_flags (cc,
					   MBWMCompMgrClutterClientDontUpdate);

  hc->priv->flags |= HdCompMgrClientFlagHibernating;

  g_timeout_add (HIBERNATION_TIMEMOUT,
		 (GSourceFunc) hd_comp_mgr_client_shutdown_timeout_cb,
		 GINT_TO_POINTER (c->wm_client->window->pid));

  mb_wm_client_deliver_delete (c->wm_client);
}

void
hd_comp_mgr_wakeup_client (HdCompMgr *hmgr, HdCompMgrClient *hclient)
{
  hd_comp_mgr_launch_application (hmgr, hclient->priv->service, "RESTORE");
}

void
hd_comp_mgr_hibernate_all (HdCompMgr *hmgr, gboolean force)
{
  MBWMCompMgr     * mgr = MB_WM_COMP_MGR (hmgr);
  MBWindowManager * wm = mgr->wm;

  if (!mb_wm_stack_empty (wm))
    {
      MBWindowManagerClient * c;

      mb_wm_stack_enumerate (wm, c)
	{
	  MBWMCompMgrClutterClient * cc =
	    MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);

	  hd_comp_mgr_hibernate_client (hmgr, cc, force);
	}
    }
}

void
hd_comp_mgr_set_low_memory_state (HdCompMgr * hmgr, gboolean on)
{
  HdCompMgrPrivate * priv = hmgr->priv;

  priv->low_mem = on;
}

gboolean
hd_comp_mgr_get_low_memory_state (HdCompMgr * hmgr)
{
  HdCompMgrPrivate * priv = hmgr->priv;

  return priv->low_mem;
}

void
hd_comp_mgr_launch_application (HdCompMgr   *hmgr,
				const gchar *app_service,
				const gchar *launch_param)
{
  gchar *service, *path, *tmp = NULL;
  guint  pages_used = 0, pages_available = 0;

  if (hd_comp_mgr_memory_limits (&pages_used, &pages_available))
    {
      g_debug ("Memory: pages used %d, available %d.",
	       pages_used, pages_available);

      /* 0 means the memory usage is unknown */
      if (pages_available > 0 &&
	  pages_available < LOWMEM_LAUNCH_THRESHOLD_DISTANCE)
	{
	  /*
	   * TODO -- we probably should pop a dialog here asking the user to
	   * kill some apps as the old TN used to do; check the current spec.
	   */
	  g_debug ("Not enough memory to start application [%s].",
		   app_service);
	  return;
	}
    }
  else
    g_warning ("Failed to read memory limits; using scratchbox ???");

  /*
   * NB -- interface is identical to service
   */

  /* If we have full service name we will use it*/
  if (g_strrstr(app_service,"."))
    {
      service = g_strdup (app_service);
      tmp = g_strdup (app_service);
      path = g_strconcat ("/", g_strdelimit(tmp,".",'/'), NULL);
    }
  else /* use com.nokia prefix*/
    {
      service = g_strconcat (OSSO_BUS_ROOT, ".", app_service, NULL);
      path = g_strconcat (OSSO_BUS_ROOT_PATH, "/", app_service, NULL);
    }

  hd_dbus_launch_service (hmgr->priv->dbus_connection,
			  service,
			  path,
			  service,
			  OSSO_BUS_TOP,
			  launch_param);

  g_free (service);
  g_free (path);
  g_free (tmp);
}

static gchar *
hd_comp_mgr_service_from_xwindow (HdCompMgr *hmgr, Window xid)
{
  MBWindowManager  *wm;
/*   HdCompMgrPrivate *priv = hmgr->priv; */
  gchar            *service;
  XClassHint        class_hint;
  Status            status = 0;

  wm = MB_WM_COMP_MGR (hmgr)->wm;

  memset(&class_hint, 0, sizeof(XClassHint));

  mb_wm_util_trap_x_errors ();

  status = XGetClassHint(wm->xdpy, xid, &class_hint);

  if (mb_wm_util_untrap_x_errors () || !status || !class_hint.res_name)
    goto out;

  /*
   * FIXME -- need to implement this bit once we have
   * the desktop data store in place.
   */
#if 0
  service = g_hash_table_lookup (apps,
				(gconstpointer)class_hint.res_name);
#else
  service = NULL;
#endif

 out:
  if (class_hint.res_class)
    XFree(class_hint.res_class);

  if (class_hint.res_name)
    XFree(class_hint.res_name);

  if (service)
    return g_strdup (service);

  return NULL;
}

static gboolean
hd_comp_mgr_memory_limits (guint *pages_used, guint *pages_available)
{
  guint    lowmem_allowed;
  gboolean result;
  FILE    *lowmem_allowed_f, *pages_used_f;

  result = FALSE;

  lowmem_allowed_f = fopen (LOWMEM_PROC_ALLOWED, "r");
  pages_used_f     = fopen (LOWMEM_PROC_USED, "r");

  if (lowmem_allowed_f && pages_used_f)
    {
      fscanf (lowmem_allowed_f, "%u", &lowmem_allowed);
      fscanf (pages_used_f, "%u", pages_used);

      if (*pages_used < lowmem_allowed)
	*pages_available = lowmem_allowed - *pages_used;
      else
	*pages_available = 0;

      result = TRUE;
    }
  else
    {
      g_warning ("Could not read lowmem page stats.");
    }

  if (lowmem_allowed_f)
    fclose(lowmem_allowed_f);

  if (pages_used_f)
    fclose(pages_used_f);

  return result;
}
