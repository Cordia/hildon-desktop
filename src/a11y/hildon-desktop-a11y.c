/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2009 Nokia Corporation.
 *
 * Author:  Alejandro Pinheiro <apinheiro@igalia.com>
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

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <cail/cail.h>

#include "hildon-desktop-a11y.h"
#include "launcher/hda-launcher.h"
#include "home/hda-home-init.h"
#include "tail/tail.h"

#define ENV_VAR      "ENABLE_HILDON_DESKTOP_A11Y"
#define VALUE_STRING "TRUE"

static gboolean
_a11y_invoke_module                    (const gchar  *libname,
                                        gboolean      init);
static char *
_a11y_create_accessibility_module_name (const gchar *libname);

static gboolean
_should_enable_a11y                    ();

static gboolean
_a11y_invoke_module (const gchar  *libname,
                     gboolean      init)
{
  GModule    *handle;
  void      (*invoke_fn) (void);
  const char *method;
  gboolean    retval = FALSE;
  gchar      *module_name;

  if (init)
    method = "gnome_accessibility_module_init";
  else
    method = "gnome_accessibility_module_shutdown";

  module_name = _a11y_create_accessibility_module_name (libname);

  if (!module_name) {
    g_warning ("Accessibility: failed to find module '%s' which "
               "is needed to make this application accessible",
               libname);

  } else if (!(handle = g_module_open (module_name, G_MODULE_BIND_LAZY))) {
    g_warning ("Accessibility: failed to load module '%s': '%s'",
               libname, g_module_error ());

  } else if (!g_module_symbol (handle, method, (gpointer *)&invoke_fn)) {
    g_warning ("Accessibility: error library '%s' does not include "
               "method '%s' required for accessibility support",
               libname, method);
    g_module_close (handle);

  } else {
    g_debug ("Module %s loaded successfully", libname);
    retval = TRUE;
    invoke_fn ();
  }

  g_free (module_name);

  return retval;
}

static gchar *
_a11y_create_accessibility_module_name (const gchar *libname)
{
  gchar *fname;
  gchar *retval;

  fname = g_strconcat (libname, "." G_MODULE_SUFFIX, NULL);

  retval = g_strconcat ("/usr/lib/gtk-2.0/modules", G_DIR_SEPARATOR_S, fname, NULL);

  g_free (fname);

  g_debug ("retval = %s", retval);

  return retval;
}

static gboolean
_should_enable_a11y                    ()
{
  const gchar *value = NULL;
  gint found = 0;

  value = g_getenv (ENV_VAR);
  if (value == NULL)
    {
      return FALSE;
    }

  found = !g_ascii_strcasecmp (value, VALUE_STRING);

  if (found)
    {
      return TRUE;
    }

  return FALSE;
}

/* public methods */
void
hildon_desktop_a11y_init (void)
{
  if (!_should_enable_a11y ())
    {
      g_debug ("Current % doesn't allow a11y support. Avoiding load a11y modules",
               ENV_VAR);
      return;
    }

  /* gail */
  _a11y_invoke_module ("libgail", TRUE);

  /* cail : FIXME, this should be done using a module too */

  cail_accessibility_module_init();

  /* Init the concrete steps */
  hda_launcher_accessibility_init ();
  hda_home_accessibility_init ();
  tail_accessibility_init ();

  /* atk-bridge */
  _a11y_invoke_module ("libatk-bridge", TRUE);
}
