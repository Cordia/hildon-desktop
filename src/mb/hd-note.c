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

#include "hd-note.h"
#include "hd-comp-mgr.h"
#include "hd-util.h"
#include "hd-wm.h"
#include "hd-render-manager.h"

#include <matchbox/theme-engines/mb-wm-theme.h>
#include <matchbox/theme-engines/mb-wm-theme-xml.h>

/* libmatchbox defines %LIKELY() wrong, work it around
 * for mbwm_return_val_if_fail(). */
#undef LIKELY
#define LIKELY(expr) (__builtin_expect(!!(expr), 1))

#undef  G_LOG_DOMAIN
#define G_LOG_DOMAIN "hd-note"

static void hd_note_realize (MBWindowManagerClient *client);
static Bool hd_note_request_geometry (MBWindowManagerClient *client,
				      MBGeometry            *new_geometry,
				      MBWMClientReqGeomType  flags);
static MBWMStackLayerType hd_note_stacking_layer(MBWindowManagerClient *client);
static void hd_note_stack (MBWindowManagerClient *client, int flags);

/* Properties of an IncomingEvent that can be queried, we cache
 * and notice if change. */
static HdAtoms IEProperties[] =
{
  HD_ATOM_HILDON_INCOMING_EVENT_NOTIFICATION_ICON,
  HD_ATOM_HILDON_INCOMING_EVENT_NOTIFICATION_TIME,
  HD_ATOM_HILDON_INCOMING_EVENT_NOTIFICATION_COUNT,
  HD_ATOM_HILDON_INCOMING_EVENT_NOTIFICATION_SUMMARY,
  HD_ATOM_HILDON_INCOMING_EVENT_NOTIFICATION_MESSAGE,
  HD_ATOM_HILDON_INCOMING_EVENT_NOTIFICATION_DESTINATION,
};

static char *
get_x_window_string_property (HdNote *self, HdAtoms atom_id)
{
  return hd_util_get_x_window_string_property (
                                  MB_WM_CLIENT (self)->wmref,
                                  MB_WM_CLIENT (self)->window->xwindow,
                                  atom_id);
}

static void
resize_note (XConfigureEvent *xev, MBWindowManagerClient *client)
{
  int n, s, w, e;
  MBGeometry geom;
  MBWindowManager *wm;

  wm = client->wmref;
  if (wm->theme)
    mb_wm_theme_get_decor_dimensions (wm->theme, client, &n, &s, &w, &e);
  else
    n = s = w = e = 0;

  /* Set the frame geometry */
  geom.x      = 0;
  geom.width  = wm->xdpy_width;
  geom.height = n + client->window->geometry.height + s;

  /* Set geom.y */
  if (HD_NOTE (client)->note_type == HdNoteTypeBanner)
    { /* Display right below the application title bar. */
      MBWMXmlClient *c;
      MBWMXmlDecor  *d;

      geom.y = wm->theme
          && (c = mb_wm_xml_client_find_by_type (wm->theme->xml_clients,
                                                 MBWMClientTypeApp)) != NULL
          && (d = mb_wm_xml_decor_find_by_type (c->decors,
                                                MBWMDecorTypeNorth)) != NULL
        ? d->height : 40;
    }
  else if (HD_NOTE (client)->note_type == HdNoteTypeInfo)
    /* Center vertically. */
    geom.y = (wm->xdpy_height-geom.height) / 2;
  else /* Confirmation */
    /* Align to bottom. */
    geom.y = wm->xdpy_height - geom.height;

  hd_note_request_geometry (client, &geom, MBWMClientReqGeomForced);
}

/* Called when a %HdIncomingEvent's X window property has changed. */
static void
x_window_property_changed (XPropertyEvent *event, HdNote *self)
{
  guint i;
  HdCompMgr *cmgr = HD_COMP_MGR (MB_WM_CLIENT (self)->wmref->comp_mgr);

  /* Do we recognize the changed property? */
  for (i = 0; i < G_N_ELEMENTS (IEProperties); i++)
    {
      if (event->atom != hd_comp_mgr_get_atom (cmgr, IEProperties[i]))
        continue;

      /*
       * Invalidate the cache and emit a signal.
       *
       * TODO When several properties are changed at once we emit
       *      %HdNoteSignalChanged for each of them, which is not
       *      good.  We can make use of hildon-home's custom that
       *      whenever it updates a "switcher window" (notification
       *      thumbnail at our side) it always sets the DESTINATION
       *      property of the window and always does it last. Other
       *      than that it occasionly sets the TIME of the window
       *      individually.  In the end we could define a separate
       *      signal for TIME and ignore property updates until
       *      DESTINATION is finally set.
       *
       *      if (property == DESTINATION)
       *      {
       *        signal(HdNoteSignalChanged)
       *        mass = FALSE;
       *      } else if (mass)
       *        ignore;
       *      else if (property == TIME)
       *        emit(HdNoteSignalTimeChanged)
       *      else
       *        mass = TRUE;
       */
      if (self->properties[i])
        XFree (self->properties[i]);
      self->properties[i] = NULL;
      mb_wm_object_signal_emit (MB_WM_OBJECT (self), HdNoteSignalChanged);
      break;
    }
}

static void
hd_note_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  client->client_type  = MBWMClientTypeNote;
  client->realize      = hd_note_realize;
  client->geometry     = hd_note_request_geometry;
  client->stack        = hd_note_stack;
  client->stacking_layer = hd_note_stacking_layer;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdNote";
#endif
}

static void
hd_note_destroy (MBWMObject *this)
{
  HdNote *note = HD_NOTE (this);

  if (note->modal_blocker_cb_id)
    mb_wm_main_context_x_event_handler_remove (
                                MB_WM_CLIENT (this)->wmref->main_ctx,
                                ButtonRelease,
                                note->modal_blocker_cb_id);
  if (note->note_type == HdNoteTypeBanner
      || note->note_type == HdNoteTypeInfo
      || note->note_type == HdNoteTypeConfirmation)
    mb_wm_main_context_x_event_handler_remove (
                                MB_WM_CLIENT (this)->wmref->main_ctx,
                                ConfigureNotify,
                                note->screen_size_changed_cb_id);
  if (note->note_type == HdNoteTypeIncomingEvent)
    {
      guint i;

      mb_wm_main_context_x_event_handler_remove (
                                  MB_WM_CLIENT (this)->wmref->main_ctx,
                                  PropertyNotify,
                                  note->property_changed_cb_id);
      for (i = 0; i < G_N_ELEMENTS (IEProperties); i++)
        if (note->properties[i])
          XFree (note->properties[i]);
    }
}

static int
hd_note_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient *client = MB_WM_CLIENT (this);
  MBWindowManager       *wm = client->wmref;
  HdNote                *note = HD_NOTE (this);
  char                  *prop;

  prop = get_x_window_string_property (note,
                                       HD_ATOM_HILDON_NOTIFICATION_TYPE);
  if (prop != NULL)
    {
      if (!strcmp (prop, "_HILDON_NOTIFICATION_TYPE_BANNER"))
	note->note_type = HdNoteTypeBanner;
      else if (!strcmp (prop, "_HILDON_NOTIFICATION_TYPE_INFO"))
	note->note_type = HdNoteTypeInfo;
      else if (!strcmp (prop, "_HILDON_NOTIFICATION_TYPE_CONFIRMATION"))
	note->note_type = HdNoteTypeConfirmation;
      else if (!strcmp (prop, "_HILDON_NOTIFICATION_TYPE_PREVIEW"))
	note->note_type = HdNoteTypeIncomingEventPreview;
      else if (!strcmp (prop, "_HILDON_NOTIFICATION_TYPE_INCOMING_EVENT"))
	note->note_type = HdNoteTypeIncomingEvent;
      else
	{
	  g_warning ("Unknown hildon notification type.");
	}

      XFree (prop);
    }

  if (note->note_type == HdNoteTypeIncomingEvent)
    {
      MBGeometry geom;
      geom.width  = client->frame_geometry.width;
      geom.height = client->frame_geometry.height;
      geom.x = 0;
      geom.y = -(1+geom.height);

      /* Stack it as low as possible to make it "disappear" from the screen.
       * It will remain mapped, but the user cannot click it directly. */
      client->stacking_layer = MBWMStackLayerUnknown;

      /* Leave it up to the client to specify size; we just want it off of
       * the screen. See comments below under HdNoteTypeIncomingEventPreview */
      hd_note_request_geometry (client, &geom,
                                MBWMClientReqGeomForced);

      note->property_changed_cb_id = mb_wm_main_context_x_event_handler_add (
                       wm->main_ctx, client->window->xwindow, PropertyNotify,
                       (MBWMXEventFunc)x_window_property_changed, note);
      return 1;
    }

  if (note->note_type == HdNoteTypeIncomingEventPreview)
    {
      int n, s, w, e;
      MBGeometry geom;

      if (wm->theme)
        mb_wm_theme_get_decor_dimensions (wm->theme, client, &n, &s, &w, &e);
      else
        n = s = w = e = 0;

      geom.x = HD_COMP_MGR_TOP_LEFT_BTN_WIDTH;
      geom.y = 0;
      geom.width  = w + client->window->geometry.width  + e;
      geom.height = n + client->window->geometry.height + s;

      hd_note_request_geometry (client, &geom, MBWMClientReqGeomForced);
    }
  else /* Banner, Info, Confirmation */
    {
      resize_note (NULL, client);
      note->screen_size_changed_cb_id =
        mb_wm_main_context_x_event_handler_add (wm->main_ctx,
                                                wm->root_win->xwindow,
                                                ConfigureNotify,
                                                (MBWMXEventFunc)resize_note,
                                                client);
    }

  return 1;
}

int
hd_note_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdNoteClass),
	sizeof (HdNote),
	hd_note_init,
	hd_note_destroy,
	hd_note_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_NOTE, 0);
    }

  return type;
}

static void
hd_note_realize (MBWindowManagerClient *client)
{
  MBWindowManagerClientClass* parent_klass =
    MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(client)));

  parent_klass->realize (client);
  if (HD_NOTE (client)->note_type == HdNoteTypeInfo)
    /* Close information notes when clicked outside. */
    HD_NOTE(client)->modal_blocker_cb_id =
            hd_util_modal_blocker_realize (client, FALSE);
  else if (HD_NOTE (client)->note_type == HdNoteTypeConfirmation)
    /* Ping confirmation notes when clicked outside. */
    HD_NOTE(client)->modal_blocker_cb_id =
            hd_util_modal_blocker_realize (client, TRUE);
}

static Bool
hd_note_request_geometry (MBWindowManagerClient *client,
			  MBGeometry            *new_geometryp,
			  MBWMClientReqGeomType  flags)
{
  const MBGeometry * geom;
  Bool               change_pos;
  Bool               change_size;
  MBWindowManager *wm = client->wmref;

  /* create a copy here, as we don't want to change the value that
   * was passed in... */
  MBGeometry         new_geometry = *new_geometryp;

  /*
   * When we get an internal geometry request, like from the layout manager,
   * the new geometry applies to the frame; however, if the request is
   * external from ConfigureRequest, it is new geometry of the client window,
   * so we need to take care to handle this right.
   */
  geom = (flags & MBWMClientReqGeomIsViaConfigureReq) ?
    &client->window->geometry : &client->frame_geometry;

  if (HD_IS_CONFIRMATION_NOTE (client))
    {
      int diff;
      /* make sure it's bottom-aligned */
      diff = (new_geometry.y + new_geometry.height) - wm->xdpy_height;
      new_geometry.y -= diff;
    }

  /*
   * We only allow resizing and moving in the y axis, since notes are
   * fullscreen.
   */
  change_pos = (geom->y != new_geometry.y);
  change_size = (geom->height != new_geometry.height);

  if (change_size || (flags & MBWMClientReqGeomForced))
    {
      int north = 0, south = 0, west = 0, east = 0;

      if (client->decor)
	mb_wm_theme_get_decor_dimensions (wm->theme, client,
					  &north, &south, &west, &east);

      if (flags & MBWMClientReqGeomIsViaConfigureReq)
	{
	  /*
	   * Calculate the frame size from the window size
	   */
	  MBWM_DBG ("ConfigureRequest [%d,%d;%dx%d] -> [%d,%d;%dx%d]\n",
		    client->window->geometry.x,
		    client->window->geometry.y,
		    client->window->geometry.width,
		    client->window->geometry.height,
		    new_geometry.x,
		    new_geometry.y,
		    new_geometry.width,
		    new_geometry.height);

	  client->window->geometry.x      = new_geometry.x;
	  client->window->geometry.y      = new_geometry.y;
	  client->window->geometry.width  = new_geometry.width;
	  client->window->geometry.height = new_geometry.height;

	  client->frame_geometry.x
	    = client->window->geometry.x - west;
	  client->frame_geometry.y
	    = client->window->geometry.y - north;
	  client->frame_geometry.width
	    = client->window->geometry.width + (west + east);
	  client->frame_geometry.height
	    = client->window->geometry.height + (south + north);

	}
      else
	{
	  /*
	   * Internal request, e.g., from layout manager; work out client
	   * window size from the provided frame size.
	   */
	  client->frame_geometry.x      = new_geometry.x;
	  client->frame_geometry.y      = new_geometry.y;
	  client->frame_geometry.width  = new_geometry.width;
	  client->frame_geometry.height = new_geometry.height;

	  client->window->geometry.x
	    = client->frame_geometry.x + west;
	  client->window->geometry.y
	    = client->frame_geometry.y + north;
	  client->window->geometry.width
	    = client->frame_geometry.width - (west + east);
	  client->window->geometry.height
	    = client->frame_geometry.height - (south + north);
	}

      mb_wm_client_geometry_mark_dirty (client);

      return True; /* Geometry accepted */
    }
  else if (change_pos)
    {
      /*
       * Change of position only, just move both windows, no need to
       * mess about with the decor.
       */
      int x_diff = geom->x - new_geometry.x;
      int y_diff = geom->y - new_geometry.y;

      client->frame_geometry.x -= x_diff;
      client->frame_geometry.y -= y_diff;
      client->window->geometry.x -= x_diff;
      client->window->geometry.y -= y_diff;

      mb_wm_client_geometry_mark_dirty (client);

      return True;
    }

  return True; /* Geometry accepted */
}

/* Do not let our parent class decide our stacking layer if we're
 * an incoming event. */
MBWMStackLayerType
hd_note_stacking_layer(MBWindowManagerClient *client)
{
  MBWindowManagerClientClass *parent;
  Window dialog;
  MBWindowManager *wm;

  if (HD_NOTE (client)->note_type == HdNoteTypeIncomingEvent)
    return MBWMStackLayerUnknown;

  /* if there is an application menu and 'application not responding' note,
   * stack the note above the menu */
  wm = client->wmref;
  dialog = hd_wm_get_hung_client_dialog_xid (wm);
  if (dialog && dialog == client->window->xwindow)
    {
      MBWindowManagerClient *c;

      for (c = wm->stack_top; c && c != wm->desktop; c = c->stacked_below)
        {
          if (mb_wm_client_is_map_confirmed (c) &&
              MB_WM_CLIENT_CLIENT_TYPE (c) == HdWmClientTypeAppMenu)
            return c->stacking_layer;
        }
    }

  parent = MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS (
                                   MB_WM_OBJECT (client)));
  return parent->stacking_layer (client);
}

MBWindowManagerClient*
hd_note_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client
    = MB_WM_CLIENT (mb_wm_object_new (HD_TYPE_NOTE,
				      MBWMObjectPropWm,           wm,
				      MBWMObjectPropClientWindow, win,
				      NULL));

  return client;
}

/* Called when the actor of the note is clicked.
 * Synthetize a ButtonPressEvent if the circumstances allow
 * the activation of the notification's action. */
void
hd_note_clicked (HdNote *self, void *unused, void *actor)
{
  const MBWindowManagerClient *c = MB_WM_CLIENT (self);
  const MBWMCompMgrClient *cmgrc;

  /* verify that the client hasn't gone yet */
  cmgrc = g_object_get_data(actor, "HD-MBWMCompMgrClutterClient");
  if (!cmgrc || cmgrc->wm_client != c || !c->wmref)
    return;

  if (hd_wm_close_modal_blockers (c->wmref))
    hd_util_click (MB_WM_CLIENT (self));
}

static void
hd_note_stack (MBWindowManagerClient *client,
	       int                    flags)
{
  if ((HD_NOTE (client)->note_type == HdNoteTypeBanner ||
       HD_NOTE (client)->note_type == HdNoteTypeInfo) &&
      mb_wm_client_get_transient_for (client))
    {
      /* we need do nothing here; we are only overriding
       * the base routine which restacks the parent window,
       * which we don't want to do.  See NB#121341.
       */
    }
  else
    {
      MBWindowManagerClientClass* parent_klass =
	MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_PARENT_CLASS(MB_WM_OBJECT(client)));

      parent_klass->stack (client, flags);
    }
}


/* Define an accessor function that caches @IEProperties[@prop]'s value
 * in #HdNote.properties and returns it, which must not be XFree()d. */
#define DEFINE_ACCESSOR(prop, field)                                          \
const char *hd_note_get_##field (HdNote *self)                                \
{                                                                             \
  mbwm_return_val_if_fail (self->note_type == HdNoteTypeIncomingEvent, NULL); \
  if (!self->properties[prop])                                                \
    {                                                                         \
      mb_wm_util_async_trap_x_errors (                                        \
          MB_WM_CLIENT(self)->wmref->xdpy);                                      \
      self->properties[prop] = get_x_window_string_property (self,            \
                  IEProperties[prop]);                                        \
      mb_wm_util_async_untrap_x_errors ();                                    \
    }                                                                         \
  return self->properties[prop];                                              \
}

DEFINE_ACCESSOR(0, icon);
DEFINE_ACCESSOR(1, time);
DEFINE_ACCESSOR(2, count);
DEFINE_ACCESSOR(3, summary);
DEFINE_ACCESSOR(4, message);
DEFINE_ACCESSOR(5, destination);
