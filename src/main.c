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
#include <clutter/x11/clutter-x11.h>
#include <clutter/clutter-container.h>

#include "hd-wm.h"

enum {
  KEY_ACTION_PAGE_NEXT,
  KEY_ACTION_PAGE_PREV,
  KEY_ACTION_TOGGLE_FULLSCREEN,
  KEY_ACTION_TOGGLE_DESKTOP,
};

static void
key_binding_func (MBWindowManager   *wm,
		  MBWMKeyBinding    *binding,
		  void              *userdata)
{
  printf(" ### got key press ### \n");
  int action;

  action = (int)(userdata);

  switch (action)
    {
    case KEY_ACTION_PAGE_NEXT:
      mb_wm_cycle_apps (wm, False);
      break;
    case KEY_ACTION_PAGE_PREV:
      mb_wm_cycle_apps (wm, True);
      break;
    case KEY_ACTION_TOGGLE_FULLSCREEN:
      printf(" ### KEY_ACTION_TOGGLE_FULLSCREEN ### \n");
      break;
    case KEY_ACTION_TOGGLE_DESKTOP:
      printf(" ### KEY_ACTION_TOGGLE_DESKTOP ### \n");
      mb_wm_toggle_desktop (wm);
      break;
    }
}

int
main (int argc, char **argv)
{
  Display * dpy = NULL;
  MBWindowManager *wm;

  g_log_set_always_fatal (G_LOG_LEVEL_ERROR    |
			  G_LOG_LEVEL_CRITICAL |
			  G_LOG_LEVEL_WARNING);

  mb_wm_object_init();

#if USE_GTK
  printf ("initializing gtk\n");

  gtk_init (&argc, &argv);
  dpy = GDK_DISPLAY();
#endif

#if ENABLE_CLUTTER_COMPOSITE_MANAGER
  /*
   * If using clutter, we share the display connection, and hook
   * our xevent handler into the clutter main loop.
   *
   * If we are also doing gtk integration, we need to make clutter to
   * use the gtk display connection.
   */
  if (dpy)
    clutter_x11_set_display (dpy);

#if USE_GTK
  clutter_x11_disable_event_retrieval ();
#endif

  clutter_init (&argc, &argv);

  if (!dpy)
    dpy = clutter_x11_get_default_display ();

#endif

  wm = MB_WINDOW_MANAGER (mb_wm_object_new (HD_TYPE_WM,
					    MBWMObjectPropArgc, argc,
					    MBWMObjectPropArgv, argv,
					    MBWMObjectPropDpy,  dpy,
					    NULL));

  mb_wm_init (wm);

  if (wm == NULL)
    mb_wm_util_fatal_error("OOM?");

  mb_wm_keys_binding_add_with_spec (wm,
				    "<alt>d",
				    key_binding_func,
				    NULL,
				    (void*)KEY_ACTION_TOGGLE_DESKTOP);

  mb_wm_keys_binding_add_with_spec (wm,
				    "<alt>n",
				    key_binding_func,
				    NULL,
				    (void*)KEY_ACTION_PAGE_NEXT);

  mb_wm_keys_binding_add_with_spec (wm,
				    "<alt>p",
				    key_binding_func,
				    NULL,
				    (void*)KEY_ACTION_PAGE_PREV);

  mb_wm_main_loop(wm);

  mb_wm_object_unref (MB_WM_OBJECT (wm));

#if MBWM_WANT_DEBUG
  mb_wm_object_dump ();
#endif

  return 0;
}
