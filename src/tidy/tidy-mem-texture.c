/* Created by Gordon Williams <gordon.williams@collabora.co.uk>
 *
 * This is like TidyMemTexture, but it allows a small region of the
 * texture to be used */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tidy-mem-texture.h"
#include <clutter/clutter-actor.h>

#include <string.h>
#include "cogl/cogl.h"

#define EXACT_ROW_LENGTH 0
/* We can only turn this off (which will be much quicker) when we have the
 * GLES driver/clutter that supports UNPACK_ROW_LENGTH */

/* ------------------------------------------------------------------------- */

/* nice size for SGX? below 100 tends to slow framerate, and increases
 * number of textures that need updating. */
#define TILE_SIZE_X 480
#define TILE_SIZE_Y 480

/* ------------------------------------------------------------------------- */

G_DEFINE_TYPE (TidyMemTexture,
	       tidy_mem_texture,
	       CLUTTER_TYPE_ACTOR);

#define CLUTTER_MEM_TEXTURE_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), TIDY_TYPE_MEM_TEXTURE, TidyMemTexturePrivate))

/* ------------------------------------------------------------------------- */

typedef struct _TidyMemTextureTile
{
  ClutterGeometry pos; /* actual position in texture */
  ClutterGeometry modified; /* geometry for the area modified */
  CoglHandle texture;
} TidyMemTextureTile;

struct _TidyMemTexturePrivate
{
  /* pointer to texture in memory */
  const guchar *texture_ptr;
  /* width and height of the memory texture */
  gint texture_width, texture_height;
  /* BYTES per pixel */
  gint texture_bpp;
  CoglPixelFormat texture_format;
#if EXACT_ROW_LENGTH
  /* Buffer the size of a tile, used to copy the required data in... */
  guchar *tile_buffer;
#endif

  /* Offset of the memory texture in this actor */
  ClutterFixed offset_x;
  ClutterFixed offset_y;
  ClutterFixed scale_x;
  ClutterFixed scale_y;

  GList *tiles; /* of TidyMemTextureTile */
};

/* ------------------------------------------------------------------------- */

static void
tidy_mem_texture_free_data(TidyMemTexture *texture);
static void
tidy_mem_texture_update_modified(TidyMemTexture *texture,
                                 TidyMemTextureTile *tile);
static void
tidy_mem_texture_tile_coords(TidyMemTexture *texture,
                             TidyMemTextureTile *tile,
                             ClutterFixed *x1,
                             ClutterFixed *y1,
                             ClutterFixed *x2,
                             ClutterFixed *y2);
static gboolean
tidy_mem_texture_tile_visible(TidyMemTexture *texture,
                              TidyMemTextureTile *tile,
                              gint width, gint height);
/* ------------------------------------------------------------------------- */

static void
tidy_mem_texture_paint (ClutterActor *self)
{
  TidyMemTexture              *texture = TIDY_MEM_TEXTURE(self);
  TidyMemTexturePrivate       *priv;
  gint                         x_1, y_1, x_2, y_2;
  ClutterColor                 col = { 0xff, 0xff, 0xff, 0xff };

  gint                        width, height;
  GList                       *tiles;

  priv = TIDY_MEM_TEXTURE (self)->priv;

  /* parent texture may have been hidden, there for need to make sure its
   * realised with resources available.
  */
  col.alpha = clutter_actor_get_paint_opacity (self);
  cogl_color (&col);

  clutter_actor_get_allocation_coords (self, &x_1, &y_1, &x_2, &y_2);
  width = x_2 - x_1;
  height = y_2 - y_1;

  /* changetextures before we start our rendering pass */
  for (tiles = priv->tiles; tiles; tiles = tiles->next)
    {
      TidyMemTextureTile *tile = tiles->data;
      if (tidy_mem_texture_tile_visible(texture, tile, width, height))
        {
          /* we're visible, so update if modified, and render... */
          if (tile->modified.width>0 && tile->modified.height>0)
            tidy_mem_texture_update_modified(texture, tile);
        }
    }
  /*next, do our rendering */
  for (tiles = priv->tiles; tiles; tiles = tiles->next)
    {
      TidyMemTextureTile *tile = tiles->data;
      if (tidy_mem_texture_tile_visible(texture, tile, width, height))
        {
          ClutterFixed x1,y1,x2,y2;
          tidy_mem_texture_tile_coords(texture, tile, &x1, &y1, &x2, &y2);
          cogl_texture_rectangle (tile->texture,
                                  x1, y1, x2, y2,
                                  0, 0, CFX_ONE, CFX_ONE);
        }
    }
}


static void
tidy_mem_texture_dispose (GObject *object)
{
  TidyMemTexture         *self = TIDY_MEM_TEXTURE(object);
  /*TidyMemTexturePrivate  *priv = self->priv;*/

  tidy_mem_texture_free_data(self);
  G_OBJECT_CLASS (tidy_mem_texture_parent_class)->dispose (object);
}

static void
tidy_mem_texture_finalize (GObject *object)
{
  G_OBJECT_CLASS (tidy_mem_texture_parent_class)->finalize (object);
}


static void
tidy_mem_texture_class_init (TidyMemTextureClass *klass)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->paint =
    tidy_mem_texture_paint;

  gobject_class->finalize     = tidy_mem_texture_finalize;
  gobject_class->dispose      = tidy_mem_texture_dispose;

  g_type_class_add_private (gobject_class, sizeof (TidyMemTexturePrivate));
}

static void
tidy_mem_texture_init (TidyMemTexture *self)
{
  TidyMemTexturePrivate *priv;

  self->priv = priv = CLUTTER_MEM_TEXTURE_GET_PRIVATE (self);

  priv->texture_ptr = 0;
  priv->texture_width = 0;
  priv->texture_height = 0;
  priv->texture_bpp = 0;
  priv->texture_format = 0;
  priv->offset_x = 0;
  priv->offset_y = 0;
  priv->scale_x = CFX_ONE;
  priv->scale_y = CFX_ONE;

  priv->tiles = 0;
}

/**
 * tidy_mem_texture_new:
 * @texture: a #ClutterTexture, or %NULL
 *
 * Creates an efficient 'sub' of a pre-existing texture with which it
 * shares the underlying pixbuf data.
 *
 * You can use tidy_mem_texture_set_parent_texture() to change the
 * subd texture.
 *
 * Return value: the newly created #TidyMemTexture
 */
TidyMemTexture *
tidy_mem_texture_new ()
{
  return g_object_new (TIDY_TYPE_MEM_TEXTURE,
		       NULL);
}

static void
tidy_mem_texture_free_data(TidyMemTexture *texture)
{
  TidyMemTexturePrivate *priv;

  if (!TIDY_IS_MEM_TEXTURE(texture))
    return;
  priv = texture->priv;

  while (priv->tiles)
    {
      TidyMemTextureTile *tile = priv->tiles->data;
      cogl_texture_unref(tile->texture);
      g_free(tile);
      priv->tiles = priv->tiles->next;
    }
  g_list_free(priv->tiles);
  priv->tiles = 0;

#if EXACT_ROW_LENGTH
  if (priv->tile_buffer)
    {
      g_free(priv->tile_buffer);
      priv->tile_buffer = 0;
    }
#endif
}

static void
tidy_mem_texture_tile_coords(TidyMemTexture *texture,
                             TidyMemTextureTile *tile,
                             ClutterFixed *x1,
                             ClutterFixed *y1,
                             ClutterFixed *x2,
                             ClutterFixed *y2)
{
  TidyMemTexturePrivate *priv = texture->priv;

  *x1 = CFX_QMUL(
          priv->offset_x+CLUTTER_INT_TO_FIXED(tile->pos.x),
          priv->scale_x);
  *y1 = CFX_QMUL(
          priv->offset_y+CLUTTER_INT_TO_FIXED(tile->pos.y),
          priv->scale_y);
  *x2 = CFX_QMUL(
          priv->offset_x+CLUTTER_INT_TO_FIXED(tile->pos.x+tile->pos.width),
          priv->scale_x);
  *y2 = CFX_QMUL(
          priv->offset_y+CLUTTER_INT_TO_FIXED(tile->pos.y+tile->pos.height),
          priv->scale_y);
}

static gboolean
tidy_mem_texture_tile_visible(TidyMemTexture *texture,
                              TidyMemTextureTile *tile,
                              gint width, gint height)
{
  ClutterFixed x1, y1, x2, y2;
  tidy_mem_texture_tile_coords(texture, tile, &x1, &y1, &x2, &y2);
  return (x2 >= 0 && y2 >= 0 &&
          x1 <= CLUTTER_INT_TO_FIXED(width) &&
          y1 <= CLUTTER_INT_TO_FIXED(height));
}

/* Copies the pixels into a buffer with the correct row stride so
 * we can get the data into OpenGL quickly. */
static void
tidy_mem_texture_update_modified(TidyMemTexture *texture,
                                  TidyMemTextureTile *tile)
{
  TidyMemTexturePrivate *priv = texture->priv;
  gint rowstride = priv->texture_width * priv->texture_bpp;
#if EXACT_ROW_LENGTH
  gint y;
  gint rowlength = tile->modified.width * priv->texture_bpp;
  guchar *ptr_dst = priv->tile_buffer;
#endif
  const guchar *ptr_src = &priv->texture_ptr[
                 (tile->pos.x + tile->modified.x +
                 (tile->pos.y + tile->modified.y)*priv->texture_width) *
                 priv->texture_bpp];

#if EXACT_ROW_LENGTH
  for (y=0;y<tile->modified.height;y++)
    {
      memcpy(ptr_dst, ptr_src, rowlength);
      ptr_src += rowstride;
      ptr_dst += rowlength;
    }

  cogl_texture_set_region(tile->texture,
                          0, 0,
                          tile->modified.x, tile->modified.y,
                          tile->modified.width, tile->modified.height,
                          tile->modified.width, tile->modified.height,
                          priv->texture_format,
                          tile->modified.width * priv->texture_bpp,
                          priv->tile_buffer);
#else
  cogl_texture_set_region(tile->texture,
                            0, 0,
                            tile->modified.x, tile->modified.y,
                            tile->modified.width, tile->modified.height,
                            tile->modified.width, tile->modified.height,
                            priv->texture_format,
                            rowstride,
                            ptr_src);
#endif

  /* set modified area to 0 */
  tile->modified.x = 0;
  tile->modified.y = 0;
  tile->modified.width = 0;
  tile->modified.height = 0;
}

void tidy_mem_texture_set_data(TidyMemTexture *texture,
                               const guchar *data,
                               gint width, gint height,
                               gint bytes_per_pixel)
{
  TidyMemTexturePrivate *priv;
  if (!TIDY_IS_MEM_TEXTURE(texture))
    return;
  priv = texture->priv;

  /* free everything first */
  tidy_mem_texture_free_data(texture);
  /* Set texture and data... */
  priv->texture_ptr = data;
  if (priv->texture_ptr)
    {
      gint tiles_x, tiles_y;
      gint x,y;
      priv->texture_width = width;
      priv->texture_height = height;
      priv->texture_bpp = bytes_per_pixel;
      priv->texture_format = 0;
      tiles_x = (priv->texture_width+TILE_SIZE_X-1) / TILE_SIZE_X;
      tiles_y = (priv->texture_height+TILE_SIZE_Y-1) / TILE_SIZE_Y;
      switch (priv->texture_bpp)
        {
          case 1:
            priv->texture_format = COGL_PIXEL_FORMAT_G_8;
            break;
          case 2:
            priv->texture_format = COGL_PIXEL_FORMAT_RGB_565;
            break;
          case 3:
            priv->texture_format = COGL_PIXEL_FORMAT_RGB_888;
            break;
          case 4:
            priv->texture_format = COGL_PIXEL_FORMAT_RGBA_8888;
            break;
        }
#if EXACT_ROW_LENGTH
      /* allocate tile buffer */
      priv->tile_buffer = g_malloc(TILE_SIZE_X * TILE_SIZE_Y * priv->texture_bpp);
#endif
      /* allocate tiles */
      for (y=0;y<tiles_y;y++)
        for (x=0;x<tiles_x;x++)
          {
            TidyMemTextureTile *tile = g_malloc(sizeof(TidyMemTextureTile));
            priv->tiles = g_list_append(priv->tiles, tile);
            /* set coords */
            tile->pos.x = x*TILE_SIZE_X;
            tile->pos.y = y*TILE_SIZE_Y;
            tile->pos.width = TILE_SIZE_X;
            tile->pos.height = TILE_SIZE_Y;
            /* make texture smaller if it would go over the big texture */
            if (tile->pos.x+tile->pos.width > priv->texture_width)
              tile->pos.width = priv->texture_width - tile->pos.x;
            if (tile->pos.y+tile->pos.height > priv->texture_height)
              tile->pos.height = priv->texture_height - tile->pos.y;
            /* alloc texture */
            tile->texture = cogl_texture_new_with_size(
                tile->pos.width, tile->pos.height, -1 /* no waste */,
                FALSE, priv->texture_format);
            /* set whole area to be modified */
            tile->modified.x = 0;
            tile->modified.y = 0;
            tile->modified.width = tile->pos.width;
            tile->modified.height = tile->pos.height;
          }
    }
  else
    {
      /* No texture, just set everything to 0 */
      priv->texture_width = 0;
      priv->texture_height = 0;
      priv->texture_bpp = 0;
      priv->texture_format = 0;
    }
}

void tidy_mem_texture_damage(TidyMemTexture *texture,
                             gint x, gint y,
                             gint width, gint height)
{
  TidyMemTexturePrivate *priv;
  GList *tiles;
  guint actor_width, actor_height;
  gboolean redraw = FALSE;

  if (!TIDY_IS_MEM_TEXTURE(texture))
    return;
  priv = texture->priv;

  clutter_actor_get_size(CLUTTER_ACTOR(texture), &actor_width, &actor_height);

  for (tiles = priv->tiles; tiles; tiles = tiles->next)
    {
      TidyMemTextureTile *tile = tiles->data;

      if (tile->pos.x <= x+width &&
          tile->pos.y <= y+height &&
          tile->pos.x+tile->pos.width >= x &&
          tile->pos.y+tile->pos.height >= y)
        {
          /* work out geometry of modified area */
          ClutterGeometry mod;
          mod.x = x - tile->pos.x;
          mod.y = y - tile->pos.y;
          if (mod.x < 0) mod.x = 0;
          if (mod.y < 0) mod.y = 0;
          mod.width = (x+width) - (tile->pos.x + mod.x);
          mod.height = (y+height) - (tile->pos.y + mod.y);
          if (mod.width+mod.x > tile->pos.width)
            mod.width = tile->pos.width - mod.x;
          if (mod.height+mod.y > tile->pos.height)
            mod.height = tile->pos.height - mod.y;

          /* FIXME: width and height are unsigned, i.e. always >= 0 */
          if (tile->modified.width>=0 && tile->modified.height>=0)
            {
              /* if we already have damage, extend damaged area */
              gint oldx2, oldy2, newx2, newy2;
              oldx2 = tile->modified.x + tile->modified.width;
              oldy2 = tile->modified.y + tile->modified.height;
              newx2 = mod.x + mod.width;
              newy2 = mod.y + mod.height;

              if (mod.x < tile->modified.x)
                tile->modified.x = mod.x;
              if (mod.y < tile->modified.y)
                tile->modified.y = mod.y;
              if (newx2 > oldx2)
                oldx2 = newx2;
              if (newy2 > oldy2)
                oldy2 = newy2;
              tile->modified.width = oldx2 - tile->modified.x;
              tile->modified.height = oldy2 - tile->modified.y;
            }
          else
            {
              /* else just set damaged area */
              tile->modified = mod;
            }

          /* only redraw if the changed tile is visible */
          if (tidy_mem_texture_tile_visible(texture, tile,
                                            actor_width, actor_height))
            redraw = TRUE;
        }
    }

  if (redraw)
    clutter_actor_queue_redraw(CLUTTER_ACTOR(texture));
}

void tidy_mem_texture_set_offset(TidyMemTexture *texture,
                                 ClutterFixed x, ClutterFixed y)
{
  TidyMemTexturePrivate *priv;
  if (!TIDY_IS_MEM_TEXTURE(texture))
    return;
  priv = texture->priv;

  if (priv->offset_x!=x || priv->offset_y!=y)
    {
      priv->offset_x = x;
      priv->offset_y = y;

      clutter_actor_queue_redraw(CLUTTER_ACTOR(texture));
    }
}

void tidy_mem_texture_set_scale(TidyMemTexture *texture,
                                ClutterFixed scale_x,
                                ClutterFixed scale_y)
{
  TidyMemTexturePrivate *priv;
  if (!TIDY_IS_MEM_TEXTURE(texture))
    return;
  priv = texture->priv;

  if (priv->scale_x!=scale_x || priv->scale_y!=scale_y)
    {
      priv->scale_x = scale_x;
      priv->scale_y = scale_y;

      clutter_actor_queue_redraw(CLUTTER_ACTOR(texture));
    }
}
