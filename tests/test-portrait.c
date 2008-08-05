/*
 * This file is part of hildon-desktop tests
 *
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Author:  Robert Bragg <bob@o-hand.com>
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

/*
 * This creates a top level window and sets the _HILDON_SUPPORTS_PORTRAIT_MODE
 * property before mapping it. It then lets you toggle the property via a
 * button. Since dialogs may need special care for applications using portrait
 * mode, this test lets you spawn two kinds of dialogs: Either a dialog that
 * itself has the _HILDON_SUPPORTS_PORTRAIT_MODE property set on it or a
 * regular dialog without the property set.
 */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#define PORTRAIT_PROPERTY_NAME "_HILDON_SUPPORTS_PORTRAIT_MODE"

typedef struct _TestState
{
  GtkWidget *top_window;
  gboolean   portrait_mode;
} TestState;

static void
set_portrait_mode_hint (Window window, gboolean portrait)
{
  Atom _hildon_supports_portrait_mode =
    XInternAtom (GDK_DISPLAY (),
		 PORTRAIT_PROPERTY_NAME,
		 False);

  if (portrait)
    {
      guint32 portrait_mode_val = 1;
      g_print ("Setting %s = 1 for window = 0x%08lx\n",
	       PORTRAIT_PROPERTY_NAME, window);
      XChangeProperty (GDK_DISPLAY (),
		       window,
		       _hildon_supports_portrait_mode,
		       XA_CARDINAL,
		       32,
		       PropModeReplace,
		       (unsigned char*)&portrait_mode_val,
		       1);
    }
  else
    {
      g_print ("Deleting %s property for window  0x%08lx\n",
	       PORTRAIT_PROPERTY_NAME, window);
      XDeleteProperty (GDK_DISPLAY (),
		       window,
		       _hildon_supports_portrait_mode);
    }
}

static gboolean
toggle_portrait_button_release (GtkWidget	*widget,
				GdkEventButton	*event,
				gpointer         data)
{
  TestState *state = data;

  state->portrait_mode = state->portrait_mode ? FALSE : TRUE;

  gtk_button_set_label (GTK_BUTTON (widget),
			state->portrait_mode ? "Delete portrait property"
			  : "Set portrait property");

  set_portrait_mode_hint (GDK_WINDOW_XID (state->top_window->window),
			  state->portrait_mode);

  return FALSE;
}

static gboolean
open_portrait_dialog_button_release (GtkWidget	    *widget,
				     GdkEventButton  *event,
				     gpointer         data)
{
  TestState *state = data;
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (GTK_WINDOW (state->top_window),
				   0,
				   GTK_MESSAGE_INFO,
				   GTK_BUTTONS_NONE,
				   "This dialog has %s set on it\n",
				   PORTRAIT_PROPERTY_NAME);
  gtk_widget_realize (dialog);
  set_portrait_mode_hint (GDK_WINDOW_XID (dialog->window), TRUE);
  gtk_dialog_run (GTK_DIALOG (dialog));

  return FALSE;
}

static gboolean
open_normal_dialog_button_release (GtkWidget	    *widget,
				   GdkEventButton  *event,
				   gpointer         data)
{
  TestState *state = data;
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (GTK_WINDOW (state->top_window),
				   0,
				   GTK_MESSAGE_INFO,
				   GTK_BUTTONS_NONE,
				   "This dialog does _not_ have %s "
				   "set on it\n",
				   PORTRAIT_PROPERTY_NAME);
  gtk_widget_realize (dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));

  return FALSE;
}

int
main (int argc, char **argv)
{
  TestState state;
  GtkWidget *vbox, *toggle_portrait;
  GtkWidget *open_portrait_dialog, *open_normal_dialog;

  gtk_init (&argc, &argv);

  state.top_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_realize (state.top_window);

  state.portrait_mode = 1;
  set_portrait_mode_hint (GDK_WINDOW_XID (state.top_window->window),
			  state.portrait_mode);

  vbox = gtk_vbox_new (TRUE, 10);
  gtk_container_add (GTK_CONTAINER (state.top_window), vbox);

  toggle_portrait = gtk_button_new_with_label ("Delete portrait property");
  g_signal_connect (toggle_portrait,
		    "button-release-event",
		    G_CALLBACK (toggle_portrait_button_release),
		    &state);
  gtk_box_pack_start (GTK_BOX (vbox),
		      toggle_portrait,
		      TRUE,
		      TRUE,
		      0);

  open_portrait_dialog = gtk_button_new_with_label ("Open portrait dialog");
  g_signal_connect (open_portrait_dialog,
		    "button-release-event",
		    G_CALLBACK (open_portrait_dialog_button_release),
		    &state);
  gtk_box_pack_start (GTK_BOX (vbox),
		      open_portrait_dialog,
		      TRUE,
		      TRUE,
		      0);

  open_normal_dialog = gtk_button_new_with_label ("Open normal dialog");
  g_signal_connect (open_normal_dialog,
		    "button-release-event",
		    G_CALLBACK (open_normal_dialog_button_release),
		    &state);
  gtk_box_pack_start (GTK_BOX (vbox),
		      open_normal_dialog,
		      TRUE,
		      TRUE,
		      0);

  gtk_widget_show_all (state.top_window);

  gtk_main ();

  return 0;
}

