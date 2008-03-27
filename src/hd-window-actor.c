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

#include "hd-window-actor.h"
#include "hd-comp-window.h"

enum
{
  PROP_COMP_WINDOW = 1
};

struct _HdWindowActorPrivate
{
  HdCompWindow         *comp_window;
};

static void hd_window_actor_class_init (HdWindowActorClass *klass);
static void hd_window_actor_init       (HdWindowActor *self);
static void hd_window_actor_dispose    (GObject *object);
static void hd_window_actor_finalize   (GObject *object);
static void hd_window_actor_set_property (GObject       *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec);
static void hd_window_actor_get_property (GObject          *object,
                                          guint             prop_id,
                                          GValue           *value,
                                          GParamSpec       *pspec);

G_DEFINE_TYPE (HdWindowActor, hd_window_actor, CLUTTER_X11_TYPE_TEXTURE_PIXMAP);

static void
hd_window_actor_class_init (HdWindowActorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec   *pspec;

  g_type_class_add_private (klass, sizeof (HdWindowActorPrivate));

  object_class->dispose = hd_window_actor_dispose;
  object_class->finalize = hd_window_actor_finalize;
  object_class->set_property = hd_window_actor_set_property;
  object_class->get_property = hd_window_actor_get_property;

  pspec = g_param_spec_pointer ("comp-window",
                                "Comp window",
                                "Composite manager client",
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class,
                                   PROP_COMP_WINDOW,
                                   pspec);
}

static void
hd_window_actor_init (HdWindowActor *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            HD_TYPE_WINDOW_ACTOR,
                                            HdWindowActorPrivate);
}

static void
hd_window_actor_dispose (GObject *object)
{
  G_OBJECT_CLASS (hd_window_actor_parent_class)->dispose (object);
}

static void
hd_window_actor_finalize (GObject *object)
{
  G_OBJECT_CLASS (hd_window_actor_parent_class)->finalize (object);
}

static void
hd_window_actor_set_property (GObject       *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  HdWindowActorPrivate *priv = HD_WINDOW_ACTOR (object)->priv;

  switch (prop_id)
    {
      case PROP_COMP_WINDOW:
          priv->comp_window = g_value_get_pointer (value);
          break;
      default:
          G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          break;
    }

}

static void
hd_window_actor_get_property (GObject          *object,
                              guint             prop_id,
                              GValue           *value,
                              GParamSpec       *pspec)
{
  HdWindowActorPrivate *priv = HD_WINDOW_ACTOR (object)->priv;

  switch (prop_id)
    {
      case PROP_COMP_WINDOW:
          g_value_set_pointer (value, priv->comp_window);
          break;
      default:
          G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
          break;

    }

}

void
hd_window_actor_activate (HdWindowActor *self)
{
  g_return_if_fail (HD_IS_WINDOW_ACTOR (self));

  if (!self->priv->comp_window)
    return;

  hd_comp_window_activate (self->priv->comp_window);
}
