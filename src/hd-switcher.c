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
#include "config.h"
#endif

#include "hd-switcher.h"
#include "hd-window-group.h"
#include "hd-window-actor.h"
#include "hd-stage.h"

#include <clutter/clutter-texture.h>
#include <clutter/clutter-container.h>

#define BUTTON_IMAGE "bg-image.png"

enum
{
  PROP_WINDOW_GROUP = 1,
  PROP_TOP_WINDOW_GROUP
};

struct _HdSwitcherPrivate
{
  ClutterActor         *window_group;
  ClutterActor         *top_window_group;
  ClutterActor         *button;
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

static void hd_switcher_clicked (HdSwitcher *switcher);
static void hd_switcher_item_selected (HdSwitcher *switcher, ClutterActor *actor);

G_DEFINE_TYPE (HdSwitcher, hd_switcher, CLUTTER_TYPE_GROUP);

static void
hd_switcher_class_init (HdSwitcherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec   *pspec;

  g_type_class_add_private (klass, sizeof (HdSwitcherPrivate));

  object_class->dispose = hd_switcher_dispose;
  object_class->finalize = hd_switcher_finalize;
  object_class->set_property = hd_switcher_set_property;
  object_class->get_property = hd_switcher_get_property;

  pspec = g_param_spec_object ("window-group",
                               "Window group",
                               "Group containing the windows",
                               HD_TYPE_WINDOW_GROUP,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class,
                                   PROP_WINDOW_GROUP,
                                   pspec);

  pspec = g_param_spec_object ("top-window-group",
                               "Top window group",
                               "Group containing menus and dialogs",
                               CLUTTER_TYPE_GROUP,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class,
                                   PROP_TOP_WINDOW_GROUP,
                                   pspec);

}

static void
hd_switcher_init (HdSwitcher *self)
{
  GError       *error = NULL;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            HD_TYPE_SWITCHER,
                                            HdSwitcherPrivate);

  self->priv->button = clutter_texture_new_from_file (BUTTON_IMAGE, &error);

  if (error)
    {
      g_error (error->message);
    }

  clutter_container_add_actor (CLUTTER_CONTAINER (self), self->priv->button);
  clutter_actor_set_position (self->priv->button, 0, 0);
  clutter_actor_set_reactive (self->priv->button, TRUE);
  clutter_actor_show (self->priv->button);

  g_signal_connect_swapped (self->priv->button, "button-release-event",
                            G_CALLBACK (hd_switcher_clicked),
                            self);

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
      case PROP_WINDOW_GROUP:
          priv->window_group = g_value_get_object (value);
          clutter_container_add_actor (CLUTTER_CONTAINER (object),
                                       priv->window_group);
          if (priv->top_window_group)
            clutter_container_lower_child (CLUTTER_CONTAINER (object),
                                           priv->window_group,
                                           priv->top_window_group);
          clutter_actor_set_anchor_point_from_gravity (priv->window_group,
                                                       CLUTTER_GRAVITY_CENTER);

          g_signal_connect_swapped (priv->window_group, "item-selected",
                                    G_CALLBACK (hd_switcher_item_selected),
                                    object);
          break;
      case PROP_TOP_WINDOW_GROUP:
          priv->top_window_group = g_value_get_object (value);
          clutter_container_add_actor (CLUTTER_CONTAINER (object),
                                       priv->top_window_group);

          if (priv->window_group)
            clutter_container_raise_child (CLUTTER_CONTAINER (object),
                                           priv->top_window_group,
                                           priv->window_group);

          clutter_container_raise_child (CLUTTER_CONTAINER (object),
                                         priv->button,
                                         priv->top_window_group);


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
      case PROP_WINDOW_GROUP:
          g_value_set_object (value, priv->window_group);
          break;
      case PROP_TOP_WINDOW_GROUP:
          g_value_set_object (value, priv->top_window_group);
          break;
      default:
          G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          break;

    }
}

static void
hd_switcher_clicked (HdSwitcher *switcher)
{
  HdSwitcherPrivate *priv = HD_SWITCHER (switcher)->priv;

  if (!priv->window_group) return;

  hd_window_group_zoom_item (HD_WINDOW_GROUP (priv->window_group), NULL);
}

static void
hd_switcher_item_selected (HdSwitcher *switcher, ClutterActor *actor)
{
  if (!actor)
    {
      hd_stage_grab_pointer (hd_get_default_stage ());
    }
  else
    {
      hd_stage_ungrab_pointer (hd_get_default_stage ());

      if (HD_IS_WINDOW_ACTOR (actor))
        {
          HdWindowActor *window_actor = HD_WINDOW_ACTOR (actor);

          hd_window_actor_activate (window_actor);
        }
    }
}
