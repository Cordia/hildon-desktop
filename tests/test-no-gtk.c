/* Simple test program that does not depend on Gtk/Gdk, to show how
 * fullscreen etc. properties can be set. */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

static void set_non_compositing (Display *display, Window xwindow)
{
        Atom atom;
        int one = 1;

        atom = XInternAtom (display, "_HILDON_NON_COMPOSITED_WINDOW", False);

        XChangeProperty (display,
                       xwindow,
                       atom,
                       XA_INTEGER, 32, PropModeReplace,
                       (unsigned char *) &one, 1);
}

static void set_fullscreen (Display *dpy, Window w)
{
  Atom wm_state, state_fs;

  wm_state = XInternAtom (dpy, "_NET_WM_STATE", False);
  state_fs = XInternAtom (dpy, "_NET_WM_STATE_FULLSCREEN", False);

  XChangeProperty (dpy, w, wm_state,
                   XA_ATOM, 32, PropModeReplace,
                   (unsigned char *) &state_fs, 1);
}

static void set_no_transitions (Display *dpy, Window w)
{
  Atom no_trans;
  int one = 1;

  no_trans = XInternAtom (dpy, "_HILDON_WM_ACTION_NO_TRANSITIONS", False);

  XChangeProperty (dpy, w, no_trans,
                   XA_CARDINAL, 32, PropModeReplace,
                   (unsigned char *)&one, 1);
}

static void raise_window (Display *dpy, Window w)
{
  XClientMessageEvent xclient;

  printf ("%s: window %lx\n", __func__, w);
  memset (&xclient, 0, sizeof (xclient));
  xclient.type = ClientMessage;
  xclient.window = w;
  xclient.message_type = XInternAtom (dpy, "_NET_ACTIVE_WINDOW", False);
  xclient.format = 32;
  xclient.data.l[0] = 1; /* requestor type; we're an app */
  xclient.data.l[1] = CurrentTime; /* should use a proper timestamp here */
  xclient.data.l[2] = None; /* currently active window */
  xclient.data.l[3] = 0;
  xclient.data.l[4] = 0;
      
  XSendEvent (dpy, DefaultRootWindow (dpy), False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              (XEvent *)&xclient);
}

static void set_window_type (Display *dpy, Window w)
{
  Atom w_type, normal;

  w_type = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE", False);
  normal = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);

  XChangeProperty (dpy, w, w_type,
                   XA_ATOM, 32, PropModeReplace,
                   (unsigned char *) &normal, 1);
}

/* this can be used to set fullscreenness after mapping the window */
#if 0
static void set_fullscreen (Display *display, Window xwindow)
{
        XClientMessageEvent xclient;
  
#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */  
  
  memset (&xclient, 0, sizeof (xclient));
  xclient.type = ClientMessage;
  xclient.window = xwindow;
  xclient.message_type = XInternAtom (display, "_NET_WM_STATE", False);
  xclient.format = 32;
  xclient.data.l[0] = _NET_WM_STATE_ADD;
  xclient.data.l[1] = XInternAtom (display, "_NET_WM_STATE_FULLSCREEN", False);
  xclient.data.l[2] = None;
  xclient.data.l[3] = 0;
  xclient.data.l[4] = 0;
  
  XSendEvent (display, DefaultRootWindow (display), False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              (XEvent *)&xclient);
}
#endif

int main(int argc, char **argv)
{
        Display *dpy;
        Window w;
        XWMHints *wmhints;
        time_t start_time;
        int raised = 0;

        /* TODO: X error handling is missing */

        dpy = XOpenDisplay(NULL);
        w = XCreateSimpleWindow(dpy, DefaultRootWindow (dpy), 0, 0,
                                800, 480, 0, 0, 0);

        set_fullscreen(dpy, w);
        set_window_type(dpy, w);

        /* tell the window manager that we want keyboard focus */
        wmhints = XAllocWMHints();
        wmhints->input = True;
        wmhints->flags = InputHint;
        XSetWMHints(dpy, w, wmhints);
        XFree(wmhints);

        /* optional: disable compositing for this window to speed up */
        set_non_compositing(dpy, w);

        /* optional: disable compositor's transitions for this window */
        set_no_transitions(dpy, w);

        XMapWindow(dpy, w);  /* map the window */

        start_time = time(NULL);

        /* listen to property changes just to receive some X events
         * for the timer */
        XSelectInput (dpy, DefaultRootWindow (dpy), PropertyChangeMask);

        for (;;) {
                XEvent xev;

                /* useless main loop that just reads incoming X events */
                XNextEvent(dpy, &xev);

                if (!raised && time(NULL) - start_time > 10)
                  {
                    raised = 1;
                    raise_window(dpy, w);
                  }
        }

        return 0;
}


