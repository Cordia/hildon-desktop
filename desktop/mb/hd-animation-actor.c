/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Tomas Frydrych <tf@o-hand.com>
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

#include "hd-animation-actor.h"
#include "hd-comp-mgr.h"
#include "hd-wm.h"

#include <sys/time.h>
#include <time.h>

#define CLIENT_MESSAGE_DEBUG 0//1

#if CLIENT_MESSAGE_DEBUG == 1
#    define CM_DEBUG(format, args...) g_debug(format, ## args)
#else
#    define CM_DEBUG(format, args...)
#endif

static Atom show_atom;
static Atom position_atom;
static Atom rotation_atom;
static Atom scale_atom;
static Atom anchor_atom;
static Atom ready_atom;
static Atom parent_atom;

static gboolean atoms_initialized = 0;

void
hd_animation_actor_show (MBWindowManagerClient *client);
static Bool
hd_animation_actor_request_geometry (MBWindowManagerClient *client,
				     MBGeometry            *new_geometry,
				     MBWMClientReqGeomType  flags);

static void
hd_animation_actor_client_message (XClientMessageEvent *xev, void *userdata)
{
  HdAnimationActor         *self = HD_ANIMATION_ACTOR (userdata);
  MBWindowManagerClient    *client = MB_WM_CLIENT (self);

  if (!client->window)
  {
      g_warning ("Stray client message: no window!\n");
      return;
  }

  MBWMCompMgrClutterClient *cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (client->cm_client);
  ClutterActor             *actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);

  if (!CLUTTER_IS_ACTOR(actor))
  {
      g_warning ("Stray client message: no actor!\n");
      return;
  }

  if (xev->message_type == show_atom)
  {
      gboolean show = (gboolean) xev->data.l[0];
      guint    opacity = (guint) xev->data.l[1] & 0xff;

      CM_DEBUG ("AnimationActor %p: show(show=%d, opacity=%d)\n",
	       	self, show, opacity);

      self->show = show;

      if (show)
          clutter_actor_show (actor);
      else
          clutter_actor_hide (actor);

      clutter_actor_set_opacity (actor, opacity);

  }
  else if (xev->message_type == position_atom)
  {
      gint x = (gint) xev->data.l[0];
      gint y = (gint) xev->data.l[1];
      gint depth = (gint) xev->data.l[2];

      CM_DEBUG ("AnimationActor %p: position(x=%d, y=%d, depth=%d)\n",
	       	self, x, y, depth);
      clutter_actor_set_position (actor, x, y);
      clutter_actor_set_depth (actor, depth);
  }
  else if (xev->message_type == rotation_atom)
  {
      guint  axis    = (guint)  xev->data.l[0];
      gint32 degrees = (gint32) xev->data.l[1];
      gint   x       = (gint)   xev->data.l[2];
      gint   y       = (gint)   xev->data.l[3];
      gint   z       = (gint)   xev->data.l[4];

      CM_DEBUG ("AnimationActor %p: rotation(axis=%d, deg=%d, x=%d, y=%d, z=%d)\n",
               self, axis, degrees, x, y, z);

      ClutterRotateAxis clutter_axis;
      switch (axis) {
          case 0: clutter_axis = CLUTTER_X_AXIS; break;
          case 1: clutter_axis = CLUTTER_Y_AXIS; break;
          default: clutter_axis = CLUTTER_Z_AXIS;
      }

      clutter_actor_set_rotationx (actor,
                                   clutter_axis,
                                   degrees,
                                   x, y, z);
  }
  else if (xev->message_type == scale_atom)
  {
      gint32 x_scale = (gint32) xev->data.l[0];
      gint32 y_scale = (gint32) xev->data.l[1];

      CM_DEBUG ("AnimationActor %p: scale(x_scale=%u, y_scale=%u)\n",
	       self, x_scale, y_scale);
      clutter_actor_set_scalex (actor, x_scale, y_scale);
  }
  else if (xev->message_type == anchor_atom)
  {
      guint gravity = (guint) xev->data.l[0];
      gint  x       = (gint)  xev->data.l[1];
      gint  y       = (gint)  xev->data.l[2];

      CM_DEBUG ("AnimationActor %p: anchor(gravity=%u, x=%d, y=%d)\n",
               self, gravity, x, y);

      ClutterGravity clutter_gravity;

      switch (gravity)
      {
	  case 1: clutter_gravity = CLUTTER_GRAVITY_NORTH; break;
	  case 2: clutter_gravity = CLUTTER_GRAVITY_NORTH_EAST; break;
	  case 3: clutter_gravity = CLUTTER_GRAVITY_EAST; break;
	  case 4: clutter_gravity = CLUTTER_GRAVITY_SOUTH_EAST; break;
	  case 5: clutter_gravity = CLUTTER_GRAVITY_SOUTH; break;
	  case 6: clutter_gravity = CLUTTER_GRAVITY_SOUTH_WEST; break;
	  case 7: clutter_gravity = CLUTTER_GRAVITY_WEST; break;
	  case 8: clutter_gravity = CLUTTER_GRAVITY_NORTH_WEST; break;
	  case 9: clutter_gravity = CLUTTER_GRAVITY_CENTER; break;

	  default: clutter_gravity = CLUTTER_GRAVITY_NONE; break;
      }

      if (clutter_gravity == CLUTTER_GRAVITY_NONE)
      {
	  clutter_actor_move_anchor_point (actor, x, y);
      }
      else
      {
	  clutter_actor_move_anchor_point_from_gravity
	      (actor, clutter_gravity);
      }
  }
  else if (xev->message_type == parent_atom)
  {
      Window win = (Window) xev->data.l[0];

      CM_DEBUG ("AnimationActor %p: parent(win=%lu)\n",
               self, win);

       /* Unparent the actor */
      ClutterActor *parent = clutter_actor_get_parent (actor);
      if (parent)
        {
          clutter_container_remove_actor (CLUTTER_CONTAINER (parent),
                                        actor);
        }

      if (win != 0)
      {
	  /* re-parent the actor to another actor */

	  MBWindowManagerClient *parent_client = NULL;
	  MBWMCompMgrClutterClient *parent_cclient = NULL;
	  parent = NULL;

	  /* Many things can go wrong if the parent X window is not
	   * mapped yet (WM client or clutter compositing client or
	   * clutter actor may be missing). Silently bail out if
	   * any of this happens. */

	  parent_client =
	      mb_wm_managed_client_from_xwindow (client->wmref, win);

	  if (parent_client)
	      parent_cclient =
		  MB_WM_COMP_MGR_CLUTTER_CLIENT (parent_client->cm_client);

	  if (parent_cclient)
	      parent = mb_wm_comp_mgr_clutter_client_get_actor (parent_cclient);

	  if (parent)
          {
            clutter_container_add_actor (CLUTTER_CONTAINER (parent),
                                         actor);
         }
      }

      if (self->show)
          clutter_actor_show (actor);
      else
          clutter_actor_hide (actor);
  }
  else
  {
      CM_DEBUG ("AnimationActor %p: UNKNOWN MESSAGE %lu (%lu,%lu,%lu,%lu,%lu)\n",
	       self,
	       xev->message_type,
	       xev->data.l[0],
	       xev->data.l[1],
	       xev->data.l[2],
	       xev->data.l[3],
	       xev->data.l[4]);
      return;
  }
}

void
hd_animation_actor_show (MBWindowManagerClient *client)
{
  MBWMCompMgrClutterClient *cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (client->cm_client);

  /* Indicate to the libmatchbox code that the actor visibility and positioning
   * logic will be overriden. */

  mb_wm_comp_mgr_clutter_client_set_flags (cclient,
                                           MBWMCompMgrClutterClientDontShow|
                                           MBWMCompMgrClutterClientDontPosition);


}

// -------------------------------------------------------------

static void
hd_animation_actor_realize (MBWindowManagerClient *client)
{
    HdAnimationActor         *self = HD_ANIMATION_ACTOR (client);
    MBWindowManager          *wm = client->wmref;
    MBWMClientWindow         *win = client->window;
    Window                   window = win->xwindow;

    /* This is a bit of a hack, but for the sake of optimization,
     * we cache all atoms used by the HildonAnimationActor ClientMessage
     * interface in our static variables. */

    if (!atoms_initialized)
    {
	HdCompMgr             *hmgr = HD_COMP_MGR (wm->comp_mgr);

	show_atom = hd_comp_mgr_get_atom
	    (hmgr, HD_ATOM_HILDON_ANIMATION_CLIENT_MESSAGE_SHOW);
	position_atom = hd_comp_mgr_get_atom
	    (hmgr, HD_ATOM_HILDON_ANIMATION_CLIENT_MESSAGE_POSITION);
	rotation_atom = hd_comp_mgr_get_atom
	    (hmgr, HD_ATOM_HILDON_ANIMATION_CLIENT_MESSAGE_ROTATION);
	scale_atom = hd_comp_mgr_get_atom
	    (hmgr, HD_ATOM_HILDON_ANIMATION_CLIENT_MESSAGE_SCALE);
	anchor_atom = hd_comp_mgr_get_atom
	    (hmgr, HD_ATOM_HILDON_ANIMATION_CLIENT_MESSAGE_ANCHOR);
	parent_atom = hd_comp_mgr_get_atom
	    (hmgr, HD_ATOM_HILDON_ANIMATION_CLIENT_MESSAGE_PARENT);
	ready_atom = hd_comp_mgr_get_atom
	    (hmgr, HD_ATOM_HILDON_ANIMATION_CLIENT_READY);

	atoms_initialized = 1;
    }

  /* Install a ClientMessage event handler */

  self->client_message_handler_id =
      mb_wm_main_context_x_event_handler_add (wm->main_ctx,
					      window,
					      ClientMessage,
					      (MBWMXEventFunc)
					      hd_animation_actor_client_message,
					      client);

  /* Force StructureNotifyMask event input on the window.
   *
   * We don't know if any event mask has been previously selected,
   * so we go thought XGetWindowAttributes() to obtain our own event
   * mask and update it with StructureNotifyMask. */

  XWindowAttributes xwa;
  XGetWindowAttributes (wm->xdpy, window, &xwa);

  long event_mask = xwa.your_event_mask | StructureNotifyMask;

  CM_DEBUG ("updating event mask: 0x%08lx -> 0x%08lx\n",
	    xwa.your_event_mask, event_mask);
  XSelectInput (wm->xdpy, window, event_mask);

  /* Set the ready atom on the window -- everything is in place to receive
   * ClientMessage events. */
  long val = 1;
  XChangeProperty (wm->xdpy, window,
		   ready_atom,
		   XA_ATOM, 32, PropModeReplace,
		   (unsigned char *) &val, 1);
}

static void
hd_animation_actor_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client                 = (MBWindowManagerClientClass *)klass;

  client->client_type    = HdWmClientTypeAnimationActor;
  client->geometry       = hd_animation_actor_request_geometry;
  client->realize        = hd_animation_actor_realize;
  client->show           = hd_animation_actor_show;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdAnimationActor";
#endif

}


static void
hd_animation_actor_destroy (MBWMObject *this)
{
    HdAnimationActor      *self = HD_ANIMATION_ACTOR (this);
    MBWindowManagerClient *client = MB_WM_CLIENT (this);
    MBWindowManager       *wm = client->wmref;

    if (self->client_message_handler_id)
    {
        mb_wm_main_context_x_event_handler_remove (wm->main_ctx,
                                                   ClientMessage,
                                                   self->client_message_handler_id);
    }
}

static int
hd_animation_actor_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient *client = MB_WM_CLIENT (this);
  MBWMClientWindow      *win = client->window;
  MBWindowManager	*wm = client->wmref;
  MBGeometry             geom;

  if (!wm)
      return 0;

  /* Animation actors are not reactive and, therefore, are input-transparent.
   * Since they are going to be moved around using clutter calls, X will know
   * nothing of their repositioning. It will, therefore, not dispatch any
   * exposure or visibility events for the windows covered by animation actors.
   * To avoid all this, position the animation actors outside of the screen
   * viewable area. */

  geom.x = wm->xdpy_width;
  geom.y = wm->xdpy_height;
  geom.width = win->geometry.width;
  geom.height = win->geometry.height;

  mb_wm_client_set_layout_hints (client,
                                 LayoutPrefPositionFree|
                                 LayoutPrefMovable|
                                 LayoutPrefVisible);
  hd_animation_actor_request_geometry (client,
				       &geom,
				       MBWMClientReqGeomForced);

  return 1;
}

static Bool
hd_animation_actor_request_geometry (MBWindowManagerClient *client,
				     MBGeometry            *new_geometry,
				     MBWMClientReqGeomType  flags)
{
    CM_DEBUG ("%s: x=%d y=%d w=%d h=%d f=0x%x\n",
	     __FUNCTION__,
	     new_geometry->x,
	     new_geometry->y,
	     new_geometry->width,
	     new_geometry->height,
	     flags);

    if (!(flags & MBWMClientReqGeomForced))
    {
	/* Geometry that is not forced is checked for sanity.
	 * For animation actors, the sanity criteria is that the
	 * window is off-screen.
	 */
	MBWindowManager	*wm = client->wmref;

	if (new_geometry->x < wm->xdpy_width ||
	    new_geometry->y < wm->xdpy_height)
	    return False; /* Geometry rejected */
    }

    client->frame_geometry.x      = new_geometry->x;
    client->frame_geometry.y      = new_geometry->y;
    client->frame_geometry.width  = new_geometry->width;
    client->frame_geometry.height = new_geometry->height;

    client->window->geometry.x      = new_geometry->x;
    client->window->geometry.y      = new_geometry->y;
    client->window->geometry.width  = new_geometry->width;
    client->window->geometry.height = new_geometry->height;

    mb_wm_client_geometry_mark_dirty (client);

    return True; /* Geometry accepted */
}

int
hd_animation_actor_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdAnimationActorClass),
	sizeof (HdAnimationActor),
	hd_animation_actor_init,
	hd_animation_actor_destroy,
	hd_animation_actor_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_APP, 0);
    }

  return type;
}

MBWindowManagerClient*
hd_animation_actor_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client
    = MB_WM_CLIENT(mb_wm_object_new (HD_TYPE_ANIMATION_ACTOR,
				     MBWMObjectPropWm,           wm,
				     MBWMObjectPropClientWindow, win,
				     NULL));
  return client;
}

