/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2010 Moises Martinez
 *
 * Author:  Moises Martinez <moimart@gmail.com>
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

#ifndef __HD_TN_LAYOUT_H__
#define __HD_TN_LAYOUT_H__

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define HD_TYPE_TN_LAYOUT            (hd_tn_layout_get_type ())
#define HD_TN_LAYOUT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_TN_LAYOUT, HdTnLayout))
#define HD_IS_TN_LAYOUT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_TN_LAYOUT))
#define HD_TN_LAYOUT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_TN_LAYOUT, HdTnLayoutClass))
#define HD_IS_TN_LAYOUT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_TN_LAYOUT))
#define HD_TN_LAYOUT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_TN_LAYOUT, HdTnLayoutClass))

typedef struct _HdTnLayout        HdTnLayout;
typedef struct _HdTnLayoutPrivate HdTnLayoutPrivate;
typedef struct _HdTnLayoutClass   HdTnLayoutClass;

struct _HdTnLayout
{
  GObject parent_instance;

  HdTnLayoutPrivate *priv;
};

struct _HdTnLayoutClass
{
  GObjectClass parent_class;

  void (*calculate) (HdTnLayout *layout, 
		     GList *thumbnails, 
		     ClutterActor *grid); 
 
  gboolean (*within_grid) (HdTnLayout *layout, 
			   ClutterButtonEvent *event,
			   GList *thumbnails,
			   ClutterActor *grid);

  gboolean (*animation_in_progress) (HdTnLayout *layout);

  void (*stop_animation) (HdTnLayout *layout);

  void (*last_active_window) (HdTnLayout *layout,
				 ClutterActor *window);
};

GType hd_tn_layout_get_type (void) G_GNUC_CONST;

/* Static class member */
HdTnLayout *hd_tn_layout_factory_get_layout (void); 

void hd_tn_layout_calculate (HdTnLayout *layout, 
			     GList *thumbnails,
			     ClutterActor *grid);

void hd_tn_layout_get_thumbnail_size (HdTnLayout *layout, guint *width, guint *height);

gboolean hd_tn_layout_within_grid (HdTnLayout *layout, 
				   ClutterButtonEvent *event, 
				   GList *thumbnails, 
				   ClutterActor *grid);

gboolean hd_tn_layout_animation_in_progress (HdTnLayout *layout);

void hd_tn_layout_stop_animation (HdTnLayout *layout);

void hd_tn_layout_last_active_window (HdTnLayout *layout,
				      ClutterActor *window);

#define HD_TYPE_DEFAULT_LAYOUT       (hd_default_layout_get_type ())
#define HD_DEFAULT_LAYOUT(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_DEFAULT_LAYOUT, HdDefaultLayout))
#define HD_IS_DEFAULT_LAYOUT(obj)    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_DEFAULT_LAYOUT))
#define HD_DEFAULT_LAYOUT_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_DEFAULT_LAYOUT, HdDefaultLayoutClass))
#define HD_IS_DEFAULT_LAYOUT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_DEFAULT_LAYOUT))
#define HD_DEFAULT_LAYOUT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_DEFAULT_LAYOUT, HdDefaultLayoutClass))

typedef struct _HdDefaultLayout        HdDefaultLayout;
typedef struct _HdDefaultLayoutPrivate HdDefaultLayoutPrivate;
typedef struct _HdDefaultLayoutClass   HdDefaultLayoutClass;

struct _HdDefaultLayout
{
  HdTnLayout parent_instance;

  HdDefaultLayoutPrivate *priv;
};

struct _HdDefaultLayoutClass
{
  HdTnLayoutClass parent_class;
};

GType hd_default_layout_get_type (void) G_GNUC_CONST;

HdTnLayout *hd_default_layout_new (void);

G_END_DECLS

#endif /* __HD_TN_LAYOUT_H__ */
