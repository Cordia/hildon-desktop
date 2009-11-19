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


/* The code below is to handle stacks of Offscreen buffers - for example when
 * rendering to a tidy-blur-group *while* rendering to a tidy-cached-group.
 * It also deals with properly saving the scissor state, as pretty much all
 * the time we want to avoid it when rendering to a texture, but reinstate it
 * as we left it afterwards.
 *
 * Note that push/pop calls should be matched - and if not, things will
 * segfault. */
/* ------------------------------------------------ */
typedef struct {
  GLboolean scissor_enabled;
  GLint scissor_box[4];

  CoglHandle fbo;
} OffscreenStackEntry;

/* We'd never be rendering to more that 4 buffers at once! most is 2.
 * Position 0 is used to store information about the Window (eg. scissor
 * box) */
OffscreenStackEntry offscreen_buffer_stack[4];
int offscreen_buffer_idx = 0;
/* ------------------------------------------------  */


void tidy_util_cogl_push_offscreen_buffer(CoglHandle fbo) {
  /* save state of scissoring */
  offscreen_buffer_stack[offscreen_buffer_idx].scissor_enabled =
      glIsEnabled (GL_SCISSOR_TEST);
  glGetIntegerv (GL_SCISSOR_BOX,
      offscreen_buffer_stack[offscreen_buffer_idx].scissor_box);
  /* Save current FBO on top of stack */
  offscreen_buffer_idx++;
  offscreen_buffer_stack[offscreen_buffer_idx].fbo = fbo;
  cogl_draw_buffer(COGL_OFFSCREEN_BUFFER, fbo);

  /* Remove scissoring as we're rendering to a texture */
  glScissor (0, 0, 0, 0);
  glDisable (GL_SCISSOR_TEST);
}

void tidy_util_cogl_pop_offscreen_buffer() {
  offscreen_buffer_idx--;
  if (offscreen_buffer_idx > 0)
    cogl_draw_buffer(COGL_OFFSCREEN_BUFFER,
        offscreen_buffer_stack[offscreen_buffer_idx].fbo);
  else
    cogl_draw_buffer(COGL_WINDOW_BUFFER, 0);
  /* Reinstate scissoring state that we saved previously */
  if (offscreen_buffer_stack[offscreen_buffer_idx].scissor_enabled)
    glEnable (GL_SCISSOR_TEST);
  else
    glDisable (GL_SCISSOR_TEST);
  glScissor (offscreen_buffer_stack[offscreen_buffer_idx].scissor_box[0],
             offscreen_buffer_stack[offscreen_buffer_idx].scissor_box[1],
             offscreen_buffer_stack[offscreen_buffer_idx].scissor_box[2],
             offscreen_buffer_stack[offscreen_buffer_idx].scissor_box[3]);


}

