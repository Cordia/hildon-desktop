/* tidy-scroll-bar.c: Scroll bar actor
 *
 * Copyright (C) 2008 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Written by: Chris Lord <chris@openedhand.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <clutter/clutter.h>

#include "tidy-scroll-bar.h"
#include "tidy-marshal.h"
#include "tidy-stylable.h"
#include "tidy-enum-types.h"
#include "tidy-private.h"

static void tidy_stylable_iface_init (TidyStylableIface *iface);
static void on_style_change (TidyStyle     *style,
                             TidyScrollBar *bar);

G_DEFINE_TYPE_WITH_CODE (TidyScrollBar, tidy_scroll_bar, TIDY_TYPE_ACTOR,
                         G_IMPLEMENT_INTERFACE (TIDY_TYPE_STYLABLE,
                                                tidy_stylable_iface_init))

#define TIDY_SCROLL_BAR_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), TIDY_TYPE_SCROLL_BAR, TidyScrollBarPrivate))

struct _TidyScrollBarPrivate
{
  TidyAdjustment *adjustment;
  guint           refresh_source;
  
  gfloat          x_origin;
  
  ClutterActor   *handle;
  ClutterActor   *texture;

  CoglColor       bg_color;
  
};

enum
{
  PROP_0,

  PROP_HANDLE,
  PROP_TEXTURE,
  PROP_ADJUSTMENT
};

static void
tidy_scroll_bar_get_property (GObject    *gobject,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  TidyScrollBarPrivate *priv = TIDY_SCROLL_BAR (gobject)->priv;

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      g_value_set_object (value, priv->adjustment);
      break;

    case PROP_TEXTURE:
      g_value_set_object (value, priv->texture);
      break;

    case PROP_HANDLE:
      g_value_set_object (value, priv->handle);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
tidy_scroll_bar_set_property (GObject      *gobject,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  TidyScrollBar *bar = TIDY_SCROLL_BAR (gobject);

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      tidy_scroll_bar_set_adjustment (bar, g_value_get_object (value));
      break;

    case PROP_HANDLE:
      tidy_scroll_bar_set_handle (bar, g_value_get_object (value));
      break;

    case PROP_TEXTURE:
      tidy_scroll_bar_set_texture (bar, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
tidy_scroll_bar_dispose (GObject *gobject)
{
  TidyScrollBar *bar = TIDY_SCROLL_BAR (gobject);
  TidyScrollBarPrivate *priv = bar->priv;

  g_signal_handlers_disconnect_by_func (tidy_stylable_get_style (TIDY_STYLABLE (bar)),
                                        G_CALLBACK (on_style_change), bar);

  if (priv->refresh_source)
    {
      g_source_remove (priv->refresh_source);
      priv->refresh_source = 0;
    }
  
  if (priv->adjustment)
    tidy_scroll_bar_set_adjustment (bar, NULL);

  if (priv->texture)
    {
      clutter_actor_destroy (priv->texture);
      priv->texture = NULL;
    }

  if (priv->handle)
    {
      clutter_actor_destroy (priv->handle);
      priv->handle = NULL;
    }

  G_OBJECT_CLASS (tidy_scroll_bar_parent_class)->dispose (gobject);
}

static void
tidy_scroll_bar_paint (ClutterActor *actor)
{
  TidyScrollBarPrivate *priv = TIDY_SCROLL_BAR (actor)->priv;

  if (priv->texture && CLUTTER_ACTOR_IS_VISIBLE (priv->texture))
    clutter_actor_paint (priv->texture);
  else
    {
      CoglColor bg_color;
      gfloat w, h;

      clutter_actor_get_size (actor, &w, &h);

      bg_color = priv->bg_color;
      cogl_color_set_alpha_float(&bg_color,
                                 clutter_actor_get_opacity (actor)
                                 * cogl_color_get_alpha_float(&priv->bg_color));

      cogl_set_source_color (&bg_color);
      cogl_rectangle (0.0f, 0.0f, w, h);
    }

  if (priv->handle && CLUTTER_ACTOR_IS_VISIBLE (priv->handle))
    clutter_actor_paint (priv->handle);
}

static void
tidy_scroll_bar_pick (ClutterActor       *actor,
                      const ClutterColor *pick_color)
{
  TidyScrollBarPrivate *priv = TIDY_SCROLL_BAR (actor)->priv;

  CLUTTER_ACTOR_CLASS (tidy_scroll_bar_parent_class)->pick (actor, pick_color);

  if (priv->handle &&
      CLUTTER_ACTOR_IS_VISIBLE (priv->handle) &&
      priv->adjustment)
    clutter_actor_paint (priv->handle);
}

static void
tidy_scroll_bar_allocate (ClutterActor           *actor,
                          const ClutterActorBox  *box,
                          ClutterAllocationFlags  flags)
{
  TidyScrollBarPrivate *priv = TIDY_SCROLL_BAR (actor)->priv;

  /* Chain up */
  CLUTTER_ACTOR_CLASS (tidy_scroll_bar_parent_class)->
    allocate (actor, box, flags);

  if (priv->texture)
    clutter_actor_set_size (priv->texture,
                             box->x2 - box->x1,
                             box->y2 - box->y1);

  if (priv->adjustment)
    {
      gfloat real_width, real_height;
      gfloat lower, upper, page_size, size, increment;
      ClutterActorBox child_box;
      guint min_size, max_size;
      TidyPadding padding;

      tidy_adjustment_get_values (priv->adjustment,
                                  NULL,
                                  &lower,
                                  &upper,
                                  NULL,
                                  NULL,
                                  &page_size);

      tidy_actor_get_padding (TIDY_ACTOR (actor), &padding);

      real_width = box->x2 - box->x1 - padding.left - padding.right;
      real_height = box->y2 - box->y1 - padding.top - padding.bottom;

      if (upper == lower)
        increment = 1.0f;
      else
        increment = page_size * (upper - lower);
      
      size = real_width / increment;
      if (size > real_width) size = real_width;

      tidy_stylable_get (TIDY_STYLABLE (actor),
                         "min-size", &min_size,
                         "max-size", &max_size,
                         NULL);

      clutter_actor_get_position (priv->handle, &child_box.x1, &child_box.y1);
      child_box.x2 = child_box.x1 +
                      MIN (max_size,
                           MAX (min_size, size));
      child_box.y2 = child_box.y1 + real_height;
      
      clutter_actor_allocate (priv->handle,
                              &child_box,
                              flags);
    }
}

static void
on_style_change (TidyStyle     *style,
                 TidyScrollBar *bar)
{
  TidyScrollBarPrivate *priv = bar->priv;
  CoglColor *color = NULL;

  tidy_stylable_get (TIDY_STYLABLE (bar), "bg-color", &color, NULL);
  if (color)
    {
      priv->bg_color = *color;
      cogl_color_free (color);
    }

#ifdef MAEGO_DISABLED
  if (CLUTTER_IS_RECTANGLE (priv->handle))
    {
      tidy_stylable_get (TIDY_STYLABLE (bar), "active-color", &color, NULL);
      if (color)
        {
          cogl_rectangle_set_color (CLUTTER_RECTANGLE (priv->handle), color);
          cogl_color_free (color);
        }
    }
#endif
}

static void
bar_reactive_notify_cb (GObject *gobject,
                        GParamSpec *arg1,
                        gpointer user_data)
{
  TidyScrollBar *bar = TIDY_SCROLL_BAR (gobject);
  
  clutter_actor_set_reactive (bar->priv->handle,
                              clutter_actor_get_reactive (CLUTTER_ACTOR (bar)));
}

static GObject*
tidy_scroll_bar_constructor (GType                  type,
                             guint                  n_properties,
                             GObjectConstructParam *properties)
{
  GObjectClass         *gobject_class;
  GObject              *obj;
  TidyScrollBar        *bar;
  TidyScrollBarPrivate *priv;

  gobject_class = G_OBJECT_CLASS (tidy_scroll_bar_parent_class);
  obj = gobject_class->constructor (type, n_properties, properties);

  bar  = TIDY_SCROLL_BAR (obj);
  priv = TIDY_SCROLL_BAR_GET_PRIVATE (bar);

  if (!priv->handle)
    tidy_scroll_bar_set_handle (bar, NULL);

  g_signal_connect (bar, "notify::reactive",
                    G_CALLBACK (bar_reactive_notify_cb), NULL);
  g_signal_connect (tidy_stylable_get_style (TIDY_STYLABLE (bar)),
                    "changed", G_CALLBACK (on_style_change), bar);

  /* Manually call on_style_change */
  on_style_change (NULL, bar);

  return obj;
}

static void
tidy_scroll_bar_class_init (TidyScrollBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TidyScrollBarPrivate));

  object_class->get_property = tidy_scroll_bar_get_property;
  object_class->set_property = tidy_scroll_bar_set_property;
  object_class->dispose      = tidy_scroll_bar_dispose;
  object_class->constructor  = tidy_scroll_bar_constructor;

  actor_class->allocate       = tidy_scroll_bar_allocate;
  actor_class->paint          = tidy_scroll_bar_paint;
  actor_class->pick           = tidy_scroll_bar_pick;
  
  g_object_class_install_property 
           (object_class,
            PROP_ADJUSTMENT,
            g_param_spec_object ("adjustment",
                                 "Adjustment",
                                 "The adjustment",
                                 TIDY_TYPE_ADJUSTMENT,
                                 TIDY_PARAM_READWRITE));

  g_object_class_install_property 
           (object_class,
            PROP_HANDLE,
            g_param_spec_object ("handle",
                                 "Handle",
                                 "Actor to use for the scrollbars handle",
                                 CLUTTER_TYPE_ACTOR,
                                 TIDY_PARAM_READWRITE));
}

static void
tidy_stylable_iface_init (TidyStylableIface *iface)
{
  static gboolean is_initialized = FALSE;

  if (!is_initialized)
    {
      GParamSpec *pspec;

      pspec = g_param_spec_uint ("min-size",
                                 "Minimum grabber size",
                                 "Minimum size of the scroll grabber, in px",
                                 0, G_MAXUINT, 32,
                                 G_PARAM_READWRITE);
      tidy_stylable_iface_install_property (iface, 
                                            TIDY_TYPE_SCROLL_BAR, pspec);

      pspec = g_param_spec_uint ("max-size",
                                 "Maximum grabber size",
                                 "Maximum size of the scroll grabber, in px",
                                 0, G_MAXINT16, G_MAXINT16,
                                 G_PARAM_READWRITE);
      tidy_stylable_iface_install_property (iface, 
                                            TIDY_TYPE_SCROLL_BAR, pspec);
    }
}

static void
move_slider (TidyScrollBar *bar, gfloat x, gfloat y, gboolean interpolate)
{
  gfloat position, lower, upper, page_size;
  gfloat ux, width;
  
  TidyScrollBarPrivate *priv = bar->priv;

  if (!priv->adjustment)
    return;

  if (!clutter_actor_transform_stage_point (CLUTTER_ACTOR(bar),
                                            x, y,
                                            &ux, NULL))
    return;
  
  width = clutter_actor_get_width (CLUTTER_ACTOR (bar)) -
          clutter_actor_get_width (priv->handle);
  
  if (width == 0)
    return;
  
  ux -= priv->x_origin;
  ux = CLAMP (ux, 0, width);

  tidy_adjustment_get_values (priv->adjustment,
                              NULL,
                              &lower,
                              &upper,
                              NULL,
                              NULL,
                              &page_size);

  position = (ux / width) * (upper - lower - page_size) + lower;
  
#ifdef MAEGO_DISABLED
  if (interpolate)
    {
      guint mfreq = clutter_get_motion_events_frequency ();
      guint fps = clutter_get_default_frame_rate ();
      guint n_frames = fps / mfreq;
      
      tidy_adjustment_interpolate (priv->adjustment,
                                   position,
                                   n_frames,
                                   fps);
      return;
    }
#endif
  
  tidy_adjustment_set_value (priv->adjustment, position);
}

static gboolean
motion_event_cb (TidyScrollBar *bar,
                 ClutterMotionEvent *event,
                 gpointer user_data)
{
  move_slider (bar, event->x, event->y, TRUE);

  return TRUE;
}

static gboolean
button_release_event_cb (TidyScrollBar *bar,
                         ClutterButtonEvent *event,
                         gpointer user_data)
{
  if (event->button != 1)
    return FALSE;
  
  g_signal_handlers_disconnect_by_func (bar, motion_event_cb, NULL);
  g_signal_handlers_disconnect_by_func (bar, button_release_event_cb, NULL);

  move_slider (bar, event->x, event->y, FALSE);
  
  clutter_ungrab_pointer ();

  return TRUE;
}

static gboolean
button_press_event_cb (ClutterActor       *actor,
                       ClutterButtonEvent *event,
                       TidyScrollBar      *bar)
{
  TidyScrollBarPrivate *priv = bar->priv;
  
  if (event->button != 1)
    return FALSE;
  
  if (!clutter_actor_transform_stage_point (actor,
                                            event->x, event->y,
                                            &priv->x_origin, NULL))
    return FALSE;
  
  g_signal_connect_after (bar, "motion-event",
                          G_CALLBACK (motion_event_cb), NULL);
  g_signal_connect_after (bar, "button-release-event",
                          G_CALLBACK (button_release_event_cb), NULL);
  
  clutter_grab_pointer (CLUTTER_ACTOR (bar));
  
  return TRUE;
}

static void
tidy_scroll_bar_init (TidyScrollBar *self)
{
  self->priv = TIDY_SCROLL_BAR_GET_PRIVATE (self);
}

ClutterActor *
tidy_scroll_bar_new (TidyAdjustment *adjustment)
{
  return g_object_new (TIDY_TYPE_SCROLL_BAR,
                       "adjustment", adjustment,
                       NULL);
}

ClutterActor *
tidy_scroll_bar_new_with_handle (TidyAdjustment *adjustment, 
                                 ClutterActor   *handle)
{
  return g_object_new (TIDY_TYPE_SCROLL_BAR,
                       "adjustment", adjustment,
                       "handle", handle,
                       NULL);
}


static gboolean
tidy_scroll_bar_refresh (TidyScrollBar *bar)
{
  ClutterActor *actor = CLUTTER_ACTOR (bar);
  TidyScrollBarPrivate *priv = bar->priv;
  gfloat width, button_width;
  gfloat lower, upper, value, page_size;
  gfloat x, position;
  
  /* Work out scroll handle size */
  tidy_adjustment_get_values (priv->adjustment,
                              &value,
                              &lower,
                              &upper,
                              NULL,
                              NULL,
                              &page_size);

  if (upper - page_size <= lower)
    {
      clutter_actor_set_position (CLUTTER_ACTOR (priv->handle), 0, 0);
      priv->refresh_source = 0;
      return FALSE;
    }
  
  width = clutter_actor_get_width (actor);
  button_width = clutter_actor_get_width (priv->handle);

  position = (value - lower) / (upper - lower - page_size);

  /* Set padding on trough */
  x = position * (width - button_width);
  clutter_actor_set_position (CLUTTER_ACTOR (priv->handle), x, 0);
  
  clutter_actor_queue_redraw (actor);
  
  priv->refresh_source = 0;
  return FALSE;
}

static void
adjustment_changed_cb (TidyScrollBar *bar)
{
  TidyScrollBarPrivate *priv = bar->priv;
  
  if (!priv->refresh_source)
    priv->refresh_source = g_idle_add_full (CLUTTER_PRIORITY_REDRAW,
                                            (GSourceFunc)
                                            tidy_scroll_bar_refresh,
                                            bar,
                                            NULL);
}

void
tidy_scroll_bar_set_adjustment (TidyScrollBar *bar,
                                TidyAdjustment *adjustment)
{
  TidyScrollBarPrivate *priv;
  
  g_return_if_fail (TIDY_IS_SCROLL_BAR (bar));
  
  priv = bar->priv;
  if (priv->adjustment)
    {
      g_signal_handlers_disconnect_by_func (priv->adjustment,
                                            clutter_actor_queue_relayout,
                                            bar);
      g_signal_handlers_disconnect_by_func (priv->adjustment,
                                            adjustment_changed_cb,
                                            bar);
      g_object_unref (priv->adjustment);
      priv->adjustment = NULL;
    }

  if (adjustment)
    {
      priv->adjustment = g_object_ref (adjustment);

      g_signal_connect_swapped (priv->adjustment, "notify::value",
                                G_CALLBACK (adjustment_changed_cb),
                                bar);
      g_signal_connect_swapped (priv->adjustment, "changed",
				G_CALLBACK (clutter_actor_queue_relayout),
				bar);

      clutter_actor_queue_relayout (CLUTTER_ACTOR (bar));
    }
}

TidyAdjustment *
tidy_scroll_bar_get_adjustment (TidyScrollBar *bar)
{
  g_return_val_if_fail (TIDY_IS_SCROLL_BAR (bar), NULL);
  
  return bar->priv->adjustment;
}

void
tidy_scroll_bar_set_handle (TidyScrollBar *bar,
                            ClutterActor *handle)
{
  TidyScrollBarPrivate *priv;
  
  g_return_if_fail (TIDY_IS_SCROLL_BAR (bar));
  g_return_if_fail (!handle || CLUTTER_IS_ACTOR (handle));
  
  priv = bar->priv;

  if (handle && (priv->handle == handle))
    return;

  if (priv->handle)
    {
      g_signal_handlers_disconnect_by_func(priv->handle, 
                                           G_CALLBACK (button_press_event_cb),
                                           bar);

      clutter_actor_unparent (priv->handle);
      priv->handle = NULL;
    }

  if (!handle)
    {
      /* default handle */
      ClutterColor *color;
      
      handle = clutter_rectangle_new ();
      clutter_actor_set_name(handle, "TidyScrollBar:handle");
  
      tidy_stylable_get (TIDY_STYLABLE (bar), "active-color", &color, NULL);
      if (color)
        {
          clutter_rectangle_set_color (CLUTTER_RECTANGLE (handle), color);
          clutter_color_free (color);
        }
    }

  priv->handle = handle;
  clutter_actor_set_parent (priv->handle, CLUTTER_ACTOR (bar));
  
  clutter_actor_show (priv->handle);
  clutter_actor_set_reactive (priv->handle, TRUE);
  
  g_signal_connect_after (priv->handle, "button-press-event",
                          G_CALLBACK (button_press_event_cb), bar);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (bar));

  g_object_notify (G_OBJECT (bar), "handle");
}

ClutterActor *
tidy_scroll_bar_get_handle (TidyScrollBar *bar)
{
  g_return_val_if_fail (TIDY_IS_SCROLL_BAR (bar), NULL);

  return bar->priv->handle;
}

void
tidy_scroll_bar_set_texture (TidyScrollBar *bar,
                             ClutterActor  *texture)
{
  TidyScrollBarPrivate *priv;

  g_return_if_fail (TIDY_IS_SCROLL_BAR (bar));
  g_return_if_fail (texture == NULL || CLUTTER_IS_ACTOR (texture));

  priv = bar->priv;

  if (priv->texture)
    {
      clutter_actor_unparent (priv->texture);
      priv->texture = NULL;
    }

  if (texture)
    {
      gfloat width, height;

      clutter_actor_get_size (CLUTTER_ACTOR (bar), &width, &height);

      priv->texture = texture;
      clutter_actor_set_parent (priv->texture, CLUTTER_ACTOR (bar));

      clutter_actor_set_position (priv->texture, 0, 0);
      clutter_actor_set_size (priv->texture, width, height);
    }

  if (CLUTTER_ACTOR_IS_VISIBLE (bar))
    clutter_actor_queue_redraw (CLUTTER_ACTOR (bar));
}

ClutterActor *
tidy_scroll_bar_get_texture (TidyScrollBar *bar)
{
  g_return_val_if_fail (TIDY_IS_SCROLL_BAR (bar), NULL);

  return bar->priv->texture;
}

