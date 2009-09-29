/* Created by Gordon Williams <gordon.williams@collabora.co.uk>
 *
 * This class is able to render all of its children into a buffer, which
 * it can use to speed up rendering, or to continue showing images of its'
 * children after they have been destroyed. */

#include "tidy-cached-group.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <clutter/clutter-container.h>

#include <cogl/cogl.h>

#include <string.h>
#include <locale.h>

#define TIDY_CACHED_GROUP_DEFAULT_DOWNSAMPLING  2.0

struct _TidyCachedGroupPrivate
{
  /* Internal TidyCachedGroup stuff */
  CoglHandle tex;
  CoglHandle fbo;

  gboolean use_alpha; /* whether to use an alpha channel in our textures */

  /* 0 = render as normal ClutterGroup, 1 = render fully cached image */
  float cache_amount;
  /* if anything changed we need to recalculate preblur */
  gboolean source_changed;
  /* how much quality loss you can afford when rendering cached texture */
  float downsample;
};

G_DEFINE_TYPE (TidyCachedGroup,
               tidy_cached_group,
               CLUTTER_TYPE_GROUP);

/* An implementation for the ClutterGroup::paint() vfunc,
   painting all the child actors: */
static void
tidy_cached_group_paint (ClutterActor *actor)
{
  ClutterColor    white = { 0xff, 0xff, 0xff, 0xff };
  ClutterColor    bgcol = { 0x00, 0x00, 0x00, 0xff };
  ClutterColor    col = { 0xff, 0xff, 0xff, 0xff };
  gint            x_1, y_1, x_2, y_2;

  if (!TIDY_IS_CACHED_GROUP(actor))
    return;

  ClutterGroup *group = CLUTTER_GROUP(actor);
  TidyCachedGroup *container = TIDY_CACHED_GROUP(group);
  TidyCachedGroupPrivate *priv = container->priv;

  /* If we are rendering normally then shortcut all this, and
   just render directly without the texture */
  if (priv->cache_amount< 0.01)
    {
      /* render direct */
      CLUTTER_ACTOR_CLASS (tidy_cached_group_parent_class)->paint(actor);
      return;
    }

  clutter_actor_get_allocation_coords (actor, &x_1, &y_1, &x_2, &y_2);

#ifdef __i386__
  if (!cogl_features_available(COGL_FEATURE_OFFSCREEN))
    { /* If we can't render offscreen properly, just render normally. */
      CLUTTER_ACTOR_CLASS (tidy_cached_group_parent_class)->paint(actor);
      return;
    }
#endif

  int width = x_2 - x_1;
  int height = y_2 - y_1;
  int exp_width = width/priv->downsample;
  int exp_height = height/priv->downsample;
  int tex_width = 0;
  int tex_height = 0;

  /* check sizes */
  if (priv->tex)
    {
      tex_width = cogl_texture_get_width(priv->tex);
      tex_height = cogl_texture_get_height(priv->tex);
    }
  /* free texture if the size is wrong */
  if (tex_width!=exp_width || tex_height!=exp_height) {
    if (priv->fbo)
      {
        cogl_offscreen_unref(priv->fbo);
        cogl_texture_unref(priv->tex);
        priv->fbo = 0;
        priv->tex = 0;
      }
    priv->source_changed = TRUE;
  }
  /* create the texture + offscreen buffer if they didn't exist. */
  if (!priv->tex)
    {
      tex_width = exp_width;
      tex_height = exp_height;

      priv->tex = cogl_texture_new_with_size(
                tex_width, tex_height, 0, FALSE /*mipmap*/,
                priv->use_alpha ? COGL_PIXEL_FORMAT_RGBA_8888 :
                                  COGL_PIXEL_FORMAT_RGB_565);
      cogl_texture_set_filters(priv->tex, CGL_NEAREST, CGL_NEAREST);
      priv->fbo = cogl_offscreen_new_to_texture (priv->tex);
    }

  /* Draw children into an offscreen buffer */
  if (priv->source_changed)
    {
      cogl_draw_buffer(COGL_OFFSCREEN_BUFFER, priv->fbo);
      cogl_push_matrix();
      /* translate a bit to let bilinear filter smooth out intermediate pixels */
      cogl_translatex(CFX_ONE/2,CFX_ONE/2,0);
      cogl_scale(CFX_ONE*tex_width/width, CFX_ONE*tex_height/height);

      cogl_paint_init(&bgcol);
      cogl_color (&white);
      CLUTTER_ACTOR_CLASS (tidy_cached_group_parent_class)->paint(actor);

      cogl_pop_matrix();
      cogl_draw_buffer(COGL_WINDOW_BUFFER, 0);

      priv->source_changed = FALSE;
    }

  /* Render what we've blurred to the screen */
  col.alpha = clutter_actor_get_paint_opacity (actor);

  /* if cache_amount isn't 1, we merge the two images by rendering the
   * real one first, then rendering the other one after... */
  if (priv->cache_amount < 0.99)
    {
      cogl_color (&white);
      /* And we must render ourselves properly so we can render
       * the blur over the top */
      cogl_push_matrix();
      CLUTTER_ACTOR_CLASS (tidy_cached_group_parent_class)->paint(actor);
      cogl_pop_matrix();
      col.alpha = (int)(priv->cache_amount*255);
    }

  /* Now we render the image we have... */
  cogl_color (&col);

  cogl_texture_rectangle (priv->tex,
                          0, 0,
                          CLUTTER_INT_TO_FIXED (width),
                          CLUTTER_INT_TO_FIXED (height),
                          0, 0, CFX_ONE, CFX_ONE);
}

static void
tidy_cached_group_dispose (GObject *gobject)
{
  TidyCachedGroup *container = TIDY_CACHED_GROUP(gobject);
  TidyCachedGroupPrivate *priv = container->priv;

  if (priv->fbo)
    {
      cogl_offscreen_unref(priv->fbo);
      cogl_texture_unref(priv->tex);
      priv->fbo = 0;
      priv->tex = 0;
    }

  G_OBJECT_CLASS (tidy_cached_group_parent_class)->dispose (gobject);
}

static void
tidy_cached_group_class_init (TidyCachedGroupClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TidyCachedGroupPrivate));

  gobject_class->dispose = tidy_cached_group_dispose;

  /* Provide implementations for ClutterActor vfuncs: */
  actor_class->paint = tidy_cached_group_paint;
}

static void
tidy_cached_group_init (TidyCachedGroup *self)
{
  TidyCachedGroupPrivate *priv;

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                   TIDY_TYPE_CACHED_GROUP,
                                                   TidyCachedGroupPrivate);
  priv->cache_amount = 0;
  priv->downsample = TIDY_CACHED_GROUP_DEFAULT_DOWNSAMPLING;
  priv->use_alpha = FALSE;
  priv->source_changed = TRUE;

  priv->tex = 0;
  priv->fbo = 0;
}

/*
 * Public API
 */

/**
 * tidy_cached_group_new:
 *
 * Creates a new render container
 *
 * Return value: the newly created #TidyCachedGroup
 */
ClutterActor *
tidy_cached_group_new (void)
{
  return g_object_new (TIDY_TYPE_CACHED_GROUP, NULL);
}

/**
  Set how much of the cached image to render.
  0 == just render normally, without cacheing
  0.5 == render half cached image, half normal image
  1 == render entitely the cached image
 */
void tidy_cached_group_set_render_cache(ClutterActor *cached_group, float amount)
{
  TidyCachedGroupPrivate *priv;

  if (!TIDY_IS_CACHED_GROUP(cached_group))
    return;

  priv = TIDY_CACHED_GROUP(cached_group)->priv;

  if (amount<0) amount=0;
  if (amount>1) amount=1;

  if (priv->cache_amount != amount)
    {
      priv->cache_amount = amount;
      if (CLUTTER_ACTOR_IS_VISIBLE(cached_group))
        clutter_actor_queue_redraw(cached_group);
    }
}

/* How much to downsample when caching.  Trades rendering speed for quality.
 * If $downsample is 0 resets the default. */
void tidy_cached_group_set_downsampling_factor(ClutterActor *cached_group,
                                               float downsample)
{
  g_return_if_fail(downsample >= 0);
  TIDY_CACHED_GROUP(cached_group)->priv->downsample =
    downsample ? : TIDY_CACHED_GROUP_DEFAULT_DOWNSAMPLING;
}

/**
 * Notifies the group that it needs to update what it has cached
 */
void tidy_cached_group_changed(ClutterActor *cached_group)
{
  TidyCachedGroupPrivate *priv;

  if (!TIDY_IS_CACHED_GROUP(cached_group))
    return;

  priv = TIDY_CACHED_GROUP(cached_group)->priv;
  priv->source_changed = TRUE;
}


