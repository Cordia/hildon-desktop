/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Johan Bilien <johan.bilien@nokia.com>
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

#include "hd-comp-window.h"
#include "hd-mb-wm-props.h"
#include "hd-window-actor.h"
#include "hd-stage.h"

#include <matchbox/core/mb-wm.h>
#include <matchbox/core/mb-wm-client.h>

#include <matchbox/client-types/mb-wm-client-menu.h>

#include <clutter/clutter-x11.h>
#include <clutter/clutter-x11-texture-pixmap.h>
#include <clutter/clutter-container.h>
#include <clutter/clutter-effect.h>

#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xcomposite.h>

static int hd_comp_window_init (MBWMObject *obj, va_list vap);
static void hd_comp_window_class_init (MBWMObjectClass *klass);
static void hd_comp_window_destroy (MBWMObject *obj);

static void hd_comp_window_show (MBWMCompMgrClient *client);
static void hd_comp_window_hide (MBWMCompMgrClient *client);
static void hd_comp_window_repair (MBWMCompMgrClient *client);
static void hd_comp_window_configure (MBWMCompMgrClient *client);
static void hd_comp_window_effect_real (HdCompWindow               *window,
                                        MBWMCompMgrClientEvent      event);

struct HdCompWindowPrivate
{
  ClutterActor         *actor;
  gboolean              shown;

  Damage                damage;
};

int
hd_comp_window_class_type ()
{
  static int type = 0;

  if (type == 0)
    {
      static MBWMObjectClassInfo info = {
        sizeof (HdCompWindowClass),
        sizeof (HdCompWindow),
        hd_comp_window_init,
        hd_comp_window_destroy,
        hd_comp_window_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_COMP_MGR_CLIENT, 0);
    }

  return type;
}

static void
hd_comp_window_class_init (MBWMObjectClass *klass)
{
  MBWMCompMgrClientClass *c_class = MB_WM_COMP_MGR_CLIENT_CLASS (klass);
  HdCompWindowClass      *w_class = HD_COMP_WINDOW_CLASS (klass);

  c_class->show      = hd_comp_window_show;
  c_class->hide      = hd_comp_window_hide;
  c_class->repair    = hd_comp_window_repair;
  c_class->configure = hd_comp_window_configure;

  w_class->effect    = hd_comp_window_effect_real;
}

static int
hd_comp_window_init (MBWMObject *obj, va_list vap)
{
  HdCompWindow *window = HD_COMP_WINDOW (obj);
  HdMbWmProp prop;
  HdCompWindowPrivate *priv;

  priv = window->priv = g_new0 (HdCompWindowPrivate, 1);

  prop = va_arg (vap, HdMbWmProp);

  while (prop)
    {
      switch (prop)
        {
          case HdMbWmPropActor:
	    window->priv->actor = va_arg (vap, ClutterActor *);
              break;
          default:
	    MBWMO_PROP_EAT (vap, prop);
        }

      prop = va_arg (vap, MBWMObjectProp);
    }

  if (!window->priv->actor)
    return 0;

  return 1;

}

static void
hd_comp_window_destroy (MBWMObject *obj)
{
  HdCompWindowPrivate  *priv = HD_COMP_WINDOW (obj)->priv;
  MBWindowManagerClient *client = MB_WM_COMP_MGR_CLIENT (obj)->wm_client;

  if (priv->damage != None)
    {
      XDamageDestroy (client->wmref->xdpy,
                      priv->damage);
      priv->damage = None;
    }

  if (priv->actor != NULL)
    {
      clutter_actor_destroy (priv->actor);
      priv->actor = NULL;
    }
}

static void
hd_comp_window_show (MBWMCompMgrClient *client)
{
  HdCompWindowPrivate          *priv = HD_COMP_WINDOW (client)->priv;
  MBWindowManagerClient        *wm_client = client->wm_client;
  Display                      *dpy = client->wm_client->wmref->xdpy;
  Window                        window;
  Pixmap                        pixmap;
  int                           error;

  /*
   * Cannot obtain a named pixmap until the window is visible
   */
  if (priv->shown)
    return;

  priv->shown = TRUE;

  window = client->wm_client->xwin_frame != None ?
           client->wm_client->xwin_frame : client->wm_client->window->xwindow;

  clutter_x11_trap_x_errors ();
  pixmap = XCompositeNameWindowPixmap (dpy, window);
  if ((error = clutter_x11_untrap_x_errors ()))
    {
      g_warning ("X error %i when getting named pixmap for %x",
                 error,
                 (guint)window);
      return;
    }

  clutter_x11_texture_pixmap_set_pixmap (CLUTTER_X11_TEXTURE_PIXMAP (priv->actor),
					   pixmap);

  clutter_actor_show (priv->actor);

  priv->damage = XDamageCreate (wm_client->wmref->xdpy,
                                wm_client->xwin_frame ? wm_client->xwin_frame:
                                                     wm_client->window->xwindow,
                                XDamageReportNonEmpty);


}

static void
hd_comp_window_hide (MBWMCompMgrClient *client)
{
  HdCompWindowPrivate  *priv = HD_COMP_WINDOW (client)->priv;

  priv->shown = FALSE;
}

static void
hd_comp_window_repair (MBWMCompMgrClient *client)
{
  HdCompWindowPrivate  *priv = HD_COMP_WINDOW (client)->priv;
  XserverRegion         parts;
  XRectangle           *rects;
  int                   n_rect, i;
  Display              *dpy = client->wm_client->wmref->xdpy;

  parts = XFixesCreateRegion (dpy, 0, 0);

  XDamageSubtract (dpy,
                   priv->damage,
                   None,
                   parts);

  rects = XFixesFetchRegion (dpy, parts, &n_rect);

  XFixesDestroyRegion (dpy, parts);

  for (i = 0; i < n_rect; i++)
    clutter_x11_texture_pixmap_update_area (CLUTTER_X11_TEXTURE_PIXMAP (priv->actor),
                                            rects[i].x, rects[i].y,
                                            rects[i].width, rects[i].height);

}

static void
hd_comp_window_configure (MBWMCompMgrClient *client)
{
  HdCompWindowPrivate  *priv    = HD_COMP_WINDOW (client)->priv;
  Display              *dpy     = client->wm_client->wmref->xdpy;
  Pixmap                pixmap = None;
  Window                window;

  if (!priv->shown)
    return;

  window = client->wm_client->xwin_frame != None ?
           client->wm_client->xwin_frame : client->wm_client->window->xwindow;

  g_object_get (priv->actor, "pixmap", &pixmap, NULL);

  if (pixmap)
    XFreePixmap (dpy, pixmap);

  pixmap = XCompositeNameWindowPixmap (dpy, window);

  clutter_x11_texture_pixmap_set_pixmap (
				       CLUTTER_X11_TEXTURE_PIXMAP (priv->actor),
				       pixmap);

}

static void
hd_comp_window_effect_complete_cb (ClutterActor *actor,
				   gpointer      data)
{
  HdCompWindow *window = data;

  mb_wm_object_unref (MB_WM_OBJECT (window));
}


static void
hd_comp_window_effect_real (HdCompWindow               *window,
                            MBWMCompMgrClientEvent      event)
{
  HdCompWindowPrivate  *priv = window->priv;
  ClutterEffectTemplate *template =
              clutter_effect_template_new_for_duration (200,
                                                        CLUTTER_ALPHA_RAMP_INC);

  switch (event)
    {
      case MBWMCompMgrClientEventMap:
          clutter_actor_set_opacity (priv->actor, 0);
	  mb_wm_object_ref (MB_WM_OBJECT (window));
          clutter_effect_fade (template,
			       priv->actor,
			       0xFF,
			       hd_comp_window_effect_complete_cb,
			       window);
          break;
      case MBWMCompMgrClientEventUnmap:
          clutter_actor_set_opacity (priv->actor, 0xFF);
	  mb_wm_object_ref (MB_WM_OBJECT (window));
          clutter_effect_fade (template,
			       priv->actor,
			       0,
			       hd_comp_window_effect_complete_cb,
			       window);
          break;
      default:
          g_object_unref (template);
          break;
    }
}

void
hd_comp_window_effect (HdCompWindow            *window,
                       MBWMCompMgrClientEvent   event)
{
  HdCompWindowClass *klass;

  klass = HD_COMP_WINDOW_CLASS (MB_WM_OBJECT_GET_CLASS (window));

  if (klass->effect)
    klass->effect (window, event);

}

void
hd_comp_window_activate (HdCompWindow *window)
{
  MBWindowManagerClient        *client =
      MB_WM_COMP_MGR_CLIENT (window)->wm_client;

  mb_wm_activate_client (client->wmref, client);
}
