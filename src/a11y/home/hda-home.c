/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Author:  Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
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

/**
 * SECTION:hda-home
 * @short_description: Implementation of the ATK interfaces for a #HdHome
 * @see_also: #HdHome
 *
 * #HdaHome implements the required ATK interfaces of #HdHome
 * In concrete it set a proper ATK_ROLE and add two actions to switch
 * between homes.
 *
 * Anyway, hd-home using a X window in order to manage the press and release
 * events, so the press/release/click actions could not work as normal.
 *
 * For the moment, you can bypass this problem (from testing POV) using the
 * event generator:
 * <example>
 * <title>Using Event Generator to click on home</title>
 * <programlisting>
 *   import atspi
 * <!-- -->
 *   ev = atspi.eventGenerator (300, 300, 1)
 *   ev.click (300, 300, 1)
 *  </programlisting>
 *  </example>
 *
 * In the same way, you could use this approach to change between homes, but
 * the actions are more easy to use:
 * <example>
 * <title>Using Event Generator to change home</title>
 * <programlisting>
 *   import atspi
 * <!-- -->
 *   ev = atspi.eventGenerator (300, 300, 1)
 *   point1 = [300, 300, 1]
 *   point2 = [320, 300, 1]
 *   ev.drag (point1, point2, 1)
 *  </programlisting>
 *  </example>
 *
 */

#include <cail/cail-actor.h>

#include "home/hd-home.h"
#include "home/hd-home-view-container.h"

#include "hda-home.h"

/* GObject */
static void
hda_home_class_init                             (HdaHomeClass *klass);

static void
hda_home_init                                   (HdaHome *root);

/* AtkObject.h */

static void
hda_home_initialize                             (AtkObject *obj,
                                                 gpointer   data);
/* AtkAction -> new actions */
static ClutterActor*
_hda_home_get_home_container                    (CailActor *cail_actor);

static void
_hda_home_next_home_action                      (CailActor *cail_actor);

static void
_hda_home_previous_home_action                  (CailActor *cail_actor);


G_DEFINE_TYPE (HdaHome, hda_home, CAIL_TYPE_ACTOR);

static void
hda_home_class_init                             (HdaHomeClass *klass)
{
/*   GObjectClass *gobject_class = G_OBJECT_CLASS (klass); */
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  /* AtkObject */
  class->initialize = hda_home_initialize;
}

static void
hda_home_init                                   (HdaHome *home)
{
  CailActor *cail_actor = NULL;

  /* Adding extra actions */
  cail_actor = CAIL_ACTOR (home);
  cail_actor_add_action (cail_actor, "next-home", NULL, NULL,
                         _hda_home_next_home_action);

  cail_actor_add_action (cail_actor, "previous-home", NULL, NULL,
                         _hda_home_previous_home_action);
}


AtkObject*
hda_home_new                                    (ClutterActor *home)
{
  GObject   *object     = NULL;
  AtkObject *accessible = NULL;

  object = g_object_new (HDA_TYPE_HOME, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, home);

  return accessible;
}

/* AtkObject */

static void
hda_home_initialize                             (AtkObject   *obj,
                                                 gpointer    data)
{
  ATK_OBJECT_CLASS (hda_home_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_PANEL;
}


/* AtkAction -> using CailActor adding actions approach */
static ClutterActor*
_hda_home_get_home_container                    (CailActor *cail_actor)
{
  HdaHome      *self      = NULL;
  ClutterActor *home      = NULL;
  ClutterActor *stage     = NULL;
  ClutterActor *container = NULL;

  g_return_if_fail (HDA_IS_HOME (cail_actor));

  self = HDA_HOME (cail_actor);
  home = CLUTTER_ACTOR (atk_gobject_accessible_get_object (ATK_GOBJECT_ACCESSIBLE (self)));

  stage = clutter_actor_get_stage (home);

  /* This is quite hack, as it depends on the name, but the unique way to get that */
  container = clutter_container_find_child_by_name (CLUTTER_CONTAINER (stage),
                                                    "HdHome:view_container");

  return container;
}

static void
_hda_home_next_home_action                      (CailActor *cail_actor)
{
  ClutterActor *container = NULL;

  container = _hda_home_get_home_container (cail_actor);

  if (container != NULL)
    {
      hd_home_view_container_scroll_to_next (
                      HD_HOME_VIEW_CONTAINER (container), 0);
    }
}

static void
_hda_home_previous_home_action                  (CailActor *cail_actor)
{
  ClutterActor *container = NULL;

  container = _hda_home_get_home_container (cail_actor);

  if (container != NULL)
    {
      hd_home_view_container_scroll_to_previous (
                      HD_HOME_VIEW_CONTAINER (container), 0);
    }
}
