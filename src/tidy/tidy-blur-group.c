/* Created by Gordon Williams <gordon.williams@collabora.co.uk>
 *
 * This class blurs all of its children, also changing saturation and lightness.
 * It renders its children into a half-size texture first, then blurs this into
 * another texture, finally rendering that to the screen. Because of this, when
 * the blurring doesn't change from frame to frame, children and NOT rendered,
 * making this pretty quick. */

#include "tidy-blur-group.h"
#include "tidy-util.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <clutter/clutter-container.h>

#include <cogl/cogl.h>

#include <string.h>
#include <locale.h>

#include "util/hd-transition.h"

/* #define it something sane */
#define TIDY_IS_SANE_BLUR_GROUP(obj)    ((obj) != NULL)

/* This destroys and re-allocates the texture if the actor size changes
 * (eg. screen rotation). We may not want to do this as it takes some time. */
#define RESIZE_TEXTURE 0

/* This fixes the bug where the SGX GLSL compiler uses the current locale for
 * numbers - so '1.0' in a shader will not work when the locale says that ','
 * is a decimal separator.
 */
#define GLSL_LOCALE_FIX 1


#define VIGNETTE_TILES 7
#define VIGNETTE_COLOURS ((VIGNETTE_TILES)/2 + 1)

/* Chequer is 32x32 because that's the smallest SGX will do. If we go smaller
 * it just ends up getting put in a block that size anyway */
#define CHEQUER_SIZE (32)

/* How many steps can we perform per frame - we don't want too many or
 * we get really slow */
#define MAX_STEPS_PER_FRAME 1
/* Currently it seems we have trouble setting this to 2, as it looks like
 * there may be some SGX syncing problem causing the second iteration to
 * work with the texture from *before* the first iteration */

/* The OpenGL fragment shader used to do blur and desaturation.
 * We use 3 samples here arranged in a rough triangle. We need
 * 2 versions as GLES and GL use slightly different syntax */
#if CLUTTER_COGL_HAS_GLES
const char *BLUR_FRAGMENT_SHADER =
"precision lowp float;\n"
"varying mediump vec2  tex_coord;\n"
"varying mediump vec2  tex_coord_a;\n"
"varying mediump vec2  tex_coord_b;\n"
"uniform lowp sampler2D tex;\n"
"void main () {\n"
"  lowp vec4 color = \n"
"       texture2D (tex, vec2(tex_coord_a.x, tex_coord_a.y)) * 0.125 + \n"
"       texture2D (tex, vec2(tex_coord_a.x, tex_coord_b.y)) * 0.125 + \n"
"       texture2D (tex, vec2(tex_coord_b.x, tex_coord_b.y)) * 0.125 + \n"
"       texture2D (tex, vec2(tex_coord_b.x, tex_coord_a.y)) * 0.125 + \n"
"       texture2D (tex, vec2(tex_coord.x, tex_coord.y)) * 0.5; \n"
"  gl_FragColor = color;\n"
"}\n";
const char *BLUR_VERTEX_SHADER =
  "/* Per vertex attributes */\n"
    "attribute vec4     vertex_attrib;\n"
    "attribute vec4     tex_coord_attrib;\n"
    "attribute vec4     color_attrib;\n"
    "\n"
    "/* Transformation matrices */\n"
    "uniform mat4       modelview_matrix;\n"
    "uniform mat4       mvp_matrix; /* combined modelview and projection matrix */\n"
    "uniform mat4       texture_matrix;\n"
    "uniform mediump float blurx;\n"
    "uniform mediump float blury;\n"
    "\n"
    "/* Outputs to the fragment shader */\n"
    "varying lowp vec4       frag_color;\n"
    "varying mediump vec2    tex_coord;\n"
    "varying mediump vec2    tex_coord_a;\n"
    "varying mediump vec2    tex_coord_b;\n"
    "\n"
    "void\n"
    "main (void)\n"
    "{\n"
    "  gl_Position = mvp_matrix * vertex_attrib;\n"
    "  vec4 transformed_tex_coord = texture_matrix * tex_coord_attrib;\n"
    "  tex_coord = transformed_tex_coord.st / transformed_tex_coord.q;\n"
    "  tex_coord_a = tex_coord - vec2(blurx, blury);\n"
    "  tex_coord_b = tex_coord + vec2(blurx, blury);\n"
    "  frag_color = color_attrib;\n"
  "}\n";
const char *SATURATE_FRAGMENT_SHADER =
"precision lowp float;\n"
"varying lowp    vec4  frag_color;\n"
"varying mediump vec2  tex_coord;\n"
"uniform lowp sampler2D tex;\n"
"uniform lowp float saturation;\n"
"void main () {\n"
"  lowp vec4 color = frag_color * texture2D (tex, tex_coord);\n"
"  lowp float lightness = (color.r+color.g+color.b)*0.333*(1.0-saturation); \n"
"  gl_FragColor = vec4(\n"
"                      color.r*saturation + lightness,\n"
"                      color.g*saturation + lightness,\n"
"                      color.b*saturation + lightness,\n"
"                      color.a);\n"
"}\n";
#else
const char *BLUR_FRAGMENT_SHADER = "";
const char *BLUR_VERTEX_SHADER = "";
const char *SATURATE_FRAGMENT_SHADER = "";
#endif /* HAS_GLES */



struct _TidyBlurGroupPrivate
{
  /* Internal TidyBlurGroup stuff */
  ClutterShader *shader_blur;
  ClutterShader *shader_saturate;
  CoglHandle tex_a;
  CoglHandle fbo_a;
  CoglHandle tex_b;
  CoglHandle fbo_b;
  CoglHandle tex_chequer; /* chequer texture used for dimming video overlays */
  gboolean current_is_a;
  gboolean current_is_rotated;

  gboolean use_shader;
  float saturation; /* 0->1 how much colour there is */
  float brightness; /* 1=normal, 0=black */
  float zoom; /* amount to zoom. 1=normal, 0.5=out, 2=double-size */
  gboolean use_alpha; /* whether to use an alpha channel in our textures */
  gboolean use_mirror; /* whether to mirror the edge of teh blurred texture */
  gboolean chequer; /* whether to chequer pattern the contents -
                       for dimming video overlays */

  int blur_step;
  int current_blur_step;
  int max_blur_step;

  gint vignette_colours[VIGNETTE_COLOURS]; /* dimming for the vignette */

  /* if anything changed we need to recalculate preblur */
  gboolean source_changed;
};

/**
 * SECTION:tidy-blur-group
 * @short_description: Pixel-shader modifier class
 *
 * #TidyBlurGroup Renders all of its children to an offscreen buffer,
 * and then renders this buffer to the screen using a pixel shader.
 *
 */

G_DEFINE_TYPE (TidyBlurGroup,
               tidy_blur_group,
               CLUTTER_TYPE_GROUP);

/* When the blur group's children are modified we need to
   re-paint to the source texture. When it is only us that
   has been modified child==NULL */
static
gboolean tidy_blur_group_notify_modified_real(ClutterActor          *actor,
                                              ClutterActor          *child)
{
  if (!TIDY_IS_SANE_BLUR_GROUP(actor))
    return TRUE;

  TidyBlurGroup *container = TIDY_BLUR_GROUP(actor);
  TidyBlurGroupPrivate *priv = container->priv;
  if (child != NULL)
    priv->source_changed = TRUE;
  return TRUE;
}

static void tidy_blur_group_check_shader(TidyBlurGroup *group,
                                         ClutterShader **shader,
                                         const char *fragment_source,
                                         const char *vertex_source)
{
  TidyBlurGroupPrivate *priv = group->priv;

  if (priv->use_shader && !*shader)
   {
     GError *error = NULL;
     char   *old_locale;

#if GLSL_LOCALE_FIX
      old_locale = g_strdup (setlocale (LC_ALL, NULL));
      setlocale (LC_NUMERIC, "C");
#endif

      *shader = clutter_shader_new();
      if (fragment_source)
        clutter_shader_set_fragment_source (*shader, fragment_source, -1);
      if (vertex_source)
        clutter_shader_set_vertex_source (*shader, vertex_source, -1);
      clutter_shader_compile (*shader, &error);

      if (error)
      {
        g_warning ("unable to load shader: %s\n", error->message);
        g_error_free (error);
        priv->use_shader = FALSE;
      }

#if GLSL_LOCALE_FIX
      setlocale (LC_ALL, old_locale);
      g_free (old_locale);
#endif
   }
}

/* Allocate @priv->fbo_[ab]. */
static void
tidy_blur_group_allocate_textures (TidyBlurGroup *self)
{
  TidyBlurGroupPrivate *priv = self->priv;
  guint tex_width, tex_height;

#ifdef __i386__
  if (!cogl_features_available(COGL_FEATURE_OFFSCREEN))
    /* Don't try to allocate FBOs. */
    return;
#endif

#if RESIZE_TEXTURE
  if (priv->fbo_a && priv->fbo_b)
    /* Rotate in _paint() rather than resize. */
    return;
#endif

  /* Free the textures. */
  if (priv->fbo_a)
    {
      cogl_offscreen_unref(priv->fbo_a);
      cogl_texture_unref(priv->tex_a);
      priv->fbo_a = 0;
      priv->tex_a = 0;
    }
  if (priv->fbo_b)
    {
      cogl_offscreen_unref(priv->fbo_b);
      cogl_texture_unref(priv->tex_b);
      priv->fbo_b = 0;
      priv->tex_b = 0;
    }

  /* (Re)create the textures + offscreen buffers.  Downsample by 2.
   * We can specify mipmapping here, but we don't need it. */
  clutter_actor_get_size(CLUTTER_ACTOR(self), &tex_width, &tex_height);
  tex_width  /= 2;
  tex_height /= 2;

  priv->tex_a = cogl_texture_new_with_size(
            tex_width, tex_height, 0, FALSE /* mipmap */,
            priv->use_alpha ? COGL_PIXEL_FORMAT_RGBA_8888 :
                              COGL_PIXEL_FORMAT_RGB_565);
  cogl_texture_set_filters(priv->tex_a, CGL_NEAREST, CGL_NEAREST);
  priv->fbo_a = cogl_offscreen_new_to_texture(priv->tex_a);

  priv->tex_b = cogl_texture_new_with_size(
            tex_width, tex_height, 0, FALSE /* mipmap */,
            priv->use_alpha ? COGL_PIXEL_FORMAT_RGBA_8888 :
                              COGL_PIXEL_FORMAT_RGB_565);
  cogl_texture_set_filters(priv->tex_b, CGL_NEAREST, CGL_NEAREST);
  priv->fbo_b = cogl_offscreen_new_to_texture(priv->tex_b);

  priv->current_blur_step = 0;
  priv->source_changed = TRUE;
}

static gboolean
tidy_blur_group_children_visible(ClutterGroup *group)
{
  gint i;
  ClutterActor *actor;

  for (i = 0, actor = clutter_group_get_nth_child(group, 0);
       actor; actor = clutter_group_get_nth_child(group, ++i))
    {
      if (CLUTTER_IS_GROUP(actor))
        {
          if (tidy_blur_group_children_visible(CLUTTER_GROUP(actor)))
            return TRUE;
        }
      else
        {
          if (CLUTTER_ACTOR_IS_VISIBLE(actor))
            return TRUE;
        }
    }
  return FALSE;
}

/* Perform blur without a pixel shader */
static void
tidy_blur_group_fallback_blur(TidyBlurGroup *group, int tex_width, int tex_height)
{
  ClutterColor    col = { 0x3f, 0x3f, 0x3f, 0x3f };

  TidyBlurGroupPrivate *priv = group->priv;
  CoglHandle tex = priv->current_is_a ? priv->tex_a : priv->tex_b;
  ClutterFixed diffx, diffy;
  diffx = CLUTTER_FLOAT_TO_FIXED(1.0f / tex_width);
  diffy = CLUTTER_FLOAT_TO_FIXED(1.0f / tex_height);

  cogl_blend_func(CGL_ONE, CGL_ZERO);
  cogl_color (&col);
  cogl_texture_rectangle (tex,
                          0, 0,
                          CLUTTER_INT_TO_FIXED (tex_width),
                          CLUTTER_INT_TO_FIXED (tex_height),
                          -diffx, 0, CFX_ONE-diffx, CFX_ONE);
  cogl_blend_func(CGL_ONE, CGL_ONE);
  cogl_texture_rectangle (tex,
                            0, 0,
                            CLUTTER_INT_TO_FIXED (tex_width),
                            CLUTTER_INT_TO_FIXED (tex_height),
                            0, diffy, CFX_ONE+diffx, CFX_ONE);
  cogl_texture_rectangle (tex,
                            0, 0,
                            CLUTTER_INT_TO_FIXED (tex_width),
                            CLUTTER_INT_TO_FIXED (tex_height),
                            0, -diffy, CFX_ONE, CFX_ONE-diffy);
  cogl_texture_rectangle (tex,
                            0, 0,
                            CLUTTER_INT_TO_FIXED (tex_width),
                            CLUTTER_INT_TO_FIXED (tex_height),
                            0, diffy, CFX_ONE, CFX_ONE+diffy);
  cogl_blend_func(CGL_SRC_ALPHA, CGL_ONE_MINUS_SRC_ALPHA);
}

/* If priv->chequer, draw a chequer pattern over the screen */
static void
tidy_blur_group_do_chequer(TidyBlurGroup *group, guint width, guint height)
{
  static const ClutterColor black = { 0x00, 0x00, 0x00, 0xFF };
  TidyBlurGroupPrivate *priv = group->priv;

  if (!priv->chequer)
    return;

  cogl_color (&black);
  cogl_texture_rectangle (priv->tex_chequer,
                          0, 0,
                          CLUTTER_INT_TO_FIXED (width),
                          CLUTTER_INT_TO_FIXED (height),
                          0, 0,
                          CFX_ONE*width/CHEQUER_SIZE,
                          CFX_ONE*height/CHEQUER_SIZE);
}

/* Recursively set texture filtering state on this actor and children, and
 * save the old state in the object. */
static void
recursive_set_linear_texture_filter(ClutterActor *actor, GArray *filters)
{
  if (CLUTTER_IS_CONTAINER(actor))
    clutter_container_foreach(CLUTTER_CONTAINER(actor),
                   (ClutterCallback)recursive_set_linear_texture_filter,
                   filters);
  else if (CLUTTER_IS_TEXTURE(actor))
    {
      ClutterTexture *tex = CLUTTER_TEXTURE(actor);
      ClutterTextureQuality quality;

      quality = clutter_texture_get_filter_quality(tex);
      g_array_append_val(filters, quality);
      clutter_texture_set_filter_quality(tex, GL_LINEAR);
    }
}

/* Recursively set texture filtering state on this actor and children, and
 * save the old state in the object. */
static void
recursive_reset_texture_filter(ClutterActor *actor,
                               const ClutterTextureQuality **filtersp)
{
  if (CLUTTER_IS_CONTAINER(actor))
    clutter_container_foreach(CLUTTER_CONTAINER(actor),
                        (ClutterCallback)recursive_reset_texture_filter,
                        filtersp);
  else if (CLUTTER_IS_TEXTURE(actor))
    {
      clutter_texture_set_filter_quality(CLUTTER_TEXTURE(actor),
                                         **filtersp);
      (*filtersp)++;
    }
}

/* An implementation for the ClutterGroup::paint() vfunc,
   painting all the child actors: */
static void
tidy_blur_group_paint (ClutterActor *actor)
{
  static const ClutterColor white = { 0xff, 0xff, 0xff, 0xff };
  static const ClutterColor bgcol = { 0x00, 0x00, 0x00, 0xff };
  ClutterGroup *group         = CLUTTER_GROUP(actor);
  TidyBlurGroup *container    = TIDY_BLUR_GROUP(group);
  TidyBlurGroupPrivate *priv  = container->priv;
  gint                         steps_this_frame = 0;
  CoglHandle                   current_tex;
  ClutterActorBox              box;
  gint                         width, height, tex_width, tex_height;
  gboolean                     rotate_90;
  ClutterColor                 col;
  GArray                      *filters;
  const ClutterTextureQuality *filters_array;

  if (!TIDY_IS_SANE_BLUR_GROUP(actor))
    return;

  /* If we are rendering normally then shortcut all this, and
   just render directly without the texture */
  if (!tidy_blur_group_source_buffered(actor) ||
      !tidy_blur_group_children_visible(group))
    {
      /* set our buffer as damaged, so next time it gets re-created */
      priv->current_blur_step = 0;
      priv->source_changed = TRUE;
      /* render direct */
      CLUTTER_ACTOR_CLASS(tidy_blur_group_parent_class)->paint(actor);
      tidy_blur_group_do_chequer(container, width, height);
      return;
    }

  clutter_actor_get_allocation_box(actor, &box);
  width  = CLUTTER_UNITS_TO_DEVICE(box.x2 - box.x1);
  height = CLUTTER_UNITS_TO_DEVICE(box.y2 - box.y1);

#ifdef __i386__
  if (!cogl_features_available(COGL_FEATURE_OFFSCREEN))
    { /* If we can't blur properly do something nicer instead :) */
      /* Otherwise crash... */
      CLUTTER_ACTOR_CLASS(tidy_blur_group_parent_class)->paint(actor);
      col.blue = priv->brightness * 255;
      col.red = col.green = priv->brightness * 127;
      col.alpha = (1-priv->saturation) * 255;
      cogl_color (&col);
      cogl_rectangle (0, 0, width, height);
      tidy_blur_group_do_chequer(container, width, height);
      return;
    }
#endif

  tex_width  = cogl_texture_get_width(priv->tex_a);
  tex_height = cogl_texture_get_height(priv->tex_a);

  /* It may be that we have resized, but the texture has not.
   * If so, try and keep blurring 'nice' by rotating so that
   * we don't have a texture that is totally the wrong aspect ratio */
  /* If rotation has changed, trigger a redraw */
  rotate_90 = (tex_width > tex_height) != (width > height);
  if (priv->current_is_rotated != rotate_90)
    {
      priv->current_is_rotated = rotate_90;
      priv->source_changed = TRUE;
      priv->current_blur_step = 0;
    }

  /* Draw children into an offscreen buffer */
  if (priv->source_changed && priv->current_blur_step==0)
    {
      cogl_push_matrix();
      tidy_util_cogl_push_offscreen_buffer(priv->fbo_a);

      if (rotate_90) {
        cogl_scale(CFX_ONE*tex_width/height, CFX_ONE*tex_height/width);
        cogl_translatex(CFX_ONE*height/2, CFX_ONE*width/2, 0);
        cogl_rotate(90, 0, 0, 1);
        cogl_translatex(-CFX_ONE*width/2, -CFX_ONE*height/2, 0);
      } else {
        cogl_scale(CFX_ONE*tex_width/width, CFX_ONE*tex_height/height);
      }

      /* translate a bit to let bilinear filter smooth out intermediate pixels */
      cogl_translatex(CFX_ONE/2,CFX_ONE/2,0);

      cogl_paint_init(&bgcol);
      cogl_color (&white);
      /* Actually do the drawing of the children, but ensure that they are
       * all linear sampled so they are smoothly interpolated. Restore after. */
      filters = g_array_new(FALSE, FALSE, sizeof(ClutterTextureQuality));
      recursive_set_linear_texture_filter(actor, filters);
      CLUTTER_ACTOR_CLASS(tidy_blur_group_parent_class)->paint(actor);
      filters_array = (void *)filters->data;
      recursive_reset_texture_filter(actor, &filters_array);
      g_array_free(filters, TRUE);

      tidy_util_cogl_pop_offscreen_buffer();
      cogl_pop_matrix();

      priv->source_changed = FALSE;
      priv->current_blur_step = 0;
      priv->max_blur_step = 0;
      priv->current_is_a = TRUE;
      //g_debug("Rendered buffer");
      steps_this_frame++;
    }

  while (priv->current_blur_step < priv->blur_step &&
         steps_this_frame<MAX_STEPS_PER_FRAME)
    {
      /* blur one texture into the other */
      tidy_util_cogl_push_offscreen_buffer(
                       priv->current_is_a ? priv->fbo_b : priv->fbo_a);

      if (priv->use_shader && priv->shader_blur)
        {
          clutter_shader_set_is_enabled (priv->shader_blur, TRUE);
          clutter_shader_set_uniform_1f (priv->shader_blur, "blurx",
                                         1.0f / tex_width);
          clutter_shader_set_uniform_1f (priv->shader_blur, "blury",
                                         1.0f / tex_height);
        }

      if (priv->use_shader)
        {
          cogl_blend_func(CGL_ONE, CGL_ZERO);
          cogl_color (&white);
          cogl_texture_rectangle (priv->current_is_a ? priv->tex_a : priv->tex_b,
                                  0, 0,
                                  CLUTTER_INT_TO_FIXED (tex_width),
                                  CLUTTER_INT_TO_FIXED (tex_height),
                                  0, 0,
                                  CFX_ONE,
                                  CFX_ONE);
          cogl_blend_func(CGL_SRC_ALPHA, CGL_ONE_MINUS_SRC_ALPHA);
        }
      else
        tidy_blur_group_fallback_blur(container, tex_width, tex_height);

      if (priv->use_shader && priv->shader_blur)
        clutter_shader_set_is_enabled (priv->shader_blur, FALSE);
      tidy_util_cogl_pop_offscreen_buffer();

      //g_debug("Blurred to %d", priv->current_blur_step);
      priv->current_blur_step++;
      steps_this_frame++;
      priv->max_blur_step = priv->current_blur_step;
      priv->current_is_a = !priv->current_is_a;
      /* We've destroyed our source image, so next time we've zoomed out we
       * need to re-create it */
      priv->source_changed = TRUE;
    }

  /* If we're still not blurred enough, ask to be rendered again... */
  if (priv->current_blur_step != priv->blur_step)
    clutter_actor_queue_redraw(actor);

  ClutterFixed mx, my, zx, zy;
  mx = CLUTTER_INT_TO_FIXED (width) / 2;
  my = CLUTTER_INT_TO_FIXED (height) / 2;
  zx = CLUTTER_FLOAT_TO_FIXED(width*0.5f*priv->zoom);
  zy = CLUTTER_FLOAT_TO_FIXED(height*0.5f*priv->zoom);

  /* Render what we've blurred to the screen */
  col.red = (int)(priv->brightness*255);
  col.green = (int)(priv->brightness*255);
  col.blue = (int)(priv->brightness*255);
  col.alpha = clutter_actor_get_paint_opacity (actor);

  /* If we're blurring out, do it by adjusting the opacity of what we're
   * rendering now... */
  if (priv->blur_step==0 || (priv->blur_step < priv->max_blur_step))
    {
      priv->current_blur_step = priv->blur_step;
      if (priv->max_blur_step > 0)
        col.alpha = col.alpha * priv->current_blur_step / priv->max_blur_step;
      else
        col.alpha = 0;

      /* And we must render ourselves properly so we can render
       * the blur over the top */
      cogl_push_matrix();
      cogl_translatex(
                 CLUTTER_FLOAT_TO_FIXED(width*(1-priv->zoom)/2),
                 CLUTTER_FLOAT_TO_FIXED(height*(1-priv->zoom)/2),
                 0);
      cogl_scale(CLUTTER_FLOAT_TO_FIXED(priv->zoom),
                 CLUTTER_FLOAT_TO_FIXED(priv->zoom));

      cogl_clip_set(0, 0,
                    CLUTTER_INT_TO_FIXED(width),
                    CLUTTER_INT_TO_FIXED(height));
      CLUTTER_ACTOR_CLASS(tidy_blur_group_parent_class)->paint(actor);
      cogl_clip_unset();

      /* If we're zooming less than 1, we want to re-render everything
       * mirrored around each edge. So render a 3x3 box, and flip the
       *  */
      if (priv->zoom < 1)
        {
          gint x,y;
          for (y=0;y<3;y++)
            for (x=0;x<3;x++)
              if (x!=1 || y!=1)
                {
                  gint sx = (x==1) ? 1 : -1;
                  gint sy = (y==1) ? 1 : -1;
                  cogl_push_matrix();
                  cogl_translatex(
                             CLUTTER_FLOAT_TO_FIXED(width*(x-1) + width/2),
                             CLUTTER_FLOAT_TO_FIXED(height*(y-1) + height/2),
                             0);
                  cogl_scale( sx*CFX_ONE, sy*CFX_ONE );
                  cogl_translatex(
                                  CLUTTER_FLOAT_TO_FIXED(-width/2),
                                  CLUTTER_FLOAT_TO_FIXED(-height/2),
                                  0);
                  cogl_clip_set(0, 0,
                                CLUTTER_INT_TO_FIXED(width),
                                CLUTTER_INT_TO_FIXED(height));
                  CLUTTER_ACTOR_CLASS(tidy_blur_group_parent_class)->paint(actor);
                  cogl_clip_unset();
                  cogl_pop_matrix();
                }
        }

      cogl_pop_matrix();
    }

/*  g_debug("%s: Blur act: %d, cur:%d, max:%d - alpha:%d", __FUNCTION__,
      priv->blur_step, priv->current_blur_step, priv->max_blur_step, col.alpha);*/

  if (col.alpha == 0)
    {
      tidy_blur_group_do_chequer(container, width, height);
      return;
    }

  /* Now we render the image we have, with a desaturation pixel
   * shader */
  if (priv->use_shader && priv->shader_saturate)
    {
      clutter_shader_set_is_enabled (priv->shader_saturate, TRUE);
      clutter_shader_set_uniform_1f (priv->shader_saturate, "saturation",
                                     priv->saturation);
    }

  cogl_color (&col);

  if (rotate_90)
    {
      cogl_push_matrix();
      cogl_translatex(CFX_ONE*width/2, CFX_ONE*height/2, 0);
      cogl_rotate(90, 0, 0, 1);
      cogl_scale(-CFX_ONE*height/width, -CFX_ONE*width/height);
      cogl_translatex(-CFX_ONE*width/2, -CFX_ONE*height/2, 0);
    }

  /* Set the blur texture to linear interpolation - so we draw it smoothly
   * Onto the screen */
  current_tex = priv->current_is_a ? priv->tex_a : priv->tex_b;
  cogl_texture_set_filters(current_tex, CGL_LINEAR, CGL_LINEAR);

  if ((priv->zoom >= 1) || !priv->use_mirror)
    {
      cogl_texture_rectangle (current_tex,
                              mx-zx, my-zy,
                              mx+zx, my+zy,
                              0, 0, CFX_ONE, CFX_ONE);
    }
  else
    {
      gint vignette_amt;
      float edge_expand;

      /* draw a 7x7 grid with 5x5 unmirrored, and the edges mirrored so
       * we don't see dark edges when we zoom out */
      CoglTextureVertex verts[6*(VIGNETTE_TILES*VIGNETTE_TILES)];
      CoglTextureVertex grid[(VIGNETTE_TILES+1)*(VIGNETTE_TILES+1)];
      CoglTextureVertex *v = grid;
      gint x,y;

      /* Work out how much we must expand our mirrored edges to get to the
       * edge of the screen. This rather hideous equation comes from working
       * backwards to ensure that in the line "v->x = mx+(zx*(fx...",
       * "(zx*(fx..." = mx */
      edge_expand = (VIGNETTE_TILES-2) / (priv->zoom*2.0) + VIGNETTE_TILES/2.0;

      vignette_amt = (int)((1-priv->zoom)*2048);
      if (vignette_amt<0) vignette_amt = 0;
      if (vignette_amt>255) vignette_amt = 255;
      vignette_amt = 255;

      /* work out grid points */
      for (y=0;y<=VIGNETTE_TILES;y++)
        for (x=0;x<=VIGNETTE_TILES;x++)
          {
            float fx = x, fy = y;
            gint c = 255;
            gint edge;
            /* we don't want full-size tiles for the edges - just half-size */
            if (x==0) fx = VIGNETTE_TILES - edge_expand;
            if (x==VIGNETTE_TILES) fx = edge_expand;
            if (y==0) fy = VIGNETTE_TILES - edge_expand;
            if (y==VIGNETTE_TILES) fy = edge_expand;
            /* work out vertex coords */
            v->x = mx+(zx*(fx*2-VIGNETTE_TILES)/(VIGNETTE_TILES-2));
            v->y = my+(zy*(fy*2-VIGNETTE_TILES)/(VIGNETTE_TILES-2));
            v->z = 0;
            v->tx = (fx-1) * CFX_ONE / (VIGNETTE_TILES-2);
            v->ty = (fy-1) * CFX_ONE / (VIGNETTE_TILES-2);
            /* mirror edges */
            if (v->tx < 0)
              v->tx = -v->tx;
            if (v->tx > CFX_ONE)
              v->tx = CFX_ONE*2 - v->tx;
            if (v->ty < 0)
              v->ty = -v->ty;
            if (v->ty > CFX_ONE)
              v->ty = CFX_ONE*2 - v->ty;
            /* Colour value...
             * 'edge' is the distance from the edge (almost) - it is whichever
             * is the smallest out of the distances to all 4 edges. */
            edge = MIN(MIN(x, MIN(y, MIN(VIGNETTE_TILES-x, VIGNETTE_TILES-y))),
                       VIGNETTE_COLOURS-1);
            c = priv->vignette_colours[edge];
            v->color.red = col.red * c / 255;
            v->color.green = col.green * c / 255;
            v->color.blue = col.blue * c / 255;
            v->color.alpha = col.alpha;
            /* next vertex */
            v++;
          }

      /* now work out actual vertices - join the grid points with
       * 2 triangles to make a quad */
      v = verts;
      for (y=0;y<VIGNETTE_TILES;y++)
        for (x=0;x<VIGNETTE_TILES;x++)
          {
            CoglTextureVertex *grid_pt = &grid[x + y*(VIGNETTE_TILES+1)];
            /* We triangulate in 2 different ways depending on where we
             * are in the grid - because otherwise the resulting interpolation
             * offends MartinG.
             *   ___    ___
             *  |\  |  |  /|
             *  | \ |  | / |
             *  |__\|  |/__|
             *  TL/BR  TR/BL
             */
            if ((x<(VIGNETTE_TILES/2)) == (y<(VIGNETTE_TILES/2)))
              {
                v[0] = grid_pt[0]; /* tri 1 */
                v[1] = grid_pt[1];
                v[2] = grid_pt[1+(VIGNETTE_TILES+1)];
                v[3] = v[0]; /* tri 2 */
                v[4] = v[2];
                v[5] = grid_pt[VIGNETTE_TILES+1];
              }
            else
              {
                v[0] = grid_pt[VIGNETTE_TILES+1]; /* tri 1 */
                v[1] = grid_pt[0];
                v[2] = grid_pt[1];
                v[3] = v[0]; /* tri 2 */
                v[4] = v[2];
                v[5] = grid_pt[1+(VIGNETTE_TILES+1)];
              }
            v+=6;
          }
      /* render! */
      cogl_texture_triangles (current_tex,
                              6*(VIGNETTE_TILES*VIGNETTE_TILES),
                              verts,
                              TRUE);
    }

  /* Reset the filters on the current texture ready for normal blurring */
  cogl_texture_set_filters(current_tex, CGL_NEAREST, CGL_NEAREST);

  if (rotate_90)
    {
      cogl_pop_matrix();
    }

  if (priv->use_shader && priv->shader_saturate)
    clutter_shader_set_is_enabled (priv->shader_saturate, FALSE);

  tidy_blur_group_do_chequer(container, width, height);
}

static void
tidy_blur_group_dispose (GObject *gobject)
{
  TidyBlurGroup *container = TIDY_BLUR_GROUP(gobject);
  TidyBlurGroupPrivate *priv = container->priv;

  if (priv->fbo_a)
    {
      cogl_offscreen_unref(priv->fbo_a);
      cogl_texture_unref(priv->tex_a);
      priv->fbo_a = 0;
      priv->tex_a = 0;
    }
  if (priv->fbo_b)
    {
      cogl_offscreen_unref(priv->fbo_b);
      cogl_texture_unref(priv->tex_b);
      priv->fbo_b = 0;
      priv->tex_b = 0;
    }
  if (priv->tex_chequer)
    {
      cogl_texture_unref(priv->tex_chequer);
      priv->tex_chequer = 0;
    }

  G_OBJECT_CLASS (tidy_blur_group_parent_class)->dispose (gobject);
}

static void
tidy_blur_group_class_init (TidyBlurGroupClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TidyBlurGroupPrivate));

  gobject_class->dispose = tidy_blur_group_dispose;

  /* Provide implementations for ClutterActor vfuncs: */
  if (!hd_transition_get_int("blur", "turbo", 0))
    actor_class->paint = tidy_blur_group_paint;
  actor_class->notify_modified = tidy_blur_group_notify_modified_real;
}

static void
tidy_blur_group_init (TidyBlurGroup *self)
{
  TidyBlurGroupPrivate *priv;
  gint i,x,y;
  guchar dither_data[CHEQUER_SIZE*CHEQUER_SIZE];

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                   TIDY_TYPE_BLUR_GROUP,
                                                   TidyBlurGroupPrivate);
  priv->blur_step = 0;
  priv->current_blur_step = 0;
  priv->max_blur_step = 0;
  priv->saturation = 1;
  priv->brightness = 1;
  priv->zoom = 1;
  priv->use_alpha = TRUE;
  priv->use_mirror = FALSE;
  priv->source_changed = TRUE;

#if CLUTTER_COGL_HAS_GLES
  priv->use_shader = cogl_features_available(COGL_FEATURE_SHADERS_GLSL);
#else
  priv->use_shader = FALSE; /* For now, as Xephyr hates us */
#endif
  priv->shader_blur = 0;
  priv->shader_saturate = 0;

  priv->tex_a = 0;
  priv->fbo_a = 0;
  priv->tex_b = 0;
  priv->fbo_b = 0;
  priv->current_is_a = TRUE;
  priv->current_is_rotated = FALSE;
  /* dimming for the vignette */
  for (i=0;i<VIGNETTE_COLOURS;i++)
    priv->vignette_colours[i] = 255;
  priv->vignette_colours[0] = 0;
  priv->vignette_colours[1] = 128;

  /* Dimming texture - a 32x32 chequer pattern */
  i=0;
  for (y=0;y<CHEQUER_SIZE;y++)
    for (x=0;x<CHEQUER_SIZE;x++)
      {
        /* A 50:50 chequer pattern:
         * dither_data[i++] = ((x&1) == (y&1)) ? 255 : 0;*/

        /* 25:75 pattern */
        gint d = x + y;
        dither_data[i++] = ((d&3) == 0) ? 0 : 255;
      }
  priv->tex_chequer = cogl_texture_new_from_data(
      CHEQUER_SIZE,
      CHEQUER_SIZE,
      0,
      0,
      COGL_PIXEL_FORMAT_A_8,
      COGL_PIXEL_FORMAT_A_8,
      CHEQUER_SIZE,
      dither_data);

  tidy_blur_group_check_shader(self, &priv->shader_blur,
                               BLUR_FRAGMENT_SHADER, BLUR_VERTEX_SHADER);
  tidy_blur_group_check_shader(self, &priv->shader_saturate,
                               SATURATE_FRAGMENT_SHADER, 0);

  g_signal_connect(self, "notify::allocation",
                   G_CALLBACK(tidy_blur_group_allocate_textures), NULL);
}

/*
 * Public API
 */

/**
 * tidy_blur_group_new:
 *
 * Creates a new render container
 *
 * Return value: the newly created #TidyBlurGroup
 */
ClutterActor *
tidy_blur_group_new (void)
{
  return g_object_new (TIDY_TYPE_BLUR_GROUP, NULL);
}

/**
 * tidy_blur_group_set_chequer:
 *
 * Sets whether to chequer the contents with a 50:50 pattern of black dots
 */
void tidy_blur_group_set_chequer(ClutterActor *blur_group, gboolean chequer)
{
  TidyBlurGroupPrivate *priv;

  if (!TIDY_IS_SANE_BLUR_GROUP(blur_group))
    return;

  priv = TIDY_BLUR_GROUP(blur_group)->priv;

  if (priv->chequer != chequer)
    {
      priv->chequer = chequer;
      if (CLUTTER_ACTOR_IS_VISIBLE(blur_group))
        clutter_actor_queue_redraw(blur_group);
    }
}

/**
 * tidy_blur_group_set_blur:
 *
 * Sets the amount of blur (in pixels)
 */
void tidy_blur_group_set_blur(ClutterActor *blur_group, float blur)
{
  TidyBlurGroupPrivate *priv;
  gint step = (int)blur;
  /* Don't set step to be 0 if blur isn't. This fixes the case where
   * saturation!=0 but blur is, and blur is needlessly recalculated */
  if (step==0 && blur!=0)
    step = 1;

  if (!TIDY_IS_SANE_BLUR_GROUP(blur_group))
    return;

  priv = TIDY_BLUR_GROUP(blur_group)->priv;

  if (priv->blur_step != step)
    {
      priv->blur_step = step;
      if (step==0)
        priv->current_blur_step = 0;

      if (CLUTTER_ACTOR_IS_VISIBLE(blur_group))
        clutter_actor_queue_redraw(blur_group);
    }
}

/**
 * tidy_blur_group_set_saturation:
 *
 * Sets the saturation (1 = normal, 0=black and white)
 */
void tidy_blur_group_set_saturation(ClutterActor *blur_group, float saturation)
{
  TidyBlurGroupPrivate *priv;

  if (!TIDY_IS_SANE_BLUR_GROUP(blur_group))
    return;

  priv = TIDY_BLUR_GROUP(blur_group)->priv;

  if (priv->saturation != saturation)
    {
      priv->saturation = saturation;
      if (CLUTTER_ACTOR_IS_VISIBLE(blur_group))
        clutter_actor_queue_redraw(blur_group);
    }
}

/**
 * tidy_blur_group_set_brightness:
 *
 * Sets the brightness (1 = normal, 0=black)
 */
void tidy_blur_group_set_brightness(ClutterActor *blur_group, float brightness)
{
  TidyBlurGroupPrivate *priv;

  if (!TIDY_IS_SANE_BLUR_GROUP(blur_group))
    return;

  priv = TIDY_BLUR_GROUP(blur_group)->priv;

  if (priv->brightness != brightness)
    {
      priv->brightness = brightness;
      if (CLUTTER_ACTOR_IS_VISIBLE(blur_group))
        clutter_actor_queue_redraw(blur_group);
    }
}


/**
 * tidy_blur_group_set_zoom:
 *
 * Set how far to zoom in on what has been blurred
 * 1=normal, 0.5=out, 2=double-size
 */
void tidy_blur_group_set_zoom(ClutterActor *blur_group, float zoom)
{
  TidyBlurGroupPrivate *priv;

  if (!TIDY_IS_SANE_BLUR_GROUP(blur_group))
    return;

  priv = TIDY_BLUR_GROUP(blur_group)->priv;

  if (priv->zoom != zoom)
    {
      priv->zoom = zoom;
      if (CLUTTER_ACTOR_IS_VISIBLE(blur_group))
        clutter_actor_queue_redraw(blur_group);
    }
}

/**
 * tidy_blur_group_get_zoom:
 *
 * Get how far to zoom in on what has been blurred
 * 1=normal, 0.5=out, 2=double-size
 */
float tidy_blur_group_get_zoom(ClutterActor *blur_group)
{
  TidyBlurGroupPrivate *priv;

  if (!TIDY_IS_SANE_BLUR_GROUP(blur_group))
    return 1.0f;

  priv = TIDY_BLUR_GROUP(blur_group)->priv;

  return priv->zoom;
}

/**
 * tidy_blur_group_set_use_alpha:
 *
 * Sets whether to use an alpha channel in the textures used for blurring.
 * Only useful if we're blurring something transparent
 */
void tidy_blur_group_set_use_alpha(ClutterActor *blur_group, gboolean alpha)
{
  TidyBlurGroupPrivate *priv;

  if (!TIDY_IS_SANE_BLUR_GROUP(blur_group))
    return;

  priv = TIDY_BLUR_GROUP(blur_group)->priv;

  priv->use_alpha = alpha;
}

/**
 * tidy_blur_group_set_use_mirror:
 *
 * Sets whether to mirror the blurred texture when it is zoomed out, or just
 * leave the edges dark...
 */
void tidy_blur_group_set_use_mirror(ClutterActor *blur_group, gboolean mirror)
{
  TidyBlurGroupPrivate *priv;

  if (!TIDY_IS_SANE_BLUR_GROUP(blur_group))
    return;

  priv = TIDY_BLUR_GROUP(blur_group)->priv;

  priv->use_mirror = mirror;
}

/**
 * tidy_blur_group_set_source_changed:
 *
 * Forces the blur group to update. Only needed at the moment because
 * actor_remove doesn't appear to send notify events
 */
void tidy_blur_group_set_source_changed(ClutterActor *blur_group)
{
  TidyBlurGroupPrivate *priv;

  if (!TIDY_IS_SANE_BLUR_GROUP(blur_group))
    return;

  priv = TIDY_BLUR_GROUP(blur_group)->priv;
  priv->source_changed = TRUE;
  /* This will actually force a redraw */
  priv->current_blur_step = 0;
  clutter_actor_queue_redraw(blur_group);
}

/**
 * tidy_blur_group_hint_source_changed:
 *
 * Notifies the blur group that it needs to update next time it becomes
 * unblurred.
 */
void tidy_blur_group_hint_source_changed(ClutterActor *blur_group)
{
  TidyBlurGroupPrivate *priv;

  if (!TIDY_IS_SANE_BLUR_GROUP(blur_group))
    return;

  priv = TIDY_BLUR_GROUP(blur_group)->priv;
  priv->source_changed = TRUE;
}


/**
 * tidy_blur_group_source_buffered:
 *
 * Return true if this blur group is currently buffering it's actors. Used
 * when this actually needs to blur or desaturate its children
 */
gboolean tidy_blur_group_source_buffered(ClutterActor *blur_group)
{
  TidyBlurGroupPrivate *priv;

  if (!TIDY_IS_SANE_BLUR_GROUP(blur_group))
    return FALSE;

  priv = TIDY_BLUR_GROUP(blur_group)->priv;
  return !(priv->blur_step==0 && priv->saturation==1 && priv->brightness==1);
}
