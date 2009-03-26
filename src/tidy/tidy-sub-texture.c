/* Created by Gordon Williams <gordon.williams@collabora.co.uk>
 *
 * This is like ClutterCloneTexture, but it allows a small region of the
 * texture to be used */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tidy-sub-texture.h"
#include <clutter/clutter-actor.h>

#include "cogl/cogl.h"

enum
{
  PROP_0,
  PROP_PARENT_TEXTURE,
};

G_DEFINE_TYPE (TidySubTexture,
	       tidy_sub_texture,
	       CLUTTER_TYPE_ACTOR);

#define CLUTTER_SUB_TEXTURE_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), TIDY_TYPE_SUB_TEXTURE, TidySubTexturePrivate))

struct _TidySubTexturePrivate
{
  ClutterTexture      *parent_texture;
  ClutterGeometry      region;
};

static void
tidy_sub_texture_get_preferred_width (ClutterActor *self,
                                           ClutterUnit   for_height,
                                           ClutterUnit  *min_width_p,
                                           ClutterUnit  *natural_width_p)
{
  TidySubTexturePrivate *priv = TIDY_SUB_TEXTURE (self)->priv;
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
tidy_sub_texture_get_preferred_height (ClutterActor *self,
                                            ClutterUnit   for_width,
                                            ClutterUnit  *min_height_p,
                                            ClutterUnit  *natural_height_p)
{
  TidySubTexturePrivate *priv = TIDY_SUB_TEXTURE (self)->priv;
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
tidy_sub_texture_paint (ClutterActor *self)
{
  TidySubTexturePrivate  *priv;
  ClutterActor                *parent_texture;
  gint                         x_1, y_1, x_2, y_2;
  ClutterColor                 col = { 0xff, 0xff, 0xff, 0xff };
  CoglHandle                   cogl_texture;
  ClutterFixed                 t_x, t_y, t_w, t_h;
  guint                        tex_width, tex_height;
  ClutterGeometry              region;

  priv = TIDY_SUB_TEXTURE (self)->priv;

  /* no need to paint stuff if we don't have a texture to sub */
  if (!priv->parent_texture)
    return;

  /* parent texture may have been hidden, there for need to make sure its
   * realised with resources available.
  */
  parent_texture = CLUTTER_ACTOR (priv->parent_texture);
  if (!CLUTTER_ACTOR_IS_REALIZED (parent_texture))
    clutter_actor_realize (parent_texture);

  col.alpha = clutter_actor_get_paint_opacity (self);
  cogl_color (&col);

  clutter_actor_get_allocation_coords (self, &x_1, &y_1, &x_2, &y_2);

  cogl_texture = clutter_texture_get_cogl_texture (priv->parent_texture);

  if (cogl_texture == COGL_INVALID_HANDLE)
    return;

  tex_width = cogl_texture_get_width (cogl_texture);
  tex_height = cogl_texture_get_height (cogl_texture);
  region = priv->region;
  if (region.width==0 && region.height==0)
    {
      region.x = 0;
      region.y = 0;
      region.width = tex_width;
      region.height = tex_height;
    }

  t_x = CLUTTER_FLOAT_TO_FIXED(region.x / (float)tex_width);
  t_y = CLUTTER_FLOAT_TO_FIXED(region.y / (float)tex_height);
  t_w = CLUTTER_FLOAT_TO_FIXED(region.width / (float)tex_width);
  t_h = CLUTTER_FLOAT_TO_FIXED(region.height / (float)tex_height);

  /* Parent paint translated us into position */
  cogl_texture_rectangle (cogl_texture, 0, 0,
			  CLUTTER_INT_TO_FIXED (x_2 - x_1),
			  CLUTTER_INT_TO_FIXED (y_2 - y_1),
			  t_x, t_y, t_x+t_w, t_y+t_h);
}

static void
set_parent_texture (TidySubTexture *ctexture,
		    ClutterTexture      *texture)
{
  TidySubTexturePrivate *priv = ctexture->priv;
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
tidy_sub_texture_dispose (GObject *object)
{
  TidySubTexture         *self = TIDY_SUB_TEXTURE(object);
  TidySubTexturePrivate  *priv = self->priv;

  if (priv->parent_texture)
    g_object_unref (priv->parent_texture);

  priv->parent_texture = NULL;

  G_OBJECT_CLASS (tidy_sub_texture_parent_class)->dispose (object);
}

static void
tidy_sub_texture_finalize (GObject *object)
{
  G_OBJECT_CLASS (tidy_sub_texture_parent_class)->finalize (object);
}

static void
tidy_sub_texture_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)
{
  TidySubTexture        *ctexture = TIDY_SUB_TEXTURE (object);
  TidySubTexturePrivate *priv;

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
tidy_sub_texture_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
  TidySubTexture *ctexture = TIDY_SUB_TEXTURE (object);
  TidySubTexturePrivate *priv;

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
tidy_sub_texture_class_init (TidySubTextureClass *klass)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint =
    tidy_sub_texture_paint;
  actor_class->get_preferred_width =
    tidy_sub_texture_get_preferred_width;
  actor_class->get_preferred_height =
    tidy_sub_texture_get_preferred_height;

  gobject_class->finalize     = tidy_sub_texture_finalize;
  gobject_class->dispose      = tidy_sub_texture_dispose;
  gobject_class->set_property = tidy_sub_texture_set_property;
  gobject_class->get_property = tidy_sub_texture_get_property;

  g_object_class_install_property
    (gobject_class, PROP_PARENT_TEXTURE,
     g_param_spec_object ("parent-texture",
			  "Parent Texture",
			  "The parent texture to sub",
			  CLUTTER_TYPE_TEXTURE,
			  G_PARAM_READABLE | G_PARAM_WRITABLE |
			  G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
			  G_PARAM_STATIC_BLURB ));

  g_type_class_add_private (gobject_class, sizeof (TidySubTexturePrivate));
}

static void
tidy_sub_texture_init (TidySubTexture *self)
{
  TidySubTexturePrivate *priv;
  ClutterGeometry null_region = {0,0,0,0};

  self->priv = priv = CLUTTER_SUB_TEXTURE_GET_PRIVATE (self);
  priv->parent_texture = NULL;
  priv->region = null_region;
}

/**
 * tidy_sub_texture_new:
 * @texture: a #ClutterTexture, or %NULL
 *
 * Creates an efficient 'sub' of a pre-existing texture with which it
 * shares the underlying pixbuf data.
 *
 * You can use tidy_sub_texture_set_parent_texture() to change the
 * subd texture.
 *
 * Return value: the newly created #TidySubTexture
 */
TidySubTexture *
tidy_sub_texture_new (ClutterTexture *texture)
{
  g_return_val_if_fail (texture == NULL || CLUTTER_IS_TEXTURE (texture), NULL);

  return g_object_new (TIDY_TYPE_SUB_TEXTURE,
 		       "parent-texture", texture,
		       NULL);
}

/**
 * tidy_sub_texture_get_parent_texture:
 * @sub: a #TidySubTexture
 *
 * Retrieves the parent #ClutterTexture used by @sub.
 *
 * Return value: a #ClutterTexture actor, or %NULL
 */
ClutterTexture *
tidy_sub_texture_get_parent_texture (TidySubTexture *sub)
{
  g_return_val_if_fail (TIDY_IS_SUB_TEXTURE (sub), NULL);

  return sub->priv->parent_texture;
}

/**
 * tidy_sub_texture_set_parent_texture:
 * @sub: a #TidySubTexture
 * @texture: a #ClutterTexture or %NULL
 *
 * Sets the parent texture subd by the #TidySubTexture.
 */
void
tidy_sub_texture_set_parent_texture (TidySubTexture *sub,
                                          ClutterTexture      *texture)
{
  g_return_if_fail (TIDY_IS_SUB_TEXTURE (sub));
  g_return_if_fail (texture == NULL || CLUTTER_IS_TEXTURE (texture));

  g_object_ref (sub);

  set_parent_texture (sub, texture);

  g_object_notify (G_OBJECT (sub), "parent-texture");
  g_object_unref (sub);
}

void tidy_sub_texture_set_region (TidySubTexture *sub,
                                  ClutterGeometry *region)
{
  g_return_if_fail (TIDY_IS_SUB_TEXTURE (sub));
  sub->priv->region = *region;
}

