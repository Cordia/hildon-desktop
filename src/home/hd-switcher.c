/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Johan Bilien <johan.bilien@nokia.com>
 *          Tomas Frydrych <tf@o-hand.com>
 *          Kimmo Hämäläinen <kimmo.hamalainen@nokia.com>
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
#define ICON_IMAGE_LAUNCHER "qgn_general_add"
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
  ClutterActor         *status_menu;

  ClutterActor         *switcher_group;
  ClutterActor         *launcher_group;
  ClutterActor         *menu_group;

  MBWMCompMgrClutter   *comp_mgr;

  /* KIMMO: using full ints */
  gboolean              showing_switcher;
  gboolean              showing_launcher;
  gboolean              switcher_mode; /* What button is visible */
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

static void hd_switcher_hide_buttons (HdSwitcher * switcher);

static void hd_switcher_setup_buttons (HdSwitcher * switcher,
				       gboolean switcher_mode);

static void hd_switcher_hide_switcher (HdSwitcher * switcher);
static void hd_switcher_hide_launcher (HdSwitcher * switcher);

static void hd_switcher_home_mode_changed (HdHome         *home,
					   HdHomeMode      mode,
					   HdSwitcher     *switcher);

static void hd_switcher_group_background_clicked (HdSwitcher   *switcher,
						  ClutterActor *actor);
static void hd_switcher_home_background_clicked (HdSwitcher   *switcher,
						 ClutterActor *actor);
static void hd_switcher_hide_launcher_after_launch (HdSwitcher *switcher);

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
launcher_back_button_clicked (ClutterActor *actor, ClutterEvent *event,
                              gpointer *data)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (data)->priv;
  HdHome	    *home =
      HD_HOME (hd_comp_mgr_get_home (HD_COMP_MGR (priv->comp_mgr)));
  HdSwitcherGroup *group = HD_SWITCHER_GROUP (priv->switcher_group);

  g_debug("launcher_back_button_clicked\n");
  hd_switcher_hide_launcher (HD_SWITCHER (data));

  if (hd_switcher_group_have_children (group))
    {
      priv->showing_switcher = TRUE;
      if (priv->status_area)
        clutter_actor_hide (priv->status_area);
      clutter_actor_show_all (priv->switcher_group);
    }
  else
    {
      hd_home_ungrab_pointer (home);
      if (priv->status_area)
        clutter_actor_show (priv->status_area);
    }

  /* show the buttons again */
  hd_switcher_setup_buttons (HD_SWITCHER (data), TRUE);
}

static void
hd_switcher_constructed (GObject *object)
{
  GError            *error = NULL;
  ClutterActor      *self = CLUTTER_ACTOR (object);
  HdSwitcherPrivate *priv = HD_SWITCHER (object)->priv;
  guint              button_width, button_height;
  HdHome	    *home =
    HD_HOME (hd_comp_mgr_get_home (HD_COMP_MGR (priv->comp_mgr)));

  priv->launcher_group =
    hd_get_application_launcher (HD_SWITCHER(object),
                                 hd_switcher_hide_launcher_after_launch);
  hd_launcher_group_set_back_button_cb (priv->launcher_group,
        G_CALLBACK (launcher_back_button_clicked), object);

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
  g_signal_connect_swapped (home, "background-clicked",
                            G_CALLBACK (hd_switcher_home_background_clicked),
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

  g_debug("hd_switcher_menu_clicked, switcher=%p\n", switcher);
  clutter_actor_show_all (priv->menu_group);
  clutter_actor_raise_top (priv->menu_group);

  return TRUE;
}

static gboolean
hd_switcher_clicked (HdSwitcher *switcher)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  HdHome	    *home =
    HD_HOME (hd_comp_mgr_get_home (HD_COMP_MGR (priv->comp_mgr)));

  g_debug("entered hd_switcher_clicked: switcher=%p ss=%d sl=%d\n", switcher,
          priv->showing_switcher, priv->showing_launcher);
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
      int do_grab = 0;
      g_debug("hd_switcher_clicked: show launcher, switcher=%p\n", switcher);
      /* KIMMO: if switcher was visible, we already have grab */
      if (!priv->showing_switcher)
        do_grab = 1;
      else
        hd_switcher_hide_switcher (switcher);

      /* ensure that home is on top of applications */
      hd_comp_mgr_top_home (HD_COMP_MGR (priv->comp_mgr));

      /* don't show buttons when launcher is visible */
      hd_switcher_hide_buttons (switcher);

      clutter_actor_show (priv->launcher_group);
      priv->showing_launcher = TRUE;
      if (do_grab)
        hd_home_grab_pointer (home);
    }
  else if (priv->showing_launcher ||
	   (!priv->showing_switcher && priv->switcher_mode))
    {
      g_debug("hd_switcher_clicked: show switcher, switcher=%p\n", switcher);
      /* KIMMO: keep a grab when either launcher or switcher is visible */
      if (!priv->showing_launcher && !priv->showing_switcher)
        hd_home_grab_pointer (home);
      else if (priv->showing_launcher)
        hd_switcher_hide_launcher (switcher);

      priv->showing_switcher = TRUE;

      if (priv->status_area)
        clutter_actor_hide (priv->status_area);
      clutter_actor_show_all (priv->switcher_group);

      /*
       * Setup buttons in launcher mode
       */
      hd_switcher_setup_buttons (switcher, FALSE);
    }

  return TRUE;
}

/* KIMMO: this is called from launcher code -- FIXME */
static void
hd_switcher_hide_launcher_after_launch (HdSwitcher *switcher)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;
  g_debug("hd_switcher_hide_launcher_after_launch: switcher=%p\n", switcher);
  if (priv->showing_launcher)
    {
      HdHome	    *home =
        HD_HOME (hd_comp_mgr_get_home (HD_COMP_MGR (priv->comp_mgr)));
      hd_home_ungrab_pointer (home);
      hd_switcher_hide_launcher (switcher);
      /* lower home to show the application */
      hd_comp_mgr_lower_home_actor(HD_COMP_MGR (priv->comp_mgr));
    }
}

static void
hd_switcher_item_selected (HdSwitcher *switcher, ClutterActor *actor)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  g_debug("hd_switcher_item_selected: switcher=%p actor=%p\n", switcher,
          actor);
  if (actor)
    {
      MBWMCompMgrClient     *cclient;
      MBWindowManagerClient *c;
      MBWindowManager       *wm;
      HdCompMgrClient       *hclient;
      HdHome                *home =
            HD_HOME (hd_comp_mgr_get_home (HD_COMP_MGR (priv->comp_mgr)));

      cclient =
	g_object_get_data (G_OBJECT (actor), "HD-MBWMCompMgrClutterClient");

      g_assert (cclient);

      c = cclient->wm_client;

      wm = c->wmref;
      hclient = HD_COMP_MGR_CLIENT (c->cm_client);

      /* KIMMO: ungrab when an item was selected from the switcher */
      if (priv->showing_switcher)
        {
          hd_home_ungrab_pointer (home);
          hd_switcher_hide_switcher (switcher);
        }
      /* KIMMO: lower home to show the application */
      hd_comp_mgr_lower_home_actor(HD_COMP_MGR (priv->comp_mgr));

      if (!hd_comp_mgr_client_is_hibernating (hclient))
	{
          g_debug("hd_switcher_item_selected: calling "
                  "mb_wm_activate_client c=%p\n", c);
          mb_wm_activate_client (wm, c);
	}
      else
	{
          g_debug("hd_switcher_item_selected: calling "
                  "hd_comp_mgr_wakeup_client comp_mgr=%p hclient=%p\n",
                  priv->comp_mgr, hclient);
	  hd_comp_mgr_wakeup_client (HD_COMP_MGR (priv->comp_mgr), hclient);
	}

      hd_switcher_setup_buttons (switcher, TRUE);
    }
  else
    {
      HdSwitcherGroup *group = HD_SWITCHER_GROUP (priv->switcher_group);
      HdHome *home =
            HD_HOME (hd_comp_mgr_get_home (HD_COMP_MGR (priv->comp_mgr)));
      if (!hd_switcher_group_have_children (group))
        {
          /* switcher group is empty and will disappear -> ungrab */
          hd_home_ungrab_pointer (home);
          if (priv->status_area)
            clutter_actor_show (priv->status_area);
        }
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
hd_switcher_add_status_menu (HdSwitcher *switcher, ClutterActor *sa)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  priv->status_menu = sa;
  hd_switcher_hide_buttons (switcher);
}

void
hd_switcher_remove_status_menu (HdSwitcher *switcher, ClutterActor *sa)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  priv->status_menu = NULL;
  hd_switcher_setup_buttons (switcher, TRUE);
}

void
hd_switcher_add_status_area (HdSwitcher *switcher, ClutterActor *sa)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  priv->status_area = sa;

  /* this is normally done in hd_switcher_add_window_actor for app windows */
  hd_switcher_setup_buttons (switcher, TRUE);
}

void
hd_switcher_remove_status_area (HdSwitcher *switcher, ClutterActor *sa)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  priv->status_area = NULL;
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
      if (priv->status_area)
        clutter_actor_show (priv->status_area);
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
hd_switcher_hide_buttons (HdSwitcher * switcher)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  clutter_actor_hide (priv->button_switcher);
  clutter_actor_hide (priv->button_launcher);
  priv->switcher_mode = FALSE;
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
  else if (!priv->switcher_mode && switcher_mode && !have_children)
    {
      /* used after hiding launcher and there is no children */
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

  /*
   * Now request stack sync from the CM, in case there were some changes while
   * the window actors were adopted by the switcher.
   */
  hd_comp_mgr_sync_stacking (HD_COMP_MGR (priv->comp_mgr));
}

static void
hd_switcher_hide_launcher (HdSwitcher *switcher)
{
  /* FIXME once we have the launcher */
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  priv->showing_launcher = FALSE;

  clutter_actor_hide (priv->launcher_group);
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
  g_debug("hd_switcher_home_mode_changed: switcher=%p mode=%d\n",
          switcher, mode);
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
  /* FIXME */
  button_width = TOP_LEFT_BUTTON_WIDTH;

  if (priv->status_area)
    clutter_actor_get_size (priv->status_area,
			    &status_width, &status_height);

  if (control_width)
    *control_width = button_width + status_width;

  if (control_height)
    *control_height = button_height;
}

static void
hd_switcher_home_background_clicked (HdSwitcher   *switcher,
	   			         ClutterActor *actor)
{
  g_debug("hd_switcher_home_background_clicked: switcher=%p\n", switcher);
  hd_switcher_group_background_clicked (switcher, actor);
}

static void
hd_switcher_group_background_clicked (HdSwitcher   *switcher,
				      ClutterActor *actor)
{
  HdSwitcherPrivate *priv = switcher->priv;

  g_debug("hd_switcher_group_background_clicked: switcher=%p\n", switcher);
  if (priv->showing_switcher)
    {
      /* Switcher was visible and the user tapped on the background:
       * ungrab and hide the switcher */
      HdHome *home =
            HD_HOME (hd_comp_mgr_get_home (HD_COMP_MGR (priv->comp_mgr)));
      hd_home_ungrab_pointer (home);
      hd_switcher_deactivate (HD_SWITCHER (switcher));
    }
  else if (priv->showing_launcher)
    {
      /* Launcher was visible and the user tapped on the background:
       * ungrab and hide the launcher */
      HdHome *home =
            HD_HOME (hd_comp_mgr_get_home (HD_COMP_MGR (priv->comp_mgr)));
      hd_home_ungrab_pointer (home);
      hd_switcher_hide_launcher (switcher);
    }

  if (priv->status_area)
    clutter_actor_show (priv->status_area);
  hd_comp_mgr_top_home (HD_COMP_MGR (priv->comp_mgr));
}
