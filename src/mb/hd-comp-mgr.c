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
#include "hd-task-navigator.h"
#include "hd-home.h"
#include "hd-dbus.h"
#include "hd-atoms.h"
#include "hd-util.h"
#include "hd-transition.h"
#include "hd-wm.h"
#include "hd-home-applet.h"
#include "hd-app.h"
#include "hd-gtk-style.h"
#include "hd-applet-layout-manager.h"
#include "hd-note.h"
#include "hd-render-manager.h"
#include "launcher/hd-launcher.h"

#include <matchbox/core/mb-wm.h>
#include <matchbox/core/mb-window-manager.h>
#include <matchbox/core/mb-wm-client.h>
#include <matchbox/theme-engines/mb-wm-theme.h>

#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/shape.h>

#include <clutter/clutter-container.h>
#include <clutter/x11/clutter-x11.h>

#include "../tidy/tidy-blur-group.h"

#include <sys/types.h>
#include <signal.h>
#include <math.h>

#define HIBERNATION_TIMEMOUT 3000 /* as suggested by 31410#10 */

#define OSSO_BUS_ROOT          "com.nokia"
#define OSSO_BUS_ROOT_PATH     "/com/nokia"
#define OSSO_BUS_TOP           "top_application"

#define LOWMEM_PROC_ALLOWED    "/proc/sys/vm/lowmem_allowed_pages"
#define LOWMEM_PROC_USED       "/proc/sys/vm/lowmem_used_pages"
#define LOWMEM_LAUNCH_THRESHOLD_DISTANCE 2500

/* We need this because these things pop up on startup and then don't go away,
 * leaving us with a needlessly blurred background */
#define BLUR_FOR_WINDOW(c) (!(c)->window || \
              !(g_str_equal((c)->window->name, "SystemUI root window")))

static gchar * hd_comp_mgr_service_from_xwindow (HdCompMgr *hmgr, Window xid);

static gboolean hd_comp_mgr_memory_limits (guint *pages_used,
					   guint *pages_available);

struct HdCompMgrPrivate
{
  MBWindowManagerClient *desktop;
  HdRenderManager       *render_manager;

  HdSwitcher            *switcher_group;
  ClutterActor          *home;

  GHashTable            *hibernating_apps;

  Atom                   atoms[_HD_ATOM_LAST];

  DBusConnection        *dbus_connection;

  HdAppletLayoutManager *applet_manager[4];

  gboolean               stack_sync      : 1;
  gboolean               low_mem         : 1;

  gint                   unmap_effect_running;

  MBWindowManagerClient *status_area_client;

  /* Clients who need to block the Tasksk button. */
  GHashTable            *tasks_button_blockers;
};

/*
 * A helper object to store manager's per-client data
 */

struct HdCompMgrClientPrivate
{
  guint                 hibernation_key;
  gchar                *service;

  gboolean              hibernating   : 1;
  gboolean              can_hibernate : 1;
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
    priv->can_hibernate = TRUE;
  else
    priv->can_hibernate = FALSE;

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

  return priv->hibernating;
}

static int  hd_comp_mgr_init (MBWMObject *obj, va_list vap);
static void hd_comp_mgr_class_init (MBWMObjectClass *klass);
static void hd_comp_mgr_destroy (MBWMObject *obj);
static void hd_comp_mgr_register_client (MBWMCompMgr *mgr,
                                         MBWindowManagerClient *c);
static void hd_comp_mgr_unregister_client (MBWMCompMgr *mgr, MBWindowManagerClient *c);
static void hd_comp_mgr_map_notify
                        (MBWMCompMgr *mgr, MBWindowManagerClient *c);
static void hd_comp_mgr_unmap_notify
                        (MBWMCompMgr *mgr, MBWindowManagerClient *c);
static void hd_comp_mgr_turn_on (MBWMCompMgr *mgr);
static void hd_comp_mgr_effect (MBWMCompMgr *mgr, MBWindowManagerClient *c, MBWMCompMgrClientEvent event);
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
  cm_klass->register_client   = hd_comp_mgr_register_client;
  cm_klass->client_event      = hd_comp_mgr_effect;
  cm_klass->turn_on           = hd_comp_mgr_turn_on;
  cm_klass->map_notify        = hd_comp_mgr_map_notify;
  cm_klass->unmap_notify      = hd_comp_mgr_unmap_notify;
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
  HdTaskNavigator      *task_nav;
  ClutterActor         *stage;
  ClutterActor         *arena;
  gint                  i;

  priv = hmgr->priv = g_new0 (HdCompMgrPrivate, 1);

  hd_atoms_init (wm->xdpy, priv->atoms);

  priv->dbus_connection = hd_dbus_init (hmgr);

  hd_gtk_style_init ();

  stage = clutter_stage_get_default ();

  /*
   * Create the home group before the switcher, so the switcher can
   * connect it's signals to it.
   */
  priv->home = g_object_new (HD_TYPE_HOME, "comp-mgr", cmgr, NULL);

  clutter_actor_set_reactive (priv->home, TRUE);

  g_signal_connect_swapped (priv->home, "background-clicked",
                            G_CALLBACK (hd_comp_mgr_home_clicked),
                            cmgr);

  clutter_actor_show (priv->home);

  task_nav = hd_task_navigator_new ();

  priv->render_manager = hd_render_manager_create(hmgr,
		                                  hd_launcher_get(),
		                                  hd_launcher_get_group(),
		                                  HD_HOME(priv->home),
						  task_nav);
  clutter_actor_set_size (CLUTTER_ACTOR(priv->render_manager),
                          wm->xdpy_width, wm->xdpy_height);
  clutter_container_add_actor(CLUTTER_CONTAINER (stage),
                              CLUTTER_ACTOR(priv->render_manager));

  /* NB -- home must be constructed before constructing the switcher;
   */
  priv->switcher_group = g_object_new (HD_TYPE_SWITCHER,
				       "comp-mgr", cmgr,
				       "task-nav", task_nav,
				       NULL);

  /* FIXME: shouldn't this be in render manager? */
  hd_home_fixup_operator_position (HD_HOME(priv->home));

  /* Take our comp-mgr-clutter's 'arena' and hide it for now. We
   * don't want anything in it visible, as we'll take out what we want to
   * render. */
  arena = mb_wm_comp_mgr_clutter_get_arena(MB_WM_COMP_MGR_CLUTTER(cmgr));
  if (arena)
    {
      clutter_actor_hide(arena);
    }

  /*
   * Create a hash table for hibernating windows.
   */
  priv->hibernating_apps =
    g_hash_table_new_full (g_int_hash,
			   g_int_equal,
			   NULL,
			   (GDestroyNotify)mb_wm_object_unref);

  priv->tasks_button_blockers = g_hash_table_new (NULL, NULL);

  for (i = 0; i < 4; ++i)
    priv->applet_manager[i] = hd_applet_layout_manager_new ();

  hd_render_manager_set_state(HDRM_STATE_HOME);

  return 1;
}

static void
hd_comp_mgr_destroy (MBWMObject *obj)
{
  HdCompMgrPrivate * priv = HD_COMP_MGR (obj)->priv;

  if (priv->hibernating_apps)
    g_hash_table_destroy (priv->hibernating_apps);
  g_object_unref( priv->render_manager );
}

void
hd_comp_mgr_setup_input_viewport (HdCompMgr *hmgr, ClutterGeometry *geom,
                                  int count)
{
  XserverRegion      region;
  Window             overlay;
  Window             clutter_window;
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
                KeyPressMask | KeyReleaseMask |
                PointerMotionMask );

  g_debug("%s: setting viewport", __FUNCTION__);
  if (count > 0)
    {
      XRectangle *rectangle = g_new (XRectangle, count);
      guint      i;
      for (i = 0; i < count; i++)
        {
          rectangle[i].x      = geom[i].x;
          rectangle[i].y      = geom[i].y;
          rectangle[i].width  = geom[i].width;
          rectangle[i].height = geom[i].height;
          g_debug("%s: region %d, %d, %d, %d", __FUNCTION__,
              geom[i].x, geom[i].y, geom[i].width, geom[i].height);
        }
      region = XFixesCreateRegion (wm->xdpy, rectangle, count);
      g_free (rectangle);
    }
  else
    region = XFixesCreateRegion (wm->xdpy, NULL, 0);

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
                KeyPressMask | KeyReleaseMask |
                PointerMotionMask);

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
  MBWMCompMgrClass * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));

  /*
   * The parent class turn_on method deals with setting up the input shape on
   * the overlay window; so we first call it, and then change the shape to
   * suit our custom needs.
   */
  if (parent_klass->turn_on)
    parent_klass->turn_on (mgr);

}


static void
hd_comp_mgr_register_client (MBWMCompMgr           * mgr,
			     MBWindowManagerClient * c)
{
  HdCompMgrPrivate              * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClass              * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));

  g_debug ("%s, c=%p ctype=%d", __FUNCTION__, c, MB_WM_CLIENT_CLIENT_TYPE (c));
  if (MB_WM_CLIENT_CLIENT_TYPE (c) == MBWMClientTypeDesktop)
    {
      priv->desktop = c;
      return;
    }

  if (parent_klass->register_client)
    parent_klass->register_client (mgr, c);
}


static void
hd_comp_mgr_unregister_client (MBWMCompMgr *mgr, MBWindowManagerClient *c)
{
  ClutterActor                  * actor;
  HdCompMgrPrivate              * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClass              * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));
  MBWMCompMgrClutterClient      * cclient =
    MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);
  HdCompMgrClient               * hclient = HD_COMP_MGR_CLIENT (c->cm_client);

  g_debug ("%s, c=%p ctype=%d", __FUNCTION__, c, MB_WM_CLIENT_CLIENT_TYPE (c));
  actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);

  if (g_hash_table_remove (priv->tasks_button_blockers, c)
      && g_hash_table_size (priv->tasks_button_blockers) == 0)
    { /* Last system modal dialog being unmapped, undo evil. */
      hd_render_manager_set_reactive(TRUE);
      hd_render_manager_set_blur_app(FALSE);
    }

  if (hclient->priv->hibernating)
    {
      /*
       * We want to hold onto the CM client object, so we can continue using
       * the actor.
       */
      mb_wm_object_ref (MB_WM_OBJECT (cclient));

      g_hash_table_insert (priv->hibernating_apps,
			   & hclient->priv->hibernation_key,
			   hclient);

      hd_switcher_hibernate_window_actor (priv->switcher_group,
					  actor);
    }
  /*
   * If the actor is an application, remove it also to the switcher
   */
  else if (MB_WM_CLIENT_CLIENT_TYPE (c) == MBWMClientTypeApp)
    {
      HdApp *app = HD_APP(c);
      MBWMCompMgrClutterClient * prev = NULL;

      if (actor)
        {
          /* handle hildon-stackable-window */
          if (HD_IS_APP (app))
            {
              /* if we are secondary, there must be leader and probably even followers */
              if (app->leader && app->secondary_window)
                {
                  /* show the topmost follower and replace switcher actor for the stackable */
                  prev = MB_WM_COMP_MGR_CLUTTER_CLIENT ((hd_app_get_prev_group_member(app))->cm_client);

                  clutter_actor_show (mb_wm_comp_mgr_clutter_client_get_actor(prev));

                  hd_switcher_replace_window_actor (priv->switcher_group, actor,
                                                    mb_wm_comp_mgr_clutter_client_get_actor(prev));
                }
              else if (!(c->window->ewmh_state & MBWMClientWindowEWMHStateSkipTaskbar))
                /* We are the leader, just remove actor from switcher.
                 * NOTE The test above breaks if the client changed
                 * the flag after it's been mapped. */
                hd_switcher_remove_window_actor (priv->switcher_group,
                                                 actor);
            }

          g_object_set_data (G_OBJECT (actor),
                             "HD-MBWMCompMgrClutterClient", NULL);
        }
      else
        /* We wasn't mapped in the first place. */;
    }
  else if (MB_WM_CLIENT_CLIENT_TYPE (c) == HdWmClientTypeStatusArea)
    {
      ClutterActor  *sa;

      sa = mb_wm_comp_mgr_clutter_client_get_actor (cclient);
      hd_home_remove_status_area (HD_HOME (priv->home), sa);

      priv->status_area_client = NULL;
    }
  else if (MB_WM_CLIENT_CLIENT_TYPE (c) == HdWmClientTypeStatusMenu)
    {
      ClutterActor  *sa;

      sa = mb_wm_comp_mgr_clutter_client_get_actor (cclient);
      hd_home_remove_status_menu (HD_HOME (priv->home), sa);
    }
  else if (MB_WM_CLIENT_CLIENT_TYPE (c) == HdWmClientTypeHomeApplet)
    {
      ClutterActor  *applet;
      MBGeometry     geom;
      HdHomeApplet  *happlet = HD_HOME_APPLET (c);
      gint           view_id = happlet->view_id;
      gint           layer   = happlet->applet_layer;

      if (view_id < 0)
	{
	  /* FIXME -- handle sticky applets */
	  view_id = 0;
	}

      mb_wm_client_get_coverage (c, &geom);
      hd_applet_layout_manager_reclaim_geometry (priv->applet_manager[view_id],
						 layer, &geom);

      applet = mb_wm_comp_mgr_clutter_client_get_actor (cclient);

      hd_home_remove_applet (HD_HOME (priv->home), applet);
    }
  /* Dialogs and Notes (including notifications) have already been dealt
   * with in hd_comp_mgr_effect().  This is because by this time we don't
   * have information about transiency. */

  if (parent_klass->unregister_client)
    parent_klass->unregister_client (mgr, c);
}

/* Returns the client @c is transient for.  Some clients (notably menus)
 * don't have their c->transient_for field set even though they are
 * transient.  Figure it out from the c->window in this case. */
static MBWindowManagerClient *
hd_comp_mgr_get_client_transient_for (MBWindowManagerClient *c)
{
  Window xtransfor;

  if (c->transient_for)
    return c->transient_for;

  xtransfor = c->window->xwin_transient_for;
  return xtransfor && xtransfor != c->window->xwindow
      && xtransfor != c->wmref->root_win->xwindow
    ? mb_wm_managed_client_from_xwindow (c->wmref, xtransfor)
    : NULL;
}

static void
hd_comp_mgr_texture_update_area(ClutterActor* actor,
                                int x, int y, int width, int height)
{
  ClutterFixed offsetx = 0, offsety = 0;
  ClutterActor *parent, *it;

  if (!CLUTTER_IS_ACTOR(actor) || !CLUTTER_ACTOR_IS_VISIBLE(actor))
    return;

  /* TFP textures are usually bundled into another group, and it is
   * this group that sets visibility - so we must check it too */
  parent = clutter_actor_get_parent(actor);
  if (parent && !CLUTTER_ACTOR_IS_VISIBLE(parent))
    return;

  /* We DON'T do this if we're in the task switcher, because it
   * breaks all the scaling + has a scroller that's a nightmare
   * to deal with */
  if (hd_render_manager_get_state() == HDRM_STATE_TASK_NAV)
    return;

  /* Assume no zoom/rotate is happening here as we have simple windows */
  it = actor;
  while (it && !CLUTTER_IS_STAGE(it))
    {
      ClutterFixed px,py;
      clutter_actor_get_positionu(it, &px, &py);
      offsetx += px;
      offsety += py;
      it = clutter_actor_get_parent(it);
    }

  {
    ClutterActor *stage = clutter_actor_get_stage(actor);
    ClutterGeometry area = {x + CLUTTER_FIXED_TO_INT(offsetx),
                            y + CLUTTER_FIXED_TO_INT(offsety),
                            width, height};

    /*g_debug("%s: UPDATE %d, %d, %d, %d", __FUNCTION__,
            area.x, area.y, area.width, area.height);*/

    /* Queue a redraw, but without updating the whole area */
    clutter_actor_queue_redraw_damage(stage);
    clutter_stage_set_damaged_area(stage, area);
  }
}

/* Hook onto and X11 texture pixmap children of this actor */
static void
hd_comp_mgr_hook_update_area(ClutterActor *actor)
{
  if (CLUTTER_IS_GROUP(actor))
    {
      gint i;
      gint n = clutter_group_get_n_children(CLUTTER_GROUP(actor));

      for (i=0;i<n;i++)
        {
          ClutterActor *child =
              clutter_group_get_nth_child(CLUTTER_GROUP(actor), i);
          if (CLUTTER_X11_IS_TEXTURE_PIXMAP(child))
            {
              g_signal_connect(G_OBJECT(child), "update-area",
                             G_CALLBACK(hd_comp_mgr_texture_update_area), 0);
              clutter_actor_set_allow_redraw(child, FALSE);
            }
        }
    }
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
  MBWMClientType             ctype;

  g_debug ("%s, c=%p ctype=%d", __FUNCTION__, c, MB_WM_CLIENT_CLIENT_TYPE (c));
  /*
   * Parent class map_notify creates the actor representing the client.
   */
  if (MB_WM_CLIENT_CLIENT_TYPE (c) == MBWMClientTypeDesktop)
    return;

  if (parent_klass->map_notify)
    parent_klass->map_notify (mgr, c);

  /* Hide the Edit button if it is currently shown */
  if (priv->home)
    hd_home_hide_edit_button (HD_HOME (priv->home));

  /* For some reason adding an applet sets focus back to
   * the top application, so we need put the desktop back
   * in the correct order. */
  if (STATE_NEED_DESKTOP(hd_render_manager_get_state()))
    {
      mb_wm_handle_show_desktop(mgr->wm, TRUE);
    }

  /*
   * If the actor is an appliation, add it also to the switcher
   * If it is Home applet, add it to the home
   */
  ctype = MB_WM_CLIENT_CLIENT_TYPE (c);

  cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);
  actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);

  g_object_set_data (G_OBJECT (actor),
		     "HD-MBWMCompMgrClutterClient", cclient);

  hd_comp_mgr_hook_update_area(actor);

  /* deactivate launcher and switcher in case of new window */
  if (STATE_ONE_OF(hd_render_manager_get_state(),
                   HDRM_STATE_LAUNCHER | HDRM_STATE_TASK_NAV)
      && !(ctype == MBWMClientTypeNote
           && HD_NOTE (c)->note_type == HdNoteTypeIncomingEvent)
      && ctype != HdWmClientTypeHomeApplet)
  {
    hd_render_manager_set_state(HDRM_STATE_HOME);
  }

  /* Do evil.  Do we need to block the Tasks button? */
  if ((ctype == MBWMClientTypeDialog ||
      (ctype == MBWMClientTypeNote &&
       HD_NOTE (c)->note_type != HdNoteTypeIncomingEvent))
      && hd_util_is_client_system_modal (c))
    {
      if (g_hash_table_size (priv->tasks_button_blockers) == 0)
        { /* First system modal client, allow it to receive events and
           * block the switcher buttons. */
          hd_render_manager_set_reactive(FALSE);
        }

      /*
       * Save the client's address and undo evil when the last of its
       * kind is unregistered.  We need to do this way because maps
       * and unmaps arrive unreliable, for exampe we get not unmap
       * notification for VKB.  In other cases we may receive two
       * maps for the same client.
       */
      g_hash_table_insert (priv->tasks_button_blockers, c, GINT_TO_POINTER(1));
    }

  if (ctype == HdWmClientTypeHomeApplet)
    {
      HdHomeApplet * applet  = HD_HOME_APPLET (c);
      char         * applet_id = applet->applet_id;

      g_object_set_data_full (G_OBJECT (actor), "HD-applet-id",
                              g_strdup (applet_id), (GDestroyNotify) g_free);

      hd_home_add_applet (HD_HOME (priv->home), actor);
      return;
    }
  else if (ctype == HdWmClientTypeStatusArea)
    {
      hd_home_add_status_area (HD_HOME (priv->home), actor);

      priv->status_area_client = c;
      return;
    }
  else if (ctype == HdWmClientTypeStatusMenu)
    {
      hd_home_add_status_menu (HD_HOME (priv->home), actor);
      return;
    }
  else if (ctype == HdWmClientTypeAppMenu)
    {
      /* If we're in a state that needs a grab,
       * go back to home as the dialog will need the grab. */
      if (STATE_NEED_GRAB(hd_render_manager_get_state()))
        hd_render_manager_set_state(HDRM_STATE_HOME);
    }
  else if (ctype == MBWMClientTypeNote)
    {
      if (HD_NOTE (c)->note_type == HdNoteTypeIncomingEvent)
        {
          /* Unparent @actor from its desktop and leave it
           * up to the swithcer to show it wherever it wants. */
          ClutterActor *parent = clutter_actor_get_parent (actor);
          clutter_container_remove_actor (CLUTTER_CONTAINER (parent), actor);
          hd_switcher_add_notification (priv->switcher_group,
                                        HD_NOTE (c));
        }
      else if (c->transient_for)
        hd_switcher_add_dialog (priv->switcher_group, c, actor);
      else
        {
          /* Notes need to be pulled out right infront of the blur group
           * manually, as they are not given focus */
          hd_render_manager_add_to_front_group(actor);
        }
      return;
    }
  else if (ctype == MBWMClientTypeDialog)
    {
      /* if this is a dialog and we're in a state that needed grab,
       * go back to home as the dialog will need the grab. */
      if (STATE_NEED_GRAB(hd_render_manager_get_state()))
        hd_render_manager_set_state(HDRM_STATE_HOME);

      if (c->transient_for)
        hd_switcher_add_dialog (priv->switcher_group, c, actor);
      return;
    }
  else if (c->window->net_type == c->wmref->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_POPUP_MENU])
    {
      MBWindowManagerClient *transfor;

      if ((transfor = hd_comp_mgr_get_client_transient_for (c)) != NULL)
          hd_switcher_add_dialog_explicit (HD_SWITCHER (priv->switcher_group),
                                           c, actor, transfor);
      return;
    }
  else if (c->window->net_type == c->wmref->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_POPUP_MENU])
    {
      MBWindowManagerClient *transfor;

      if ((transfor = hd_comp_mgr_get_client_transient_for (c)) != NULL)
          hd_switcher_add_dialog_explicit (HD_SWITCHER (priv->switcher_group),
                                           c, actor, transfor);
      return;
    }
  else if (ctype != MBWMClientTypeApp)
    return;

  hclient = HD_COMP_MGR_CLIENT (cclient);
  hkey = hclient->priv->hibernation_key;

  hclient_h = g_hash_table_lookup (priv->hibernating_apps, &hkey);

  if (hclient_h)
    {
      ClutterActor             * actor_h;
      MBWMCompMgrClutterClient * cclient_h;

      cclient_h = MB_WM_COMP_MGR_CLUTTER_CLIENT (hclient_h);

      actor_h = mb_wm_comp_mgr_clutter_client_get_actor (cclient_h);

      hd_switcher_replace_window_actor (priv->switcher_group,
					actor_h, actor);

      g_hash_table_remove (priv->hibernating_apps, &hkey);
    }
  else
    {
      HdApp *app = HD_APP(c);

      if (HD_IS_APP (app))
      {
        if (app->secondary_window && app->leader)
          {
            ClutterActor *top_actor;
            MBWMCompMgrClutterClient *top;

            if (!app->leader->followers->next)
              { /* First secondary window, the top is the leader. */
                top = MB_WM_COMP_MGR_CLUTTER_CLIENT (MB_WM_CLIENT (app->leader)->cm_client);
                top_actor = mb_wm_comp_mgr_clutter_client_get_actor (top);
                hd_switcher_replace_window_actor (priv->switcher_group,
                                                  top_actor, actor);
                clutter_actor_hide (top_actor);
              }
            else
              { /* Subsequenct secondary, the top is the last of the followers. */
                GList *l;

                /* Hide the followers and replace the last one preceeding us
                 * with outselfves in the switcher. */
                for (l = app->leader->followers; l && l->next; l = l->next)
                  {
                    g_return_if_fail (l != NULL);
                    top = MB_WM_COMP_MGR_CLUTTER_CLIENT (MB_WM_CLIENT (l->data)->cm_client);
                    top_actor = mb_wm_comp_mgr_clutter_client_get_actor (top);
                    clutter_actor_hide (top_actor);
                    g_return_if_fail(l->next != NULL);
                    if (l->next->data == app)
                      { /* We should be the last of the followers. */
                        hd_switcher_replace_window_actor (priv->switcher_group,
                                                          top_actor, actor);
                        g_return_if_fail (!l->next->next);
                        break;
                      }
                  }
              }
          }
        else if (!(c->window->ewmh_state & MBWMClientWindowEWMHStateSkipTaskbar))
          {
            /* Taskbar == task switcher in our case.
             * Introduced for systemui. */
            hd_switcher_add_window_actor (priv->switcher_group, actor);
            /* and make sure we're in app mode as we'll want to show this
             * new app */
            if (!STATE_IS_APP(hd_render_manager_get_state()))
                hd_render_manager_set_state(HDRM_STATE_APP);
          }
      }
    }
}

static void
hd_comp_mgr_unmap_notify (MBWMCompMgr *mgr, MBWindowManagerClient *c)
{
  HdCompMgrPrivate          * priv = HD_COMP_MGR (mgr)->priv;
  MBWMClientType            c_type = MB_WM_CLIENT_CLIENT_TYPE (c);
  MBWMCompMgrClutterClient *cclient;
  MBWindowManagerClient    *transfor = 0;

  cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (c->cm_client);
  if (HD_IS_NOTE (c) && HD_NOTE (c)->note_type == HdNoteTypeIncomingEvent)
    {
      hd_switcher_remove_notification (priv->switcher_group,
                                       HD_NOTE (c));
      return;
    }
  else if (c_type == MBWMClientTypeNote || c_type == MBWMClientTypeDialog)
    transfor = c->transient_for;
  else if (c->window->net_type == c->wmref->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_POPUP_MENU])
    transfor = hd_comp_mgr_get_client_transient_for (c);
  else
    return;

  if (transfor)
    { /* Remove application-transient dialogs from the switcher. */
      ClutterActor *actor;
      actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);
      if (actor)
        hd_switcher_remove_dialog (priv->switcher_group, actor);
    }
}


static void
hd_comp_mgr_effect (MBWMCompMgr                *mgr,
                    MBWindowManagerClient      *c,
                    MBWMCompMgrClientEvent      event)
{
  HdCompMgr      *hmgr = HD_COMP_MGR(mgr);
  MBWMClientType c_type = MB_WM_CLIENT_CLIENT_TYPE (c);
  g_debug ("%s, c=%p ctype=%d event=%d",
            __FUNCTION__, c, MB_WM_CLIENT_CLIENT_TYPE (c), event);

  /*HdCompMgrPrivate *priv = HD_COMP_MGR (mgr)->priv;*/
  if (event == MBWMCompMgrClientEventUnmap)
    {
      if (c_type == MBWMClientTypeDialog ||
          c_type == MBWMClientTypeNote ||
          c_type == HdWmClientTypeStatusMenu ||
          c_type == HdWmClientTypeAppMenu ||
          c_type == MBWMClientTypeMenu
          )
        {
          if (BLUR_FOR_WINDOW(c))
            hd_render_manager_set_blur_app(FALSE);
        }

      if (c_type == HdWmClientTypeStatusMenu)
        hd_transition_popup(hmgr, c, MBWMCompMgrClientEventUnmap);
      else if (HD_IS_NOTE(c)  &&
               HD_NOTE(c)->note_type == HdNoteTypeIncomingEventPreview)
        hd_transition_popup(hmgr, c, MBWMCompMgrClientEventUnmap);
      else if (c_type == MBWMClientTypeDialog)
        hd_transition_popup(hmgr, c, MBWMCompMgrClientEventUnmap);
      else if (c_type == MBWMClientTypeNote)
        hd_transition_fade(hmgr, c, MBWMCompMgrClientEventUnmap);
      else if (c_type == MBWMClientTypeApp)
        {
          hd_transition_close_app (hmgr, c);
        }

      /* after the effect, restack */
      hd_comp_mgr_restack(mgr);
    }
  else if (event == MBWMCompMgrClientEventMap)
    {
      /* before the effect, restack */
      hd_comp_mgr_restack(mgr);

      if (c_type == MBWMClientTypeDialog ||
          c_type == MBWMClientTypeNote ||
          c_type == HdWmClientTypeStatusMenu ||
          c_type == HdWmClientTypeAppMenu ||
          c_type == MBWMClientTypeMenu)
        {
          if (BLUR_FOR_WINDOW(c))
            hd_render_manager_set_blur_app(TRUE);
        }

      if (c_type == HdWmClientTypeStatusMenu)
        hd_transition_popup(hmgr, c, MBWMCompMgrClientEventMap);
      else if (c_type == MBWMClientTypeDialog)
        hd_transition_popup(hmgr, c, MBWMCompMgrClientEventMap);
      else if (HD_IS_NOTE(c) &&
               HD_NOTE(c)->note_type == HdNoteTypeIncomingEventPreview)
        hd_transition_popup(hmgr, c, MBWMCompMgrClientEventMap);
      else if (c_type == MBWMClientTypeNote)
        hd_transition_fade(hmgr, c, MBWMCompMgrClientEventMap);
      else if (c_type == MBWMClientTypeApp)
        {
          /* Look if it's a stackable window. */
          HdApp *app = HD_APP (c);
          if (app->secondary_window)
            /* FIXME: Transitions. */
            g_debug ("%s: Mapping secondary app window.\n", __FUNCTION__);
          /* We're now showing this app, so remove our app
           * starting screen if we had one */
          hd_launcher_window_created();
        }
    }
}

void
hd_comp_mgr_restack (MBWMCompMgr * mgr)
{
  HdCompMgrPrivate         * priv = HD_COMP_MGR (mgr)->priv;
  MBWMCompMgrClass         * parent_klass =
    MB_WM_COMP_MGR_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(mgr)));
  MBWindowManagerClient    * highest_fs;
  HdTaskNavigator          *tn;

  /* g_debug ("%s", __FUNCTION__); */

  tn = HD_TASK_NAVIGATOR (
        hd_switcher_get_task_navigator (priv->switcher_group));
  highest_fs = mb_wm_stack_get_highest_full_screen (mgr->wm);

  /* Hide the Edit button if it is currently shown */
  if (priv->home)
    hd_home_hide_edit_button (HD_HOME (priv->home));

  /*
   * We use the parent class restack() method to do the stacking, but as our
   * switcher shares actors with the CM, we cannot run this when the switcher
   * is showing, or an unmap effect is in progress; instead we set a flag, and
   * let the switcher request stack sync when it closes.
   */

  if (priv->unmap_effect_running ||
      STATE_NEED_TASK_NAV(hd_render_manager_get_state()))
    {
      priv->stack_sync = TRUE;
    }
  else
    {
      if (parent_klass->restack)
	parent_klass->restack (mgr);

      /*MBWindowManager *wm = mgr->wm;
      MBWindowManagerClient *c = wm->stack_bottom;
      int i = 0;
      while (c)
        {
          ClutterActor *a = 0;

          if (c->cm_client)
            a = mb_wm_comp_mgr_clutter_client_get_actor(MB_WM_COMP_MGR_CLUTTER_CLIENT(c->cm_client));
          g_debug("%s: STACK %d : %s %s %s", __FUNCTION__, i,
              c->name?c->name:"?",
              (a && clutter_actor_get_name(a)) ?  clutter_actor_get_name(a) : "?",
              (wm->desktop==c) ? "DESKTOP" : "");
          i++;
          c = c->stacked_above;
        }*/

      hd_render_manager_restack();
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
      if (!priv->unmap_effect_running)
        {
          priv->stack_sync = FALSE;
          hd_comp_mgr_restack (MB_WM_COMP_MGR (hmgr));
        }
      else
        g_debug("%s: Called, but effects running = %d",
                __FUNCTION__, priv->unmap_effect_running);
    }
}

static void
hd_comp_mgr_home_clicked (HdCompMgr *hmgr, ClutterActor *actor)
{
  hd_render_manager_set_state(HDRM_STATE_HOME);
}

/*
 * Shuts down a client, handling hibernated applications correctly.
 * if @close_all and @cc is associated with a window stack then
 * close all windows in the stack, otherwise only @cc's.
 */
void
hd_comp_mgr_close_app (HdCompMgr *hmgr, MBWMCompMgrClutterClient *cc,
                       gboolean close_all)
{
  HdCompMgrPrivate      * priv = hmgr->priv;
  HdCompMgrClient       * h_client = HD_COMP_MGR_CLIENT (cc);

  g_return_if_fail (cc != NULL);
  if (h_client->priv->hibernating)
    {
      ClutterActor * actor;

      actor = mb_wm_comp_mgr_clutter_client_get_actor (cc);

      hd_switcher_remove_window_actor (priv->switcher_group,
				       actor);

      mb_wm_object_unref (MB_WM_OBJECT (cc));
    }
  else
    {
      MBWindowManagerClient * c = MB_WM_COMP_MGR_CLIENT (cc)->wm_client;

      if (close_all && HD_IS_APP (c) && HD_APP (c)->secondary_window
          && HD_APP (c)->leader)
        {
          c = MB_WM_CLIENT (HD_APP (c)->leader);
          hd_app_close_followers (HD_APP (c));
          mb_wm_client_deliver_delete (c);
        }
      else /* Either primary or a secondary who's lost its leader. */
        mb_wm_client_deliver_delete (c);
    }
}

void
hd_comp_mgr_close_client (HdCompMgr *hmgr, MBWMCompMgrClutterClient *cc)
{
  hd_comp_mgr_close_app (hmgr, cc, FALSE);
}

void
hd_comp_mgr_hibernate_client (HdCompMgr *hmgr,
			      MBWMCompMgrClutterClient *cc,
			      gboolean force)
{
  MBWMCompMgrClient * c  = MB_WM_COMP_MGR_CLIENT (cc);
  HdCompMgrClient   * hc = HD_COMP_MGR_CLIENT (cc);

  if (!force && !(hc->priv->can_hibernate))
    return;

  mb_wm_comp_mgr_clutter_client_set_flags (cc,
					   MBWMCompMgrClutterClientDontUpdate);

  hc->priv->hibernating = TRUE;

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
  gchar            *service = NULL;
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

Atom
hd_comp_mgr_get_atom (HdCompMgr *hmgr, HdAtoms id)
{
  HdCompMgrPrivate *priv = hmgr->priv;

  if (id >= _HD_ATOM_LAST)
    return (Atom) 0;

  return priv->atoms[id];
}

ClutterActor *
hd_comp_mgr_get_home (HdCompMgr *hmgr)
{
  HdCompMgrPrivate *priv = hmgr->priv;

  return priv->home;
}

GObject *
hd_comp_mgr_get_switcher (HdCompMgr *hmgr)
{
  HdCompMgrPrivate *priv = hmgr->priv;

  return G_OBJECT(priv->switcher_group);
}

gint
hd_comp_mgr_get_current_home_view_id (HdCompMgr *hmgr)
{
  HdCompMgrPrivate *priv = hmgr->priv;

  return hd_home_get_current_view_id (HD_HOME (priv->home));
}

MBWindowManagerClient *
hd_comp_mgr_get_desktop_client (HdCompMgr *hmgr)
{
  HdCompMgrPrivate *priv = hmgr->priv;

  return priv->desktop;
}

gint
hd_comp_mgr_request_home_applet_geometry (HdCompMgr  *hmgr,
					  gint        view_id,
					  MBGeometry *geom)
{
  HdCompMgrPrivate *priv = hmgr->priv;
  gint              layer;

  if (view_id < 0)
    {
      /*
       * FIXME -- what is to happen to sticky applets ?
       */
      view_id = 0;
    }

  /*
   * We support 4 views only
   */
  g_assert (view_id < 4);

  layer =
    hd_applet_layout_manager_request_geometry (priv->applet_manager[view_id],
					       geom);

  return layer;
}

gint
hd_comp_mgr_get_home_applet_layer_count (HdCompMgr *hmgr, gint view_id)
{
  HdCompMgrPrivate *priv = hmgr->priv;
  gint              count;

  if (view_id < 0)
    {
      /*
       * FIXME -- what is to happen to sticky applets ?
       */
      view_id = 0;
    }

  /*
   * We support 4 views only
   */
  g_assert (view_id < 4);

  count =
    hd_applet_layout_manager_get_layer_count (priv->applet_manager[view_id]);

  return count;
}

static void
dump_clutter_actor_tree (ClutterActor *actor, GString *indent)
{
  const gchar *name;
  MBWMCompMgrClient *cmgrc;

  if (!indent)
    indent = g_string_new ("");

  if (!(name = clutter_actor_get_name (actor)) && CLUTTER_IS_LABEL (actor))
    name = clutter_label_get_text (CLUTTER_LABEL (actor));
  cmgrc = g_object_get_data(G_OBJECT (actor), "HD-MBWMCompMgrClutterClient");

  g_debug ("actor[%u]: %s%p (type=%s, name=%s, win=0x%lx), "
           "mapped: %d, realized: %d, reactive: %d",
           indent->len, indent->str, actor,
           G_OBJECT_TYPE_NAME (actor), name,
           cmgrc && cmgrc->wm_client && cmgrc->wm_client->window
               ? cmgrc->wm_client->window->xwindow : 0,
           CLUTTER_ACTOR_IS_MAPPED (actor)   != 0,
           CLUTTER_ACTOR_IS_REALIZED (actor) != 0,
           CLUTTER_ACTOR_IS_REACTIVE (actor) != 0);
  if (CLUTTER_IS_CONTAINER (actor))
    {
      g_string_append_c (indent, ' ');
      clutter_container_foreach (CLUTTER_CONTAINER (actor),
                                 (ClutterCallback)dump_clutter_actor_tree,
                                 indent);
      g_string_truncate (indent, indent->len-1);
    }
}

void
hd_comp_mgr_dump_debug_info (const gchar *tag)
{
  int revert;
  Window focus;
  MBWMRootWindow *root;
  MBWindowManagerClient *mbwmc;

  if (tag)
    g_debug ("%s", tag);

  g_debug ("Windows:");
  root = mb_wm_root_window_get (NULL);
  mb_wm_stack_enumerate_reverse (root->wm, mbwmc)
    g_debug (" client=%p, type=%d, trfor=%p, win=0x%lx, group=0x%lx, name=%s",
             mbwmc, MB_WM_CLIENT_CLIENT_TYPE (mbwmc), mbwmc->transient_for,
             mbwmc && mbwmc->window ? mbwmc->window->xwindow : 0,
             mbwmc && mbwmc->window ? mbwmc->window->xwin_group : 0,
             mbwmc && mbwmc->window ? mbwmc->window->name : "<unset>");
  mb_wm_object_unref (MB_WM_OBJECT (root));

  XGetInputFocus (clutter_x11_get_default_display (), &focus, &revert);
  g_debug ("input focus: 0x%lx", focus);
  if (revert == RevertToParent)
    g_debug ("input focus reverts to parent");
  else if (revert == RevertToPointerRoot)
    g_debug ("input focus reverts to pointer root");
  else if (revert == RevertToNone)
    g_debug ("input focus reverts to none");
  else
    g_debug ("input focus reverts to %d", revert);

  dump_clutter_actor_tree (clutter_stage_get_default (), NULL);
}

void
hd_comp_mgr_set_status_area_stacking(HdCompMgr *hmgr, gboolean visible)
{
  HdCompMgrPrivate         * priv = hmgr->priv;

  g_debug ("%s: %s", __FUNCTION__, visible ? "MID": "BOTTOM");

  if (priv->status_area_client)
    {
      /* This needs to be TopMid to clear the desktop window that
       * HdHome creates */
      priv->status_area_client->stacking_layer =
          visible ? MBWMStackLayerTopMid : MBWMStackLayerBottom;
      /* and make sure we restack now... */
      mb_wm_client_stacking_mark_dirty(priv->status_area_client);
    }
}

void hd_comp_mgr_set_effect_running(HdCompMgr *hmgr, gboolean running)
{
  if (!HD_IS_COMP_MGR(hmgr))
    return;

  HdCompMgrPrivate         * priv = hmgr->priv;
  if (running)
    {
      priv->unmap_effect_running++;
    }
  else
    {
      priv->unmap_effect_running--;
      if (priv->unmap_effect_running == 0)
        hd_comp_mgr_sync_stacking(hmgr);
      if (priv->unmap_effect_running < 0)
        g_warning("%s: unmap_effect_running < 0", __FUNCTION__);
    }
}
