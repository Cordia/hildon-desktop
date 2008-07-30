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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hd-wm.h"
#include "hd-comp-mgr.h"
#include "hd-desktop.h"
#include "hd-app.h"

#include <matchbox/core/mb-wm-object.h>
#include <matchbox/core/mb-wm.h>
#include <matchbox/client-types/mb-wm-client-desktop.h>

#include <clutter/clutter-main.h>
#include <clutter/x11/clutter-x11.h>

#include "hd-home-applet.h"
#include "hd-note.h"
#include "hd-app-menu.h"
#include "hd-dialog.h"

static int  hd_wm_init       (MBWMObject *object, va_list vap);
static void hd_wm_destroy    (MBWMObject *object);
static void hd_wm_class_init (MBWMObjectClass *klass);
static MBWindowManagerClient* hd_wm_client_new (MBWindowManager *,
						MBWMClientWindow *);

static MBWMCompMgr * hd_wm_comp_mgr_new (MBWindowManager *wm);

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

  wm->modality_type = MBWMModalitySystem;

  return 1;
}

static void
hd_wm_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClass *wm_class = MB_WINDOW_MANAGER_CLASS (klass);

  wm_class->comp_mgr_new = hd_wm_comp_mgr_new;
  wm_class->client_new   = hd_wm_client_new;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdWm";
#endif
}

static void
hd_wm_destroy (MBWMObject *object)
{
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

  if (win->override_redirect && wm_class)
    return wm_class->client_new (wm, win);
  else if (win->net_type ==
      hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_WM_WINDOW_TYPE_HOME_APPLET))
    {
      printf ("### is home applet ###\n");
      return hd_home_applet_new (wm, win);
    }
  else if (win->net_type ==
      hd_comp_mgr_get_atom (hmgr, HD_ATOM_HILDON_WM_WINDOW_TYPE_APP_MENU))
    {
      printf ("### is application menu ###\n");
      return hd_app_menu_new (wm, win);
    }
  else if (win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_DESKTOP])
    {
      printf ("### is desktop ###\n");
      /* Only one desktop allowed */
      if (wm->desktop)
	return NULL;

      return hd_desktop_new (wm, win);
    }
  else if (win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_NORMAL])
    {
      printf ("### is application ###\n");

      return hd_app_new (wm, win);
    }
  else if (win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_DIALOG])
    {
      printf ("### is dialog ###\n");

      return hd_dialog_new (wm, win);
    }
  else if (win->net_type == wm->atoms[MBWM_ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION])
    {
      MBWM_DBG ("### is notification ###\n");
      return hd_note_new (wm, win);
    }
  else if (wm_class)
    return wm_class->client_new (wm, win);

  return NULL;
}
