/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
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

#ifndef __HD_TASK_MANAGER_H__
#define __HD_TASK_MANAGER_H__

#include <glib.h>
#include <glib-object.h>

#include <libhildondesktop/libhildondesktop.h>

G_BEGIN_DECLS

#define HD_TYPE_TASK_MANAGER            (hd_task_manager_get_type ())
#define HD_TASK_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_TASK_MANAGER, HDTaskManager))
#define HD_TASK_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  HD_TYPE_TASK_MANAGER, HDTaskManagerClass))
#define HD_IS_TASK_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_TASK_MANAGER))
#define HD_IS_TASK_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  HD_TYPE_TASK_MANAGER))
#define HD_TASK_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  HD_TYPE_TASK_MANAGER, HDTaskManagerClass))

typedef struct _HDTaskManager        HDTaskManager;
typedef struct _HDTaskManagerClass   HDTaskManagerClass;
typedef struct _HDTaskManagerPrivate HDTaskManagerPrivate;

struct _HDTaskManager 
{
  GObject gobject;

  HDTaskManagerPrivate *priv;
};

struct _HDTaskManagerClass 
{
  GObjectClass parent_class;
};

GType            hd_task_manager_get_type     (void);

HDTaskManager *hd_task_manager_get            (void);

GtkTreeModel    *hd_task_manager_get_model    (HDTaskManager *manager);

void             hd_task_manager_install_task (HDTaskManager *manager,
                                               GtkTreeIter     *iter);

G_END_DECLS

#endif /* __HD_TASK_MANAGER_H__ */
