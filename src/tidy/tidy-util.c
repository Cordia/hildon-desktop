#include "tidy-util.h"

/*  Hack to (mostly) fill glyph cache, useful on MBX.
 *
 *  FIXME: untested
*/
void
tidy_util_preload_glyphs (char *font, ...)
{
  va_list args;

  va_start (args, font);

  while (font)
    {
      /* Hold on to your hat.. */
      ClutterActor *foo;
      ClutterColor  text_color = { 0xff, 0xff, 0xff, 0xff };

      foo = clutter_label_new_full
	                (font,
			 "abcdefghijklmnopqrstuvwxyz"
			 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			 "1234567890&()*.,';:-_+=[]{}#@?><\"!`%\\|/ ",
			 &text_color);
      if (foo)
	{
	  clutter_actor_realize(foo);
	  clutter_actor_paint(foo);
	  g_object_unref (foo);
	}

      font = va_arg (args, char*);
    }

  va_end (args);
}

/* ------------------------------------------------ */
/* We'd never be rendering to more that 4 buffers at once! most is 2 */
CoglHandle offscreen_buffer_stack[4];
int offscreen_buffer_idx = -1;
/* ------------------------------------------------  */


void tidy_util_cogl_push_offscreen_buffer(CoglHandle fbo) {
  offscreen_buffer_stack[++offscreen_buffer_idx] = fbo;
  cogl_draw_buffer(COGL_OFFSCREEN_BUFFER, fbo);
}

void tidy_util_cogl_pop_offscreen_buffer() {
  offscreen_buffer_idx--;
  if (offscreen_buffer_idx >= 0)
    cogl_draw_buffer(COGL_OFFSCREEN_BUFFER,
        offscreen_buffer_stack[offscreen_buffer_idx]);
  else
    cogl_draw_buffer(COGL_WINDOW_BUFFER, 0);

}

