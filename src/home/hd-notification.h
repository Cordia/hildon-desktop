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

#ifndef __HD_NOTIFICATION_H__
#define __HD_NOTIFICATION_H__

#include <glib.h>
#include <glib-object.h>

#include "tidy/tidy-frame.h"

G_BEGIN_DECLS

#define HD_TYPE_NOTIFICATION            (hd_notification_get_type ())
#define HD_NOTIFICATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_NOTIFICATION, HdNotification))
#define HD_NOTIFICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_NOTIFICATION, HdNotificationClass))
#define HD_IS_NOTIFICATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_NOTIFICATION))
#define HD_IS_NOTIFICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_NOTIFICATION))
#define HD_NOTIFICATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_NOTIFICATION, HdNotificationClass))

typedef struct _HdNotification        HdNotification;
typedef struct _HdNotificationClass   HdNotificationClass;
typedef struct _HdNotificationPrivate HdNotificationPrivate;

struct _HdNotificationClass
{
  TidyFrameClass parent_class;
};

struct _HdNotification
{
  TidyFrame                 parent;

  HdNotificationPrivate    *priv;
};

GType hd_notification_get_type (void);

void hd_notification_set_heading (HdNotification *notification, const gchar *txt);
void hd_notification_set_text (HdNotification *notification, const gchar *txt);
void hd_notification_set_icon (HdNotification *notification, const gchar *txt);

G_END_DECLS

#endif
