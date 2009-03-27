/* Created by Gordon Williams <gordon.williams@collabora.co.uk>
 *
 * This blurs part of a texture */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tidy-highlight.h"
#include <clutter/clutter-actor.h>

#include "cogl/cogl.h"

enum
{
  PROP_0,
  PROP_PARENT_TEXTURE,
};

  const char *HIGHLIGHT_FRAGMENT_SHADER =
  "precision lowp float;\n"
  "varying mediump vec2 tex_coord;\n"
  "varying lowp vec4 frag_color;\n"
  "uniform lowp sampler2D tex;\n"
  "uniform mediump float blurx;\n"
  "uniform mediump float blury;\n"
  "void main () {\n"
  "  mediump float bx = blurx*0.5; \n"
  "  mediump float by = blury*0.5; \n"
  "  lowp float alpha = \n"
  "       texture2D (tex, vec2(tex_coord.x, tex_coord.y-by)).a * 0.125 + \n"
  "       texture2D (tex, vec2(tex_coord.x, tex_coord.y+by)).a * 0.125 + \n"
  "       texture2D (tex, vec2(tex_coord.x-bx, tex_coord.y)).a * 0.125 + \n"
  "       texture2D (tex, vec2(tex_coord.x+bx, tex_coord.y)).a * 0.125 + \n"
  "       texture2D (tex, vec2(tex_coord.x, tex_coord.y-blury)).a * 0.09375 + \n"
  "       texture2D (tex, vec2(tex_coord.x, tex_coord.y+blury)).a * 0.09375 + \n"
  "       texture2D (tex, vec2(tex_coord.x-blurx, tex_coord.y)).a * 0.09375 + \n"
  "       texture2D (tex, vec2(tex_coord.x+blurx, tex_coord.y)).a * 0.09375 + \n"
  "       texture2D (tex, vec2(tex_coord.x-blurx, tex_coord.y-blury)).a * 0.03125 + \n"
  "       texture2D (tex, vec2(tex_coord.x+blurx, tex_coord.y+blury)).a * 0.03125 + \n"
  "       texture2D (tex, vec2(tex_coord.x-blurx, tex_coord.y+blury)).a * 0.03125 + \n"
  "       texture2D (tex, vec2(tex_coord.x+blurx, tex_coord.y-blury)).a * 0.03125;\n"
  "  lowp vec4 color = frag_color; \n"
  "  color.a = color.a * alpha; \n"
  "  gl_FragColor = color;\n"
  "}\n";

G_DEFINE_TYPE (TidyHighlight,
	       tidy_highlight,
	       CLUTTER_TYPE_ACTOR);

#define CLUTTER_HIGHLIGHT_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), TIDY_TYPE_HIGHLIGHT, TidyHighlightPrivate))

struct _TidyHighlightPrivate
{
  ClutterTexture      *parent_texture;
  ClutterShader       *shader;

  float                amount;
  ClutterColor         color;
};

static void
tidy_highlight_get_preferred_width (ClutterActor *self,
                                           ClutterUnit   for_height,
                                           ClutterUnit  *min_width_p,
                                           ClutterUnit  *natural_width_p)
{
  TidyHighlightPrivate *priv = TIDY_HIGHLIGHT(self)->priv;
  ClutterActor *parent_texture;
  ClutterActorClass *parent_texture_class;

  /* Note that by calling the get_width_request virtual method directly
   * and skipping the clutter_actor_get_preferred_width() wrapper, we
   * are ignoring any size request override set on the parent texture
   * and just getting the normal size of the parent texture.
   */
  parent_texture = CLUTTER_ACTOR (priv->parent_texture);
  if (!parent_texture)
    {
      if (min_width_p)
        *min_width_p = 0;

      if (natural_width_p)
        *natural_width_p = 0;

      return;
    }

  parent_texture_class = CLUTTER_ACTOR_GET_CLASS (parent_texture);
  parent_texture_class->get_preferred_width (parent_texture,
                                             for_height,
                                             min_width_p,
                                             natural_width_p);
}

static void
tidy_highlight_get_preferred_height (ClutterActor *self,
                                            ClutterUnit   for_width,
                                            ClutterUnit  *min_height_p,
                                            ClutterUnit  *natural_height_p)
{
  TidyHighlightPrivate *priv = TIDY_HIGHLIGHT (self)->priv;
  ClutterActor *parent_texture;
  ClutterActorClass *parent_texture_class;

  /* Note that by calling the get_height_request virtual method directly
   * and skipping the clutter_actor_get_preferred_height() wrapper, we
   * are ignoring any size request override set on the parent texture and
   * just getting the normal size of the parent texture.
   */
  parent_texture = CLUTTER_ACTOR (priv->parent_texture);
  if (!parent_texture)
    {
      if (min_height_p)
        *min_height_p = 0;

      if (natural_height_p)
        *natural_height_p = 0;

      return;
    }

  parent_texture_class = CLUTTER_ACTOR_GET_CLASS (parent_texture);
  parent_texture_class->get_preferred_height (parent_texture,
                                              for_width,
                                              min_height_p,
                                              natural_height_p);
}

static void
tidy_highlight_paint (ClutterActor *self)
{
  TidyHighlightPrivate  *priv;
  ClutterActor                *parent_texture;
  gint                         x_1, y_1, x_2, y_2;
  ClutterColor                 col = { 0xff, 0xff, 0xff, 0xff };
  CoglHandle                   cogl_texture;
  guint                        tex_width, tex_height;

  priv = TIDY_HIGHLIGHT (self)->priv;

  /* no need to paint stuff if we don't have a texture to sub */
  if (!priv->parent_texture)
    return;

  /* parent texture may have been hidden, there for need to make sure its
   * realised with resources available.
  */
  parent_texture = CLUTTER_ACTOR (priv->parent_texture);
  if (!CLUTTER_ACTOR_IS_REALIZED (parent_texture))
    clutter_actor_realize (parent_texture);

  col = priv->color;
  col.alpha = clutter_actor_get_paint_opacity (self);
  cogl_color (&col);

  clutter_actor_get_allocation_coords (self, &x_1, &y_1, &x_2, &y_2);

  cogl_texture = clutter_texture_get_cogl_texture (priv->parent_texture);

  if (cogl_texture == COGL_INVALID_HANDLE)
    return;

  tex_width = cogl_texture_get_width (cogl_texture);
  tex_height = cogl_texture_get_height (cogl_texture);

  if (priv->shader)
    {
      clutter_shader_set_is_enabled (priv->shader, TRUE);
      clutter_shader_set_uniform_1f (priv->shader, "blurx",
                                     priv->amount / tex_width);
      clutter_shader_set_uniform_1f (priv->shader, "blury",
                                     priv->amount / tex_height);
    }

  /* Parent paint translated us into position */
  cogl_texture_rectangle (cogl_texture, 0, 0,
			  CLUTTER_INT_TO_FIXED (x_2 - x_1),
			  CLUTTER_INT_TO_FIXED (y_2 - y_1),
			  0, 0, CFX_ONE, CFX_ONE);

  if (priv->shader)
    clutter_shader_set_is_enabled (priv->shader, FALSE);
}

static void
set_parent_texture (TidyHighlight *ctexture,
		    ClutterTexture      *texture)
{
  TidyHighlightPrivate *priv = ctexture->priv;
  ClutterActor *actor = CLUTTER_ACTOR (ctexture);
  gboolean was_visible = CLUTTER_ACTOR_IS_VISIBLE (ctexture);

  if (priv->parent_texture)
    {
      g_object_unref (priv->parent_texture);
      priv->parent_texture = NULL;

      if (was_visible)
        clutter_actor_hide (actor);
    }

  if (texture)
    {
      priv->parent_texture = g_object_ref (texture);

      /* queue a redraw if the subd texture is already visible */
      if (CLUTTER_ACTOR_IS_VISIBLE (priv->parent_texture) &&
          was_visible)
        {
          clutter_actor_show (actor);
          clutter_actor_queue_redraw (actor);
        }

      clutter_actor_queue_relayout (actor);
    }

}

static void
tidy_highlight_dispose (GObject *object)
{
  TidyHighlight         *self = TIDY_HIGHLIGHT(object);
  TidyHighlightPrivate  *priv = self->priv;

  if (priv->parent_texture)
    g_object_unref (priv->parent_texture);

  priv->parent_texture = NULL;

  G_OBJECT_CLASS (tidy_highlight_parent_class)->dispose (object);
}

static void
tidy_highlight_finalize (GObject *object)
{
  G_OBJECT_CLASS (tidy_highlight_parent_class)->finalize (object);
}

static void
tidy_highlight_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)
{
  TidyHighlight        *ctexture = TIDY_HIGHLIGHT (object);
  TidyHighlightPrivate *priv;

  priv = ctexture->priv;

  switch (prop_id)
    {
    case PROP_PARENT_TEXTURE:
      set_parent_texture (ctexture, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
tidy_highlight_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
  TidyHighlight *ctexture = TIDY_HIGHLIGHT (object);
  TidyHighlightPrivate *priv;

  priv = ctexture->priv;

  switch (prop_id)
    {
    case PROP_PARENT_TEXTURE:
      g_value_set_object (value, ctexture->priv->parent_texture);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
tidy_highlight_class_init (TidyHighlightClass *klass)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint =
    tidy_highlight_paint;
  actor_class->get_preferred_width =
    tidy_highlight_get_preferred_width;
  actor_class->get_preferred_height =
    tidy_highlight_get_preferred_height;

  gobject_class->finalize     = tidy_highlight_finalize;
  gobject_class->dispose      = tidy_highlight_dispose;
  gobject_class->set_property = tidy_highlight_set_property;
  gobject_class->get_property = tidy_highlight_get_property;

  g_object_class_install_property
    (gobject_class, PROP_PARENT_TEXTURE,
     g_param_spec_object ("parent-texture",
			  "Parent Texture",
			  "The parent texture to sub",
			  CLUTTER_TYPE_TEXTURE,
			  G_PARAM_READABLE | G_PARAM_WRITABLE |
			  G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
			  G_PARAM_STATIC_BLURB ));

  g_type_class_add_private (gobject_class, sizeof (TidyHighlightPrivate));
}

static void
tidy_highlight_init (TidyHighlight *self)
{
  TidyHighlightPrivate *priv;
  GError           *error = NULL;
  ClutterColor     white = {0xFF, 0xFF, 0xFF, 0xFF};

  self->priv = priv = CLUTTER_HIGHLIGHT_GET_PRIVATE (self);
  priv->parent_texture = NULL;
  priv->amount = 0;
  priv->color = white;

  priv->shader = clutter_shader_new();
  clutter_shader_set_fragment_source (priv->shader, HIGHLIGHT_FRAGMENT_SHADER, -1);
  clutter_shader_compile (priv->shader, &error);

  if (error)
  {
    g_warning ("unable to load shader: %s\n", error->message);
    g_error_free (error);
    clutter_shader_release(priv->shader);
    priv->shader = 0;
  }
}

/**
 * tidy_highlight_new:
 * @texture: a #ClutterTexture, or %NULL
 *
 * Creates an efficient 'sub' of a pre-existing texture with which it
 * shares the underlying pixbuf data.
 *
 * You can use tidy_highlight_set_parent_texture() to change the
 * subd texture.
 *
 * Return value: the newly created #TidyHighlight
 */
TidyHighlight *
tidy_highlight_new (ClutterTexture *texture)
{
  g_return_val_if_fail (texture == NULL || CLUTTER_IS_TEXTURE (texture), NULL);

  return g_object_new (TIDY_TYPE_HIGHLIGHT,
 		       "parent-texture", texture,
		       NULL);
}

void tidy_highlight_set_amount (TidyHighlight *sub,
                                float amount)
{
  g_return_if_fail (TIDY_IS_HIGHLIGHT (sub));
  if (amount != sub->priv->amount) {
    sub->priv->amount = amount;
    clutter_actor_queue_redraw(CLUTTER_ACTOR(sub));
  }
}

void tidy_highlight_set_color (TidyHighlight *sub,
                               ClutterColor *col)
{
  g_return_if_fail (TIDY_IS_HIGHLIGHT (sub));

  sub->priv->color = *col;
  clutter_actor_queue_redraw(CLUTTER_ACTOR(sub));
}

