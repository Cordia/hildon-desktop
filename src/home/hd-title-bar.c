/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Gordon Williams <gordon.williams@collabora.co.uk>
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

/* This class handles drawing of things in the title bar right at the top
 * of the screen - currently just top-left buttons (which have to animate)
 */

#include "tidy/tidy-sub-texture.h"

#include "hd-title-bar.h"
#include "mb/hd-app.h"
#include "mb/hd-comp-mgr.h"
#include "mb/hd-decor.h"
#include "mb/hd-theme.h"
#include "hd-render-manager.h"
#include "hd-gtk-utils.h"

#include <matchbox/theme-engines/mb-wm-theme-png.h>
#include <matchbox/theme-engines/mb-wm-theme-xml.h>

struct _HdTitleBarPrivate
{
  ClutterActor          *btn_switcher;
  ClutterActor          *btn_launcher;

  MBWMTheme             *theme;
  /* All the theme images are jammed into this one image */
  ClutterTexture        *theme_image;
};

/* HdHomeThemeButtonBack, MBWMDecorButtonClose */

/* ------------------------------------------------------------------------- */

G_DEFINE_TYPE (HdTitleBar, hd_title_bar, CLUTTER_TYPE_GROUP);
#define HD_TITLE_BAR_GET_PRIVATE(obj) \
                (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                HD_TYPE_TITLE_BAR, HdTitleBarPrivate))

static ClutterActor *
hd_title_bar_top_left_button_new (HdTitleBar *bar, const char *icon_name);

/* ------------------------------------------------------------------------- */

#define ICON_IMAGE_SWITCHER "qgn_tswitcher_application"
#define ICON_IMAGE_LAUNCHER "qgn_general_add"
#define TOP_LEFT_BUTTON_HIGHLIGHT_TEXTURE "launcher-button-highlight.png"

/* margin to left of the app title */
#define HD_TITLE_BAR_TITLE_MARGIN 24

enum
{
  CLICKED_TOP_LEFT,
  PRESS_TOP_LEFT,
  LEAVE_TOP_LEFT,
  LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0, };

/* ------------------------------------------------------------------------- */

static void
hd_title_bar_init (HdTitleBar *bar)
{
  ClutterActor *actor = CLUTTER_ACTOR(bar);
  HdTitleBarPrivate *priv = bar->priv = HD_TITLE_BAR_GET_PRIVATE(bar);

  clutter_actor_set_position(actor, 0, 0);
  clutter_actor_set_size(actor,
                    HD_COMP_MGR_SCREEN_WIDTH, HD_COMP_MGR_TOP_MARGIN);
  /* Task Navigator Button */
  priv->btn_switcher = hd_title_bar_top_left_button_new (
      bar, ICON_IMAGE_SWITCHER);
  hd_render_manager_set_button (HDRM_BUTTON_TASK_NAV, priv->btn_switcher);

  /* Task Launcher Button */
  priv->btn_launcher = hd_title_bar_top_left_button_new (
      bar, ICON_IMAGE_LAUNCHER);
  hd_render_manager_set_button(HDRM_BUTTON_LAUNCHER, priv->btn_launcher);

  priv->theme = 0;
  priv->theme_image = 0;
}

static void
hd_title_bar_dispose (GObject *obj)
{
}

static void
hd_title_bar_class_init (HdTitleBarClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (HdTitleBarPrivate));

  gobject_class->dispose = hd_title_bar_dispose;

  signals[CLICKED_TOP_LEFT] =
      g_signal_new ("clicked-top-left",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);
  signals[PRESS_TOP_LEFT] =
        g_signal_new ("press-top-left",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
  signals[LEAVE_TOP_LEFT] =
        g_signal_new ("leave-top-left",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}

void
hd_title_bar_set_theme(HdTitleBar *bar, MBWMTheme *theme)
{
  HdTitleBarPrivate *priv = bar->priv;

  if (priv->theme != theme)
    {
      if (priv->theme_image)
        {
          clutter_actor_destroy(CLUTTER_ACTOR(priv->theme_image));
          priv->theme_image = 0;
          /* TODO: Clone textures? */
        }

      priv->theme = theme;
      if (priv->theme)
        {
          priv->theme_image =
            g_object_ref( clutter_texture_new_from_file(
                priv->theme->image_filename, 0 ));
          /* TODO: error handle? */
          if (!priv->theme_image) {
            return;
          }
          clutter_container_add_actor(CLUTTER_CONTAINER(bar),
                                      CLUTTER_ACTOR(priv->theme_image));
          clutter_actor_hide(CLUTTER_ACTOR(priv->theme_image));
        }
    }
}

void
hd_title_bar_set_show(HdTitleBar *bar, gboolean show)
{
  if (show)
    clutter_actor_show(CLUTTER_ACTOR(bar));
  else
    clutter_actor_hide(CLUTTER_ACTOR(bar));
}

/* ------------------------------------------------------------------------- */

static void
hd_title_bar_top_left_clicked (HdTitleBar *bar)
{
  g_signal_emit (bar, signals[CLICKED_TOP_LEFT], 0);
}

static void
hd_title_bar_top_left_press (HdTitleBar *bar)
{
  g_signal_emit (bar, signals[PRESS_TOP_LEFT], 0);
}

static void
hd_title_bar_top_left_leave (HdTitleBar *bar)
{
  g_signal_emit (bar, signals[LEAVE_TOP_LEFT], 0);
}

/* ------------------------------------------------------------------------- */
static ClutterActor *
hd_title_bar_top_left_button_new (HdTitleBar *bar, const char *icon_name)
{
  ClutterActor    *top_left_button;
  ClutterActor    *top_left_button_icon;
  ClutterActor    *top_left_button_highlight;
  ClutterGeometry  geom;
  GtkIconTheme    *icon_theme;
  GError          *error = NULL;

  icon_theme = gtk_icon_theme_get_default ();

  top_left_button = clutter_group_new ();
  clutter_actor_set_name (top_left_button, icon_name);

  top_left_button_highlight =
      clutter_texture_new_from_file (
        g_build_filename (HD_DATADIR, TOP_LEFT_BUTTON_HIGHLIGHT_TEXTURE, NULL),
        &error);
  if (error)
    {
      g_debug (error->message);
      g_error_free (error);
    }
  else
    {
      clutter_actor_set_size (top_left_button_highlight,
                              HD_COMP_MGR_TOP_LEFT_BTN_WIDTH,
                              HD_COMP_MGR_TOP_LEFT_BTN_HEIGHT);
      clutter_container_add_actor (CLUTTER_CONTAINER (top_left_button),
                                   top_left_button_highlight);
    }

  top_left_button_icon =
    hd_gtk_icon_theme_load_icon (icon_theme, icon_name, 48, 0);
  clutter_actor_get_geometry (top_left_button_icon, &geom);
  clutter_actor_set_position (
                      top_left_button_icon,
                      (HD_COMP_MGR_TOP_LEFT_BTN_WIDTH/2)-(geom.width/2),
                      (HD_COMP_MGR_TOP_LEFT_BTN_HEIGHT/2)-(geom.height/2));
  clutter_container_add_actor (CLUTTER_CONTAINER (top_left_button),
                               top_left_button_icon);


  clutter_actor_set_position (top_left_button, 0, 0);
  clutter_actor_set_reactive (top_left_button, TRUE);
  g_signal_connect_swapped (top_left_button, "button-release-event",
                            G_CALLBACK (hd_title_bar_top_left_clicked),
                            bar);
  g_signal_connect_swapped (top_left_button, "button-press-event",
                              G_CALLBACK (hd_title_bar_top_left_press),
                              bar);
  g_signal_connect_swapped (top_left_button, "leave-event",
                              G_CALLBACK (hd_title_bar_top_left_leave),
                              bar);

  return top_left_button;
}
