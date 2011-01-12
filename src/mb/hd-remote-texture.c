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

#include "hd-remote-texture.h"
#include "hd-comp-mgr.h"
#include "hd-wm.h"
#include "tidy/tidy-mem-texture.h"

#include <sys/time.h>
#include <sys/shm.h>
#include <time.h>

#define CLIENT_MESSAGE_DEBUG 0//1

#if CLIENT_MESSAGE_DEBUG == 1
#    define CM_DEBUG(format, args...) g_debug(format, ## args)
#else
#    define CM_DEBUG(format, args...)
#endif

static guint32 shm_atom;
static guint32 damage_atom;
static guint32 show_atom;
static guint32 position_atom;
static guint32 offset_atom;
static guint32 scale_atom;
static guint32 parent_atom;
static guint32 ready_atom;
static gboolean atoms_initialized = 0;

void
hd_remote_texture_show (MBWindowManagerClient *client);
static Bool
hd_remote_texture_request_geometry (MBWindowManagerClient *client,
				     MBGeometry            *new_geometry,
				     MBWMClientReqGeomType  flags);
static void
hd_remote_texture_set_shm(HdRemoteTexture *tex, key_t key,
                          guint width, guint height, guint bpp);

static Bool
hd_remote_texture_client_message (XClientMessageEvent *xev, void *userdata)
{
  HdRemoteTexture         *self = HD_REMOTE_TEXTURE (userdata);
  MBWindowManagerClient    *client = MB_WM_CLIENT (self);

  if (!client->window)
  {
      g_warning ("Stray client message: no window!\n");
      return False;
  }

  MBWMCompMgrClutterClient *cclient = MB_WM_COMP_MGR_CLUTTER_CLIENT (client->cm_client);
  ClutterActor             *actor = mb_wm_comp_mgr_clutter_client_get_actor (cclient);

  if (!actor)
  {
      g_warning ("Stray client message: no actor!\n");
      return False;
  }

  if (xev->message_type == shm_atom)
    {
        key_t shm_key = (key_t) xev->data.l[0];
        guint shm_width = (guint) xev->data.l[1];
        guint shm_height = (guint) xev->data.l[2];
        guint shm_bpp = (guint) xev->data.l[3];

        hd_remote_texture_set_shm(self, shm_key,
            shm_width,
            shm_height,
            shm_bpp);

        CM_DEBUG ("RemoteTexture %p: shm(key=%d, width=%d, height=%d, bpp=%d)\n",
                  self, shm_key,
                  shm_width, shm_height, shm_bpp);
    }
  else if (xev->message_type == damage_atom)
    {
        gint x = (gint) xev->data.l[0];
        gint y = (gint) xev->data.l[1];
        gint width = (gint) xev->data.l[2];
        gint height = (gint) xev->data.l[3];

        CM_DEBUG ("RemoteTexture %p: "
                  "damage(x=%d, y=%d, width=%d, height=%d)\n",
                  self, x, y, width, height);
        tidy_mem_texture_damage(self->texture, x, y, width, height);
    }
  else if (xev->message_type == show_atom)
  {
      gboolean show = (gboolean) xev->data.l[0];
      guint    opacity = (guint) xev->data.l[1] & 0xff;

      CM_DEBUG ("RemoteTexture %p: show(show=%d, opacity=%d)\n",
	       	self, show, opacity);
      if (show)
          clutter_actor_show (actor);
      else
          clutter_actor_hide (actor);

      clutter_actor_set_opacity (CLUTTER_ACTOR(self->texture), opacity);

  }
  else if (xev->message_type == position_atom)
  {
    gint x = (gint) xev->data.l[0];
    gint y = (gint) xev->data.l[1];
    gint width = (gint) xev->data.l[2];
    gint height = (gint) xev->data.l[3];

    CM_DEBUG ("AnimationActor %p: position(x=%d, y=%d, width=%d, height=%d)\n",
               self, x, y, width, height);
    clutter_actor_set_position (actor, x, y);
    clutter_actor_set_size (actor, width, height);
    clutter_actor_set_size (CLUTTER_ACTOR(self->texture), width, height);
    clutter_actor_set_clip(CLUTTER_ACTOR(self->texture),
                           0, 0,
                           width, height);
  }
  else if (xev->message_type == offset_atom)
    {
        gfloat x = (gfloat) xev->data.l[0];
        gfloat y = (gfloat) xev->data.l[1];

        CM_DEBUG ("RemoteTexture %p: position(x=%d, y=%d)\n",
                  self, x, y);
        tidy_mem_texture_set_offset(self->texture, x, y);
    }

  else if (xev->message_type == scale_atom)
  {
      gfloat x_scale = (gfloat) xev->data.l[0];
      gfloat y_scale = (gfloat) xev->data.l[1];

      CM_DEBUG ("RemoteTexture %p: scale(x_scale=%u, y_scale=%u)\n",
	       self, x_scale, y_scale);
      tidy_mem_texture_set_scale(self->texture, x_scale, y_scale);
  }
  else if (xev->message_type == parent_atom)
  {
      Window win = (Window) xev->data.l[0];

      CM_DEBUG ("RemoteTexture %p: parent(win=%lu)\n",
               self, win);

      /* perserve actor's visibility over the unparenting/reparenting */

      gboolean show = CLUTTER_ACTOR_IS_VISIBLE (actor);

      /* unparent the actor */

      ClutterActor *parent = clutter_actor_get_parent (actor);
      if (parent)
	  clutter_container_remove_actor (CLUTTER_CONTAINER (parent),
					  actor);

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
	      clutter_container_add_actor (CLUTTER_CONTAINER (parent),
					   actor);

          clutter_container_add_actor (CLUTTER_CONTAINER (actor),
                                       CLUTTER_ACTOR(self->texture));
      }

      if (show)
          clutter_actor_show (actor);
      else
          clutter_actor_hide (actor);
  }
  else
  {
      CM_DEBUG ("RemoteTexture %p: UNKNOWN MESSAGE %lu (%lu,%lu,%lu,%lu,%lu)\n",
	       self,
	       xev->message_type,
	       xev->data.l[0],
	       xev->data.l[1],
	       xev->data.l[2],
	       xev->data.l[3],
	       xev->data.l[4]);
      return False;
  }

  return True;
}

void
hd_remote_texture_show (MBWindowManagerClient *client)
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
hd_remote_texture_realize (MBWindowManagerClient *client)
{
    HdRemoteTexture         *self = HD_REMOTE_TEXTURE (client);
    MBWindowManager          *wm = client->wmref;
    MBWMClientWindow         *win = client->window;
    Window                   window = win->xwindow;

    /* This is a bit of a hack, but for the sake of optimization,
     * we cache all atoms used by the HildonRemoteTexture ClientMessage
     * interface in our static variables. */

    if (!atoms_initialized)
    {
	HdCompMgr             *hmgr = HD_COMP_MGR (wm->comp_mgr);

	shm_atom = hd_comp_mgr_get_atom
            (hmgr, HD_ATOM_HILDON_TEXTURE_CLIENT_MESSAGE_SHM);
        damage_atom = hd_comp_mgr_get_atom
            (hmgr, HD_ATOM_HILDON_TEXTURE_CLIENT_MESSAGE_DAMAGE);
	show_atom = hd_comp_mgr_get_atom
	    (hmgr, HD_ATOM_HILDON_TEXTURE_CLIENT_MESSAGE_SHOW);
	position_atom = hd_comp_mgr_get_atom
	    (hmgr, HD_ATOM_HILDON_TEXTURE_CLIENT_MESSAGE_POSITION);
	offset_atom = hd_comp_mgr_get_atom
	    (hmgr, HD_ATOM_HILDON_TEXTURE_CLIENT_MESSAGE_OFFSET);
	scale_atom = hd_comp_mgr_get_atom
	    (hmgr, HD_ATOM_HILDON_TEXTURE_CLIENT_MESSAGE_SCALE);
	parent_atom = hd_comp_mgr_get_atom
	    (hmgr, HD_ATOM_HILDON_TEXTURE_CLIENT_MESSAGE_PARENT);
	ready_atom = hd_comp_mgr_get_atom
	    (hmgr, HD_ATOM_HILDON_TEXTURE_CLIENT_READY);

	atoms_initialized = 1;
    }

  /* Install a ClientMessage event handler */

  self->client_message_handler_id =
      mb_wm_main_context_x_event_handler_add (wm->main_ctx,
					      window,
					      ClientMessage,
					      (MBWMXEventFunc)
					      hd_remote_texture_client_message,
					      client);

  /* Force StructureNotifyMask event input on the window.
   *
   * We don't know if any event mask has been previously selected,
   * so we go thought XGetWindowAttributes() to obtain our own event
   * mask and update it with StructureNotifyMask. */

  XWindowAttributes xwa;
  XGetWindowAttributes (wm->xdpy, window, &xwa);

  long event_mask = xwa.your_event_mask | StructureNotifyMask;

  g_debug ("updating event mask: 0x%08lx -> 0x%08lx\n",
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
hd_remote_texture_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client                 = (MBWindowManagerClientClass *)klass;

  client->client_type    = HdWmClientTypeRemoteTexture;
  client->geometry       = hd_remote_texture_request_geometry;
  client->realize        = hd_remote_texture_realize;
  client->show           = hd_remote_texture_show;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdRemoteTexture";
#endif

}


static void
hd_remote_texture_destroy (MBWMObject *this)
{
  HdRemoteTexture      *self = HD_REMOTE_TEXTURE (this);
  MBWindowManagerClient *client = MB_WM_CLIENT (this);
  MBWindowManager       *wm = client->wmref;

  if (self->client_message_handler_id)
  {
      mb_wm_main_context_x_event_handler_remove (wm->main_ctx,
                                                 ClientMessage,
                                                 self->client_message_handler_id);
  }
  /* unattach ourselves if we were attached */
  hd_remote_texture_set_shm(self, 0, 0, 0, 0);
  /* free our texture */
  clutter_actor_destroy(CLUTTER_ACTOR(self->texture));
  self->texture = 0;
}

static int
hd_remote_texture_init (MBWMObject *this, va_list vap)
{
  HdRemoteTexture       *tex = HD_REMOTE_TEXTURE(this);
  MBWindowManagerClient *client = MB_WM_CLIENT (this);
  MBWMClientWindow      *win = client->window;
  MBWindowManager	*wm = client->wmref;
  MBGeometry             geom;

  if (!wm)
      return 0;

  tex->texture = g_object_ref(tidy_mem_texture_new());

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
  hd_remote_texture_request_geometry (client,
                                      &geom,
                                      MBWMClientReqGeomForced);

  return 1;
}

static Bool
hd_remote_texture_request_geometry (MBWindowManagerClient *client,
				     MBGeometry            *new_geometry,
				     MBWMClientReqGeomType  flags)
{
    g_debug ("%s: x=%d y=%d w=%d h=%d f=0x%x\n",
	     __FUNCTION__,
	     new_geometry->x,
	     new_geometry->y,
	     new_geometry->width,
	     new_geometry->height,
	     flags);

    if (!(flags & MBWMClientReqGeomForced))
	return False; /* Geometry rejected */

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
hd_remote_texture_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdRemoteTextureClass),
	sizeof (HdRemoteTexture),
	hd_remote_texture_init,
	hd_remote_texture_destroy,
	hd_remote_texture_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_APP, 0);
    }

  return type;
}

MBWindowManagerClient*
hd_remote_texture_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client
    = MB_WM_CLIENT(mb_wm_object_new (HD_TYPE_REMOTE_TEXTURE,
				     MBWMObjectPropWm,           wm,
				     MBWMObjectPropClientWindow, win,
				     NULL));
  return client;
}

static void
hd_remote_texture_set_shm(HdRemoteTexture *tex, key_t key,
                          guint width, guint height, guint bpp)
{
  int shm_id;
  /* un-attach this segment */
  if (tex->shm_addr)
    {
      tidy_mem_texture_set_data(tex->texture,
            0, 0, 0, 0);
      if (shmdt(tex->shm_addr) == -1)
        g_critical("%s: shmdt: %p is not the data segment start address "
                   "of a shared memory segment", __FUNCTION__, tex->shm_addr);
      tex->shm_addr = 0;
      tex->shm_key = 0;
      tex->shm_width = 0;
      tex->shm_height = 0;
      tex->shm_bpp = 0;
    }

  if (key == 0)
    return;

  tex->shm_key = key;
  tex->shm_width = width;
  tex->shm_height = height;
  tex->shm_bpp = bpp;
  guint size = width*height*bpp;
  if ((shm_id = shmget(key, size, 0666)) < 0)
    {
      g_critical("%s: shmget failed, size %d", __FUNCTION__, size);
      tex->shm_key = 0;
      tex->shm_width = 0;
      tex->shm_height = 0;
      tex->shm_bpp = 0;
      return;
   }
  if ((tex->shm_addr = shmat(shm_id, NULL, SHM_RDONLY)) == (guchar *)-1)
    {
      g_critical("%s: shmget failed", __FUNCTION__);
      tex->shm_key = 0;
      tex->shm_width = 0;
      tex->shm_height = 0;
      tex->shm_bpp = 0;
      tex->shm_addr = 0;
      return;
    }

  tidy_mem_texture_set_data(tex->texture,
      tex->shm_addr,
      tex->shm_width, tex->shm_height,
      tex->shm_bpp);
}


