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

#include "hd-launcher-page.h"

#include <glib-object.h>
#include <clutter/clutter.h>
#include <clutter/clutter-timeline.h>
#include <tidy/tidy-finger-scroll.h>
#include <tidy/tidy-scroll-view.h>
#include <tidy/tidy-scroll-bar.h>
#include <tidy/tidy-adjustment.h>
#include <hildon/hildon-defines.h>
#include <math.h>

#include "hd-transition.h"
#include "hildon-desktop.h"
#include "hd-launcher.h"
#include "hd-launcher-grid.h"
#include "hd-gtk-utils.h"
#include "hd-gtk-style.h"
#include "hd-comp-mgr.h"


#define I_(str) (g_intern_static_string ((str)))
#define HD_PARAM_READWRITE (G_PARAM_READWRITE | \
                            G_PARAM_STATIC_NICK | \
                            G_PARAM_STATIC_NAME | \
                            G_PARAM_STATIC_BLURB)

#define HD_LAUNCHER_PAGE_SUB_OPACITY (0.33f)

#define HD_LAUNCHER_PAGE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HD_TYPE_LAUNCHER_PAGE, HdLauncherPagePrivate))

struct _HdLauncherPagePrivate
{
  gchar *icon_name;
  gchar *text;

  ClutterActor *icon;
  ClutterActor *label;
  ClutterActor *scroller;
  ClutterActor *grid;
  ClutterActor *empty_label;

  HdLauncherPageTransition transition_type;
  ClutterTimeline *transition;
  /* Timeline works by signals, so we get horrible flicker if we ask it if it
   * is playing right after saying _start() - so we have a boolean to figure
   * out for ourselves */
  gboolean         transition_playing;
  /* When the user clicks and drags more than a certain amount, we want
   * to deselect what they had clicked on - so we must keep track of
   * movement */
  gint drag_distance;
  gint drag_last_x, drag_last_y;
};

enum
{
  PROP_0,

  PROP_LAUNCHER_PAGE_ICON_NAME,
  PROP_LAUNCHER_PAGE_TEXT
};

enum
{
  BACK_BUTTON_PRESSED,
  TILE_CLICKED,

  LAST_SIGNAL
};

static guint launcher_page_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (HdLauncherPage, hd_launcher_page, CLUTTER_TYPE_ACTOR);

/* Forward declarations. */
/* GObject methods */
static void hd_launcher_page_constructed (GObject *object);
static void hd_launcher_page_finalize (GObject *gobject);
static void hd_launcher_page_get_property (GObject    *object,
                                           guint       property_id,
                                           GValue     *value,
                                           GParamSpec *pspec);
static void hd_launcher_page_set_property (GObject      *gobject,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);
/* ClutterActor methods */
static void hd_launcher_page_get_preferred_width (ClutterActor *actor,
                                                   ClutterUnit   for_height,
                                                   ClutterUnit  *min_width_p,
                                                   ClutterUnit *natural_width_p);
static void hd_launcher_page_get_preferred_height (ClutterActor *actor,
                                                    ClutterUnit   for_width,
                                                    ClutterUnit  *min_height_p,
                                                    ClutterUnit  *natural_height_p);
static void hd_launcher_page_allocate (ClutterActor          *actor,
                                        const ClutterActorBox *box,
                                        gboolean               origin_changed);
static void hd_launcher_page_paint (ClutterActor *actor);
static void hd_launcher_page_pick (ClutterActor       *actor,
                                    const ClutterColor *pick_color);
static void hd_launcher_page_show (ClutterActor *actor);
static void hd_launcher_page_hide (ClutterActor *actor);

static void hd_launcher_page_tile_clicked (HdLauncherTile *tile,
                                           gpointer data);
static void hd_launcher_page_new_frame(ClutterTimeline *timeline,
                                       gint frame_num, gpointer data);
static void hd_launcher_page_transition_end(ClutterTimeline *timeline,
                                            gpointer data);

static void
hd_launcher_page_class_init (HdLauncherPageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (HdLauncherPagePrivate));

  gobject_class->constructed  = hd_launcher_page_constructed;
  gobject_class->get_property = hd_launcher_page_get_property;
  gobject_class->set_property = hd_launcher_page_set_property;
  gobject_class->finalize     = hd_launcher_page_finalize;

  actor_class->get_preferred_width  = hd_launcher_page_get_preferred_width;
  actor_class->get_preferred_height = hd_launcher_page_get_preferred_height;
  actor_class->allocate             = hd_launcher_page_allocate;
  actor_class->paint                = hd_launcher_page_paint;
  actor_class->pick                 = hd_launcher_page_pick;
  actor_class->show                 = hd_launcher_page_show;
  actor_class->hide                 = hd_launcher_page_hide;

  pspec = g_param_spec_string ("icon-name",
                               "Icon Name",
                               "Name of the category icon to display",
                               HD_LAUNCHER_DEFAULT_ICON,
                               G_PARAM_CONSTRUCT | HD_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_LAUNCHER_PAGE_ICON_NAME, pspec);
  pspec = g_param_spec_string ("text",
                               "Text",
                               "Text to display",
                               "Unknown",
                               G_PARAM_CONSTRUCT | HD_PARAM_READWRITE);
  g_object_class_install_property (gobject_class, PROP_LAUNCHER_PAGE_TEXT, pspec);

  launcher_page_signals[BACK_BUTTON_PRESSED] =
    g_signal_new (I_("back-button-pressed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  launcher_page_signals[TILE_CLICKED] =
    g_signal_new (I_("tile-clicked"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  HD_TYPE_LAUNCHER_TILE);
}

static void
hd_launcher_page_init (HdLauncherPage *page)
{
  page->priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  clutter_actor_set_reactive (CLUTTER_ACTOR (page), FALSE);
}

static gboolean
captured_event_cb (TidyFingerScroll *scroll,
                 ClutterEvent *event,
                 HdLauncherPage *page)
{
  HdLauncherPagePrivate *priv;

  if (!HD_IS_LAUNCHER_PAGE(page))
    return FALSE;
  priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  if (event->type == CLUTTER_BUTTON_PRESS)
    {
      ClutterButtonEvent *bevent = (ClutterButtonEvent *)event;
      priv->drag_distance = 0.0f;
      priv->drag_last_x = bevent->x;
      priv->drag_last_y = bevent->y;
    }

  return FALSE;
}

static gboolean
motion_event_cb (TidyFingerScroll *scroll,
                 ClutterMotionEvent *event,
                 HdLauncherPage *page)
{
  HdLauncherPagePrivate *priv;
  gint dx, dy;

  if (!HD_IS_LAUNCHER_PAGE(page))
    return FALSE;

  priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  dx = priv->drag_last_x - event->x;
  dy = priv->drag_last_y - event->y;
  priv->drag_last_x = event->x;
  priv->drag_last_y = event->y;
  priv->drag_distance += (int)sqrt(dx*dx + dy*dy);

  /* If we dragged too far, deselect (and de-glow) */
  if (priv->drag_distance > HD_LAUNCHER_TILE_MAX_DRAG)
    {
      hd_launcher_grid_reset(HD_LAUNCHER_GRID(priv->grid));
    }

  return FALSE;
}

static void
hd_launcher_page_constructed (GObject *object)
{
  ClutterColor text_color;
  gchar *font_string;
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (object);

  hd_gtk_style_get_text_color (HD_GTK_BUTTON_SINGLETON,
                               GTK_STATE_NORMAL,
                               &text_color);
  font_string = hd_gtk_style_get_font_string (HD_GTK_BUTTON_SINGLETON);
  priv->empty_label = clutter_label_new_full(font_string,
                                             _("tana_li_of_noapps"),
                                             &text_color);
  clutter_label_set_line_wrap (CLUTTER_LABEL (priv->empty_label), TRUE);
  clutter_label_set_ellipsize (CLUTTER_LABEL (priv->empty_label),
                               PANGO_ELLIPSIZE_NONE);
  clutter_label_set_alignment (CLUTTER_LABEL (priv->empty_label),
                               PANGO_ALIGN_CENTER);
  clutter_label_set_line_wrap_mode (CLUTTER_LABEL (priv->empty_label),
                                    PANGO_WRAP_WORD);
  clutter_actor_set_parent (priv->empty_label, CLUTTER_ACTOR (object));
  g_free (font_string);

  priv->scroller = tidy_finger_scroll_new (TIDY_FINGER_SCROLL_MODE_KINETIC);
  clutter_actor_set_parent (priv->scroller, CLUTTER_ACTOR (object));

  priv->grid = g_object_ref (hd_launcher_grid_new ());
  clutter_container_add_actor (CLUTTER_CONTAINER (priv->scroller),
                               priv->grid);
  priv->transition = g_object_ref (clutter_timeline_new_for_duration (1000));
  g_signal_connect (priv->transition, "new-frame",
                    G_CALLBACK (hd_launcher_page_new_frame), object);
  g_signal_connect (priv->transition, "completed",
                    G_CALLBACK (hd_launcher_page_transition_end), object);
  priv->transition_playing = FALSE;

  /* Add callbacks for de-selecting an icon after the user has moved
   * their finger more than a certain amount */
  g_signal_connect (priv->scroller,
                    "captured-event",
                    G_CALLBACK (captured_event_cb),
                    object);
  g_signal_connect (priv->scroller,
                    "motion-event",
                    G_CALLBACK (motion_event_cb),
                    object);
}

ClutterActor *
hd_launcher_page_new (const gchar *icon_name, const gchar *text)
{
  return g_object_new (HD_TYPE_LAUNCHER_PAGE,
                       "icon-name", icon_name,
                       "text", text,
                       NULL);
}

const gchar *
hd_launcher_page_get_icon_name (HdLauncherPage *page)
{
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  return priv->icon_name;
}

const gchar *
hd_launcher_page_get_text (HdLauncherPage *page)
{
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  return priv->text;
}

static void
hd_launcher_page_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  HdLauncherPage *page = HD_LAUNCHER_PAGE (object);

  switch (property_id)
    {
    case PROP_LAUNCHER_PAGE_ICON_NAME:
      g_value_set_string (value,
          hd_launcher_page_get_icon_name (page));
      break;

    case PROP_LAUNCHER_PAGE_TEXT:
      g_value_set_string (value,
          hd_launcher_page_get_text (page));
      break;

    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

void
hd_launcher_page_set_icon_name (HdLauncherPage *page,
                                 const gchar *icon_name)
{
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);
  guint size = 64;
  GtkIconTheme *icon_theme;
  GtkIconInfo *info;

  if (priv->icon_name)
    {
      g_free (priv->icon_name);
      priv->icon_name = NULL;
    }
  if (priv->icon)
    {
      clutter_actor_destroy (priv->icon);
      priv->icon = NULL;
    }

  if (!icon_name)
    return;

  priv->icon_name = g_strdup (icon_name);

  /* Recreate the icon actor */
  icon_theme = gtk_icon_theme_get_default();
  info = gtk_icon_theme_lookup_icon(icon_theme, priv->icon_name, size,
                                    GTK_ICON_LOOKUP_NO_SVG);
  if (info != NULL)
    {
      const gchar *fname = gtk_icon_info_get_filename(info);
      priv->icon = clutter_texture_new_from_file(fname, NULL);
      clutter_actor_set_size (priv->icon, size, size);
      clutter_actor_set_parent (priv->icon, CLUTTER_ACTOR (page));
      clutter_actor_set_reactive (priv->icon, TRUE);

      gtk_icon_info_free(info);
    }
  else
    g_warning ("%s: couldn't find icon %s\n", __FUNCTION__, priv->icon_name);
}

void
hd_launcher_page_set_text (HdLauncherPage *page,
                            const gchar *text)
{
  ClutterColor text_color;
  gchar *font_string;
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  if (priv->text)
    {
      g_free (priv->text);
      priv->text = NULL;
    }
  if (priv->label)
    {
      clutter_actor_destroy (priv->label);
      priv->label = NULL;
    }

  if (!text)
    return;

  priv->text = g_strdup (text);

  /* Recreate the label actor */
  hd_gtk_style_get_text_color (HD_GTK_BUTTON_SINGLETON,
                               GTK_STATE_NORMAL,
                               &text_color);
  font_string = hd_gtk_style_get_font_string (HD_GTK_BUTTON_SINGLETON);
  priv->label = clutter_label_new_full (font_string, priv->text, &text_color);
  clutter_label_set_line_wrap (CLUTTER_LABEL (priv->label), FALSE);
  clutter_label_set_ellipsize (CLUTTER_LABEL (priv->label), PANGO_ELLIPSIZE_END);
  clutter_label_set_alignment (CLUTTER_LABEL (priv->label), PANGO_ALIGN_CENTER);
  clutter_actor_set_parent (priv->label, CLUTTER_ACTOR (page));
  clutter_actor_set_reactive (priv->label, TRUE);

  g_free (font_string);
}

static void
hd_launcher_page_set_property (GObject      *gobject,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_LAUNCHER_PAGE_ICON_NAME:
      hd_launcher_page_set_icon_name (HD_LAUNCHER_PAGE (gobject),
                                      g_value_get_string (value));
      break;

    case PROP_LAUNCHER_PAGE_TEXT:
      hd_launcher_page_set_text (HD_LAUNCHER_PAGE (gobject),
                                 g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
hd_launcher_page_finalize (GObject *gobject)
{
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (gobject);

  g_object_ref(priv->transition);
  g_free (priv->icon_name);
  g_free (priv->text);
  clutter_actor_destroy (priv->label);
  clutter_actor_destroy (priv->icon);
  g_object_unref(priv->grid);
  clutter_actor_destroy (priv->scroller);

  G_OBJECT_CLASS (hd_launcher_page_parent_class)->finalize (gobject);
}

static void
hd_launcher_page_get_preferred_width (ClutterActor *actor,
                                       ClutterUnit   for_height,
                                       ClutterUnit  *min_width_p,
                                       ClutterUnit  *natural_width_p)
{
  if (min_width_p)
    *min_width_p = CLUTTER_UNITS_FROM_DEVICE (HD_LAUNCHER_PAGE_WIDTH);

  if (natural_width_p)
    *natural_width_p = CLUTTER_UNITS_FROM_DEVICE (HD_LAUNCHER_PAGE_WIDTH);
}

static void
hd_launcher_page_get_preferred_height (ClutterActor *actor,
                                        ClutterUnit   for_width,
                                        ClutterUnit  *min_height_p,
                                        ClutterUnit  *natural_height_p)
{
  if (min_height_p)
    *min_height_p = CLUTTER_UNITS_FROM_DEVICE (HD_LAUNCHER_PAGE_HEIGHT);

  if (natural_height_p)
    *natural_height_p = CLUTTER_UNITS_FROM_DEVICE (HD_LAUNCHER_PAGE_HEIGHT);
}

static void
hd_launcher_page_allocate (ClutterActor          *actor,
                            const ClutterActorBox *box,
                            gboolean               origin_changed)
{
  HdLauncherPagePrivate *priv;
  ClutterActorBox nbox;
  ClutterActorClass *parent_class;

  /* chain up to get the allocation stored */
  parent_class = CLUTTER_ACTOR_CLASS (hd_launcher_page_parent_class);
  parent_class->allocate (actor, box, origin_changed);

  priv = HD_LAUNCHER_PAGE_GET_PRIVATE (actor);

  if (priv->icon)
    {
      nbox.x1 = CLUTTER_UNITS_FROM_DEVICE (HILDON_MARGIN_DEFAULT);
      nbox.y1 = CLUTTER_UNITS_FROM_DEVICE (HILDON_MARGIN_DEFAULT);
      nbox.x2 = CLUTTER_UNITS_FROM_DEVICE (HILDON_MARGIN_DEFAULT + 64);
      nbox.y2 = CLUTTER_UNITS_FROM_DEVICE (HILDON_MARGIN_DEFAULT + 64);

      clutter_actor_allocate (priv->icon, &nbox, origin_changed);
    }

  if (priv->label)
  {
    guint x1, y1;
    ClutterUnit label_width, label_height;
    clutter_actor_get_preferred_size (priv->label, NULL, NULL,
                                      &label_width, &label_height);
    x1 = (HILDON_MARGIN_DEFAULT*2) + 64;
    y1 = HILDON_MARGIN_DEFAULT +
         ((64 - CLUTTER_UNITS_TO_DEVICE (label_height)) / 2);
    nbox.x1 = CLUTTER_UNITS_FROM_DEVICE (x1);
    nbox.y1 = CLUTTER_UNITS_FROM_DEVICE (y1);
    nbox.x2 = CLUTTER_UNITS_FROM_DEVICE (x1) + label_width;
    nbox.y2 = CLUTTER_UNITS_FROM_DEVICE (y1) + label_height;

    clutter_actor_allocate (priv->label, &nbox, origin_changed);
  }

  if (priv->empty_label)
    {
      guint x1, y1;
      ClutterUnit label_width, label_height;
      clutter_actor_get_preferred_size(priv->empty_label,
          NULL, NULL,
          &label_width, &label_height);

      x1 = (HD_LAUNCHER_PAGE_WIDTH - CLUTTER_UNITS_TO_DEVICE (label_width)) / 2;
      y1 = ((HD_LAUNCHER_PAGE_HEIGHT - HD_LAUNCHER_PAGE_YMARGIN -
              CLUTTER_UNITS_TO_DEVICE (label_height))/2) +
            HD_LAUNCHER_PAGE_YMARGIN;
      nbox.x1 = CLUTTER_UNITS_FROM_DEVICE (x1);
      nbox.y1 = CLUTTER_UNITS_FROM_DEVICE (y1);
      nbox.x2 = CLUTTER_UNITS_FROM_DEVICE (x1) + label_width;
      nbox.y2 = CLUTTER_UNITS_FROM_DEVICE (y1) + label_height;
      clutter_actor_allocate (priv->empty_label, &nbox, origin_changed);
    }
  else
    {
      /* The scroller */
      nbox.x1 = CLUTTER_UNITS_FROM_DEVICE (0);
      nbox.y1 = CLUTTER_UNITS_FROM_DEVICE (HD_LAUNCHER_PAGE_YMARGIN);
      nbox.x2 = CLUTTER_UNITS_FROM_DEVICE (HD_LAUNCHER_PAGE_WIDTH);
      nbox.y2 = CLUTTER_UNITS_FROM_DEVICE (HD_LAUNCHER_PAGE_HEIGHT);
      clutter_actor_allocate (priv->scroller, &nbox, origin_changed);
    }
}

static void
hd_launcher_page_paint (ClutterActor *actor)
{
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (actor);

  if (!CLUTTER_ACTOR_IS_VISIBLE (actor))
    return;

  if (priv->icon && CLUTTER_ACTOR_IS_VISIBLE (priv->icon))
    clutter_actor_paint (priv->icon);

  if (priv->label && CLUTTER_ACTOR_IS_VISIBLE (priv->label))
    clutter_actor_paint (priv->label);

  if (priv->empty_label && CLUTTER_ACTOR_IS_VISIBLE (priv->empty_label))
    clutter_actor_paint (priv->empty_label);
  else if (priv->scroller && CLUTTER_ACTOR_IS_VISIBLE (priv->scroller))
    clutter_actor_paint (priv->scroller);
}

static void
hd_launcher_page_pick (ClutterActor       *actor,
                       const ClutterColor *pick_color)
{
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (actor);

  CLUTTER_ACTOR_CLASS (hd_launcher_page_parent_class)->pick (actor, pick_color);

  if (priv->label && CLUTTER_ACTOR_IS_VISIBLE (priv->label))
    clutter_actor_paint (priv->label);

  if (priv->icon && CLUTTER_ACTOR_IS_VISIBLE (priv->icon))
    clutter_actor_paint (priv->icon);

  if (priv->empty_label && CLUTTER_ACTOR_IS_VISIBLE (priv->empty_label))
      clutter_actor_paint (priv->empty_label);
  else if (priv->scroller && CLUTTER_ACTOR_IS_VISIBLE (priv->scroller))
    clutter_actor_paint (priv->scroller);
}

static void
hd_launcher_page_show (ClutterActor *actor)
{
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (actor);

  if (priv->icon)
    clutter_actor_show (priv->icon);

  if (priv->label)
    clutter_actor_show (priv->label);

  if (priv->grid)
    hd_launcher_grid_reset_v_adjustment (HD_LAUNCHER_GRID (priv->grid));

  if (priv->empty_label)
    clutter_actor_show (priv->empty_label);
  else if (priv->scroller)
    clutter_actor_show (priv->scroller);

  CLUTTER_ACTOR_SET_FLAGS (actor, CLUTTER_ACTOR_MAPPED);
}

static void
hd_launcher_page_hide (ClutterActor *actor)
{
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (actor);

  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_MAPPED);

  if (priv->icon)
    clutter_actor_hide (priv->icon);

  if (priv->label)
    clutter_actor_hide (priv->label);

  if (priv->empty_label)
      clutter_actor_hide (priv->empty_label);
  else if (priv->scroller)
    clutter_actor_hide (priv->scroller);
}

ClutterActor *
hd_launcher_page_get_grid (HdLauncherPage *page)
{
  return (HD_LAUNCHER_PAGE_GET_PRIVATE (page))->grid;
}

void
hd_launcher_page_add_tile (HdLauncherPage *page, HdLauncherTile* tile)
{
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);
  g_return_if_fail(HD_IS_LAUNCHER_PAGE(page));

  if (priv->empty_label)
    {
      clutter_actor_destroy (priv->empty_label);
      priv->empty_label = NULL;
    }

  clutter_container_add_actor (CLUTTER_CONTAINER (priv->grid),
                               CLUTTER_ACTOR (tile));
  clutter_actor_queue_relayout (CLUTTER_ACTOR (page));

  g_signal_connect (tile, "clicked",
                    G_CALLBACK (hd_launcher_page_tile_clicked),
                    page);
}

static void
hd_launcher_page_tile_clicked (HdLauncherTile *tile, gpointer data)
{
  g_signal_emit (HD_LAUNCHER_PAGE(data),
                 launcher_page_signals[TILE_CLICKED],
                 0, tile);
}

static const char *
hd_launcher_page_get_transition_string(
    HdLauncherPageTransition trans_type)
{
  switch (trans_type)
  {
    case HD_LAUNCHER_PAGE_TRANSITION_IN :
      return "launcher_in";
    case HD_LAUNCHER_PAGE_TRANSITION_OUT :
      return "launcher_out";
    case HD_LAUNCHER_PAGE_TRANSITION_OUT_BACK :
      return "launcher_out_back";
    case HD_LAUNCHER_PAGE_TRANSITION_LAUNCH :
      return "launcher_launch";
    case HD_LAUNCHER_PAGE_TRANSITION_IN_SUB :
      return "launcher_in_sub";
    case HD_LAUNCHER_PAGE_TRANSITION_OUT_SUB :
      return "launcher_out_sub";
    case HD_LAUNCHER_PAGE_TRANSITION_BACK :
      return "launcher_back";
    case HD_LAUNCHER_PAGE_TRANSITION_FORWARD :
      return "launcher_forward";
  }
  return "unknown";
}

void hd_launcher_page_transition(HdLauncherPage *page, HdLauncherPageTransition trans_type)
{
  HdLauncherPagePrivate *priv;

  if (!HD_IS_LAUNCHER_PAGE(page))
    return;

  priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  /* check for the case where we're hiding when already hidden */
  if (!CLUTTER_ACTOR_IS_VISIBLE(page) &&
      trans_type == HD_LAUNCHER_PAGE_TRANSITION_OUT)
    return;
  /* check for the case where we're launching and then hiding, and just use
   * the launching animation */
  if (clutter_timeline_is_playing(priv->transition) &&
      priv->transition_type == HD_LAUNCHER_PAGE_TRANSITION_LAUNCH &&
      trans_type == HD_LAUNCHER_PAGE_TRANSITION_OUT)
    return;
  /* Reset all the tiles in the grid, so they don't have any blurring */
  hd_launcher_grid_reset(HD_LAUNCHER_GRID(priv->grid));
  hd_launcher_load_blur_amounts();

  priv->transition_type = trans_type;
  switch (priv->transition_type) {
    case HD_LAUNCHER_PAGE_TRANSITION_IN:
         clutter_actor_show(CLUTTER_ACTOR(page));
         hd_launcher_set_top_blur(0, 1);
         break;
    case HD_LAUNCHER_PAGE_TRANSITION_IN_SUB:
         clutter_actor_show(CLUTTER_ACTOR(page));
         break;
    case HD_LAUNCHER_PAGE_TRANSITION_OUT:
    case HD_LAUNCHER_PAGE_TRANSITION_OUT_SUB:
         /* already shown */
         break;
    case HD_LAUNCHER_PAGE_TRANSITION_LAUNCH:
         /* already shown */
         break;
    case HD_LAUNCHER_PAGE_TRANSITION_BACK:
    case HD_LAUNCHER_PAGE_TRANSITION_FORWARD:
    case HD_LAUNCHER_PAGE_TRANSITION_OUT_BACK:
         /* already shown */
         break;
  }

  clutter_timeline_pause(priv->transition);
  clutter_timeline_rewind(priv->transition);

  clutter_timeline_set_duration(priv->transition,
      hd_transition_get_int(
          hd_launcher_page_get_transition_string(priv->transition_type),
          "duration",
          500 /* default value */));

  clutter_timeline_start(priv->transition);
  priv->transition_playing = TRUE;

  /* force a call to lay stuff out before it gets drawn properly */
  hd_launcher_page_new_frame(priv->transition, 0, page);
}

void hd_launcher_page_transition_stop(HdLauncherPage *page)
{
  HdLauncherPagePrivate *priv;

  if (!HD_IS_LAUNCHER_PAGE(page))
    return;

  priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  if (priv->transition_playing)
    {
      gint frames;
      /* force a call to lay stuff out as if the transition has ended */
      frames = clutter_timeline_get_n_frames(priv->transition);
      hd_launcher_page_new_frame(priv->transition, frames, page);
      hd_launcher_page_transition_end(priv->transition, page);
    }
}

static void
hd_launcher_page_new_frame(ClutterTimeline *timeline,
                          gint frame_num, gpointer data)
{
  HdLauncherPage *page = HD_LAUNCHER_PAGE(data);
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);
  gint frames;
  float amt;

  if (!HD_IS_LAUNCHER_PAGE(data))
    return;

  frames = clutter_timeline_get_n_frames(timeline);
  amt = frame_num / (float)frames;

  hd_launcher_grid_transition(HD_LAUNCHER_GRID(priv->grid),
                              page,
                              priv->transition_type,
                              amt);

    switch (priv->transition_type)
      {
    case HD_LAUNCHER_PAGE_TRANSITION_IN:
    case HD_LAUNCHER_PAGE_TRANSITION_IN_SUB:
        if (priv->icon)
          clutter_actor_set_opacity(priv->icon, (int)(255*amt));
        if (priv->label)
          clutter_actor_set_opacity(priv->label, (int)(255*amt));
        break;
    case HD_LAUNCHER_PAGE_TRANSITION_OUT:
    case HD_LAUNCHER_PAGE_TRANSITION_LAUNCH:
    case HD_LAUNCHER_PAGE_TRANSITION_OUT_SUB:
        if (priv->icon)
          clutter_actor_set_opacity(priv->icon, 255-(int)(255*amt));
        if (priv->label)
          clutter_actor_set_opacity(priv->label, 255-(int)(255*amt));
        break;
    case HD_LAUNCHER_PAGE_TRANSITION_BACK:
        hd_launcher_set_top_blur(amt,
                    1-(HD_LAUNCHER_PAGE_SUB_OPACITY * amt));
        break;
    case HD_LAUNCHER_PAGE_TRANSITION_FORWARD:
        hd_launcher_set_top_blur(1-amt,
                    1-(HD_LAUNCHER_PAGE_SUB_OPACITY * (1-amt)));
        break;
    case HD_LAUNCHER_PAGE_TRANSITION_OUT_BACK:
        hd_launcher_set_top_blur(1, 1-amt);
        break;
      }
}

static void
hd_launcher_page_transition_end(ClutterTimeline *timeline,
                                gpointer data)
{
  HdLauncherPage *page = HD_LAUNCHER_PAGE(data);
  HdLauncherPagePrivate *priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  if (!HD_IS_LAUNCHER_PAGE(data))
    return;

  hd_launcher_grid_transition(HD_LAUNCHER_GRID(priv->grid),
                              page,
                              priv->transition_type,
                              1.0f);

  switch (priv->transition_type) {
    case HD_LAUNCHER_PAGE_TRANSITION_IN:
    case HD_LAUNCHER_PAGE_TRANSITION_IN_SUB:
    case HD_LAUNCHER_PAGE_TRANSITION_FORWARD:
    case HD_LAUNCHER_PAGE_TRANSITION_BACK:
    case HD_LAUNCHER_PAGE_TRANSITION_OUT_BACK:
         /* already shown */
         break;
    case HD_LAUNCHER_PAGE_TRANSITION_OUT:
    case HD_LAUNCHER_PAGE_TRANSITION_LAUNCH:
         clutter_actor_hide(CLUTTER_ACTOR(page));
         hd_launcher_hide_final();
         break;
    case HD_LAUNCHER_PAGE_TRANSITION_OUT_SUB:
         clutter_actor_hide(CLUTTER_ACTOR(page));
         break;
  }

  priv->transition_playing = FALSE;
}

ClutterFixed hd_launcher_page_get_scroll_y(HdLauncherPage *page)
{
  HdLauncherPagePrivate *priv;
  ClutterActor *bar;
  TidyAdjustment *adjust;

  if (!HD_IS_LAUNCHER_PAGE(page))
    return 0;

  priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);

  bar = tidy_scroll_view_get_vscroll_bar (TIDY_SCROLL_VIEW(priv->scroller));
  adjust = tidy_scroll_bar_get_adjustment (TIDY_SCROLL_BAR(bar));
  return tidy_adjustment_get_valuex( adjust );
}

void hd_launcher_page_set_drag_distance(HdLauncherPage *page, float d)
{
  HdLauncherPagePrivate *priv;

  if (!HD_IS_LAUNCHER_PAGE(page))
      return;

  priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);
  priv->drag_distance = d;
}

/* Return how far the user has dragged */
float hd_launcher_page_get_drag_distance(HdLauncherPage *page)
{
  HdLauncherPagePrivate *priv;

  if (!HD_IS_LAUNCHER_PAGE(page))
      return 0;

  priv = HD_LAUNCHER_PAGE_GET_PRIVATE (page);
  return priv->drag_distance;
}
