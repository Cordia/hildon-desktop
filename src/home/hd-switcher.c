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
#include "config.h"
#endif

#include "hd-switcher.h"
#include "hd-switcher-group.h"
#include "hd-launcher-utils.h"
#include "hd-comp-mgr.h"
#include "hd-util.h"
#include "hd-edit-menu.h"
#include "hd-home.h"
#include "hd-gtk-utils.h"

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>

#include <matchbox/core/mb-wm.h>
#include <matchbox/core/mb-wm-object.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr-clutter.h>

#define ICON_IMAGE_SWITCHER "qgn_tswitcher_application"
#define ICON_IMAGE_LAUNCHER "qgn_tswitcher_application"
#define BUTTON_IMAGE_MENU     "menu-button.png"

#define TOP_LEFT_BUTTON_HIGHLIGHT_TEXTURE "launcher-button-highlight.png"
#define TOP_LEFT_BUTTON_WIDTH	112
#define TOP_LEFT_BUTTON_HEIGHT	56

enum
{
  PROP_COMP_MGR = 1,
};

struct _HdSwitcherPrivate
{
  ClutterActor         *button_switcher;
  ClutterActor         *button_launcher;
  ClutterActor         *button_menu;

  ClutterActor         *status_area;

  ClutterActor         *switcher_group;
  ClutterActor         *launcher_group;
  ClutterActor         *menu_group;

  MBWMCompMgrClutter   *comp_mgr;

  gboolean              showing_switcher : 1;
  gboolean              showing_launcher : 1;
  gboolean              switcher_mode    : 1; /* What button is visible */
};

static void hd_switcher_class_init (HdSwitcherClass *klass);
static void hd_switcher_init       (HdSwitcher *self);
static void hd_switcher_dispose    (GObject *object);
static void hd_switcher_finalize   (GObject *object);

static void hd_switcher_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec);

static void hd_switcher_get_property (GObject      *object,
                                      guint         prop_id,
                                      GValue       *value,
                                      GParamSpec   *pspec);

static void hd_switcher_constructed (GObject *object);

static gboolean hd_switcher_clicked (HdSwitcher *switcher);

static ClutterActor * hd_switcher_top_left_button_new (const char *icon_name);

static gboolean hd_switcher_menu_clicked (HdSwitcher *switcher);

static void hd_switcher_item_selected (HdSwitcher *switcher,
				       ClutterActor *actor);

static void hd_switcher_setup_buttons (HdSwitcher * switcher,
				       gboolean switcher_mode);

static void hd_switcher_hide_switcher (HdSwitcher * switcher);
static void hd_switcher_hide_launcher (HdSwitcher * switcher);

static void hd_switcher_home_mode_changed (HdHome         *home,
					   HdHomeMode      mode,
					   HdSwitcher     *switcher);

static void hd_switcher_group_background_clicked (HdSwitcher   *switcher,
						  ClutterActor *actor);

G_DEFINE_TYPE (HdSwitcher, hd_switcher, CLUTTER_TYPE_GROUP);

static void
hd_switcher_class_init (HdSwitcherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec   *pspec;

  g_type_class_add_private (klass, sizeof (HdSwitcherPrivate));

  object_class->dispose      = hd_switcher_dispose;
  object_class->finalize     = hd_switcher_finalize;
  object_class->set_property = hd_switcher_set_property;
  object_class->get_property = hd_switcher_get_property;
  object_class->constructed  = hd_switcher_constructed;

  pspec = g_param_spec_pointer ("comp-mgr",
				"Composite Manager",
				"MBWMCompMgrClutter Object",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class,
                                   PROP_COMP_MGR,
                                   pspec);

}

static void
hd_switcher_constructed (GObject *object)
{
  GError            *error = NULL;
  ClutterActor      *self = CLUTTER_ACTOR (object);
  HdSwitcherPrivate *priv = HD_SWITCHER (object)->priv;
  guint              button_width, button_height;

  priv->launcher_group = hd_get_application_launcher ();
  priv->switcher_group = g_object_new (HD_TYPE_SWITCHER_GROUP,
				       "comp-mgr", priv->comp_mgr,
				       NULL);

  clutter_container_add (CLUTTER_CONTAINER (self),
                         priv->switcher_group,
                         priv->launcher_group,
                         NULL);

  clutter_actor_hide (priv->switcher_group);
  clutter_actor_hide (priv->launcher_group);

  g_signal_connect_swapped (priv->switcher_group, "item-selected",
			    G_CALLBACK (hd_switcher_item_selected),
			    self);
  g_signal_connect_swapped (priv->switcher_group, "background-clicked",
                            G_CALLBACK (hd_switcher_group_background_clicked),
                            self);

  priv->menu_group =
    g_object_new (HD_TYPE_EDIT_MENU,
		  "comp-mgr", priv->comp_mgr,
		  "home", hd_comp_mgr_get_home (HD_COMP_MGR (priv->comp_mgr)),
		  NULL);

  clutter_container_add_actor (CLUTTER_CONTAINER (self),
			       priv->menu_group);

  clutter_actor_hide (priv->menu_group);

  priv->button_switcher =
    hd_switcher_top_left_button_new (ICON_IMAGE_SWITCHER);

  clutter_container_add_actor (CLUTTER_CONTAINER (self),
			       priv->button_switcher);

  clutter_actor_set_position (priv->button_switcher, 0, 0);
  clutter_actor_set_reactive (priv->button_switcher, TRUE);

  clutter_actor_hide (priv->button_switcher);

  g_signal_connect_swapped (priv->button_switcher, "button-release-event",
                            G_CALLBACK (hd_switcher_clicked),
                            self);

  priv->button_launcher =
    hd_switcher_top_left_button_new (ICON_IMAGE_LAUNCHER);

  clutter_container_add_actor (CLUTTER_CONTAINER (self),
			       priv->button_launcher);

  clutter_actor_set_position (priv->button_launcher, 0, 0);
  clutter_actor_set_reactive (priv->button_launcher, TRUE);
  clutter_actor_show (priv->button_launcher);

  g_signal_connect_swapped (priv->button_launcher, "button-release-event",
                            G_CALLBACK (hd_switcher_clicked),
                            self);

  clutter_actor_get_size (priv->button_switcher,
			  &button_width, &button_height);

#if 0
  priv->status_area = g_object_new ("HD_STATUS_AREA", NULL);
  clutter_actor_set_position (priv->status_area, button_width, 0);
#endif

  priv->button_menu =
    clutter_texture_new_from_file (
	g_build_filename (HD_DATADIR, BUTTON_IMAGE_MENU, NULL),
	&error);
  if (error)
    {
      g_error (error->message);
      priv->button_menu = clutter_rectangle_new ();
      clutter_actor_set_size (priv->button_menu, 200, 60);
    }

  clutter_container_add_actor (CLUTTER_CONTAINER (self),
			       priv->button_menu);

  clutter_actor_set_position (priv->button_menu, 0, 0);
  clutter_actor_set_reactive (priv->button_menu, TRUE);
  clutter_actor_hide (priv->button_menu);

  g_signal_connect_swapped (priv->button_menu, "button-release-event",
                            G_CALLBACK (hd_switcher_menu_clicked),
                            self);

  g_signal_connect (hd_comp_mgr_get_home (HD_COMP_MGR (priv->comp_mgr)),
		    "mode-changed",
		    G_CALLBACK (hd_switcher_home_mode_changed), self);
}

static void
hd_switcher_init (HdSwitcher *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
					    HD_TYPE_SWITCHER,
					    HdSwitcherPrivate);
}

static void
hd_switcher_dispose (GObject *object)
{
  G_OBJECT_CLASS (hd_switcher_parent_class)->dispose (object);
}

static void
hd_switcher_finalize (GObject *object)
{
  G_OBJECT_CLASS (hd_switcher_parent_class)->finalize (object);
}

static void
hd_switcher_set_property (GObject       *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (object)->priv;

  switch (prop_id)
    {
    case PROP_COMP_MGR:
      priv->comp_mgr = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hd_switcher_get_property (GObject      *object,
                          guint         prop_id,
                          GValue       *value,
                          GParamSpec   *pspec)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (object)->priv;

  switch (prop_id)
    {
    case PROP_COMP_MGR:
      g_value_set_pointer (value, priv->comp_mgr);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* I wonder if this utility function should go somewhere else. I think
 * conceptually the top left button isn't owned by the switcher, so
 * I think it would make sense. */
static ClutterActor *
hd_switcher_top_left_button_new (const char *icon_name)
{
  ClutterActor	  *top_left_button;
  ClutterActor    *top_left_button_bg;
  ClutterActor    *top_left_button_icon;
  ClutterActor    *top_left_button_highlight;
  ClutterColor     clr = {0x0, 0x0, 0x0, 0xff};
  ClutterGeometry  geom;
  GtkIconTheme	  *icon_theme;
  GError	  *error = NULL;

  icon_theme = gtk_icon_theme_get_default ();

  top_left_button = clutter_group_new ();

  top_left_button_bg = clutter_rectangle_new_with_color (&clr);

  /* FIXME - This is a bit yukky, but to allow the button to appear
   * rounded, the background is smaller than the size of the button
   * so that the corner does not poke out of the rounded highlight
   * texture. It assumes that the hightlight texture has full opacity
   * along the bottom and right edges. */
  clutter_actor_set_size (top_left_button_bg,
			  TOP_LEFT_BUTTON_WIDTH - 1,
			  TOP_LEFT_BUTTON_HEIGHT - 1);
  clutter_container_add_actor (CLUTTER_CONTAINER (top_left_button),
			       top_left_button_bg);

  top_left_button_icon =
    hd_gtk_icon_theme_load_icon (icon_theme, icon_name, 48, 0);
  clutter_actor_get_geometry (top_left_button_icon, &geom);
  clutter_actor_set_position (top_left_button_icon,
			      (TOP_LEFT_BUTTON_WIDTH/2)-(geom.width/2),
			      (TOP_LEFT_BUTTON_HEIGHT/2)-(geom.height/2));
  clutter_container_add_actor (CLUTTER_CONTAINER (top_left_button),
			       top_left_button_icon);

  top_left_button_highlight =
    clutter_texture_new_from_file (
      g_build_filename (HD_DATADIR, TOP_LEFT_BUTTON_HIGHLIGHT_TEXTURE, NULL),
      &error);
  if (error)
    {
      g_debug (error->message);
      g_error_free (error);
    }
  else
    {
      clutter_actor_set_size (top_left_button_highlight,
			      TOP_LEFT_BUTTON_WIDTH, TOP_LEFT_BUTTON_HEIGHT);
      clutter_container_add_actor (CLUTTER_CONTAINER (top_left_button),
				   top_left_button_highlight);
    }

  return top_left_button;
}

static gboolean
hd_switcher_menu_clicked (HdSwitcher *switcher)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  clutter_actor_show_all (priv->menu_group);
  clutter_actor_raise_top (priv->menu_group);

  return TRUE;
}

static gboolean
hd_switcher_clicked (HdSwitcher *switcher)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  /*
   * We have the following scenarios:
   *
   * 1. Showing Switcher: the active button is the launch button; we
   *    shutdown the switcher and execute the launcher instead.
   *
   * 2. Showing Launcher: the active button is the switcher button; we shutdown
   *    the the launcher and execute the switcher instead.
   *
   * 3. Neither switcher no launcher visible:
   *    a. We are in switcher mode: we launch the switcher.
   *    b. We are in launcher mode: we launch the launcher.
   */
  if (priv->showing_switcher ||
      (!priv->showing_launcher && !priv->switcher_mode))
    {
      hd_switcher_hide_switcher (switcher);

      /*
       * Setup buttons in switcher mode, if appropriate
       */
      hd_switcher_setup_buttons (switcher, TRUE);

      clutter_actor_show (priv->launcher_group);
      priv->showing_launcher = TRUE;
    }
  else if (priv->showing_launcher ||
	   (!priv->showing_switcher && priv->switcher_mode))
    {
      hd_switcher_hide_launcher (switcher);

      priv->showing_switcher = TRUE;

      clutter_actor_show_all (priv->switcher_group);

      /*
       * Setup buttons in launcher mode
       */
      hd_switcher_setup_buttons (switcher, FALSE);
    }

  return TRUE;
}

static void
hd_switcher_item_selected (HdSwitcher *switcher, ClutterActor *actor)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  if (!actor)
    {
      hd_util_grab_pointer ();
    }
  else
    {
      MBWMCompMgrClient     *cclient;
      MBWindowManagerClient *c;
      MBWindowManager       *wm;
      HdCompMgrClient       *hclient;

      cclient =
	g_object_get_data (G_OBJECT (actor), "HD-MBWMCompMgrClutterClient");

      g_assert (cclient);

      c = cclient->wm_client;

      wm = c->wmref;
      hclient = HD_COMP_MGR_CLIENT (c->cm_client);

      if (!hd_comp_mgr_client_is_hibernating (hclient))
	{
	  mb_wm_activate_client (wm, c);
	}
      else
	{
	  hd_comp_mgr_wakeup_client (HD_COMP_MGR (priv->comp_mgr), hclient);
	}

      hd_switcher_hide_switcher (switcher);
      hd_switcher_setup_buttons (switcher, TRUE);
    }
}

void
hd_switcher_add_window_actor (HdSwitcher * switcher, ClutterActor * actor)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  hd_switcher_group_add_actor (HD_SWITCHER_GROUP (priv->switcher_group),
			       actor);

  /*
   * Setup buttons in switcher mode, if appropriate
   */
  hd_switcher_setup_buttons (switcher, TRUE);
}

void
hd_switcher_remove_window_actor (HdSwitcher * switcher, ClutterActor * actor)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  HdSwitcherGroup   *group = HD_SWITCHER_GROUP (priv->switcher_group);
  gboolean           have_children;

  hd_switcher_group_remove_actor (group, actor);

  have_children = hd_switcher_group_have_children (group);

  if (!have_children && priv->showing_switcher)
    {
      /*
       * Must close the switcher
       */
      hd_switcher_hide_switcher (switcher);
    }

  /*
   * Setup buttons in switcher mode, if appropriate
   */
  hd_switcher_setup_buttons (switcher, TRUE);
}

void
hd_switcher_replace_window_actor (HdSwitcher   * switcher,
				  ClutterActor * old,
				  ClutterActor * new)
{
  HdSwitcherGroup *group = HD_SWITCHER_GROUP (switcher->priv->switcher_group);

  hd_switcher_group_replace_actor (group, old, new);
}

void
hd_switcher_hibernate_window_actor (HdSwitcher   * switcher,
				    ClutterActor * actor)
{
  HdSwitcherGroup *group = HD_SWITCHER_GROUP (switcher->priv->switcher_group);

  hd_switcher_group_hibernate_actor (group, actor);
}

void
hd_switcher_get_button_geometry (HdSwitcher * switcher, ClutterGeometry * geom)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  clutter_actor_get_geometry (priv->button_switcher, geom);
}

static void
hd_switcher_setup_buttons (HdSwitcher * switcher, gboolean switcher_mode)
{
  HdSwitcherPrivate * priv = HD_SWITCHER (switcher)->priv;
  HdSwitcherGroup   * group = HD_SWITCHER_GROUP (priv->switcher_group);
  gboolean            have_children;

  /*
   * Switcher mode can only be entered if there is something to switch
   */
  have_children = hd_switcher_group_have_children (group);

  if (switcher_mode && !priv->switcher_mode && have_children)
    {
      clutter_actor_show (priv->button_switcher);
      clutter_actor_hide (priv->button_launcher);

      priv->switcher_mode = TRUE;
    }
  else if (priv->switcher_mode && (!switcher_mode || !have_children))
    {
      clutter_actor_hide (priv->button_switcher);
      clutter_actor_show (priv->button_launcher);

      priv->switcher_mode = FALSE;
    }
}

gboolean
hd_switcher_showing_switcher (HdSwitcher * switcher)
{
  HdSwitcherPrivate * priv = HD_SWITCHER (switcher)->priv;

  return priv->showing_switcher;
}

static void
hd_switcher_hide_switcher (HdSwitcher * switcher)
{
  HdSwitcherPrivate * priv = HD_SWITCHER (switcher)->priv;

  priv->showing_switcher = FALSE;

  clutter_actor_hide_all (priv->switcher_group);

  hd_util_ungrab_pointer ();

  /*
   * Now request stack sync from the CM, in case there were some changes while
   * the window actors were adopted by the switcher.
   */
  hd_comp_mgr_sync_stacking (HD_COMP_MGR (priv->comp_mgr));
}

static void
hd_switcher_hide_launcher (HdSwitcher * switcher)
{
  /* FIXME once we have the launcher */
  HdSwitcherPrivate * priv = HD_SWITCHER (switcher)->priv;

  priv->showing_launcher = FALSE;

  clutter_actor_hide (priv->launcher_group);
  hd_util_ungrab_pointer ();
}

void
hd_switcher_deactivate (HdSwitcher * switcher)
{
  hd_switcher_hide_switcher (switcher);
  hd_switcher_setup_buttons (switcher, TRUE);
}

static void
hd_switcher_show_menu_button (HdSwitcher * switcher)
{
  HdSwitcherPrivate * priv = HD_SWITCHER (switcher)->priv;

  clutter_actor_hide (priv->button_switcher);
  clutter_actor_hide (priv->button_launcher);
  priv->switcher_mode = !priv->switcher_mode;

  clutter_actor_show_all (priv->button_menu);
  clutter_actor_raise_top (priv->button_menu);
}

static void
hd_switcher_hide_menu_button (HdSwitcher * switcher)
{
  HdSwitcherPrivate * priv = HD_SWITCHER (switcher)->priv;

  clutter_actor_hide (priv->button_menu);
  hd_switcher_setup_buttons (switcher, TRUE);
}

static void
hd_switcher_home_mode_changed (HdHome         *home,
			       HdHomeMode      mode,
			       HdSwitcher     *switcher)
{
  if (mode == HD_HOME_MODE_EDIT)
    hd_switcher_show_menu_button (switcher);
  else
    hd_switcher_hide_menu_button (switcher);
}

void
hd_switcher_get_control_area_size (HdSwitcher *switcher,
				   guint *control_width,
				   guint *control_height)
{
  HdSwitcherPrivate *priv = switcher->priv;
  guint              button_width, button_height;
  guint              status_width = 0, status_height = 0;

  clutter_actor_get_size (priv->button_launcher,
			  &button_width, &button_height);

  if (priv->status_area)
    clutter_actor_get_size (priv->status_area,
			    &status_width, &status_height);

  if (control_width)
    *control_width = button_width + status_width;

  if (control_height)
    *control_height = button_height;
}

static void
hd_switcher_group_background_clicked (HdSwitcher   *switcher,
				      ClutterActor *actor)
{
  HdSwitcherPrivate *priv = switcher->priv;

  hd_switcher_deactivate (HD_SWITCHER (switcher));
  hd_comp_mgr_top_home (HD_COMP_MGR (priv->comp_mgr));
}
