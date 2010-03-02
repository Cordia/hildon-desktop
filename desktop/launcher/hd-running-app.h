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
 * A HdRunningApp contains the run-time info of an active app.
 */

#ifndef __HD_RUNNING_APP_H__
#define __HD_RUNNING_APP_H__

#include <glib-object.h>
#include <time.h>

#include "hd-launcher-app.h"

G_BEGIN_DECLS

#define HD_TYPE_RUNNING_APP            (hd_running_app_get_type ())
#define HD_RUNNING_APP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HD_TYPE_RUNNING_APP, HdRunningApp))
#define HD_IS_RUNNING_APP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HD_TYPE_RUNNING_APP))
#define HD_RUNNING_APP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HD_TYPE_RUNNING_APP, HdRunningAppClass))
#define HD_IS_RUNNING_APP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HD_TYPE_RUNNING_APP))
#define HD_RUNNING_APP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HD_TYPE_RUNNING_APP, HdRunningAppClass))

typedef struct _HdRunningApp           HdRunningApp;
typedef struct _HdRunningAppPrivate    HdRunningAppPrivate;
typedef struct _HdRunningAppClass      HdRunningAppClass;

struct _HdRunningApp
{
  GObject parent_instance;

  HdRunningAppPrivate *priv;
};

struct _HdRunningAppClass
{
  GObjectClass parent_class;
};

GType           hd_running_app_get_type (void) G_GNUC_CONST;
HdRunningApp   *hd_running_app_new      (HdLauncherApp *launcher);

typedef enum {
  HD_APP_STATE_INACTIVE = 0,
  HD_APP_STATE_HIBERNATED,
  HD_APP_STATE_PRESTARTED,
  HD_APP_STATE_LOADING,
  HD_APP_STATE_WAKING,
  HD_APP_STATE_SHOWN
} HdRunningAppState;

HdRunningAppState hd_running_app_get_state (HdRunningApp *app);
void              hd_running_app_set_state (HdRunningApp *app,
                                             HdRunningAppState state);
gboolean hd_running_app_is_executing     (HdRunningApp *app);
gboolean hd_running_app_is_hibernating   (HdRunningApp *app);
gboolean hd_running_app_is_inactive      (HdRunningApp *app);

HdLauncherApp  *hd_running_app_get_launcher_app  (HdRunningApp *app);
void            hd_running_app_set_launcher_app  (HdRunningApp *app,
                                                  HdLauncherApp *launcher);

GPid hd_running_app_get_pid (HdRunningApp *app);
void hd_running_app_set_pid (HdRunningApp *app, GPid pid);
time_t hd_running_app_get_last_launch (HdRunningApp *app);
void   hd_running_app_set_last_launch (HdRunningApp *app, time_t time);

/* Some convenience functions. */
const gchar *hd_running_app_get_service (HdRunningApp *app);
const gchar *hd_running_app_get_id      (HdRunningApp *app);

G_END_DECLS

#endif /* __HD_RUNNING_APP_H__ */
