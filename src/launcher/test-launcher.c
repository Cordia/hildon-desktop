#include <stdlib.h>
#include <glib-object.h>
#include <clutter/clutter.h>

#include "tidy/tidy-finger-scroll.h"

#include "hd-task-launcher.h"
#include "hd-dummy-launcher.h"

static ClutterActor *top_launcher = NULL;
static ClutterActor *sub_launcher = NULL;

static ClutterTimeline *timeline   = NULL;

static ClutterBehaviour *zoom_behaviour = NULL;
static ClutterBehaviour *fade_behaviour = NULL;

static ClutterEffectTemplate *tmpl = NULL;

typedef struct
{
  HdTaskLauncher *launcher;
  guint n_items;
  guint current_pos;
} PopulateClosure;

static gboolean
populate_top_launcher (gpointer data)
{
  PopulateClosure *closure = data;
  HdLauncherItem *item;

  if (g_random_int_range (0, 10) < 4)
    item = hd_dummy_launcher_new (HD_APPLICATION_LAUNCHER);
  else
    item = hd_dummy_launcher_new (HD_SECTION_LAUNCHER);

  hd_task_launcher_add_item (closure->launcher, item);

  g_print ("item [%d] is %s\n",
           closure->current_pos + 1,
           hd_launcher_item_get_item_type (item) == HD_APPLICATION_LAUNCHER
             ? "application"
             : "section");

  closure->current_pos += 1;

  if (closure->current_pos == closure->n_items)
    return FALSE;

  return TRUE;
}

static gboolean
populate_sub_launcher (gpointer data)
{
  PopulateClosure *closure = data;
  HdLauncherItem *item;

  item = hd_dummy_launcher_new (HD_APPLICATION_LAUNCHER);
  hd_task_launcher_add_item (closure->launcher, item);

  closure->current_pos += 1;

  if (closure->current_pos == closure->n_items)
    return FALSE;

  return TRUE;
}

static void
populate_launcher_cleanup (gpointer data)
{
  PopulateClosure *closure = data;

  g_object_unref (closure->launcher);
  g_free (closure);
}

static void
lazily_populate_top_launcher (HdTaskLauncher *launcher,
                              gint            n_items)
{
  PopulateClosure *closure;

  closure = g_new0 (PopulateClosure, 1);
  closure->launcher = g_object_ref (launcher);
  closure->n_items = n_items > 0
    ? n_items
    : g_random_int_range (1, (n_items * -1));
  closure->current_pos = 0;

  clutter_threads_add_idle_full (G_PRIORITY_DEFAULT_IDLE + 50,
                                 populate_top_launcher,
                                 closure,
                                 populate_launcher_cleanup);
}

static void
lazily_populate_sub_launcher (HdTaskLauncher *launcher,
                              gint            n_items)
{
  PopulateClosure *closure;

  closure = g_new0 (PopulateClosure, 1);
  closure->launcher = g_object_ref (launcher);
  closure->n_items = n_items > 0
    ? n_items
    : g_random_int_range (1, (n_items * -1));
  closure->current_pos = 0;

  clutter_threads_add_idle_full (G_PRIORITY_DEFAULT_IDLE + 50,
                                 populate_sub_launcher,
                                 closure,
                                 populate_launcher_cleanup);
}

static void
on_top_item_clicked (HdTaskLauncher *launcher,
                     HdLauncherItem *item)
{
  g_print ("%s: item `%s' (type: %s) clicked\n",
           G_STRLOC,
           hd_launcher_item_get_text (item),
           hd_launcher_item_get_item_type (item) == HD_APPLICATION_LAUNCHER
             ? "application"
             : "group");

  if (hd_launcher_item_get_item_type (item) == HD_SECTION_LAUNCHER)
    {
      lazily_populate_sub_launcher (HD_TASK_LAUNCHER (sub_launcher), -12);

      clutter_timeline_set_direction (timeline, CLUTTER_TIMELINE_FORWARD);
      clutter_timeline_rewind (timeline);
      clutter_timeline_start (timeline);

      clutter_actor_show (sub_launcher);

      clutter_effect_fade (tmpl, top_launcher, 0, NULL, NULL);
      clutter_effect_depth (tmpl, top_launcher, -100, NULL, NULL);
    }
  else
    {
      /* no-op */
    }
}

static void
on_sub_item_clicked (HdTaskLauncher *launcher,
                     HdLauncherItem *item)
{
  g_print ("%s: sub item `%s' clicked\n",
           G_STRLOC,
           hd_launcher_item_get_text (item));

  hd_task_launcher_clear (HD_TASK_LAUNCHER (sub_launcher));

  clutter_timeline_set_direction (timeline, CLUTTER_TIMELINE_BACKWARD);
  clutter_timeline_rewind (timeline);
  clutter_timeline_start (timeline);

  clutter_actor_hide (sub_launcher);

  clutter_effect_fade (tmpl, top_launcher, 255, NULL, NULL);
  clutter_effect_depth (tmpl, top_launcher, 0, NULL, NULL);
}

static gboolean
on_key_press (ClutterActor    *actor,
              ClutterKeyEvent *event,
              gpointer         user_data)
{
  guint keyval;

  keyval = clutter_key_event_symbol (event);

  if (keyval == CLUTTER_q || keyval == CLUTTER_Escape)
    {
      clutter_main_quit ();
      return TRUE;
    }

  return FALSE;
}

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  ClutterActor *scroll;
  ClutterColor stage_color = { 0, 0, 0, 255 };

  clutter_init (&argc, &argv);

  timeline = clutter_timeline_new_for_duration (250);

  tmpl = clutter_effect_template_new_for_duration (250, CLUTTER_ALPHA_SINE_INC);

  fade_behaviour =
    clutter_behaviour_opacity_new (clutter_alpha_new_full (timeline,
                                                           CLUTTER_ALPHA_SINE_INC,
                                                           NULL, NULL),
                                   0, 255);
  zoom_behaviour =
    clutter_behaviour_depth_new (clutter_alpha_new_full (timeline,
                                                         CLUTTER_ALPHA_RAMP_INC,
                                                         NULL, NULL),
                                 200, 0);

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_actor_set_size (stage, 800, 480);
  g_signal_connect (stage,
                    "key-press-event", G_CALLBACK (on_key_press),
                    NULL);

  scroll = tidy_finger_scroll_new (TIDY_FINGER_SCROLL_MODE_KINETIC);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), scroll);
  clutter_actor_set_size (scroll, 760, 400);

  top_launcher = hd_task_launcher_new ();
  clutter_actor_set_width (top_launcher, 760);
  clutter_container_add_actor (CLUTTER_CONTAINER (scroll), top_launcher);
  g_signal_connect (top_launcher,
                    "item-clicked", G_CALLBACK (on_top_item_clicked),
                    NULL);

  sub_launcher = hd_task_launcher_new ();
  clutter_actor_set_width (sub_launcher, 760);
  clutter_actor_hide (sub_launcher);
  clutter_stage_add (CLUTTER_STAGE (stage), sub_launcher);
  g_signal_connect (sub_launcher,
                    "item-clicked", G_CALLBACK (on_sub_item_clicked),
                    NULL);

  clutter_behaviour_apply (zoom_behaviour, sub_launcher);
  clutter_behaviour_apply (fade_behaviour, sub_launcher);

  lazily_populate_top_launcher (HD_TASK_LAUNCHER (top_launcher), 64);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (zoom_behaviour);
  g_object_unref (fade_behaviour);

  g_object_unref (timeline);

  return EXIT_SUCCESS;
}
