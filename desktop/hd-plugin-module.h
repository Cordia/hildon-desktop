/*
 * This file is part of libhildondesktop
 *
 * Copyright (C) 2006, 2008 Nokia Corporation.
 *
 * Author:  Moises Martinez <moises.martinez@nokia.com>
 *
 * Based on libhildondesktop.h from hildon-desktop
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

#ifndef __HD_PLUGIN_MODULE_H__
#define __HD_PLUGIN_MODULE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define HD_TYPE_PLUGIN_MODULE         (hd_plugin_module_get_type ())
#define HD_PLUGIN_MODULE(o)   	      (G_TYPE_CHECK_INSTANCE_CAST ((o), HD_TYPE_PLUGIN_MODULE, HDPluginModule))
#define HD_PLUGIN_MODULE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), HD_TYPE_PLUGIN_MODULE, HDPluginModuleClass))
#define HD_IS_PLUGIN_MODULE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), HD_TYPE_PLUGIN_MODULE))
#define HD_IS_PLUGIN_MODULE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), HD_TYPE_PLUGIN_MODULE))
#define HD_PLUGIN_MODULE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), HD_TYPE_PLUGIN_MODULE, HDPluginModuleClass))

#define HD_PLUGIN_MODULE_DL_FILENAME "hd-plugin-module-dl-filename"

typedef struct _HDPluginModule        HDPluginModule;
typedef struct _HDPluginModuleClass   HDPluginModuleClass;
typedef struct _HDPluginModulePrivate HDPluginModulePrivate;

struct _HDPluginModule
{
  GTypeModule parent;

  HDPluginModulePrivate  *priv;
};

struct _HDPluginModuleClass
{
  GTypeModuleClass parent_class;
};

GType           hd_plugin_module_get_type   (void);

HDPluginModule *hd_plugin_module_new        (const gchar    *path);

GObject        *hd_plugin_module_get_object (HDPluginModule *module);

void            hd_plugin_module_add_type   (HDPluginModule *module,
                                             GType           type);

/**
 * SECTION:hd-plugin-module-macros
 * @short_description: Support for the definition of Hildon Desktop plugins.
 * @include: libhildondesktop/libhildondesktop.h
 *
 * To define Hildon Desktop plugins the macros HD_DEFINE_PLUGIN() or 
 * HD_DEFINE_PLUGIN_MODULE_EXTENDED() should be used.
 *
 * They are similar to the G_DEFINE_DYNAMIC_TYPE() macro but adds code to
 * dynamically register the class on module loading.
 *
 * <example>
 * <title>Using HD_DEFINE_PLUGIN_MODULE() to define a Home widget</title>
 * <programlisting>
 * #ifndef __EXAMPLE_HOME_APPLET_H__
 * #define __EXAMPLE_HOME_APPLET_H__
 * #include <libhildondesktop/libhildondesktop.h>
 *
 * G_BEGIN_DECLS
 *
 * typedef struct _ExampleHomeApplet        ExampleHomeApplet;
 * typedef struct _ExampleHomeAppletClass   ExampleHomeAppletClass;
 *   
 * struct _ExampleHomeApplet
 * {
 *   HDHomePluginItem parent;
 * };
 *
 * struct _ExampleHomeAppletClass
 * {
 *   HDHomePluginItemClass parent;
 * };
 *   
 * GType example_home_applet_get_type (void);
 *
 * G_END_DECLS
 * 
 * #endif
 * </programlisting>
 * <programlisting>
 * #include "example-home-applet.h"
 * 
 * HD_DEFINE_PLUGIN_MODULE (ExampleHomeApplet, example_home_applet, HD_TYPE_HOME_PLUGIN_ITEM);
 * 
 * static void
 * example_home_applet_class_finalize (ExampleHomeAppletClass *klass)
 * {
 * }
 *
 * static void
 * example_home_applet_class_init (ExampleHomeAppletClass *klass)
 * {
 * }
 * 
 * static void
 * example_home_applet_init (ExampleHomeApplet *applet)
 * {
 * }
 * </programlisting>
 * </example>
 **/

/**
 * HD_PLUGIN_MODULE_SYMBOLS:
 * @t_n: The name of the object type, in lowercase, with words separated by '_'.  (ex: object_type)
 *
 * Defines exported functions to load and unload the modules. It is used by 
 * HD_DEFINE_PLUGIN_MODULE() and should usually not used directly.
 *
 **/
#define HD_PLUGIN_MODULE_SYMBOLS(t_n)					\
G_MODULE_EXPORT void hd_plugin_module_load (HDPluginModule *plugin);	\
void hd_plugin_module_load (HDPluginModule *plugin)			\
{									\
  t_n##_register_type (G_TYPE_MODULE (plugin));				\
  hd_plugin_module_add_type (plugin, t_n##_get_type ());		\
}									\
G_MODULE_EXPORT void hd_plugin_module_unload (HDPluginModule *plugin); 	\
void hd_plugin_module_unload (HDPluginModule *plugin)			\
{									\
}

/**
 * HD_PLUGIN_MODULE_SYMBOLS_CODE:
 * @t_n: The name of the object type, in lowercase, with words separated by '_'.  (ex: object_type)
 * @CODE_LOAD: code executed when the plugin is loaded.
 * @CODE_UNLOAD: code executed when the plugin is unloaded.
 *
 * Defines exported functions to load and unload the modules. It is used by 
 * HD_DEFINE_PLUGIN_MODULE_EXTENDED() and should usually not used directly. 
 *
 **/
#define HD_PLUGIN_MODULE_SYMBOLS_CODE(t_n, CODE_LOAD, CODE_UNLOAD)	\
G_MODULE_EXPORT void hd_plugin_module_load (HDPluginModule *plugin); 	\
void hd_plugin_module_load (HDPluginModule *plugin)		 	\
{									\
  t_n##_register_type (G_TYPE_MODULE (plugin));				\
  hd_plugin_module_add_type (plugin, t_n##_get_type ());		\
  { CODE_LOAD }								\
}									\
G_MODULE_EXPORT void hd_plugin_module_unload (HDPluginModule *plugin); 	\
void hd_plugin_module_unload (HDPluginModule *plugin)			\
{									\
  { CODE_UNLOAD }							\
}

/**
 * HD_DEFINE_PLUGIN_MODULE_EXTENDED:
 * @TN: The name of the object type, in Camel case. (ex: ObjectType)
 * @t_n: The name of the object type, in lowercase, with words separated by '_'.  (ex: object_type)
 * @T_P: The GType of the parent (ex: #STATUSBAR_TYPE_ITEM)
 * @CODE: Custom code that gets inserted in the *_register_type() function 
 * @CODE_LOAD: code executed when the plugin is loaded.
 * @CODE_UNLOAD: code executed when the plugin is unloaded.
 *
 * Register an object supplied by a plugin in Hildon Desktop.
 *
 * See also G_DEFINE_DYNAMIC_TYPE().
 */
#define HD_DEFINE_PLUGIN_MODULE_EXTENDED(TN, t_n, T_P, CODE, CODE_LOAD, CODE_UNLOAD)   	\
G_DEFINE_DYNAMIC_TYPE_EXTENDED (TN, t_n, T_P, 0, CODE)                             	\
HD_PLUGIN_MODULE_SYMBOLS_CODE (t_n, CODE_LOAD, CODE_UNLOAD)

/**
 * HD_DEFINE_PLUGIN_MODULE:
 * @TN: The name of the object type, in Camel case. (ex: ObjectType)
 * @t_n: The name of the object type, in lowercase, with words separated by '_'.  (ex: object_type)
 * @T_P: The GType of the parent (ex: #STATUSBAR_TYPE_ITEM)
 *
 * Register an object supplied by a plugin in Hildon Desktop.
 * 
 * See also to G_DEFINE_DYNAMIC_TYPE().
 */
#define HD_DEFINE_PLUGIN_MODULE(TN, t_n, T_P)			\
HD_DEFINE_PLUGIN_MODULE_EXTENDED (TN, t_n, T_P, {}, {}, {})


G_END_DECLS

#endif /*__HD_PLUGIN_MODULE_H__*/
