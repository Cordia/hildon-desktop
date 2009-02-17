/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Author:  Alejandro Pinheiro <apinheiro@igalia.com>
 *
 * Based on gailfactory.h from GAIL
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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

#ifndef _HDA_FACTORY_H__
#define _HDA_FACTORY_H__

#include <glib-object.h>
#include <atk/atkobject.h>

#define HDA_ACCESSIBLE_FACTORY(type, type_as_function, opt_create_accessible)	\
										\
static GType									\
type_as_function ## _factory_get_accessible_type (void)				\
{										\
  return type;									\
}										\
										\
static AtkObject*								\
type_as_function ## _factory_create_accessible (GObject *obj)			\
{										\
  ClutterActor *actor;								\
  AtkObject *accessible;							\
										\
  g_return_val_if_fail (CLUTTER_ACTOR (obj), NULL);				\
										\
  actor = CLUTTER_ACTOR (obj);                                                  \
										\
  accessible = opt_create_accessible (actor);					\
										\
  return accessible;								\
}										\
										\
static void									\
type_as_function ## _factory_class_init (AtkObjectFactoryClass *klass)		\
{										\
  klass->create_accessible   = type_as_function ## _factory_create_accessible;	\
  klass->get_accessible_type = type_as_function ## _factory_get_accessible_type;\
}										\
										\
static GType									\
type_as_function ## _factory_get_type (void)					\
{										\
  static GType t = 0;								\
										\
  if (!t)									\
  {										\
    char *name;									\
    static const GTypeInfo tinfo =						\
    {										\
      sizeof (AtkObjectFactoryClass),					\
      NULL, NULL, (GClassInitFunc) type_as_function ## _factory_class_init,			\
      NULL, NULL, sizeof (AtkObjectFactory), 0, NULL, NULL			\
    };										\
										\
    name = g_strconcat (g_type_name (type), "Factory", NULL);			\
    t = g_type_register_static (						\
	    ATK_TYPE_OBJECT_FACTORY, name, &tinfo, 0);				\
    g_free (name);								\
  }										\
										\
  return t;									\
}

#define HDA_ACTOR_SET_FACTORY(widget_type, type_as_function)			\
	atk_registry_set_factory_type (atk_get_default_registry (),		\
				       widget_type,				\
				       type_as_function ## _factory_get_type ())

#endif
