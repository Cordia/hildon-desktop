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


#ifndef __HD_COMP_WINDOW_H__

#include <glib/gmacros.h>
#include <matchbox/core/mb-wm-object.h>
#include <matchbox/comp-mgr/mb-wm-comp-mgr.h>

G_BEGIN_DECLS

typedef struct HdCompWindowClass   HdCompWindowClass;
typedef struct HdCompWindow        HdCompWindow;
typedef struct HdCompWindowPrivate HdCompWindowPrivate;

#define HD_COMP_WINDOW(c)       ((HdCompWindow*)(c))
#define HD_COMP_WINDOW_CLASS(c) ((HdCompWindowClass*)(c))
#define HD_TYPE_COMP_WINDOW     (hd_comp_window_class_type ())

struct HdCompWindow
{
    MBWMCompMgrClient           parent;

    HdCompWindowPrivate        *priv;
};

struct HdCompWindowClass
{
    MBWMCompMgrClientClass      parent;

    void                        (* effect) (HdCompWindow               *window,
                                            MBWMCompMgrClientEvent      event);
};

int hd_comp_window_class_type (void);

void hd_comp_window_effect (HdCompWindow *window, MBWMCompMgrClientEvent event);
void hd_comp_window_activate (HdCompWindow *window);

G_END_DECLS

#endif /* __HD_COMP_WINDOW_H__ */
