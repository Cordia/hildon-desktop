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

#include "hd-edit-menu.h"
#include "hd-comp-mgr.h"
#include "hd-home.h"
#include "hd-util.h"
#include "hd-background-dialog.h"

#include "hd-add-applet-dialog.h"
#include "hd-add-task-dialog.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>

#include <matchbox/core/mb-wm.h>

#define HDEM_PADDING_EXT 10
#define HDEM_PADDING_INT 5
#define HDEM_ITEM_HEIGHT 80
#define HDEM_ITEM_TEXT_OFFSET 20
enum
{
  PROP_COMP_MGR = 1,
  PROP_HOME,
};

struct _HdEditMenuPrivate
{
  MBWMCompMgrClutter   *comp_mgr;
  HdHome               *home;

  ClutterActor         *background;

  char                 *labels[6];
};

static void hd_edit_menu_class_init (HdEditMenuClass *klass);
static void hd_edit_menu_init       (HdEditMenu *self);
static void hd_edit_menu_dispose    (GObject *object);
static void hd_edit_menu_finalize   (GObject *object);

static void hd_edit_menu_set_property (GObject      *object,
				       guint         prop_id,
				       const GValue *value,
				       GParamSpec   *pspec);

static void hd_edit_menu_get_property (GObject      *object,
				       guint         prop_id,
				       GValue       *value,
				       GParamSpec   *pspec);

static void hd_edit_menu_constructed (GObject *object);

static gboolean hd_edit_menu_item_release (ClutterActor       *item,
					   ClutterButtonEvent *event,
					   HdEditMenu         *menu);


G_DEFINE_TYPE (HdEditMenu, hd_edit_menu, CLUTTER_TYPE_GROUP);

static void
hd_edit_menu_class_init (HdEditMenuClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec   *pspec;

  g_type_class_add_private (klass, sizeof (HdEditMenuPrivate));

  object_class->dispose      = hd_edit_menu_dispose;
  object_class->finalize     = hd_edit_menu_finalize;
  object_class->set_property = hd_edit_menu_set_property;
  object_class->get_property = hd_edit_menu_get_property;
  object_class->constructed  = hd_edit_menu_constructed;

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
}

static void
hd_edit_menu_constructed (GObject *object)
{
  ClutterActor        *rect, *label;
  ClutterColor         clr_b = {0x77, 0x77, 0x77, 0xff};
  ClutterColor         clr_i = {0x44, 0x44, 0x44, 0xff};
  ClutterColor         clr_c = {0, 0, 0, 0};
  HdEditMenuPrivate   *priv = HD_EDIT_MENU (object)->priv;
  MBWindowManager     *wm = MB_WM_COMP_MGR (priv->comp_mgr)->wm;
  guint                xwidth, iwidth;
  gint                  i, j;

  xwidth = wm->xdpy_width;
  iwidth = (xwidth - 2 * HDEM_PADDING_EXT - 3 * HDEM_PADDING_INT) / 2;

  rect = clutter_rectangle_new_with_color (&clr_c);

  clutter_actor_set_size (rect, xwidth, wm->xdpy_height);
  clutter_actor_show (rect);
  clutter_actor_set_reactive (rect, TRUE);
  clutter_container_add_actor (CLUTTER_CONTAINER (object), rect);

  g_object_set_data (G_OBJECT (rect), "HD-EDIT-MENU-action",
		     GINT_TO_POINTER (-1));

  g_signal_connect (rect, "button-release-event",
		    G_CALLBACK (hd_edit_menu_item_release),
		    object);

  rect = clutter_rectangle_new_with_color (&clr_b);

  clutter_actor_set_size (rect,
			  xwidth - 2 * HDEM_PADDING_EXT,
			  (HDEM_ITEM_HEIGHT + HDEM_PADDING_INT)*3 +
			  HDEM_PADDING_INT);
  clutter_actor_set_position (rect, HDEM_PADDING_EXT, 0);
  clutter_actor_show (rect);
  clutter_actor_set_reactive (rect, FALSE);
  clutter_container_add_actor (CLUTTER_CONTAINER (object), rect);

  for (i = 0; i < 3; ++i)
    for (j = 0; j < 2; ++j)
      {
        guint lw, lh;

        rect = clutter_rectangle_new_with_color (&clr_i);

        clutter_actor_set_size (rect, iwidth, HDEM_ITEM_HEIGHT);

        clutter_actor_set_position (rect,
                  HDEM_PADDING_EXT +
                  HDEM_PADDING_INT +
                  j * (HDEM_PADDING_INT + iwidth),
                  HDEM_PADDING_INT +
                  i * (HDEM_PADDING_INT + HDEM_ITEM_HEIGHT));

        clutter_actor_show (rect);
        clutter_actor_set_reactive (rect, TRUE);
        clutter_container_add_actor (CLUTTER_CONTAINER (object), rect);

        g_assert ((j + 2 * i) < G_N_ELEMENTS (priv->labels));

        g_object_set_data (G_OBJECT (rect), "HD-EDIT-MENU-action",
               GINT_TO_POINTER (j + 2 * i));

        g_signal_connect (rect, "button-release-event",
              G_CALLBACK (hd_edit_menu_item_release),
              object);

        label = clutter_label_new_full ("Sans 16pt",
	              priv->labels[j+2*i],
	              &clr_b);

        clutter_actor_get_size (label, &lw, &lh);

        clutter_actor_set_position (label,
                  HDEM_PADDING_EXT +
                  HDEM_PADDING_INT +
                  j * (HDEM_PADDING_INT + iwidth) +
                  HDEM_ITEM_TEXT_OFFSET,
                  HDEM_PADDING_INT +
                  i * (HDEM_PADDING_INT + HDEM_ITEM_HEIGHT) +
                  (HDEM_ITEM_HEIGHT - lh)/2);

        clutter_actor_show (label);
        clutter_actor_set_reactive (label, FALSE);
        clutter_container_add_actor (CLUTTER_CONTAINER (object), label);
      }
}

static void
hd_edit_menu_init (HdEditMenu *self)
{
  HdEditMenuPrivate *priv;

  priv = self->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (self, HD_TYPE_EDIT_MENU, HdEditMenuPrivate);

  /* FIXME -- these should be loaded from somewhere and localized */
  priv->labels[0] = _("home_me_select_applets");
  priv->labels[1] = _("home_me_select_shortcuts");
  priv->labels[2] = _("home_me_select_bookmarks");
  priv->labels[3] = _("home_me_change_background");
  priv->labels[4] = _("home_me_select_contacts");
  priv->labels[5] = _("home_me_manage_views");
}

static void
hd_edit_menu_dispose (GObject *object)
{
  G_OBJECT_CLASS (hd_edit_menu_parent_class)->dispose (object);
}

static void
hd_edit_menu_finalize (GObject *object)
{
  G_OBJECT_CLASS (hd_edit_menu_parent_class)->finalize (object);
}

static void
hd_edit_menu_set_property (GObject       *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
  HdEditMenuPrivate *priv = HD_EDIT_MENU (object)->priv;

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
hd_edit_menu_get_property (GObject      *object,
			   guint         prop_id,
			   GValue       *value,
			   GParamSpec   *pspec)
{
  HdEditMenuPrivate *priv = HD_EDIT_MENU (object)->priv;

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

static gboolean
hd_edit_menu_item_release (ClutterActor       *item,
			   ClutterButtonEvent *event,
			   HdEditMenu         *menu)
{
  HdEditMenuPrivate *priv = menu->priv;
  guint              action;
  GtkWidget         *dialog;

  action = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item),
					   "HD-EDIT-MENU-action"));

  g_debug ("ACTION %d", action);

  clutter_actor_hide (CLUTTER_ACTOR (menu));

  switch (action)
    {
    case 0:
        {
          /* Ungrab pointer */
          hd_home_ungrab_pointer (priv->home);

          /* Show dialog */
          GtkWidget *dialog = hd_add_applet_dialog_new ();
          gtk_dialog_run (GTK_DIALOG (dialog));
          /* FIXME use destroy if NB#89541 is fixed */
          gtk_widget_hide (dialog);

          /* Grab pointer again */
          hd_home_grab_pointer (priv->home);
        }
      break;

    case 1:
        {
          /* Ungrab pointer */
          hd_home_ungrab_pointer (priv->home);

          /* Show dialog */
          GtkWidget *dialog = hd_add_task_dialog_new ();
          gtk_dialog_run (GTK_DIALOG (dialog));
          /* FIXME use destroy if NB#89541 is fixed */
          gtk_widget_hide (dialog);

          /* Grab pointer again */
          hd_home_grab_pointer (priv->home);
        }
      break;


    case 3:
      dialog = hd_background_dialog_new (priv->home,
					 hd_home_get_current_view (priv->home));
      hd_home_set_mode (priv->home, HD_HOME_MODE_NORMAL);
      gtk_widget_show (dialog);
      break;

    case 5:
      hd_home_set_mode (priv->home, HD_HOME_MODE_LAYOUT);
      break;
    default:;
    }

  return TRUE;
}

