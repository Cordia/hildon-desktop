#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>

#include <clutter/clutter.h>

#include "hd-dummy-launcher.h"

#define HD_LAUNCHER_ICON_SIZE   96

static const ClutterColor icon_color = { 255, 255, 255, 224 };
static const ClutterColor text_color = { 255, 255, 255, 224 };

static guint dummy_counter = 1;

G_DEFINE_TYPE (HdDummyLauncher, hd_dummy_launcher, HD_TYPE_LAUNCHER_ITEM);

static ClutterActor *
hd_dummy_launcher_get_icon (HdLauncherItem *item)
{
  ClutterActor *retval;
  guint size = HD_LAUNCHER_ICON_SIZE;

  retval = clutter_rectangle_new ();
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (retval), &icon_color);
  clutter_actor_set_size (retval, size, size);

  return retval;
}

static ClutterActor *
hd_dummy_launcher_get_label (HdLauncherItem *item)
{
  ClutterActor *retval;
  gchar *text;

  text = g_strdup_printf ("Dummy Item #%d", dummy_counter++);

  retval = clutter_label_new ();
  clutter_label_set_color (CLUTTER_LABEL (retval), &text_color);
  clutter_label_set_text (CLUTTER_LABEL (retval), text);

  return retval;
}

static void
hd_dummy_launcher_released (HdLauncherItem *item)
{
}

static void
hd_dummy_launcher_show (ClutterActor *actor)
{
  HdDummyLauncher *launcher = HD_DUMMY_LAUNCHER (actor);

  CLUTTER_ACTOR_CLASS (hd_dummy_launcher_parent_class)->show (actor);

  clutter_effect_scale (launcher->tmpl, actor,
                        1.2, 1.2,
                        NULL, NULL);
}

static void
hd_dummy_launcher_finalize (GObject *gobject)
{
  HdDummyLauncher *launcher = HD_DUMMY_LAUNCHER (gobject);

  g_object_unref (launcher->tmpl);

  G_OBJECT_CLASS (hd_dummy_launcher_parent_class)->finalize (gobject);
}

static void
hd_dummy_launcher_class_init (HdDummyLauncherClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  HdLauncherItemClass *launcher_class = HD_LAUNCHER_ITEM_CLASS (klass);

  gobject_class->finalize = hd_dummy_launcher_finalize;

  actor_class->show = hd_dummy_launcher_show;

  launcher_class->get_icon = hd_dummy_launcher_get_icon;
  launcher_class->get_label = hd_dummy_launcher_get_label;

  launcher_class->released = hd_dummy_launcher_released;
}

static void
hd_dummy_launcher_init (HdDummyLauncher *launcher)
{
  launcher->tmpl =
    clutter_effect_template_new_for_duration (150, CLUTTER_ALPHA_RAMP);
}

HdLauncherItem *
hd_dummy_launcher_new (HdLauncherItemType ltype)
{
  return g_object_new (HD_TYPE_DUMMY_LAUNCHER,
                       "launcher-type", ltype,
                       NULL);
}
