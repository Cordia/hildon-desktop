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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hd-notification.h"

#include <clutter/clutter.h>
#include "tidy/tidy-stylable.h"

enum
{
  PROP_HEADING = 1,
  PROP_TEXT,
  PROP_ICON,
};

struct _HdNotificationPrivate
{
  ClutterActor             *group;
  ClutterActor             *icon;
  ClutterActor             *heading;
  ClutterActor             *text;
};

static void hd_notification_class_init (HdNotificationClass *klass);
static void hd_notification_init       (HdNotification *self);
static void hd_notification_dispose    (GObject *object);
static void hd_notification_finalize   (GObject *object);

static void hd_notification_set_property (GObject      *object,
					  guint         prop_id,
					  const GValue *value,
					  GParamSpec   *pspec);

static void hd_notification_get_property (GObject      *object,
					  guint         prop_id,
					  GValue       *value,
					  GParamSpec   *pspec);

static void hd_notification_constructed (GObject *object);

G_DEFINE_TYPE (HdNotification, hd_notification, CLUTTER_TYPE_GROUP);

static void
hd_notification_class_init (HdNotificationClass *klass)
{
  GObjectClass      *object_class = G_OBJECT_CLASS (klass);
  GParamSpec        *pspec;

  g_type_class_add_private (klass, sizeof (HdNotificationPrivate));

  object_class->dispose      = hd_notification_dispose;
  object_class->finalize     = hd_notification_finalize;
  object_class->set_property = hd_notification_set_property;
  object_class->get_property = hd_notification_get_property;
  object_class->constructed  = hd_notification_constructed;

  pspec = g_param_spec_string ("heading",
			       "Notification heading",
			       "Notification heading",
			       NULL,
			       G_PARAM_READWRITE);

  g_object_class_install_property (object_class, PROP_HEADING, pspec);

  pspec = g_param_spec_string ("text",
			       "Notification text",
			       "Notification text",
			       NULL,
			       G_PARAM_READWRITE);

  g_object_class_install_property (object_class, PROP_TEXT, pspec);

  pspec = g_param_spec_string ("icon",
			       "Notification icon",
			       "Notification icon",
			       NULL,
			       G_PARAM_WRITABLE);

  g_object_class_install_property (object_class, PROP_ICON, pspec);
}

static void
hd_notification_constructed (GObject *object)
{
  ClutterActor             *a, *g;
  HdNotification           *self = HD_NOTIFICATION (object);
  HdNotificationPrivate    *priv = self->priv;
  ClutterColor              clr_default = {0x44,0x44,0x44,0xff};
  ClutterColor             *fg_color;

  tidy_stylable_get (TIDY_STYLABLE (object), "fg-color", &fg_color, NULL);

  if (!fg_color)
        fg_color = &clr_default;

  g = clutter_group_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (object), g);

  a = clutter_label_new ();
  clutter_label_set_color (CLUTTER_LABEL (a), fg_color);
  clutter_container_add_actor (CLUTTER_CONTAINER (g), a);
  priv->heading = a;

  a = clutter_label_new ();
  clutter_label_set_color (CLUTTER_LABEL (a), fg_color);
  clutter_container_add_actor (CLUTTER_CONTAINER (g), a);
  priv->text = a;

  a = clutter_texture_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (g), a);
  priv->icon = a;

  if (fg_color != &clr_default)
    clutter_color_free (fg_color);
}

static void
hd_notification_init (HdNotification *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
					    HD_TYPE_NOTIFICATION,
					    HdNotificationPrivate);
}

static void
hd_notification_dispose (GObject *object)
{
  G_OBJECT_CLASS (hd_notification_parent_class)->dispose (object);
}

static void
hd_notification_finalize (GObject *object)
{
  G_OBJECT_CLASS (hd_notification_parent_class)->finalize (object);
}

static void
hd_notification_set_property (GObject       *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
  HdNotification        *self = HD_NOTIFICATION (object);
  HdNotificationPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_TEXT:
      {
	const gchar * str = g_value_get_string (value);
	clutter_label_set_text (CLUTTER_LABEL (priv->text), str);
      }
      break;
    case PROP_HEADING:
      {
	const gchar * str = g_value_get_string (value);
	clutter_label_set_text (CLUTTER_LABEL (priv->heading), str);
      }
      break;
    case PROP_ICON:
      {
	const gchar * str = g_value_get_string (value);
	clutter_texture_set_from_file (CLUTTER_TEXTURE (priv->icon), str,
				       NULL);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hd_notification_get_property (GObject      *object,
			   guint         prop_id,
			   GValue       *value,
			   GParamSpec   *pspec)
{
  HdNotificationPrivate *priv = HD_NOTIFICATION (object)->priv;

  switch (prop_id)
    {
    case PROP_HEADING:
      {
	const gchar *str;
	str = clutter_label_get_text (CLUTTER_LABEL (priv->heading));
	g_value_set_string (value, str);
      }
      break;
    case PROP_TEXT:
      {
	const gchar *str;
	str = clutter_label_get_text (CLUTTER_LABEL (priv->text));
	g_value_set_string (value, str);
      }
      break;
    case PROP_ICON:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

