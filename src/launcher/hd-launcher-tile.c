/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Marc Ordinas i Llopis <marc.ordinasillopis@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hd-launcher.h"
#include "hd-launcher-tile.h"

#include <glib-object.h>
#include <clutter/clutter.h>
#include <hildon/hildon-defines.h>
#include <stdlib.h>

#include "hd-gtk-style.h"

#define I_(str) (g_intern_static_string ((str)))
#define HD_PARAM_READWRITE (G_PARAM_READWRITE | \
                            G_PARAM_STATIC_NICK | \
                            G_PARAM_STATIC_NAME | \
                            G_PARAM_STATIC_BLURB)

#define HD_LAUNCHER_TILE_GET_PRIVATE(obj)       (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HD_TYPE_LAUNCHER_TILE, HdLauncherTilePrivate))

struct _HdLauncherTilePrivate
{
  gchar *icon_name;
  gchar *text;

  ClutterActor *icon;
  ClutterActor *label;

  /* We need to know if there's been scrolling. */
  gboolean is_pressed;
  gint x_press_pos;
  gint y_press_pos;
};

enum
{
  PROP_0,

  PROP_LAUNCHER_TILE_ICON_NAME,
  PROP_LAUNCHER_TILE_TEXT
};

enum
{
  CLICKED,

  LAST_SIGNAL
};

static guint launcher_tile_signals[LAST_SIGNAL] = { 0, };

/* Forward declarations */
/*   GObject */
static void hd_launcher_tile_finalize (GObject *gobject);
static void hd_launcher_tile_get_property (GObject    *object,
                                           guint       property_id,
                                           GValue     *value,
                                           GParamSpec *pspec);
static void hd_launcher_tile_set_property (GObject      *gobject,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);
/* ClutterActor */
static void hd_launcher_tile_get_preferred_width (ClutterActor *actor,
                                                  ClutterUnit   for_height,
                                                  ClutterUnit  *min_width_p,
                                                  ClutterUnit *natural_width_p);
static void hd_launcher_tile_get_preferred_height (ClutterActor *actor,
                                                   ClutterUnit   for_width,
                                                   ClutterUnit  *min_height_p,
                                                   ClutterUnit  *natural_height_p);
static void hd_launcher_tile_allocate (ClutterActor          *actor,
                                       const ClutterActorBox *box,
                                       gboolean               origin_changed);
static void hd_launcher_tile_paint (ClutterActor *actor);
static void hd_launcher_tile_pick (ClutterActor       *actor,
                                   const ClutterColor *pick_color);
static void hd_launcher_tile_show (ClutterActor *actor);
static void hd_launcher_tile_hide (ClutterActor *actor);
static gboolean hd_launcher_tile_button_press (ClutterActor       *actor,
                                               ClutterButtonEvent *event);
static gboolean hd_launcher_tile_button_release (ClutterActor       *actor,
                                                 ClutterButtonEvent *event);

G_DEFINE_TYPE (HdLauncherTile, hd_launcher_tile, CLUTTER_TYPE_ACTOR);

static void
hd_launcher_tile_class_init (HdLauncherTileClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (HdLauncherTilePrivate));

  gobject_class->get_property = hd_launcher_tile_get_property;
  gobject_class->set_property = hd_launcher_tile_set_property;
  gobject_class->finalize     = hd_launcher_tile_finalize;

  actor_class->get_preferred_width  = hd_launcher_tile_get_preferred_width;
  actor_class->get_preferred_height = hd_launcher_tile_get_preferred_height;
  actor_class->allocate             = hd_launcher_tile_allocate;
  actor_class->paint                = hd_launcher_tile_paint;
  actor_class->pick                 = hd_launcher_tile_pick;
  actor_class->button_press_event   = hd_launcher_tile_button_press;
  actor_class->button_release_event = hd_launcher_tile_button_release;
  actor_class->show                 = hd_launcher_tile_show;
  actor_class->hide                 = hd_launcher_tile_hide;

  pspec = g_param_spec_string ("icon-name",
                               "Icon Name",
                               "Name of the icon to display",
                               HD_LAUNCHER_DEFAULT_ICON,
                               G_PARAM_CONSTRUCT | HD_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_LAUNCHER_TILE_ICON_NAME, pspec);
  pspec = g_param_spec_string ("text",
                               "Text",
                               "Text to display",
                               "Unknown",
                               G_PARAM_CONSTRUCT | HD_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_LAUNCHER_TILE_TEXT, pspec);

  launcher_tile_signals[CLICKED] =
    g_signal_new (I_("clicked"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hd_launcher_tile_init (HdLauncherTile *tile)
{
  HdLauncherTilePrivate *priv;

  tile->priv = priv = HD_LAUNCHER_TILE_GET_PRIVATE (tile);

  clutter_actor_set_reactive (CLUTTER_ACTOR (tile), TRUE);
}

HdLauncherTile *
hd_launcher_tile_new (const gchar *icon_name, const gchar *text)
{
  return g_object_new (HD_TYPE_LAUNCHER_TILE,
                       "icon-name", icon_name,
                       "text", text,
                       NULL);
}

const gchar *
hd_launcher_tile_get_icon_name (HdLauncherTile *tile)
{
  HdLauncherTilePrivate *priv = HD_LAUNCHER_TILE_GET_PRIVATE (tile);

  return priv->icon_name;
}

const gchar *
hd_launcher_tile_get_text (HdLauncherTile *tile)
{
  HdLauncherTilePrivate *priv = HD_LAUNCHER_TILE_GET_PRIVATE (tile);

  return priv->text;
}

static void
hd_launcher_tile_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  HdLauncherTile *tile = HD_LAUNCHER_TILE (object);

  switch (property_id)
    {
    case PROP_LAUNCHER_TILE_ICON_NAME:
      g_value_set_string (value,
          hd_launcher_tile_get_icon_name (tile));
      break;

    case PROP_LAUNCHER_TILE_TEXT:
      g_value_set_string (value,
          hd_launcher_tile_get_text (tile));
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

ClutterActor *
hd_launcher_tile_get_icon (HdLauncherTile *tile)
{
  HdLauncherTilePrivate *priv = HD_LAUNCHER_TILE_GET_PRIVATE (tile);

  return priv->icon;
}

ClutterActor *
hd_launcher_tile_get_label (HdLauncherTile *tile)
{
  HdLauncherTilePrivate *priv = HD_LAUNCHER_TILE_GET_PRIVATE (tile);

  return priv->label;
}

void
hd_launcher_tile_set_icon_name (HdLauncherTile *tile,
                                const gchar *icon_name)
{
  HdLauncherTilePrivate *priv = HD_LAUNCHER_TILE_GET_PRIVATE (tile);
  guint size = 64;
  GtkIconTheme *icon_theme;
  GtkIconInfo *info;

  if (priv->icon_name)
    {
      g_free (priv->icon_name);
    }
  if (icon_name)
    priv->icon_name = g_strdup (icon_name);
  else
    /* Set the default if none was passed. */
    priv->icon_name = g_strdup (HD_LAUNCHER_DEFAULT_ICON);

  /* Recreate the icon actor */
  if (priv->icon)
    {
      clutter_actor_destroy (priv->icon);
      priv->icon = NULL;
    }

  icon_theme = gtk_icon_theme_get_default();
  info = gtk_icon_theme_lookup_icon(icon_theme, priv->icon_name, size,
                                    GTK_ICON_LOOKUP_NO_SVG);
  if (info == NULL)
    {
      /* Try to get the default icon. */
      g_free (priv->icon_name);
      priv->icon_name = g_strdup (HD_LAUNCHER_DEFAULT_ICON);
      info = gtk_icon_theme_lookup_icon(icon_theme, priv->icon_name, size,
                                        GTK_ICON_LOOKUP_NO_SVG);
    }
  if (info == NULL)
    {
      g_warning ("%s: couldn't find icon %s\n", __FUNCTION__, priv->icon_name);
      return;
    }

  const gchar *fname = gtk_icon_info_get_filename(info);
  priv->icon = clutter_texture_new_from_file(fname, NULL);
  clutter_actor_set_size (priv->icon, size, size);
  clutter_actor_set_parent (priv->icon, CLUTTER_ACTOR (tile));

  gtk_icon_info_free(info);
}

void
hd_launcher_tile_set_text (HdLauncherTile *tile,
                           const gchar *text)
{
  ClutterColor text_color;
  gchar *font_string;
  HdLauncherTilePrivate *priv = HD_LAUNCHER_TILE_GET_PRIVATE (tile);

  if (!text)
    return;

  if (priv->text)
    {
      g_free (priv->text);
    }
  priv->text = g_strdup (text);

  /* Recreate the label actor */
  if (priv->label)
    {
      clutter_actor_destroy (priv->label);
    }

  hd_gtk_style_get_text_color (HD_GTK_BUTTON_SINGLETON,
                               GTK_STATE_NORMAL,
                               &text_color);
  font_string = hd_gtk_style_get_font_string (HD_GTK_BUTTON_SINGLETON);
  priv->label = clutter_label_new_full (font_string, priv->text, &text_color);

  /* FIXME: This is a huge work-around because clutter/pango do not
   * support setting ellipsize to NONE and wrap to FALSE.
   */
  clutter_label_set_line_wrap (CLUTTER_LABEL (priv->label), TRUE);
  clutter_label_set_ellipsize (CLUTTER_LABEL (priv->label),
                               PANGO_ELLIPSIZE_NONE);
  clutter_label_set_alignment (CLUTTER_LABEL (priv->label),
                               PANGO_ALIGN_CENTER);
  clutter_label_set_line_wrap_mode (CLUTTER_LABEL (priv->label),
                                    PANGO_WRAP_CHAR);
  clutter_actor_set_parent (priv->label, CLUTTER_ACTOR (tile));
  clutter_actor_set_clip (priv->label, 0, 0,
              HD_LAUNCHER_TILE_WIDTH,
              HD_LAUNCHER_TILE_HEIGHT - (64 + (HILDON_MARGIN_HALF * 2)));

  g_free (font_string);
}

static void
hd_launcher_tile_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_LAUNCHER_TILE_ICON_NAME:
      hd_launcher_tile_set_icon_name (HD_LAUNCHER_TILE (gobject),
                                      g_value_get_string (value));
      break;

    case PROP_LAUNCHER_TILE_TEXT:
      hd_launcher_tile_set_text (HD_LAUNCHER_TILE (gobject),
                                 g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
hd_launcher_tile_get_preferred_width (ClutterActor *actor,
                                      ClutterUnit   for_height,
                                      ClutterUnit  *min_width_p,
                                      ClutterUnit  *natural_width_p)
{
  if (min_width_p)
    *min_width_p = CLUTTER_UNITS_FROM_DEVICE (HD_LAUNCHER_TILE_WIDTH);

  if (natural_width_p)
    *natural_width_p = CLUTTER_UNITS_FROM_DEVICE (HD_LAUNCHER_TILE_WIDTH);
}

static void
hd_launcher_tile_get_preferred_height (ClutterActor *actor,
                                       ClutterUnit   for_width,
                                       ClutterUnit  *min_height_p,
                                       ClutterUnit  *natural_height_p)
{
  if (min_height_p)
    *min_height_p = CLUTTER_UNITS_FROM_DEVICE (HD_LAUNCHER_TILE_HEIGHT);

  if (natural_height_p)
    *natural_height_p = CLUTTER_UNITS_FROM_DEVICE (HD_LAUNCHER_TILE_HEIGHT);
}

static void
hd_launcher_tile_allocate (ClutterActor          *actor,
                           const ClutterActorBox *box,
                           gboolean               origin_changed)
{
  HdLauncherTilePrivate *priv;
  ClutterActor *icon, *label;
  ClutterActorClass *parent_class;

  /* chain up to get the allocation stored */
  parent_class = CLUTTER_ACTOR_CLASS (hd_launcher_tile_parent_class);
  parent_class->allocate (actor, box, origin_changed);

  priv = HD_LAUNCHER_TILE_GET_PRIVATE (actor);
  icon  = priv->icon;
  label = priv->label;

  if (icon)
  {
    guint x1, y1;
    ClutterActorBox icon_box;

    x1 = ((HD_LAUNCHER_TILE_WIDTH - 64) / 2);
    y1 = HILDON_MARGIN_HALF;
    icon_box.x1 = CLUTTER_UNITS_FROM_DEVICE (x1);
    icon_box.y1 = CLUTTER_UNITS_FROM_DEVICE (y1);
    icon_box.x2 = CLUTTER_UNITS_FROM_DEVICE (x1 + 64);
    icon_box.y2 = CLUTTER_UNITS_FROM_DEVICE (y1 + 64);

    clutter_actor_allocate (icon, &icon_box, origin_changed);
  }

  if (label)
  {
    ClutterActorBox label_box;
    ClutterUnit label_width;
    guint label_height, label_width_px;

    label_height = HD_LAUNCHER_TILE_HEIGHT - (64 + (HILDON_MARGIN_HALF * 2));

    clutter_actor_get_preferred_width (label,
      CLUTTER_UNITS_FROM_DEVICE(label_height),
                                NULL, &label_width);
    label_width_px = MIN (CLUTTER_UNITS_TO_DEVICE(label_width),
                          HD_LAUNCHER_TILE_WIDTH);

    label_box.x1 = CLUTTER_UNITS_FROM_DEVICE ((HD_LAUNCHER_TILE_WIDTH - label_width_px) / 2);
    label_box.y1 = CLUTTER_UNITS_FROM_DEVICE (HD_LAUNCHER_TILE_HEIGHT - label_height);
    label_box.x2 = CLUTTER_UNITS_FROM_DEVICE (((HD_LAUNCHER_TILE_WIDTH - label_width_px) / 2) + label_width_px);
    label_box.y2 = CLUTTER_UNITS_FROM_DEVICE (HD_LAUNCHER_TILE_HEIGHT);

    clutter_actor_allocate (label, &label_box, origin_changed);
  }
}

static void
hd_launcher_tile_paint (ClutterActor *actor)
{
  HdLauncherTilePrivate *priv = HD_LAUNCHER_TILE_GET_PRIVATE (actor);

  if (priv->icon && CLUTTER_ACTOR_IS_VISIBLE (priv->icon))
    clutter_actor_paint (priv->icon);

  if (priv->label && CLUTTER_ACTOR_IS_VISIBLE (priv->label))
    clutter_actor_paint (priv->label);
}

static void
hd_launcher_tile_pick (ClutterActor       *actor,
                       const ClutterColor *pick_color)
{
  HdLauncherTilePrivate *priv = HD_LAUNCHER_TILE_GET_PRIVATE (actor);

  CLUTTER_ACTOR_CLASS (hd_launcher_tile_parent_class)->pick (actor, pick_color);

  if (priv->label && CLUTTER_ACTOR_IS_VISIBLE (priv->label))
    clutter_actor_paint (priv->label);

  if (priv->icon && CLUTTER_ACTOR_IS_VISIBLE (priv->icon))
    clutter_actor_paint (priv->icon);
}

static void
hd_launcher_tile_show (ClutterActor *actor)
{
  HdLauncherTilePrivate *priv = HD_LAUNCHER_TILE_GET_PRIVATE (actor);

  if (priv->icon)
    clutter_actor_show (priv->icon);

  if (priv->label)
    clutter_actor_show (priv->label);

  CLUTTER_ACTOR_SET_FLAGS (actor, CLUTTER_ACTOR_MAPPED);
}

static void
hd_launcher_tile_hide (ClutterActor *actor)
{
  HdLauncherTilePrivate *priv = HD_LAUNCHER_TILE_GET_PRIVATE (actor);

  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_MAPPED);

  if (priv->icon)
    clutter_actor_hide (priv->icon);

  if (priv->label)
    clutter_actor_hide (priv->label);
}

static gboolean
hd_launcher_tile_button_press (ClutterActor       *actor,
                               ClutterButtonEvent *event)
{
  if (event->button == 1)
    {
      HdLauncherTilePrivate *priv = HD_LAUNCHER_TILE_GET_PRIVATE (actor);

      priv->is_pressed = TRUE;
      priv->x_press_pos = event->x;
      priv->y_press_pos = event->y;

      return TRUE;
    }

  return FALSE;
}

static gboolean
hd_launcher_tile_button_release (ClutterActor       *actor,
                                 ClutterButtonEvent *event)
{
  if (event->button == 1)
    {
      HdLauncherTilePrivate *priv = HD_LAUNCHER_TILE_GET_PRIVATE (actor);

      if (!(priv->is_pressed &&
           (abs(priv->x_press_pos - event->x) < 20) &&
           (abs(priv->y_press_pos - event->y) < 20)))
        return FALSE;

      priv->is_pressed = FALSE;

      g_signal_emit (actor, launcher_tile_signals[CLICKED], 0);

      return TRUE;
    }

  return FALSE;
}

static void
hd_launcher_tile_finalize (GObject *gobject)
{
  HdLauncherTilePrivate *priv = HD_LAUNCHER_TILE_GET_PRIVATE (gobject);

  clutter_actor_destroy (priv->label);
  clutter_actor_destroy (priv->icon);
  g_free (priv->icon_name);
  g_free (priv->text);

  G_OBJECT_CLASS (hd_launcher_tile_parent_class)->finalize (gobject);
}
