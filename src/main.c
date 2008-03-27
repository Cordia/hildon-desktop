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

#include <matchbox/core/mb-wm-object.h>

#include <clutter/clutter-main.h>
#include <clutter/clutter-stage.h>
#include <clutter/clutter-x11.h>
#include <clutter/clutter-container.h>

#include "hd-wm.h"
#include "hd-stage.h"
#include "hd-window-group.h"
#include "hd-switcher.h"
#include "hd-mb-wm-props.h"

int
main (int argc, char **argv)
{
  MBWMObject *wm;
  ClutterActor *stage, *window_group, *top_window_group, *switcher;

  g_log_set_always_fatal (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);

  clutter_init (&argc, &argv);
  mb_wm_object_init ();

  stage = hd_get_default_stage ();
  clutter_actor_show (stage);

  window_group = g_object_new (HD_TYPE_WINDOW_GROUP, NULL);
  clutter_actor_show (window_group);
  top_window_group = g_object_new (CLUTTER_TYPE_GROUP, NULL);
  clutter_actor_show (top_window_group);
  switcher = g_object_new (HD_TYPE_SWITCHER,
                           "window-group", window_group,
                           "top-window-group", top_window_group,
                           NULL);
  clutter_actor_show (switcher);
  clutter_actor_set_size (switcher,
                          clutter_actor_get_width (stage),
                          clutter_actor_get_height (stage));
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), switcher);

  wm = mb_wm_object_new (HD_TYPE_WM,
                         MBWMObjectPropDpy, clutter_x11_get_default_display (),
                         HdMbWmPropWindowGroup, window_group,
                         HdMbWmPropTopWindowGroup, top_window_group,
                         NULL);

  mb_wm_init (MB_WINDOW_MANAGER (wm));

  mb_wm_main_loop (MB_WINDOW_MANAGER (wm));

  mb_wm_object_unref (wm);

  return 0;
}
