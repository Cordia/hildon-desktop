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

#ifndef __HD_WINDOW_ACTOR_H__
#define __HD_WINDOW_ACTOR_H__

#include <glib.h>
#include <glib-object.h>
#include <clutter/clutter-x11-texture-pixmap.h>

G_BEGIN_DECLS

#define HD_TYPE_WINDOW_ACTOR            (hd_window_actor_get_type ())
#define HD_WINDOW_ACTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_WINDOW_ACTOR, HdWindowActor))
#define HD_WINDOW_ACTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_WINDOW_ACTOR, HdWindowActorClass))
#define HD_IS_WINDOW_ACTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_WINDOW_ACTOR))
#define HD_IS_WINDOW_ACTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_WINDOW_ACTOR))
#define HD_WINDOW_ACTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_WINDOW_ACTOR, HdWindowActorClass))

typedef struct _HdWindowActor      HdWindowActor;
typedef struct _HdWindowActorClass HdWindowActorClass;
typedef struct _HdWindowActorPrivate HdWindowActorPrivate;

struct _HdWindowActorClass
{
  ClutterX11TexturePixmapClass parent_class;
};

struct _HdWindowActor
{
  ClutterX11TexturePixmap parent;

  HdWindowActorPrivate *priv;
};

GType hd_window_actor_get_type          (void);
void  hd_window_actor_activate          (HdWindowActor *actor);

G_END_DECLS

#endif
