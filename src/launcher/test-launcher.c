#include <stdlib.h>
#include <glib-object.h>
#include <clutter/clutter.h>

#include "tidy/tidy-finger-scroll.h"

#include "hd-task-launcher.h"
#include "hd-dummy-launcher.h"
#include "hd-launcher-utils.h"

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
  ClutterActor *launcher;
  ClutterColor stage_color = { 0, 0, 0, 255 };

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_actor_set_size (stage, 800, 480);
  g_signal_connect (stage,
                    "key-press-event", G_CALLBACK (on_key_press),
                    NULL);

  launcher = hd_get_application_launcher ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), launcher);
  clutter_actor_set_position (launcher, 0, 64);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
