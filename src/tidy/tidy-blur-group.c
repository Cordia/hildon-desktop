/* Created by Gordon Williams <gordon.williams@collabora.co.uk> 
 * 
 * This class blurs all of its children, also changing saturation and lightness. 
 * It renders its children into a half-size texture first, then blurs this into
 * another texture, finally rendering that to the screen. Because of this, when
 * the blurring doesn't change from frame to frame, children and NOT rendered
 * unless update_children is TRUE, making this pretty quick. */

#include "tidy-blur-group.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <clutter/clutter-container.h>

#include <cogl/cogl.h>

#include <string.h>

/* The OpenGL fragment shader used to do blur and desaturation.
 * We use 3 samples here arranged in a rough triangle*/
const char *BLUR_FRAGMENT_SHADER =
#if CLUTTER_COGL_HAS_GLES
"precision lowp float;\n" 
"varying lowp vec4      frag_color;\n"
"varying mediump vec2      tex_coord;\n"
#endif /* CLUTTER_COGL_HAS_GLES */
"uniform lowp sampler2D tex;\n"
"uniform mediump float blur;\n"
"uniform lowp float saturation;\n"
"void main () {\n"
"  mediump vec2 diffa = vec2(0.0, -0.875 * blur); \n"
"  mediump vec2 diffb = vec2(0.75*blur, 0.25*blur); \n"
"  mediump vec2 diffc = vec2(-blur, 0.375*blur); \n"
#if CLUTTER_COGL_HAS_GL
"  mediump vec2 tex_coord = vec2(gl_TexCoord[0]); \n"
#endif
"  lowp vec4 color =  texture2D (tex, tex_coord+diffa)*0.333 + "
" texture2D (tex, tex_coord+diffb)*0.333 + texture2D (tex, tex_coord+diffc)*0.333;\n"
#if CLUTTER_COGL_HAS_GLES
"  color = color * frag_color;\n"
#endif /* CLUTTER_COGL_HAS_GLES */
#if CLUTTER_COGL_HAS_GL
"  color = color * gl_Color;\n"
#endif /* CLUTTER_COGL_HAS_GL */
// saturation
"  lowp float lightness = (color.r+color.g+color.b)*0.333*(1.0-saturation); \n"
"  gl_FragColor = vec4(\n"
"                      color.r*saturation + lightness,\n"
"                      color.g*saturation + lightness,\n"
"                      color.b*saturation + lightness,\n"
"                      color.a);\n"
"}\n";


struct _TidyBlurGroupPrivate
{
  /* Internal TidyBlurGroup stuff */
  ClutterShader *shader;
  CoglHandle tex_preblur;
  CoglHandle fbo_preblur;
  
  CoglHandle tex_postblur;
  CoglHandle fbo_postblur;
  
  gboolean use_shader;
  float saturation; /* 0->1 how much colour there is */
  float blur; /* amount of blur in pixels */
  float brightness; /* 1=normal, 0=black */
  gboolean update_children; /* whether we re-draw the children each frame or not */
  
  gboolean source_changed; /* if anything changed we need to recalculate preblur */
  gboolean blur_changed; /* if anything changed we need to recalculate postblur */        
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

/* An implementation for the ClutterGroup::paint() vfunc,
   painting all the child actors: */
static void
tidy_blur_group_paint (ClutterActor *actor)
{
  ClutterColor    col = { 0xff, 0xff, 0xff, 0xff };
  ClutterColor    bgcol = { 0x00, 0x00, 0x00, 0x00 };
  gint            x_1, y_1, x_2, y_2;        
        
  if (!TIDY_IS_BLUR_GROUP(actor))
    return;
  
  ClutterGroup *group = CLUTTER_GROUP(actor);
  TidyBlurGroup *container = TIDY_BLUR_GROUP(group);
  TidyBlurGroupPrivate *priv = container->priv;
  
  /* If we are rendering normally then shortcut all this, and
   just render directly without the texture */ 
  if (priv->blur==0 && priv->saturation==1 && priv->brightness==1) 
    {
    /* free our textures first to give us a bit 
     * more free memory */
      /* if (priv->fbo_preblur) {
         cogl_offscreen_unref(priv->fbo_preblur);
         cogl_texture_unref(priv->tex_preblur);       
         priv->fbo_preblur = 0;
         priv->tex_preblur = 0;
       }
       if (priv->fbo_postblur) {
         cogl_offscreen_unref(priv->fbo_postblur);
         cogl_texture_unref(priv->tex_postblur);       
         priv->fbo_postblur = 0;
         priv->tex_postblur = 0;
       }*/
      priv->source_changed = TRUE;
        
      TIDY_BLUR_GROUP_GET_CLASS(actor)->overridden_paint(actor);
      return;
    }
  
  if (priv->update_children)
    priv->source_changed = TRUE;
  
  clutter_actor_get_allocation_coords (actor, &x_1, &y_1, &x_2, &y_2);
 
  int width = x_2 - x_1;
  int height = y_2 - y_1;
  int exp_width = width/2;
  int exp_height = height/2;
  int tex_width = 0;
  int tex_height = 0;
  
  /* check sizes */
  if (priv->tex_preblur) 
    {
      tex_width = cogl_texture_get_width(priv->tex_preblur);
      tex_height = cogl_texture_get_height(priv->tex_preblur);
    }
  /* free texture if the size is wrong */
  if (tex_width!=exp_width || tex_height!=exp_height) {
    //g_print("Re-allocating texture as it changed size - %dx%d to %dx%d\n", tex_width, tex_height, exp_width, exp_height);
    if (priv->fbo_preblur) 
      {
        cogl_offscreen_unref(priv->fbo_preblur);
        cogl_texture_unref(priv->tex_preblur);
        priv->fbo_preblur = 0;      
        priv->tex_preblur = 0;
      }
    if (priv->fbo_postblur)
      {
        cogl_offscreen_unref(priv->fbo_postblur);
        cogl_texture_unref(priv->tex_postblur);       
        priv->fbo_postblur = 0;
        priv->tex_postblur = 0;
      }
  }
  /* create the texture + offscreen buffer if they didn't exist. We can specify mipmapping here, but we don't need it */
  if (!priv->tex_preblur) 
    {
      tex_width = exp_width;
      tex_height = exp_height;
      priv->tex_preblur = cogl_texture_new_with_size(tex_width, tex_height, 0, 0, COGL_PIXEL_FORMAT_RGBA_4444);
      /* set nearest texture filter - this just takes a single sample */
      cogl_texture_set_filters(priv->tex_preblur, CGL_NEAREST, CGL_NEAREST);      
      priv->fbo_preblur = cogl_offscreen_new_to_texture (priv->tex_preblur);
      priv->source_changed = TRUE;
    }
  if (!priv->tex_postblur) 
    {
      priv->tex_postblur = cogl_texture_new_with_size(tex_width, tex_height, 0, 0, COGL_PIXEL_FORMAT_RGBA_4444);
      priv->fbo_postblur = cogl_offscreen_new_to_texture (priv->tex_postblur);
      priv->blur_changed = TRUE;
    }
  
  /* Draw children into an offscreen buffer */
  if (priv->source_changed) 
    {
      cogl_draw_buffer(COGL_OFFSCREEN_BUFFER, priv->fbo_preblur);
      cogl_push_matrix();
      cogl_scale(CFX_ONE*tex_width/width, CFX_ONE*tex_height/height); 
  
      cogl_paint_init(&bgcol);
      TIDY_BLUR_GROUP_GET_CLASS(actor)->overridden_paint(actor);
  
      cogl_pop_matrix();  
      priv->blur_changed = TRUE;
      priv->source_changed = FALSE;
    }    
  
  col.red = (int)(priv->brightness*255);
  col.green = (int)(priv->brightness*255);
  col.blue = (int)(priv->brightness*255);
  col.alpha = clutter_actor_get_paint_opacity (actor);
  cogl_color (&col);

  /* if we have no shader, so attempt to create one */ 
  if (priv->use_shader && !priv->shader) 
    {
      GError           *error = NULL;
      priv->shader = clutter_shader_new();
      clutter_shader_set_fragment_source (priv->shader, BLUR_FRAGMENT_SHADER, -1);
      clutter_shader_compile (priv->shader, &error);
      if (error) 
        {
          g_warning ("unable to load shader: %s\n", error->message);
          g_error_free (error);
          priv->use_shader = FALSE;
        }
    }
  
  /* blur the pre_blur texture into the post_blur texture */
  if (priv->blur_changed)
    {
      cogl_draw_buffer(COGL_OFFSCREEN_BUFFER, priv->fbo_postblur);
      if (priv->use_shader && priv->shader) 
        {
          clutter_shader_set_is_enabled (priv->shader, TRUE);
          clutter_shader_set_uniform_1f (priv->shader, "blur", priv->blur/width);
          clutter_shader_set_uniform_1f (priv->shader, "saturation", priv->saturation);
        }
      cogl_texture_rectangle (priv->tex_preblur, 0, 0,
                              CLUTTER_INT_TO_FIXED (tex_width),
                              CLUTTER_INT_TO_FIXED (tex_height),
                              0, 0, CFX_ONE, CFX_ONE);
      if (priv->use_shader && priv->shader)
        clutter_shader_set_is_enabled (priv->shader, FALSE);
      priv->blur_changed = FALSE;      
    }
    
  /* Render the blurred texture to the screen */
  cogl_draw_buffer(COGL_WINDOW_BUFFER, 0);        
  cogl_texture_rectangle (priv->tex_postblur, 0, 0,
                          CLUTTER_INT_TO_FIXED (width),
                          CLUTTER_INT_TO_FIXED (height),
                          0, 0, CFX_ONE, CFX_ONE);
                     
}

static void
tidy_blur_group_dispose (GObject *gobject)
{
  TidyBlurGroup *container = TIDY_BLUR_GROUP(gobject);
  TidyBlurGroupPrivate *priv = container->priv;
  
  if (priv->fbo_preblur) 
    {
      cogl_offscreen_unref(priv->fbo_preblur);
      cogl_texture_unref(priv->tex_preblur);
      priv->fbo_preblur = 0;      
      priv->tex_preblur = 0;
    }
  if (priv->fbo_postblur) 
    {
      cogl_offscreen_unref(priv->fbo_postblur);
      cogl_texture_unref(priv->tex_postblur);       
      priv->fbo_postblur = 0;
      priv->tex_postblur = 0;
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
  klass->overridden_paint = actor_class->paint;
  actor_class->paint = tidy_blur_group_paint;
}

static void
tidy_blur_group_init (TidyBlurGroup *self)
{
  TidyBlurGroupPrivate *priv;

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                   TIDY_TYPE_BLUR_GROUP, TidyBlurGroupPrivate);
  priv->blur = 0;
  priv->saturation = 1;
  priv->brightness = 1;
  priv->update_children = TRUE;
  priv->blur_changed = TRUE;
  priv->source_changed = TRUE;
 
  priv->use_shader = cogl_features_available(COGL_FEATURE_SHADERS_GLSL);
  priv->tex_preblur = 0;
  priv->fbo_preblur = 0;
  priv->tex_postblur = 0;
  priv->fbo_postblur = 0;
  priv->shader = 0;
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
 * tidy_blur_group_set_blur:
 *
 * Sets the amount of blur (in pixels)
 */
void tidy_blur_group_set_blur(ClutterActor *blur_group, float blur)
{  
  if (!TIDY_IS_BLUR_GROUP(blur_group))
    return;
 
  TIDY_BLUR_GROUP(blur_group)->priv->blur = blur;
  TIDY_BLUR_GROUP(blur_group)->priv->blur_changed = TRUE;
  clutter_actor_queue_redraw(blur_group); 
}

/**
 * tidy_blur_group_set_saturation:
 *
 * Sets the saturation (1 = normal, 0=black and white)
 */
void tidy_blur_group_set_saturation(ClutterActor *blur_group, float saturation)
{
  if (!TIDY_IS_BLUR_GROUP(blur_group))
    return;
 
  TIDY_BLUR_GROUP(blur_group)->priv->saturation = saturation;
  TIDY_BLUR_GROUP(blur_group)->priv->blur_changed = TRUE;
  clutter_actor_queue_redraw(blur_group); 
}

/**
 * tidy_blur_group_set_brightness:
 *
 * Sets the brightness (1 = normal, 0=black)
 */
void tidy_blur_group_set_brightness(ClutterActor *blur_group, float brightness)
{
  if (!TIDY_IS_BLUR_GROUP(blur_group))
    return;
 
  TIDY_BLUR_GROUP(blur_group)->priv->brightness = brightness;
  TIDY_BLUR_GROUP(blur_group)->priv->blur_changed = TRUE;
  clutter_actor_queue_redraw(blur_group); 
}

/**
 * tidy_blur_group_set_update_children:
 *
 * Sets whether to bother updating children when blurring or not...
 * For blurring all children are stored in a texture, so the update
 * of this can be skipped.
 */
void tidy_blur_group_set_update_children(ClutterActor *blur_group, gboolean update)
{
  if (!TIDY_IS_BLUR_GROUP(blur_group))
    return;
 
  TIDY_BLUR_GROUP(blur_group)->priv->update_children = update;
}
