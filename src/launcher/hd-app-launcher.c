#include "hd-app-launcher.h"

#include <string.h>

#include <glib-object.h>
#include <clutter/clutter.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <dbus/dbus.h>

/* desktop entry group */
#define HD_DESKTOP_ENTRY_GROUP          "Desktop Entry"

/* desktop entry keys */
#define HD_DESKTOP_ENTRY_TYPE           "Type"
#define HD_DESKTOP_ENTRY_NAME           "Name"
#define HD_DESKTOP_ENTRY_ICON           "Icon"
#define HD_DESKTOP_ENTRY_CATEGORIES     "Categories"
#define HD_DESKTOP_ENTRY_COMMENT        "Comment"
#define HD_DESKTOP_ENTRY_EXEC           "Exec"
#define HD_DESKTOP_ENTRY_SERVICE        "X-Osso-Service"
#define HD_DESKTOP_ENTRY_TEXT_DOMAIN    "X-Text-Domain"
#define HD_DESKTOP_ENTRY_NO_DISPLAY     "NoDisplay"
#define HD_DESKTOP_ENTRY_PRELOAD_ICON   "X-Osso-Preload-Icon"
#define HD_DESKTOP_ENTRY_USER_POSITION  "X-Osso-User-Position"
#define HD_DESKTOP_ENTRY_PRELOAD_MODE   "X-Maemo-Prestared"

#define HD_APP_LAUNCHER_GET_PRIVATE(obj)        (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HD_TYPE_APP_LAUNCHER, HdAppLauncherPrivate))

struct _HdAppLauncherPrivate
{
  ClutterEffectTemplate *tmpl;

  gchar *filename;

  gchar *item_type;
  gchar *icon_name;
  gchar *name;
  gchar *comment;
  gchar *exec;
  gchar *service;
  gchar *text_domain;
  gchar *preload_image;
  gchar *preload_mode;

  gchar **categories;
  gsize n_categories;

  guint is_file : 1;
};

G_DEFINE_TYPE (HdAppLauncher, hd_app_launcher, HD_TYPE_LAUNCHER_ITEM);

static const ClutterColor text_color = { 100, 100, 100, 224 };

static ClutterActor *
hd_app_launcher_get_icon (HdLauncherItem *item)
{
  HdAppLauncherPrivate *priv = HD_APP_LAUNCHER (item)->priv;
  ClutterActor *retval = NULL;
  guint size = 64;
  GtkIconTheme *icon_theme;
  GtkIconInfo *info;
  const gchar *icon_name = priv->icon_name;

  if (icon_name == NULL)
    /* fallback -- FIXME is this correct? */
    icon_name = "qgn_list_app_installer";

  icon_theme = gtk_icon_theme_get_default();
  info = gtk_icon_theme_lookup_icon(icon_theme, icon_name, size,
                                    GTK_ICON_LOOKUP_NO_SVG);
  if (info != NULL)
    {
      const gchar *fname = gtk_icon_info_get_filename(info);
      g_debug("hd_app_launcher_get_icon: using %s for %s\n", fname, icon_name);
      retval = clutter_texture_new_from_file(fname, NULL);
      clutter_actor_set_size (retval, size, size);

      gtk_icon_info_free(info);
    }
  else
    g_debug("hd_app_launcher_get_icon: couldn't find icon %s\n", icon_name);

  return retval;
}

static ClutterActor *
hd_app_launcher_get_label (HdLauncherItem *item)
{
  HdAppLauncherPrivate *priv = HD_APP_LAUNCHER (item)->priv;
  ClutterActor *retval;
  g_debug("hd_app_launcher_get_label, item=%p label=%s\n", item, priv->name);

  retval = clutter_label_new ();
  clutter_actor_set_width (CLUTTER_ACTOR (retval), 140);
  clutter_label_set_color (CLUTTER_LABEL (retval), &text_color);
  clutter_label_set_text (CLUTTER_LABEL (retval), priv->name);
  clutter_label_set_line_wrap (CLUTTER_LABEL (retval), FALSE);
  clutter_label_set_alignment (CLUTTER_LABEL (retval), PANGO_ALIGN_CENTER);

  return retval;
}

static void
hd_app_launcher_show (ClutterActor *actor)
{
  HdAppLauncher *launcher = HD_APP_LAUNCHER (actor);

  CLUTTER_ACTOR_CLASS (hd_app_launcher_parent_class)->show (actor);

  clutter_effect_scale (launcher->priv->tmpl, actor,
                        1.2, 1.2,
                        NULL, NULL);
}

static void
hd_app_launcher_finalize (GObject *gobject)
{
  HdAppLauncherPrivate *priv = HD_APP_LAUNCHER (gobject)->priv;

  g_object_unref (priv->tmpl);

  g_free (priv->filename);

  g_free (priv->item_type);
  g_free (priv->icon_name);
  g_free (priv->name);
  g_free (priv->comment);
  g_free (priv->exec);
  g_free (priv->service);
  g_free (priv->text_domain);
  g_free (priv->preload_image);
  g_free (priv->preload_mode);

  for (gsize i = 0; i < priv->n_categories; i++)
    g_free (priv->categories[i]);

  g_free (priv->categories);

  G_OBJECT_CLASS (hd_app_launcher_parent_class)->dispose (gobject);
}

static void
hd_app_launcher_class_init (HdAppLauncherClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  HdLauncherItemClass *launcher_class = HD_LAUNCHER_ITEM_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdAppLauncherPrivate));

  gobject_class->finalize = hd_app_launcher_finalize;

  actor_class->show = hd_app_launcher_show;

  launcher_class->get_icon = hd_app_launcher_get_icon;
  launcher_class->get_label = hd_app_launcher_get_label;
}

static void
hd_app_launcher_init (HdAppLauncher *launcher)
{
  launcher->priv = HD_APP_LAUNCHER_GET_PRIVATE (launcher);

  launcher->priv->tmpl =
    clutter_effect_template_new_for_duration (150, CLUTTER_ALPHA_RAMP);
}

HdLauncherItem *
hd_app_launcher_new_from_keyfile (GKeyFile  *key_file,
                                  GError   **error)
{
  HdLauncherItem *retval;

  retval = g_object_new (HD_TYPE_APP_LAUNCHER, NULL);
  hd_app_launcher_load_from_keyfile (HD_APP_LAUNCHER (retval),
                                     key_file,
                                     error);

  return retval;
}

HdLauncherItem *
hd_app_launcher_new_from_file (const gchar  *filename,
                               GError      **error)
{
  HdAppLauncher *launcher;
  GKeyFile *key_file;

  key_file = g_key_file_new ();

  launcher = g_object_new (HD_TYPE_APP_LAUNCHER, NULL);
  if (!g_key_file_load_from_file (key_file, filename, 0, error))
    {
      g_key_file_free (key_file);
      return HD_LAUNCHER_ITEM (launcher);
    }

  if (!hd_app_launcher_load_from_keyfile (launcher, key_file, error))
    {
      g_key_file_free (key_file);
      return HD_LAUNCHER_ITEM (launcher);
    }

  launcher->priv->is_file = TRUE;
  launcher->priv->filename = g_strdup (filename);

  g_key_file_free (key_file);

  return HD_LAUNCHER_ITEM (launcher);
}

gboolean
hd_app_launcher_load_from_keyfile (HdAppLauncher  *launcher,
                                   GKeyFile       *key_file,
                                   GError        **error)
{
  HdAppLauncherPrivate *priv;
  gboolean no_display = FALSE;
  GError *parse_error = NULL;
  gsize n_categories = 0;

  g_return_val_if_fail (HD_IS_APP_LAUNCHER (launcher), FALSE);
  g_return_val_if_fail (key_file != NULL, FALSE);

  priv = launcher->priv;

  if (!g_key_file_has_group (key_file, HD_DESKTOP_ENTRY_GROUP))
    return FALSE;

  if (!g_key_file_has_key (key_file,
                           HD_DESKTOP_ENTRY_GROUP,
                           HD_DESKTOP_ENTRY_TYPE,
                           &parse_error))
    {
      g_propagate_error (error, parse_error);
      return FALSE;
    }

  if (!g_key_file_has_key (key_file,
                           HD_DESKTOP_ENTRY_GROUP,
                           HD_DESKTOP_ENTRY_NAME,
                           &parse_error))
    {
      g_propagate_error (error, parse_error);
      return FALSE;
    }

  /* skip NoDisplay entries */
  no_display = g_key_file_get_boolean (key_file,
                                       HD_DESKTOP_ENTRY_GROUP,
                                       HD_DESKTOP_ENTRY_NO_DISPLAY,
                                       &parse_error);
  if (parse_error)
    g_clear_error (&parse_error);
  else if (no_display)
    return FALSE;

  /* skip non-application types */
  priv->item_type = g_key_file_get_string (key_file,
                                           HD_DESKTOP_ENTRY_GROUP,
                                           HD_DESKTOP_ENTRY_TYPE,
                                           NULL);
  if (!priv->item_type ||
      (strcmp (priv->item_type, "Application") != 0))
    {
      g_free (priv->item_type);
      return FALSE;
    }

  priv->name = g_key_file_get_string (key_file,
                                      HD_DESKTOP_ENTRY_GROUP,
                                      HD_DESKTOP_ENTRY_NAME,
                                      &parse_error);
  if (!priv->name)
    {
      g_free (priv->item_type);
      g_free (priv->name);
      return FALSE;
    }

  priv->icon_name = g_key_file_get_string (key_file,
                                           HD_DESKTOP_ENTRY_GROUP,
                                           HD_DESKTOP_ENTRY_ICON,
                                           NULL);

  priv->comment = g_key_file_get_string (key_file,
                                         HD_DESKTOP_ENTRY_GROUP,
                                         HD_DESKTOP_ENTRY_COMMENT,
                                         NULL);

  priv->categories = g_key_file_get_string_list (key_file,
                                                 HD_DESKTOP_ENTRY_GROUP,
                                                 HD_DESKTOP_ENTRY_CATEGORIES,
                                                 &n_categories,
                                                 NULL);
  priv->n_categories = n_categories;

  priv->service = g_key_file_get_string (key_file,
                                         HD_DESKTOP_ENTRY_GROUP,
                                         HD_DESKTOP_ENTRY_SERVICE,
                                         NULL);
  g_strchomp (priv->service);

  priv->exec = g_key_file_get_string (key_file,
                                      HD_DESKTOP_ENTRY_GROUP,
                                      HD_DESKTOP_ENTRY_EXEC,
                                      NULL);
  g_strchomp (priv->exec);

  priv->text_domain = g_key_file_get_string (key_file,
                                             HD_DESKTOP_ENTRY_GROUP,
                                             HD_DESKTOP_ENTRY_TEXT_DOMAIN,
                                             NULL);

  priv->preload_image = g_key_file_get_string (key_file,
                                               HD_DESKTOP_ENTRY_GROUP,
                                               HD_DESKTOP_ENTRY_PRELOAD_ICON,
                                               NULL);

  priv->preload_mode = g_key_file_get_string (key_file,
                                              HD_DESKTOP_ENTRY_GROUP,
                                              HD_DESKTOP_ENTRY_PRELOAD_MODE,
                                              NULL);

  return TRUE;
}

G_CONST_RETURN gchar *
hd_app_launcher_get_item_type (HdAppLauncher *item)
{
  g_return_val_if_fail (HD_IS_APP_LAUNCHER (item), NULL);

  return item->priv->item_type;
}

G_CONST_RETURN gchar *
hd_app_launcher_get_icon_name (HdAppLauncher *item)
{
  g_return_val_if_fail (HD_IS_APP_LAUNCHER (item), NULL);

  return item->priv->icon_name;
}

G_CONST_RETURN gchar *
hd_app_launcher_get_name (HdAppLauncher *item)
{
  g_return_val_if_fail (HD_IS_APP_LAUNCHER (item), NULL);

  return item->priv->name;
}

G_CONST_RETURN gchar *
hd_app_launcher_get_comment (HdAppLauncher *item)
{
  g_return_val_if_fail (HD_IS_APP_LAUNCHER (item), NULL);

  return item->priv->comment;
}

G_CONST_RETURN gchar *
hd_app_launcher_get_exec (HdAppLauncher *item)
{
  g_return_val_if_fail (HD_IS_APP_LAUNCHER (item), NULL);

  return item->priv->exec;
}

G_CONST_RETURN gchar *
hd_app_launcher_get_service (HdAppLauncher *item)
{
  g_return_val_if_fail (HD_IS_APP_LAUNCHER (item), NULL);

  return item->priv->service;
}

G_CONST_RETURN gchar *
hd_app_launcher_get_text_domain (HdAppLauncher *item)
{
  g_return_val_if_fail (HD_IS_APP_LAUNCHER (item), NULL);

  return item->priv->text_domain;
}

G_CONST_RETURN gchar *
hd_app_launcher_get_preload_image (HdAppLauncher *item)
{
  g_return_val_if_fail (HD_IS_APP_LAUNCHER (item), NULL);

  return item->priv->preload_image;
}

G_CONST_RETURN gchar *
hd_app_launcher_get_preload_mode (HdAppLauncher *item)
{
  g_return_val_if_fail (HD_IS_APP_LAUNCHER (item), NULL);

  return item->priv->preload_mode;
}

gsize
hd_app_launcher_get_n_categories (HdAppLauncher *item)
{
  g_return_val_if_fail (HD_IS_APP_LAUNCHER (item), 0);

  return item->priv->n_categories;
}

gboolean
hd_app_launcher_has_category (HdAppLauncher *item,
                              const gchar   *category)
{
  g_return_val_if_fail (HD_IS_APP_LAUNCHER (item), FALSE);
  g_return_val_if_fail (category != NULL, FALSE);

  for (gsize i = 0; i < item->priv->n_categories; i++)
    {
      if (strcmp (category, item->priv->categories[i]) == 0)
        return TRUE;
    }

  return FALSE;
}


#define SERVICE_NAME_LEN        255
#define PATH_NAME_LEN           255
#define INTERFACE_NAME_LEN      255
#define TMP_NAME_LEN            255

#define OSSO_BUS_ROOT          "com.nokia"
#define OSSO_BUS_ROOT_PATH     "/com/nokia"
#define OSSO_BUS_TOP           "top_application"

static void
hd_app_launcher_activate_service (const gchar *app)
{
  gchar service[SERVICE_NAME_LEN], path[PATH_NAME_LEN],
        interface[INTERFACE_NAME_LEN], tmp[TMP_NAME_LEN];
  DBusMessage *msg = NULL;
  DBusError error;
  DBusConnection *conn;

  g_debug ("%s: app=%s\n", __FUNCTION__, app);

  /* If we have full service name we will use it */
  if (g_strrstr(app, "."))
  {
    g_snprintf(service, SERVICE_NAME_LEN, "%s", app);
    g_snprintf(interface, INTERFACE_NAME_LEN, "%s", service);
    g_snprintf(tmp, TMP_NAME_LEN, "%s", app);
    g_snprintf(path, PATH_NAME_LEN, "/%s", g_strdelimit(tmp, ".", '/'));
  }
  else /* use com.nokia prefix */
  {
    g_snprintf(service, SERVICE_NAME_LEN, "%s.%s", OSSO_BUS_ROOT, app);
    g_snprintf(path, PATH_NAME_LEN, "%s/%s", OSSO_BUS_ROOT_PATH, app);
    g_snprintf(interface, INTERFACE_NAME_LEN, "%s", service);
  }

  dbus_error_init (&error);
  conn = dbus_bus_get (DBUS_BUS_SESSION, &error);
  if (dbus_error_is_set (&error))
  {
    g_warning ("could not start: %s: %s", service, error.message);
    dbus_error_free (&error);
    return;
  }

  msg = dbus_message_new_method_call (service, path, interface, OSSO_BUS_TOP);
  if (msg == NULL)
  {
    g_warning ("failed to create message");
    return;
  }

  if (!dbus_connection_send (conn, msg, NULL))
    g_warning ("dbus_connection_send failed");

  dbus_message_unref (msg);
}

gboolean
hd_app_launcher_activate (HdAppLauncher  *item,
                          GError        **error)
{
  HdAppLauncherPrivate *priv;
  gboolean res = FALSE;

  g_return_val_if_fail (HD_IS_APP_LAUNCHER (item), FALSE);

  priv = item->priv;

  if (priv->service)
    {
      /* launch the application, or if it's already running
       * move it to the top
       */
      hd_app_launcher_activate_service (priv->service);
      return TRUE;
    }

#if 0
  if (hd_wm_is_lowmem_situation ())
    {
      if (!tn_close_application_dialog (CAD_ACTION_OPENING))
        {
          g_set_error (...);
          return FALSE;
        }
    }
#endif

  if (priv->exec)
    {
      gchar *space = strchr (priv->exec, ' ');
      gchar *exec;
      gint argc;
      gchar **argv = NULL;
      GPid child_pid;
      GError *internal_error = NULL;

      g_debug ("Executing %s: `%s'", priv->name, priv->exec);

      if (space)
        {
          gchar *cmd = g_strndup (priv->exec, space - priv->exec);
          gchar *exc = g_find_program_in_path (cmd);

          exec = g_strconcat (exc, space, NULL);

          g_free (cmd);
          g_free (exc);
        }
      else
        exec = g_find_program_in_path (priv->exec);

      if (!g_shell_parse_argv (exec, &argc, &argv, &internal_error))
        {
          g_propagate_error (error, internal_error);

          g_free (exec);
          if (argv)
            g_strfreev (argv);

          return FALSE;
        }

      res = g_spawn_async (NULL,
                           argv, NULL,
                           0,
                           NULL, NULL,
                           &child_pid,
                           &internal_error);
      if (internal_error)
        g_propagate_error (error, internal_error);

      g_free (exec);

      if (argv)
        g_strfreev (argv);

      return res;
    }
  else
    {
#if 0
      g_set_error (...);
#endif
      return FALSE;
    }

  g_assert_not_reached ();

  return FALSE;
}
