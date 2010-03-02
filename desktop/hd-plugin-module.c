/*
 * This file is part of libhildondesktop
 *
 * Copyright (C) 2006, 2008 Nokia Corporation.
 *
 * Author:  Moises Martinez <moises.martinez@nokia.com>
 * Contact: Karoliina Salminen <karoliina.t.salminen@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gmodule.h>

#include "hd-plugin-module.h"

#define HD_PLUGIN_MODULE_GET_PRIVATE(object) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((object), HD_TYPE_PLUGIN_MODULE, HDPluginModulePrivate))

G_DEFINE_TYPE (HDPluginModule, hd_plugin_module, G_TYPE_TYPE_MODULE);

enum
{
  PROP_0,
  PROP_PATH
};

struct _HDPluginModulePrivate
{
  gchar    *path;
  GModule  *library;

  GList    *gtypes;

  void     (*load)     (HDPluginModule *plugin);
  void     (*unload)   (HDPluginModule *plugin);
};

static void hd_plugin_module_get_property (GObject *object,
                                           guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec);

static void hd_plugin_module_set_property (GObject *object,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec);

static void      hd_plugin_module_finalize (GObject     *object);
static gboolean  hd_plugin_module_load     (GTypeModule *gmodule);
static void      hd_plugin_module_unload   (GTypeModule *gmodule);

static void
hd_plugin_module_class_init (HDPluginModuleClass *class)
{
  GObjectClass     *object_class;
  GTypeModuleClass *type_module_class;

  object_class      = G_OBJECT_CLASS (class);
  type_module_class = G_TYPE_MODULE_CLASS (class);

  object_class->finalize     = hd_plugin_module_finalize;
  object_class->get_property = hd_plugin_module_get_property;
  object_class->set_property = hd_plugin_module_set_property;

  type_module_class->load   = hd_plugin_module_load;
  type_module_class->unload = hd_plugin_module_unload;

  g_type_class_add_private (object_class, sizeof (HDPluginModulePrivate));

  g_object_class_install_property (object_class,
                                   PROP_PATH,
                                   g_param_spec_string("path",
                                                       "path-plugin",
                                                       "Path of the plugin",
                                                       NULL,
                                                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
}

static void
hd_plugin_module_init (HDPluginModule *plugin)
{
  plugin->priv = HD_PLUGIN_MODULE_GET_PRIVATE (plugin);

  plugin->priv->gtypes = NULL;

  plugin->priv->path      = NULL;
  plugin->priv->library   = NULL;

  plugin->priv->load      = NULL;
  plugin->priv->unload    = NULL;
}

static void
hd_plugin_module_finalize (GObject *object)
{
  HDPluginModule *plugin = HD_PLUGIN_MODULE (object);

  g_list_free (plugin->priv->gtypes);
  g_free (plugin->priv->path);

  G_OBJECT_CLASS (hd_plugin_module_parent_class)->finalize (object);
}

static void 
hd_plugin_module_get_property (GObject *object,
                               guint prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
  HDPluginModule *plugin = HD_PLUGIN_MODULE (object);

  switch (prop_id)
    {
    case PROP_PATH:
      g_value_set_string (value, plugin->priv->path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
hd_plugin_module_set_property (GObject *object,
                               guint prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
  HDPluginModule *plugin = HD_PLUGIN_MODULE (object);

  switch (prop_id)
    {
    case PROP_PATH:
      g_free (plugin->priv->path);
      plugin->priv->path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
hd_plugin_module_load (GTypeModule *gmodule)
{
  HDPluginModule *plugin = HD_PLUGIN_MODULE (gmodule);

  if (plugin->priv->path == NULL) 
    {
      g_warning ("Module path not set");

      return FALSE;
    }

  plugin->priv->library = 
    g_module_open (plugin->priv->path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);

  if (!plugin->priv->library) 
    {
      g_warning ("%s", g_module_error ());

      return FALSE;
    }

  if (!g_module_symbol (plugin->priv->library,
                        "hd_plugin_module_load",
                        (void *) &plugin->priv->load)  ||
      !g_module_symbol (plugin->priv->library,
                        "hd_plugin_module_unload",
                        (void *) &plugin->priv->unload)) 
    {
      g_warning ("%s", g_module_error ());

      g_module_close (plugin->priv->library);

      return FALSE;
    }

  /* Initialize the loaded module */
  plugin->priv->load (plugin);

  return TRUE;
}

static void
hd_plugin_module_unload (GTypeModule *gmodule)
{
  HDPluginModule *plugin = HD_PLUGIN_MODULE (gmodule);

  plugin->priv->unload (plugin);

  g_module_close (plugin->priv->library);

  plugin->priv->library  = NULL;
  plugin->priv->load     = NULL;
  plugin->priv->unload   = NULL;

  g_list_free (plugin->priv->gtypes);
  plugin->priv->gtypes = NULL;
}

HDPluginModule *
hd_plugin_module_new (const gchar *path)
{
  HDPluginModule *plugin;

  g_return_val_if_fail (path != NULL, NULL);

  plugin = g_object_new (HD_TYPE_PLUGIN_MODULE,
                         "path", path,
                         NULL);

  return plugin;
}

GObject *
hd_plugin_module_get_object (HDPluginModule *module)
{
  g_return_val_if_fail (HD_IS_PLUGIN_MODULE (module), NULL);

  if (module->priv->gtypes != NULL)
    {
      GType type = GPOINTER_TO_INT (module->priv->gtypes->data);

      return g_object_new (type, NULL);
    }

  return NULL;
}

void
hd_plugin_module_add_type (HDPluginModule *module,
                           GType           type)
{
  static GQuark dl_filename_quark = 0;
  gchar *old_dl_filename;

  g_return_if_fail (HD_IS_PLUGIN_MODULE (module));

  if (module->priv->gtypes != NULL)
    {
      g_warning ("Only one plugin type per module supported.");
      return;
    }

#if 0
  if (!g_type_is_a (type, HD_TYPE_PLUGIN_ITEM))
    {
      g_warning ("The plugin type must implement the HDPluginItem interface.");
      return;
    }
#endif

  /* Create hd-plugin-module-dl-filename quark */
  if (G_UNLIKELY (!dl_filename_quark))
    dl_filename_quark = g_quark_from_static_string (HD_PLUGIN_MODULE_DL_FILENAME);

  /* Set hd-plugin-module-dl-filename data in type */
  old_dl_filename = g_type_get_qdata (type,
                                      dl_filename_quark);
  g_free (old_dl_filename);
  g_type_set_qdata (type,
                    dl_filename_quark,
                    g_strdup (module->priv->path));

  module->priv->gtypes = g_list_append (module->priv->gtypes, GINT_TO_POINTER (type));
}
