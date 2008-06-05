/*
 * Sample Home applet.
 *
 * gcc -Wall -g -o test `pkg-config --cflags --libs gtk+-2.0` hd-applet-test.c
 */

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>          /* for XA_ATOM etc */

#define PAN_THRESHOLD 20

static guint      motion_handler_id = 0;
static GtkWidget *window = NULL;
static gint       start_x;
static Atom       pan_atom = None;

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

static gint
on_button_event (GtkWidget *widget, GdkEventButton *event)
{
  g_message ("Got applet button %s event [%f,%f].",
	     event->type == GDK_BUTTON_RELEASE ? "RELEASE" : "PRESS",
	     event->x, event->y);

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

  return FALSE;
}

int main (int argc, char *argv[])
{
  Atom wm_type, applet_type;

  gtk_init (&argc, &argv);

  wm_type = XInternAtom (GDK_DISPLAY (), "_NET_WM_WINDOW_TYPE", False);

  applet_type = XInternAtom (GDK_DISPLAY (),
			    "_HILDON_WM_WINDOW_TYPE_HOME_APPLET", False);

  pan_atom = XInternAtom (GDK_DISPLAY (), "_HILDON_CLIENT_MESSAGE_PAN", False);

  window  = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  g_signal_connect (G_OBJECT (window), "button-release-event",
		    G_CALLBACK (on_button_event), NULL);

  g_signal_connect (G_OBJECT (window), "button-press-event",
		    G_CALLBACK (on_button_event), NULL);

  gtk_window_resize (GTK_WINDOW (window), 100, 100);
  gtk_window_move (GTK_WINDOW (window), 200, 200);

  gtk_widget_realize (window);

  gdk_window_set_events (window->window,
			 GDK_BUTTON_PRESS_MASK   |
			 GDK_BUTTON_RELEASE_MASK |
			 GDK_POINTER_MOTION_MASK);

  XChangeProperty (GDK_DISPLAY (), GDK_WINDOW_XID (window->window),
		   wm_type, XA_ATOM, 32, PropModeReplace,
		   (unsigned char *)&applet_type, 1);

  gtk_widget_show_all (window);

  gtk_main();

  return 0;
}
