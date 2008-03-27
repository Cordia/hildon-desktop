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

#ifndef __HD_MB_WM_PROPS__
#define __HD_MB_WM_PROPS__

#include <matchbox/core/mb-wm-object-props.h>

G_BEGIN_DECLS

#define _MKOPROP(n, type) (((1<<4)+(n<<4))|sizeof(type))

typedef enum HdMbWmProp
  {
    HdMbWmPropActor = _MKOPROP((_MBWMObjectPropLastGlobal + 1),  void*),
    HdMbWmPropWindowGroup = _MKOPROP((_MBWMObjectPropLastGlobal + 2),  void*),
    HdMbWmPropTopWindowGroup = _MKOPROP((_MBWMObjectPropLastGlobal + 3),  void*)
  } HdMbWmProp;

G_END_DECLS

#endif
