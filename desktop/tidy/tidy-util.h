#ifndef _TIDY_UTIL
#define _TIDY_UTIL

#include <clutter/clutter.h>

void
tidy_util_preload_glyphs (char *font, ...);

/* To handle the problem where we might be doing nested writes to
 * offscreen buffers */
void tidy_util_cogl_push_offscreen_buffer(CoglHandle fbo);
void tidy_util_cogl_pop_offscreen_buffer(void);

#endif
