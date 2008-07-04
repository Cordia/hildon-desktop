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
  g_signal_emit (layout, signals[SIGNAL_OK_CLICKED], 0);
  return TRUE;
}

static void
hd_layout_dialog_constructed (GObject *object)
{
  ClutterActor        *rect, *label;
  /* FIXME -- these should come from theme */
  ClutterColor         clr_b0 = {0, 0, 0, 0xff};
  ClutterColor         clr_b1 = {0x44, 0x44, 0x44, 0xff};
  ClutterColor         clr_b2 = {0x77, 0x77, 0x77, 0xff};
  ClutterColor         clr_f  = {0xfa, 0xfa, 0xfa, 0xff};
  HdLayoutDialogPrivate   *priv = HD_LAYOUT_DIALOG (object)->priv;
  MBWindowManager     *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;
  guint                xwidth, xheight, w, h;

  xwidth = wm->xdpy_width;
  xheight = wm->xdpy_height;

  rect = clutter_rectangle_new_with_color (&clr_b0);
  clutter_actor_set_size (rect, xwidth, HDLD_HEIGHT);
  clutter_actor_set_position (rect, 0, xheight - HDLD_HEIGHT);
  clutter_container_add_actor (CLUTTER_CONTAINER (object), rect);

  rect = clutter_rectangle_new_with_color (&clr_b1);
  clutter_actor_set_size (rect, xwidth, HDLD_HEIGHT - HDLD_TITLEBAR);
  clutter_actor_set_position (rect, 0,
			      xheight - (HDLD_HEIGHT - HDLD_TITLEBAR));
  clutter_container_add_actor (CLUTTER_CONTAINER (object), rect);

  rect = clutter_rectangle_new_with_color (&clr_b2);
  clutter_actor_set_size (rect, HDLD_ACTION_WIDTH,
			  HDLD_HEIGHT - HDLD_TITLEBAR);
  clutter_actor_set_position (rect,
			      xwidth - HDLD_ACTION_WIDTH,
			      xheight - (HDLD_HEIGHT - HDLD_TITLEBAR));

  clutter_container_add_actor (CLUTTER_CONTAINER (object), rect);

  /* FIXME -- color and font (?) from theme, gettextize labels */
  label = clutter_label_new_full ("Sans 12", "Manage views", &clr_f);
  clutter_actor_show (label);
  clutter_actor_get_size (label, &w, &h);
  clutter_actor_set_position (label, (xwidth - w) / 2,
			      xheight - HDLD_HEIGHT + (HDLD_TITLEBAR - h) / 2);
  clutter_container_add_actor (CLUTTER_CONTAINER (object), label);

  label = clutter_label_new_full ("Sans 12",
				  "Activate desired Home views:", &clr_f);
  clutter_actor_show (label);
  clutter_actor_get_size (label, &w, &h);
  clutter_actor_set_position (label, HDLD_PADDING,
			      xheight-(HDLD_HEIGHT-HDLD_TITLEBAR)+HDLD_PADDING);
  clutter_container_add_actor (CLUTTER_CONTAINER (object), label);

  rect = clutter_rectangle_new_with_color (&clr_b1);
  clutter_actor_set_size (rect, HDLD_OK_WIDTH, HDLD_OK_HEIGHT);
  clutter_actor_set_position (rect,
			      xwidth - HDLD_ACTION_WIDTH + (HDLD_ACTION_WIDTH - HDLD_OK_WIDTH)/2,
			      xheight - (HDLD_HEIGHT - HDLD_TITLEBAR) + (HDLD_HEIGHT - HDLD_TITLEBAR - HDLD_OK_HEIGHT)/2);

  clutter_container_add_actor (CLUTTER_CONTAINER (object), rect);

  clutter_actor_set_reactive (rect, TRUE);

  g_signal_connect (rect, "button-release-event",
		    G_CALLBACK (hd_layout_dialog_ok_clicked),
		    object);

  label = clutter_label_new_full ("Sans 12",
				  "OK", &clr_f);
  clutter_actor_show (label);
  clutter_actor_get_size (label, &w, &h);
  clutter_actor_set_position (label,
			      xwidth - HDLD_ACTION_WIDTH + (HDLD_ACTION_WIDTH - HDLD_OK_WIDTH)/2 + (HDLD_OK_WIDTH - w)/2,
			      xheight - (HDLD_HEIGHT - HDLD_TITLEBAR) + (HDLD_HEIGHT - HDLD_TITLEBAR - HDLD_OK_HEIGHT)/2 + (HDLD_OK_HEIGHT - h)/2);
  clutter_container_add_actor (CLUTTER_CONTAINER (object), label);
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

