#include <clutter/clutter.h>

/* This just creates a clutter stage and displays a texture on it. It allows PVRTC
   files to be viewed easily (as well as other image types clutter supports) */

// gcc pvr-viewer.c -o pvr-viewer `pkg-config --cflags --libs clutter-eglx-0.8`

int main(int argc, char *argv[])
{
  ClutterActor     *tex, *stage;
  ClutterColor stage_color = { 0x00, 0x00, 0x00, 0xff };

  if (argc!=2) {
    printf("USAGE: tex_load filename.pvr\n");
    return 0;
  }

  clutter_init (&argc, &argv);

  /* Get the stage and set its size and color: */
  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, 800, 480);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  
  tex = clutter_texture_new_from_file(argv[1], 0);
  clutter_container_add_actor(CLUTTER_CONTAINER(stage), tex);

  /* Show the stage: */
  clutter_actor_show (stage);

  /* Start the main loop, so we can respond to events: */
  clutter_main ();

  return 0;
}
