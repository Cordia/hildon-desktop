#include "tidy-util.h"

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
static OffscreenStackEntry offscreen_buffer_stack[4];
static int offscreen_buffer_idx = 0;
/* ------------------------------------------------  */

/*
 * Save the current scissor in @offscreen_buffer_stack[@offscreen_buffer_idx]
 * (== @obe).  Save the new @fbo in @offscreen_buffer_stack[@offscreen_buffer_idx+1]
 * (@obe+1) for the next tidy_util_cogl_push_offscreen_buffer().
 * @offscreen_buffer_stack[0].fbo == %NULL by default, indicating
 * %COGL_WINDOW_BUFFER.
 *
 * NOTE that the modelview matrix will be reset to identity, and will not
 *      be saved if you're going from FBO to another FBO.
 */
void tidy_util_cogl_push_offscreen_buffer(CoglHandle fbo)
{
  g_assert(offscreen_buffer_idx+1 < G_N_ELEMENTS(offscreen_buffer_stack));
  OffscreenStackEntry *obe = &offscreen_buffer_stack[offscreen_buffer_idx++];

  if ((obe->scissor_enabled = glIsEnabled (GL_SCISSOR_TEST)))
    glGetIntegerv (GL_SCISSOR_BOX, obe->scissor_box);

  /* Remove scissoring as we're rendering to a texture */
  glScissor (0, 0, 0, 0);
  glDisable (GL_SCISSOR_TEST);
  cogl_draw_buffer (COGL_OFFSCREEN_BUFFER, fbo);

  obe++;
  obe->fbo = fbo;
}

/* NOTE that the modelview matrix will NOT be saved nor restored
 *      if you're switching back from FBO to another FBO. */
void tidy_util_cogl_pop_offscreen_buffer(void)
{
  g_assert(offscreen_buffer_idx > 0);
  OffscreenStackEntry *obe = &offscreen_buffer_stack[--offscreen_buffer_idx];

  /* Reinstate scissoring state that we saved previously */
  if (obe->scissor_enabled)
    {
      glEnable (GL_SCISSOR_TEST);
      glScissor (obe->scissor_box[0], obe->scissor_box[1],
                 obe->scissor_box[2], obe->scissor_box[3]);
    }
  else
    glDisable (GL_SCISSOR_TEST);

  cogl_draw_buffer (obe->fbo ? COGL_OFFSCREEN_BUFFER : COGL_WINDOW_BUFFER,
                    obe->fbo);
}
