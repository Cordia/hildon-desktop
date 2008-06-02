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
#include "hd-comp-mgr.h"
#include "hd-util.h"

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>

#include <matchbox/core/mb-wm.h>
#include <matchbox/core/mb-wm-object.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr-clutter.h>

/*
 * FIXME -- these should be loaded from theme.
 */
#define BUTTON_IMAGE_SWITCHER "bg-image-switcher.png"
#define BUTTON_IMAGE_LAUNCHER "bg-image-launcher.png"

enum
{
  PROP_COMP_MGR = 1,
};

struct _HdSwitcherPrivate
{
  ClutterActor         *button_switcher;
  ClutterActor         *button_launcher;

  ClutterActor         *status_area;

  ClutterActor         *switcher_group;
  ClutterActor         *launcher_group;

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

static void hd_switcher_item_selected (HdSwitcher *switcher,
				       ClutterActor *actor);

static void hd_switcher_setup_buttons (HdSwitcher * switcher,
				       gboolean switcher_mode);

static void hd_switcher_hide_switcher (HdSwitcher * switcher);
static void hd_switcher_hide_launcher (HdSwitcher * switcher);

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

  priv->switcher_group = g_object_new (HD_TYPE_SWITCHER_GROUP,
				       "comp-mgr", priv->comp_mgr,
				       NULL);

  clutter_container_add_actor (CLUTTER_CONTAINER (self),
			       priv->switcher_group);

  clutter_actor_hide (priv->switcher_group);

  g_signal_connect_swapped (priv->switcher_group, "item-selected",
			    G_CALLBACK (hd_switcher_item_selected),
			    self);

  priv->button_switcher =
    clutter_texture_new_from_file (BUTTON_IMAGE_SWITCHER, &error);

  if (error)
    {
      g_error (error->message);
    }

  clutter_container_add_actor (CLUTTER_CONTAINER (self),
			       priv->button_switcher);

  clutter_actor_set_position (priv->button_switcher, 0, 0);
  clutter_actor_set_reactive (priv->button_switcher, TRUE);

  clutter_actor_hide (priv->button_switcher);

  g_signal_connect_swapped (priv->button_switcher, "button-release-event",
                            G_CALLBACK (hd_switcher_clicked),
                            self);

  priv->button_launcher =
    clutter_texture_new_from_file (BUTTON_IMAGE_LAUNCHER, &error);

  if (error)
    {
      g_error (error->message);
    }

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

      /* TODO: here we activate the launcher */
#if 0
      clutter_actor_show_all (priv->launcher_group);
#endif
      priv->showing_launcher = TRUE;
    }
  else if (priv->showing_launcher ||
	   (!priv->showing_switcher && priv->switcher_mode))
    {
      hd_switcher_hide_launcher (switcher);

      priv->showing_switcher = TRUE;

      hd_comp_mgr_raise_home_actor (HD_COMP_MGR (priv->comp_mgr));
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

  hd_comp_mgr_lower_home_actor (HD_COMP_MGR (priv->comp_mgr));

  /*
   * Now request stack sync from the CM, in case there were some changes while
   * the window actors were adopted by the switcher.
   */
  hd_comp_mgr_sync_stacking (HD_COMP_MGR (priv->comp_mgr));
}

static void
hd_switcher_hide_launcher (HdSwitcher * switcher)
{
#if 0
  /* FIXME once we have the launcher */
  HdSwitcherPrivate * priv = HD_SWITCHER (switcher)->priv;

  priv->showing_launcher = FALSE;

  clutter_actor_hide_all (priv->launcher_group);
  hd_util_ungrab_pointer ();
#endif
}
