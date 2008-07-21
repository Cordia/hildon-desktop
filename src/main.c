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

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <clutter/clutter-main.h>
#include <clutter/clutter-stage.h>
#include <clutter/x11/clutter-x11.h>
#include <clutter/clutter-container.h>

#include "hd-wm.h"
#include "hd-theme.h"

enum {
  KEY_ACTION_PAGE_NEXT,
  KEY_ACTION_PAGE_PREV,
  KEY_ACTION_TOGGLE_FULLSCREEN,
  KEY_ACTION_TOGGLE_DESKTOP,
};

static unsigned int
theme_type_func (const char *theme_name, void *data)
{
  if (theme_name && !strcmp (theme_name, "hildon"))
    return HD_TYPE_THEME;

  return MB_WM_TYPE_THEME;
}

static unsigned int
theme_client_type_func (const char *type_name, void *user_data)
{
  if (!type_name)
    return 0;

  if (!strcmp (type_name, "application-menu"))
    return HdWmClientTypeAppMenu;
  else if (!strcmp (type_name, "home-applet"))
    return HdWmClientTypeHomeApplet;

  return 0;
}

static unsigned int
theme_button_type_func (const char *type_name,
			void       *user_data)
{
  if (type_name && !strcmp (type_name, "back"))
    return HdHomeThemeButtonBack;

  return 0;
}

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

  mb_wm_theme_set_custom_theme_type_func (theme_type_func, NULL);
  mb_wm_theme_set_custom_theme_alloc_func (hd_theme_alloc_func);
  mb_wm_theme_set_custom_client_type_func (theme_client_type_func, NULL);
  mb_wm_theme_set_custom_button_type_func (theme_button_type_func, NULL);

  printf ("initializing gtk\n");

  gtk_init (&argc, &argv);
  dpy = GDK_DISPLAY();

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

  /* We need to dispose the gdk display so that the GSource for the
   * X11 connection is removed which otherwise nicks all of the
   * events we want sent to clutter.
   */
  g_object_run_dispose (G_OBJECT (gdk_x11_lookup_xdisplay (GDK_DISPLAY ())));

  mb_wm_main_loop(wm);

  mb_wm_object_unref (MB_WM_OBJECT (wm));

#if MBWM_WANT_DEBUG
  mb_wm_object_dump ();
#endif

  return 0;
}
