/*
 * This file is part of hildon-desktop tests
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

/*
 * Sample Home applet.
 */

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>          /* for XA_ATOM etc */

#define PAN_THRESHOLD 20

/*
static guint      motion_handler_id = 0;
static gint       start_x;
*/
static GtkWidget *window = NULL;
static Atom       pan_atom = None;
static gint       view_id = 0;

#if 0
static void
do_home_pan (gboolean left)
{
  XEvent ev;

  memset(&ev, 0, sizeof(ev));

  ev.xclient.type = ClientMessage;
  ev.xclient.window = GDK_WINDOW_XID (window->window);
  ev.xclient.message_type = pan_atom;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = (long)left;

  XSendEvent (GDK_DISPLAY (), GDK_ROOT_WINDOW (), False,
	      StructureNotifyMask, &ev);

  XSync (GDK_DISPLAY (), False);
}

static gint
on_motion_event (GtkWidget *widget, GdkEventMotion *event)
{
  gint x = (gint) event->x;
  gint diff = x - start_x;

  if ((diff > 0 && diff > PAN_THRESHOLD) || (diff < 0 && diff < -PAN_THRESHOLD))
    {
      if (motion_handler_id)
	{
	  g_signal_handler_disconnect (window,
				       motion_handler_id);
	  motion_handler_id = 0;
	}

      do_home_pan (diff < 0 ? TRUE : FALSE);
    }

  return FALSE;
}
#endif

static gint
on_button_event (GtkWidget *widget, GdkEventButton *event)
{
  g_printerr ("Got applet button %s event [%f,%f] on view %d.\n",
	     event->type == GDK_BUTTON_RELEASE ? "RELEASE" : "PRESS",
	     event->x, event->y, view_id);

#if 0
  if (event->type == GDK_BUTTON_RELEASE)
    {
      if (motion_handler_id)
	{
	  g_signal_handler_disconnect (window, motion_handler_id);
	  motion_handler_id = 0;
	}
    }
  else
    {
      motion_handler_id = g_signal_connect (G_OBJECT (window),
					    "motion-notify-event",
					    G_CALLBACK (on_motion_event), NULL);
      start_x = (gint)event->x;
    }
#endif

  return FALSE;
}

static GdkFilterReturn
x_event_filter_func (GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
  XEvent * xev = (XEvent*)xevent;

  if (xev->type == ClientMessage)
    {
      char *type = NULL;
      XClientMessageEvent *xmsg = (XClientMessageEvent *)xev;

      type = XGetAtomName (GDK_DISPLAY (), xmsg->message_type);

      g_debug ("Got ClientMessage of type %s", type);

      XFree (type);
    }

  return GDK_FILTER_CONTINUE;
}

int main (int argc, char *argv[])
{
  Atom applet_id, wm_type, applet_type, view_id_atom;
  GtkWidget *w, *b;
  int i;

  for (i = 1; i < argc; ++i)
    {
      if (!strncmp (argv[i], "--view-id", 9))
	{
	  g_message ("got view id %s", argv[i]+10);

	  view_id = atoi (argv[i]+10);
	}
    }

  gtk_init (&argc, &argv);

  wm_type = XInternAtom (GDK_DISPLAY (), "_NET_WM_WINDOW_TYPE", False);

  applet_id = XInternAtom (GDK_DISPLAY (), "_HILDON_APPLET_ID", False);

  applet_type = XInternAtom (GDK_DISPLAY (),
			    "_HILDON_WM_WINDOW_TYPE_HOME_APPLET", False);

  pan_atom = XInternAtom (GDK_DISPLAY (), "_HILDON_CLIENT_MESSAGE_PAN", False);

  view_id_atom = XInternAtom (GDK_DISPLAY (), "_HILDON_HOME_VIEW", False);
  window  = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  g_signal_connect (G_OBJECT (window), "button-release-event",
		    G_CALLBACK (on_button_event), NULL);

  g_signal_connect (G_OBJECT (window), "button-press-event",
		    G_CALLBACK (on_button_event), NULL);

  gtk_window_resize (GTK_WINDOW (window), 300, 150);
  gtk_window_move (GTK_WINDOW (window), 200, 200);

  b = gtk_hbox_new (FALSE, 5);
  gtk_widget_show (b);
  gtk_container_add (GTK_CONTAINER (window), b);

  /*
  w = gtk_hscale_new_with_range (0.0, 100.0, 1.0);
  gtk_widget_show (w);
  gtk_box_pack_start (GTK_BOX (b), w, TRUE, TRUE, 0);

  w = gtk_vscale_new_with_range (0.0, 100.0, 1.0);
  */
  w = gtk_entry_new ();
  gtk_widget_show (w);
  gtk_box_pack_start (GTK_BOX (b), w, TRUE, TRUE, 0);

  gtk_widget_realize (window);

  gdk_window_set_events (window->window,
			 gdk_window_get_events (window->window)|
			 GDK_BUTTON_PRESS_MASK   |
			 GDK_BUTTON_RELEASE_MASK |
			 GDK_POINTER_MOTION_MASK);

  XChangeProperty (GDK_DISPLAY (), GDK_WINDOW_XID (window->window),
		   wm_type, XA_ATOM, 32, PropModeReplace,
		   (unsigned char *)&applet_type, 1);

  XChangeProperty (GDK_DISPLAY (), GDK_WINDOW_XID (window->window),
		   applet_id, XA_STRING, 8, PropModeReplace,
		   (unsigned char *) "test-applet-id", 14);

  XChangeProperty (GDK_DISPLAY (), GDK_WINDOW_XID (window->window),
		   view_id_atom, XA_CARDINAL, 32, PropModeReplace,
		   (unsigned char *)&view_id, 1);

  gdk_window_add_filter (window->window, x_event_filter_func, NULL);

  gtk_widget_show_all (window);

  gtk_main();

  return 0;
}
