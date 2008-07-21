/*
 * This file is part of hildon-desktop
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Tomas Frydrych <tf@o-hand.com>
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

#include "hd-layout-dialog.h"
#include "hd-comp-mgr.h"
#include "hd-home.h"
#include "hd-util.h"
#include "hd-gtk-style.h"

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>

#include <matchbox/core/mb-wm.h>

/* FIXME -- match to spec */
#define HDLD_HEIGHT 200
#define HDLD_ACTION_WIDTH 200
#define HDLD_TITLEBAR 40
#define HDLD_PADDING 10
#define HDLD_OK_WIDTH 100
#define HDLD_OK_HEIGHT 40
#define HDLD_THUMB_WIDTH 135
#define HDLD_THUMB_HEIGHT 90

enum
{
  PROP_COMP_MGR = 1,
  PROP_HOME,
};

enum
{
  SIGNAL_OK_CLICKED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

struct _HdLayoutDialogPrivate
{
  MBWMCompMgrClutter   *comp_mgr;
  HdHome               *home;

  ClutterActor         *background;

  GList                *highlighters;
};

static void hd_layout_dialog_class_init (HdLayoutDialogClass *klass);
static void hd_layout_dialog_init       (HdLayoutDialog *self);
static void hd_layout_dialog_dispose    (GObject *object);
static void hd_layout_dialog_finalize   (GObject *object);

static void hd_layout_dialog_set_property (GObject      *object,
					   guint         prop_id,
					   const GValue *value,
					   GParamSpec   *pspec);

static void hd_layout_dialog_get_property (GObject      *object,
					   guint         prop_id,
					   GValue       *value,
					   GParamSpec   *pspec);

static void hd_layout_dialog_constructed (GObject *object);

G_DEFINE_TYPE (HdLayoutDialog, hd_layout_dialog, CLUTTER_TYPE_GROUP);

static void
hd_layout_dialog_class_init (HdLayoutDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec   *pspec;

  g_type_class_add_private (klass, sizeof (HdLayoutDialogPrivate));

  object_class->dispose      = hd_layout_dialog_dispose;
  object_class->finalize     = hd_layout_dialog_finalize;
  object_class->set_property = hd_layout_dialog_set_property;
  object_class->get_property = hd_layout_dialog_get_property;
  object_class->constructed  = hd_layout_dialog_constructed;

  pspec = g_param_spec_pointer ("comp-mgr",
				"Composite Manager",
				"MBWMCompMgrClutter Object",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class, PROP_COMP_MGR, pspec);

  pspec = g_param_spec_pointer ("home",
				"HdHome",
				"Parent HdHome object",
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_property (object_class, PROP_HOME, pspec);

  signals[SIGNAL_OK_CLICKED] =
      g_signal_new ("ok-clicked",
                    G_OBJECT_CLASS_TYPE (object_class),
                    G_SIGNAL_RUN_FIRST,
                    G_STRUCT_OFFSET (HdLayoutDialogClass, ok_clicked),
                    NULL,
                    NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE,
                    0,
		    0);
}

static gboolean
hd_layout_dialog_ok_clicked (ClutterActor       *button,
			     ClutterButtonEvent *event,
			     HdLayoutDialog     *layout)
{
  HdLayoutDialogPrivate *priv = layout->priv;
  GList                 *l = priv->highlighters;
  gint                   i = 0;

  while (l)
    {
      ClutterActor *a = l->data;

      hd_home_set_view_status (priv->home, i, CLUTTER_ACTOR_IS_VISIBLE (a));

      ++i;
      l = l->next;
    }

  g_signal_emit (layout, signals[SIGNAL_OK_CLICKED], 0);
  return TRUE;
}

static gboolean
hd_layout_dialog_thumb_clicked (ClutterActor       *thumb,
				ClutterButtonEvent *event,
				ClutterActor       *highlighter)

{
  if (CLUTTER_ACTOR_IS_VISIBLE (highlighter))
    clutter_actor_hide (highlighter);
  else
    clutter_actor_show (highlighter);

  return TRUE;
}

static void
hd_layout_dialog_fixup_highlighters (HdLayoutDialog *dialog)
{
  HdLayoutDialogPrivate *priv = dialog->priv;
  GList                 *l;

  l = priv->highlighters;
  while (l)
    {
      ClutterActor *a = l->data;
      clutter_actor_hide (a);
      l = l->next;
    }

  l = hd_home_get_active_views (priv->home);
  while (l)
    {
      HdHomeView   *view = l->data;
      ClutterActor *h;

      guint id = hd_home_view_get_view_id (view);

      h = g_list_nth_data (priv->highlighters, id);
      clutter_actor_show (h);

      l = l->next;
    }
}

static void
hd_layout_dialog_constructed (GObject *object)
{
  HdLayoutDialogPrivate *priv = HD_LAYOUT_DIALOG (object)->priv;
  ClutterActor          *rect, *label;
  ClutterColor           clr_dark;
  ClutterColor           clr_mid;
  ClutterColor           clr_light;
  ClutterColor           clr_font;
  MBWindowManager       *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;
  guint                  xwidth, xheight, w, h;
  GList                 *l;
  gboolean               have_fbos;
  gint                   i;
  guint                  label2_height;
  gchar *		 font_string;

  xwidth = wm->xdpy_width;
  xheight = wm->xdpy_height;

  hd_gtk_style_get_dark_color (HD_GTK_BUTTON_SINGLETON,
			       GTK_STATE_NORMAL, &clr_dark);
  hd_gtk_style_get_mid_color (HD_GTK_BUTTON_SINGLETON,
			      GTK_STATE_NORMAL, &clr_mid);
  hd_gtk_style_get_light_color (HD_GTK_BUTTON_SINGLETON,
				GTK_STATE_NORMAL, &clr_light);
  font_string = hd_gtk_style_get_font_string (HD_GTK_BUTTON_SINGLETON);

  rect = clutter_rectangle_new_with_color (&clr_dark);
  clutter_actor_set_size (rect, xwidth, HDLD_HEIGHT);
  clutter_actor_set_position (rect, 0, xheight - HDLD_HEIGHT);
  clutter_actor_set_reactive (rect, TRUE);
  clutter_container_add_actor (CLUTTER_CONTAINER (object), rect);

  rect = clutter_rectangle_new_with_color (&clr_mid);
  clutter_actor_set_size (rect, xwidth, HDLD_HEIGHT - HDLD_TITLEBAR);
  clutter_actor_set_position (rect, 0,
			      xheight - (HDLD_HEIGHT - HDLD_TITLEBAR));
  clutter_container_add_actor (CLUTTER_CONTAINER (object), rect);

  rect = clutter_rectangle_new_with_color (&clr_light);
  clutter_actor_set_size (rect, HDLD_ACTION_WIDTH,
			  HDLD_HEIGHT - HDLD_TITLEBAR);
  clutter_actor_set_position (rect,
			      xwidth - HDLD_ACTION_WIDTH,
			      xheight - (HDLD_HEIGHT - HDLD_TITLEBAR));

  clutter_container_add_actor (CLUTTER_CONTAINER (object), rect);

  /* FIXME -- gettextize labels */
  hd_gtk_style_get_text_color (HD_GTK_BUTTON_SINGLETON,
			       GTK_STATE_NORMAL,
			       &clr_font);
  label = clutter_label_new_full (font_string, "Manage views", &clr_font);
  clutter_actor_show (label);
  clutter_actor_get_size (label, &w, &h);
  clutter_actor_set_position (label, (xwidth - w) / 2,
			      xheight - HDLD_HEIGHT + (HDLD_TITLEBAR - h) / 2);
  clutter_container_add_actor (CLUTTER_CONTAINER (object), label);

  label = clutter_label_new_full (font_string,
				  "Activate desired Home views:", &clr_font);
  clutter_actor_show (label);
  clutter_actor_get_size (label, &w, &h);
  label2_height = h;
  clutter_actor_set_position (label, HDLD_PADDING,
			      xheight-(HDLD_HEIGHT-HDLD_TITLEBAR)+HDLD_PADDING);
  clutter_container_add_actor (CLUTTER_CONTAINER (object), label);

  rect = clutter_rectangle_new_with_color (&clr_mid);
  clutter_actor_set_size (rect, HDLD_OK_WIDTH, HDLD_OK_HEIGHT);
  clutter_actor_set_position (rect,
			      xwidth - HDLD_ACTION_WIDTH + (HDLD_ACTION_WIDTH - HDLD_OK_WIDTH)/2,
			      xheight - (HDLD_HEIGHT - HDLD_TITLEBAR) + (HDLD_HEIGHT - HDLD_TITLEBAR - HDLD_OK_HEIGHT)/2);

  clutter_container_add_actor (CLUTTER_CONTAINER (object), rect);

  clutter_actor_set_reactive (rect, TRUE);

  g_signal_connect (rect, "button-release-event",
		    G_CALLBACK (hd_layout_dialog_ok_clicked),
		    object);

  label = clutter_label_new_full (font_string,
				  "OK", &clr_font);
  clutter_actor_show (label);
  clutter_actor_get_size (label, &w, &h);
  clutter_actor_set_position (label,
			      xwidth - HDLD_ACTION_WIDTH + (HDLD_ACTION_WIDTH - HDLD_OK_WIDTH)/2 + (HDLD_OK_WIDTH - w)/2,
			      xheight - (HDLD_HEIGHT - HDLD_TITLEBAR) + (HDLD_HEIGHT - HDLD_TITLEBAR - HDLD_OK_HEIGHT)/2 + (HDLD_OK_HEIGHT - h)/2);
  clutter_container_add_actor (CLUTTER_CONTAINER (object), label);

  g_free (font_string);

  /*
   * Now construct the thumbnails.
   */
  l = hd_home_get_all_views (priv->home);
  have_fbos = clutter_feature_available (CLUTTER_FEATURE_OFFSCREEN);
  i = 0;

  while (l)
    {
      HdHomeView   *view = l->data;
      ClutterActor *thumb = clutter_group_new ();
      ClutterActor *thumb_background;
      ClutterActor *thumb_content;
      ClutterColor clr_b = {0xff, 0xff, 0, 0xff};

      thumb_background = clutter_rectangle_new_with_color (&clr_b);
      clutter_actor_set_size (thumb_background,
			      HDLD_THUMB_WIDTH, HDLD_THUMB_HEIGHT);
      clutter_actor_hide (thumb_background);
      clutter_container_add_actor (CLUTTER_CONTAINER (thumb),
				   thumb_background);

      priv->highlighters =
	g_list_append (priv->highlighters, thumb_background);

      if (have_fbos)
	{
	  thumb_content =
	    clutter_texture_new_from_actor (CLUTTER_ACTOR (view));
	}
      else
	{
	  ClutterActor *background;
	  background = hd_home_view_get_background (view);

	  if (CLUTTER_IS_TEXTURE (background))
	    {
	      CoglHandle handle;

	      handle =
		clutter_texture_get_cogl_texture (CLUTTER_TEXTURE(background));

	      thumb_content = clutter_texture_new ();
	      clutter_texture_set_cogl_texture(CLUTTER_TEXTURE (thumb_content),
					       handle);
	    }
	  else
	    {
	      /*
	       * No fbos and no texture, so we just create a rectangle.
	       */
	      ClutterColor clr = {0xA0, 0xA0, 0xA0, 0xff};

	      thumb_content = clutter_rectangle_new_with_color (&clr);
	    }
	}

      clutter_actor_set_size (thumb_content,
			      HDLD_THUMB_WIDTH - HDLD_PADDING,
			      HDLD_THUMB_HEIGHT - HDLD_PADDING);
      clutter_actor_set_position (thumb_content,
				  HDLD_PADDING/2, HDLD_PADDING/2);
      clutter_actor_set_reactive (thumb_content, TRUE);

      g_signal_connect (thumb_content, "button-release-event",
			G_CALLBACK (hd_layout_dialog_thumb_clicked),
			thumb_background);

      clutter_container_add_actor (CLUTTER_CONTAINER (thumb),
				   thumb_content);

      clutter_actor_set_size (thumb, HDLD_THUMB_WIDTH, HDLD_THUMB_HEIGHT);
      clutter_actor_set_position (thumb, HDLD_PADDING + i * (HDLD_THUMB_WIDTH + HDLD_PADDING), xheight - HDLD_HEIGHT + HDLD_TITLEBAR + (HDLD_HEIGHT - HDLD_TITLEBAR - HDLD_THUMB_HEIGHT - label2_height) / 2 + label2_height);

      clutter_container_add_actor (CLUTTER_CONTAINER (object), thumb);

      l = l->next;
      ++i;
    }

  hd_layout_dialog_fixup_highlighters (HD_LAYOUT_DIALOG (object));
}

static void
hd_layout_dialog_init (HdLayoutDialog *self)
{
  HdLayoutDialogPrivate *priv;

  priv = self->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (self, HD_TYPE_LAYOUT_DIALOG, HdLayoutDialogPrivate);
}

static void
hd_layout_dialog_dispose (GObject *object)
{
  G_OBJECT_CLASS (hd_layout_dialog_parent_class)->dispose (object);
}

static void
hd_layout_dialog_finalize (GObject *object)
{
  G_OBJECT_CLASS (hd_layout_dialog_parent_class)->finalize (object);
}

static void
hd_layout_dialog_set_property (GObject       *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
  HdLayoutDialogPrivate *priv = HD_LAYOUT_DIALOG (object)->priv;

  switch (prop_id)
    {
    case PROP_COMP_MGR:
      priv->comp_mgr = g_value_get_pointer (value);
      break;
    case PROP_HOME:
      priv->home = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hd_layout_dialog_get_property (GObject      *object,
			       guint         prop_id,
			       GValue       *value,
			       GParamSpec   *pspec)
{
  HdLayoutDialogPrivate *priv = HD_LAYOUT_DIALOG (object)->priv;

  switch (prop_id)
    {
    case PROP_COMP_MGR:
      g_value_set_pointer (value, priv->comp_mgr);
      break;
    case PROP_HOME:
      g_value_set_pointer (value, priv->home);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

