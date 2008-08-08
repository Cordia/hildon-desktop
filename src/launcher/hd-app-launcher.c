#include "hd-app-launcher.h"

#include <string.h>

#include <glib-object.h>
#include <clutter/clutter.h>

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

  guint is_file : 1;
};

G_DEFINE_TYPE (HdAppLauncher, hd_app_launcher, HD_TYPE_LAUNCHER_ITEM);

static const ClutterColor text_color = { 255, 255, 255, 224 };

static ClutterActor *
hd_app_launcher_get_icon (HdLauncherItem *item)
{
  ClutterActor *retval;
  ClutterColor color = { 0, };
  guint size = 96;

  color.red   = g_random_int_range (0, 255);
  color.green = g_random_int_range (0, 255);
  color.blue  = g_random_int_range (0, 255);
  color.alpha = 255;

  retval = clutter_rectangle_new ();
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (retval), &color);
  clutter_actor_set_size (retval, size, size);

  return retval;
}

static ClutterActor *
hd_app_launcher_get_label (HdLauncherItem *item)
{
  HdAppLauncherPrivate *priv = HD_APP_LAUNCHER (item)->priv;
  ClutterActor *retval;

  retval = clutter_label_new ();
  clutter_label_set_color (CLUTTER_LABEL (retval), &text_color);
  clutter_label_set_text (CLUTTER_LABEL (retval), priv->name);

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

  priv->service = g_key_file_get_string (key_file,
                                         HD_DESKTOP_ENTRY_GROUP,
                                         HD_DESKTOP_ENTRY_SERVICE,
                                         NULL);

  priv->text_domain = g_key_file_get_string (key_file,
                                             HD_DESKTOP_ENTRY_GROUP,
                                             HD_DESKTOP_ENTRY_TEXT_DOMAIN,
                                             NULL);

  priv->preload_image = g_key_file_get_string (key_file,
                                               HD_DESKTOP_ENTRY_GROUP,
                                               HD_DESKTOP_ENTRY_PRELOAD_ICON,
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
