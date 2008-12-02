/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Marc Ordinas i Llopis <marc.ordinasillopis@collabora.co.uk>
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

/*
 * A HdLauncherCat contains the info on a launcher category.
 *
 * This code is based on the old hd-dummy-launcher.
 */

#ifndef __HD_LAUNCHER_CAT_H__
#define __HD_LAUNCHER_CAT_H__

#include "hd-launcher-item.h"

G_BEGIN_DECLS

#define HD_TYPE_LAUNCHER_CAT              (hd_launcher_cat_get_type ())
#define HD_LAUNCHER_CAT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_LAUNCHER_CAT, HdLauncherCat))
#define HD_IS_LAUNCHER_CAT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_LAUNCHER_CAT))
#define HD_LAUNCHER_CAT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_LAUNCHER_CAT, HdLauncherCatClass))
#define HD_IS_LAUNCHER_CAT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_LAUNCHER_CAT))
#define HD_LAUNCHER_CAT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_LAUNCHER_CAT, HdLauncherCatClass))

typedef struct _HdLauncherCat         HdLauncherCat;
typedef struct _HdLauncherCatClass    HdLauncherCatClass;

struct _HdLauncherCat
{
  HdLauncherItem parent_instance;
};

struct _HdLauncherCatClass
{
  HdLauncherItemClass parent_class;
};

GType           hd_launcher_cat_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __HD_LAUNCHER_CAT_H__ */
