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

#include "hildon-desktop.h"
#include "hd-running-app.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

#include <dbus/dbus.h>

#define HD_RUNNING_APP_GET_PRIVATE(obj)        (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HD_TYPE_RUNNING_APP, HdRunningAppPrivate))

struct _HdRunningAppPrivate
{
  HdLauncherApp *launcher_app;
  HdRunningAppState state;
  GPid pid;
  time_t last_launch;
};

G_DEFINE_TYPE (HdRunningApp, hd_running_app, G_TYPE_OBJECT);

static void
hd_running_app_finalize (GObject *gobject)
{
  HdRunningAppPrivate *priv = HD_RUNNING_APP_GET_PRIVATE (gobject);
  if (priv->launcher_app)
    {
      g_object_unref (priv->launcher_app);
      priv->launcher_app = NULL;
    }
  G_OBJECT_CLASS (hd_running_app_parent_class)->finalize (gobject);
}

static void
hd_running_app_class_init (HdRunningAppClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdRunningAppPrivate));

  gobject_class->finalize = hd_running_app_finalize;
}

static void
hd_running_app_init (HdRunningApp *app)
{
  app->priv = HD_RUNNING_APP_GET_PRIVATE (app);
}

HdRunningApp *
hd_running_app_new (HdLauncherApp *launcher)
{
  HdRunningApp *result = g_object_new (HD_TYPE_RUNNING_APP, 0);
  if (result)
    hd_running_app_set_launcher_app (result, launcher);

  return result;
}

HdRunningAppState
hd_running_app_get_state (HdRunningApp *app)
{
  HdRunningAppPrivate *priv = HD_RUNNING_APP_GET_PRIVATE (app);
  return priv->state;
}
void
hd_running_app_set_state (HdRunningApp *app, HdRunningAppState state)
{
  HdRunningAppPrivate *priv = HD_RUNNING_APP_GET_PRIVATE (app);
  priv->state = state;
}

gboolean
hd_running_app_is_executing (HdRunningApp *app)
{
  return (hd_running_app_get_state (app) == HD_APP_STATE_PRESTARTED ||
          hd_running_app_get_state (app) == HD_APP_STATE_SHOWN);
}

GPid
hd_running_app_get_pid (HdRunningApp *app)
{
  HdRunningAppPrivate *priv = HD_RUNNING_APP_GET_PRIVATE (app);
  return priv->pid;
}

void
hd_running_app_set_pid (HdRunningApp *app, GPid pid)
{
  HdRunningAppPrivate *priv = HD_RUNNING_APP_GET_PRIVATE (app);
  priv->pid = pid;
}

time_t
hd_running_app_get_last_launch (HdRunningApp *app)
{
  HdRunningAppPrivate *priv = HD_RUNNING_APP_GET_PRIVATE (app);
  return priv->last_launch;
}

void
hd_running_app_set_last_launch (HdRunningApp *app, time_t time)
{
  HdRunningAppPrivate *priv = HD_RUNNING_APP_GET_PRIVATE (app);
  priv->last_launch = time;
}

HdLauncherApp  *
hd_running_app_get_launcher_app  (HdRunningApp *app)
{
  g_return_val_if_fail (app, NULL);
  HdRunningAppPrivate *priv = HD_RUNNING_APP_GET_PRIVATE (app);
  return priv->launcher_app;
}

void
hd_running_app_set_launcher_app  (HdRunningApp *app,
                                  HdLauncherApp *launcher)
{
  HdRunningAppPrivate *priv = HD_RUNNING_APP_GET_PRIVATE (app);
  if (priv->launcher_app == launcher)
    return;

  if (priv->launcher_app)
    g_object_unref (G_OBJECT (priv->launcher_app));
  if (launcher)
    priv->launcher_app = g_object_ref (launcher);
  else
    priv->launcher_app = NULL;
}

const gchar *
hd_running_app_get_service (HdRunningApp *app)
{
  HdLauncherApp *launcher = hd_running_app_get_launcher_app (app);
  if (!launcher)
    return NULL;
  return hd_launcher_app_get_service (launcher);
}

const gchar *
hd_running_app_get_id (HdRunningApp *app)
{
  const gchar *result = NULL;
  HdLauncherApp *launcher = hd_running_app_get_launcher_app (app);
  if (!launcher)
    return "<unknown>";
  result = hd_launcher_item_get_id (HD_LAUNCHER_ITEM (launcher));
  return result ? result : "<unknown>";
}
