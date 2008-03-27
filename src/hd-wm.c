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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "hd-wm.h"
#include "hd-comp-mgr.h"
#include "hd-mb-wm-props.h"

#include <matchbox/core/mb-wm-object.h>
#include <matchbox/core/mb-wm.h>

#include <clutter/clutter-main.h>
#include <clutter/clutter-x11.h>

static int  hd_wm_init       (MBWMObject *object, va_list vap);
static void hd_wm_destroy    (MBWMObject *object);
static void hd_wm_class_init (MBWMObjectClass *klass);

static void hd_wm_main       (MBWindowManager *wm);
static MBWMCompMgr * hd_wm_comp_mgr_new (MBWindowManager *wm);

struct HdWmPrivate
{
  ClutterActor         *window_group;
  ClutterActor         *top_window_group;
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
  HdWmPrivate          *priv;
  MBWindowManager      *wm = MB_WINDOW_MANAGER (object);
  HdMbWmProp            prop;

  wm->modality_type = MBWMModalitySystem;

  priv = HD_WM (object)->priv = g_new0 (HdWmPrivate, 1);

  prop = va_arg (vap, HdMbWmProp);

  while (prop)
    {
      switch (prop)
        {
          case HdMbWmPropWindowGroup:
              priv->window_group = va_arg (vap, ClutterActor *);
              break;
          case HdMbWmPropTopWindowGroup:
              priv->top_window_group = va_arg (vap, ClutterActor *);
              break;
          default:
              MBWMO_PROP_EAT (vap, prop);
        }

      prop = va_arg (vap, MBWMObjectProp);
    }


  mb_wm_compositing_on (wm);

  return 1;
}

static void
hd_wm_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClass *wm_class = MB_WINDOW_MANAGER_CLASS (klass);

  wm_class->comp_mgr_new = hd_wm_comp_mgr_new;
  wm_class->main         = hd_wm_main;

}

static void
hd_wm_destroy (MBWMObject *object)
{
}

static MBWMCompMgr *
hd_wm_comp_mgr_new (MBWindowManager *wm)
{
  MBWMCompMgr  *mgr;
  HdWmPrivate  *priv = HD_WM (wm)->priv;

  mgr = (MBWMCompMgr *)mb_wm_object_new (HD_TYPE_COMP_MGR,
                                         MBWMObjectPropWm, wm,
                                         HdMbWmPropWindowGroup, priv->window_group,
                                         HdMbWmPropTopWindowGroup, priv->top_window_group,
                                         NULL);

  return mgr;
}

static ClutterX11FilterReturn
mb_wm_clutter_xevent_filter (XEvent *xev, ClutterEvent *cev, gpointer data)
{
  MBWindowManager * wm = data;

  mb_wm_main_context_handle_x_event (xev, wm->main_ctx);

  if (wm->sync_type)
    mb_wm_sync (wm);

  return CLUTTER_X11_FILTER_CONTINUE;
}

static void
hd_wm_main (MBWindowManager *wm)
{
  clutter_x11_add_filter (mb_wm_clutter_xevent_filter, wm);

  clutter_main ();
}
