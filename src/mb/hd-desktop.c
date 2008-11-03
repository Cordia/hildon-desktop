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

#include "hd-desktop.h"
#include "hd-comp-mgr.h"
#include "hd-home-applet.h"
#include <matchbox/theme-engines/mb-wm-theme.h>

#include <gconf/gconf-client.h>

#include <stdio.h>

#define THEME_RC_FILE ".osso/current-gtk-theme"
#define THEME_DIR "/usr/share/themes/"
#define BACKGROUNDS_DESKTOP_FILE "/usr/share/backgrounds/%s.desktop"
#define BACKGROUNDS_DESKTOP_KEY_FILE "X-File%d"
#define BACKGROUND_GCONF_KEY "/apps/hildon_home/view_%d/bg_image"
#define MAX_BACKGROUNDS 4

static void
hd_desktop_realize (MBWindowManagerClient *client);

static Bool
hd_desktop_request_geometry (MBWindowManagerClient *client,
			     MBGeometry            *new_geometry,
			     MBWMClientReqGeomType  flags);

static MBWMStackLayerType
hd_desktop_stacking_layer (MBWindowManagerClient *client);

static void
hd_desktop_theme_change (MBWindowManagerClient *client);

static void
hd_desktop_stack (MBWindowManagerClient *client, int flags);

static void
hd_desktop_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client     = (MBWindowManagerClientClass *)klass;

  client->client_type    = MBWMClientTypeDesktop;
  client->geometry       = hd_desktop_request_geometry;
  client->stacking_layer = hd_desktop_stacking_layer;
  client->stack          = hd_desktop_stack;
  client->theme_change   = hd_desktop_theme_change;
  client->realize        = hd_desktop_realize;

#if MBWM_WANT_DEBUG
  klass->klass_name = "HdDesktop";
#endif
}

static void
hd_desktop_destroy (MBWMObject *this)
{
}

static int
hd_desktop_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient    *client = MB_WM_CLIENT (this);
  MBWindowManager          *wm = NULL;
  MBGeometry                geom;

  wm = client->wmref;

  if (!wm)
    return 0;

  client->stacking_layer = MBWMStackLayerBottom;

  mb_wm_client_set_layout_hints (client,
				 LayoutPrefFullscreen|LayoutPrefVisible);

  /*
   * Initialize window geometry, so that the frame size is correct
   */
  geom.x      = 0;
  geom.y      = 0;
  geom.width  = wm->xdpy_width;
  geom.height = wm->xdpy_height;

  hd_desktop_request_geometry (client, &geom,
					 MBWMClientReqGeomForced);

  return 1;
}

int
hd_desktop_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (HdDesktopClass),
	sizeof (HdDesktop),
	hd_desktop_init,
	hd_desktop_destroy,
	hd_desktop_class_init
      };
      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_BASE, 0);
    }

  return type;
}

static Bool
hd_desktop_request_geometry (MBWindowManagerClient *client,
			     MBGeometry            *new_geometry,
			     MBWMClientReqGeomType  flags)
{
  if (flags & (MBWMClientReqGeomIsViaLayoutManager|MBWMClientReqGeomForced))
    {
      client->frame_geometry.x      = new_geometry->x;
      client->frame_geometry.y      = new_geometry->y;
      client->frame_geometry.width  = new_geometry->width;
      client->frame_geometry.height = new_geometry->height;

      mb_wm_client_geometry_mark_dirty (client);

      return True; /* Geometry accepted */
    }
  return False;
}

static MBWMStackLayerType
hd_desktop_stacking_layer (MBWindowManagerClient *client)
{
  if (client->wmref->flags & MBWindowManagerFlagDesktop)
    return MBWMStackLayerMid;

  return MBWMStackLayerBottom;
}

static gchar *
get_current_theme (void)
{
  gchar *theme_rc_filename;
  gchar *theme_rc_contents = NULL;
  gchar *theme = NULL;

  theme_rc_filename = g_build_filename (g_get_home_dir (),
                                        THEME_RC_FILE,
                                        NULL);

  if (g_file_get_contents (theme_rc_filename,
                           &theme_rc_contents,
                           NULL,
                           NULL))
    {
      gchar *sep;

      theme = strstr (theme_rc_contents, THEME_DIR);
      if (!theme)
        goto cleanup;

      theme += strlen (THEME_DIR);

      sep = strstr (theme, "/");
      if (sep)
        {
          *sep = 0;
          theme = g_strdup (theme);
        }
      else
        {
          g_free (theme);
          theme = NULL;
          goto cleanup;
        }
    }

cleanup:
  g_free (theme_rc_contents);
  g_free (theme_rc_filename);

  return theme;
}

static void
hd_desktop_theme_change (MBWindowManagerClient *client)
{
  gchar *theme, *backgrounds_file_name = NULL;
  GKeyFile *backgrounds = NULL;
  GError *error = NULL;
  GConfClient *gconf_client = NULL;
  guint i;

  theme = get_current_theme ();
  if (!theme)
    goto cleanup;

  backgrounds_file_name = g_strdup_printf (BACKGROUNDS_DESKTOP_FILE, theme);

  backgrounds = g_key_file_new ();
  if (!g_key_file_load_from_file (backgrounds,
                                  backgrounds_file_name,
                                  G_KEY_FILE_NONE,
                                  &error))
    {
      g_warning ("Could not load default background definition desktop files. %s",
                 error->message);
      g_error_free (error);
      goto cleanup;
    }

  gconf_client = gconf_client_get_default ();

  for (i = 0; i < MAX_BACKGROUNDS; i++)
    {
      gchar *desktop_key;
      gchar *background_image;

      /* The backgrounds are numbered from 1 to 4 in backgrounds/[theme].desktop */
      desktop_key = g_strdup_printf (BACKGROUNDS_DESKTOP_KEY_FILE,
                                     i + 1);

      background_image = g_key_file_get_string (backgrounds,
                                                G_KEY_FILE_DESKTOP_GROUP,
                                                desktop_key,
                                                NULL);

      if (background_image)
        {
          gchar *gconf_key;

          gconf_key = g_strdup_printf (BACKGROUND_GCONF_KEY, i);

          if (!gconf_client_set_string (gconf_client,
                                        gconf_key,
                                        background_image,
                                        &error))
            {
              g_warning ("Could not set background image in GConf. %s", error->message);
              g_error_free (error);
              error = NULL;
            }

          g_free (gconf_key);
        }

      g_free (desktop_key);
      g_free (background_image);
    }

cleanup:
  g_free (theme);
  g_free (backgrounds_file_name);
  if (backgrounds)
    g_key_file_free (backgrounds);
  if (gconf_client)
    g_object_unref (gconf_client);
}

static void
hd_desktop_realize (MBWindowManagerClient *client)
{
  /*
   * Must reparent the window to our root, otherwise we restacking of
   * pre-existing windows might fail.
   */
  printf ("#### realizing desktop\n ####");

  XReparentWindow(client->wmref->xdpy, MB_WM_CLIENT_XWIN(client),
		  client->wmref->root_win->xwindow, 0, 0);

  return;
}

static void
hd_desktop_stack (MBWindowManagerClient *client,
		  int                    flags)
{
  /* Stack to highest/lowest possible possition in stack */
  MBWMList  *l, *l_start;
  HdCompMgr *hmgr = HD_COMP_MGR (client->wmref->comp_mgr);
  gint       current_view = hd_comp_mgr_get_current_home_view_id (hmgr);
  gint       n_layers;
  gint       i;

  n_layers = hd_comp_mgr_get_home_applet_layer_count (hmgr, current_view);

  mb_wm_stack_move_top(client);

  l_start = mb_wm_client_get_transients (client);

  /*
   * First, we stack all applets (and their transients) according to their
   * stacking layer.
   */
  for (i = 0; i < n_layers; ++i)
    {
      l = l_start;

      while (l)
	{
	  MBWindowManagerClient *c = l->data;
	  if (HD_IS_HOME_APPLET (c))
	    {
	      HdHomeApplet *applet = HD_HOME_APPLET (c);

	      if ((applet->view_id < 0 || applet->view_id == current_view) &&
		  applet->applet_layer == i)
		{
		  mb_wm_client_stack (c, flags);
		}
	    }
	  else
	    mb_wm_client_stack (c, flags);

	  l = l->next;
	}
    }

  /*
   * Now we stack any other clients.
   */
  l = l_start;

  while (l)
    {
      MBWindowManagerClient *c = l->data;
      if (!HD_IS_HOME_APPLET (c))
	mb_wm_client_stack (c, flags);

      l = l->next;
    }

  mb_wm_util_list_free (l_start);
}

MBWindowManagerClient*
hd_desktop_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client = MB_WM_CLIENT(mb_wm_object_new (HD_TYPE_DESKTOP,
					  MBWMObjectPropWm,           wm,
					  MBWMObjectPropClientWindow, win,
					  NULL));

  return client;
}

