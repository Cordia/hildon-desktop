/* Simple test program that does not depend on Gtk/Gdk, to show how
 * fullscreen etc. properties can be set. */

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <X11/extensions/Xrender.h>

static void set_fullscreen (Display *dpy, Window w)
{
  Atom wm_state, state_fs;

  wm_state = XInternAtom (dpy, "_NET_WM_STATE", False);
  state_fs = XInternAtom (dpy, "_NET_WM_STATE_FULLSCREEN", False);

  XChangeProperty (dpy, w, wm_state,
                   XA_ATOM, 32, PropModeReplace,
                   (unsigned char *) &state_fs, 1);
}

static void set_live_bg (Display *display, Window xwindow, int mode)
{
        Atom atom;

        atom = XInternAtom (display, "_HILDON_LIVE_DESKTOP_BACKGROUND", False);
        fprintf (stderr, "XID: 0x%x\n", (unsigned)xwindow);

        XChangeProperty (display,
                       xwindow,
                       atom,
                       XA_INTEGER, 32, PropModeReplace,
                       (unsigned char *) &mode, 1);
}

static void draw_rect (Display *dpy, Window w, GC gc, XColor *col,
                       int x, int y)
{
  XSetForeground (dpy, gc, col->pixel);
  XFillRectangle (dpy, w, gc, x, y, 10, 10);
}

/*
static void set_no_transitions (Display *dpy, Window w)
{
  Atom no_trans;
  int one = 1;

  no_trans = XInternAtom (dpy, "_HILDON_WM_ACTION_NO_TRANSITIONS", False);

  XChangeProperty (dpy, w, no_trans,
                   XA_CARDINAL, 32, PropModeReplace,
                   (unsigned char *)&one, 1);
}
*/

static void set_window_type (Display *dpy, Window w)
{
  Atom w_type, normal;

  w_type = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE", False);
  normal = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);

  XChangeProperty (dpy, w, w_type,
                   XA_ATOM, 32, PropModeReplace,
                   (unsigned char *) &normal, 1);
}

static void set_non_compositing (Display *display, Window xwindow)
{
        Atom atom;
        int zero = 0;

        atom = XInternAtom (display, "_HILDON_NON_COMPOSITED_WINDOW", False);

        XChangeProperty (display,
                       xwindow,
                       atom,
                       XA_INTEGER, 32, PropModeReplace,
                       (unsigned char *) &zero, 1);
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

static Visual*
get_argb32_visual (Display *dpy)
{
  XVisualInfo         * xvi;
  XVisualInfo           template;
  int                   nvi;
  int                   i;
  XRenderPictFormat   * format;
  Visual              * visual = NULL;

  template.depth  = 32;
  template.class  = TrueColor;

  if ((xvi = XGetVisualInfo (dpy,
                             VisualDepthMask|VisualClassMask,
                             &template,
                             &nvi)) == NULL)
    return NULL;
  
  for (i = 0; i < nvi; i++) 
    {
      format = XRenderFindVisualFormat (dpy, xvi[i].visual);
      if (format->type == PictTypeDirect && format->direct.alphaMask)
        {
          printf("%s: ARGB visual found\n", __func__);
          visual = xvi[i].visual;
          break;
        }
    }

  XFree (xvi);
  return visual;
}

int main(int argc, char **argv)
{
        Display *dpy;
        Window w;
        GC green_gc;
        XColor green_col;
        Colormap colormap;
        char green[] = "#00ff00";
        time_t last_time;
        int mode = 1;

        if (argc == 2)
          mode = atoi(argv[1]);

        dpy = XOpenDisplay(NULL);

        if (mode > 100 || mode < -100) {
          /* use ARGB window */
          XSetWindowAttributes attrs;
          Visual *visual;

          visual = get_argb32_visual (dpy);
          colormap = XCreateColormap (dpy, DefaultRootWindow (dpy),
                                      visual, AllocNone);
          attrs.colormap = colormap;
          attrs.border_pixel = BlackPixel (dpy, 0);
          w = XCreateWindow(dpy, DefaultRootWindow (dpy), 0, 0,
                            800, 480, 0, 32,
                            InputOutput,
                            visual,
                            CWColormap | CWBorderPixel, &attrs);
        } else {
          w = XCreateWindow(dpy, DefaultRootWindow (dpy), 0, 0,
                            800, 480, 0, CopyFromParent, InputOutput,
                            CopyFromParent,
                            0, NULL);
          XSetWindowBackground (dpy, w, BlackPixel (dpy, 0));
          colormap = DefaultColormap (dpy, 0);
        }

        set_non_compositing(dpy, w);

        green_gc = XCreateGC (dpy, w, 0, NULL);
        XParseColor (dpy, colormap, green, &green_col);
        XAllocColor (dpy, colormap, &green_col);

        set_fullscreen(dpy, w);
        set_window_type(dpy, w);

        set_live_bg (dpy, w, mode);

        /* optional: disable compositor's transitions for this window */
        //set_no_transitions(dpy, w);

        XSelectInput (dpy, w,
                      ExposureMask | ButtonReleaseMask | ButtonPressMask);
        XMapWindow(dpy, w);  /* map the window */
        last_time = time(NULL);

        for (;;) {
                XEvent xev;

                if (XEventsQueued (dpy, QueuedAfterFlush))
                  XNextEvent(dpy, &xev);
                else {
                  int t = time(NULL);
                  if (t - last_time > 0) {
                    unsigned int rx = rand() * 1000 % 800 + 1,
                                 ry = rand() * 1000 % 480 + 1;
                    draw_rect (dpy, w, green_gc, &green_col, rx, ry);
                    last_time = t;
                  }
                  usleep (300000);
                  continue;
                }

                if (xev.type == Expose) {
                  printf("expose\n");

                  if (mode > 100 || mode < -100) {
                    /* draw background with transparent colour */
                    XImage ximage;
                    ximage.width = 800;
                    ximage.height = 480;
                    ximage.format = ZPixmap;
                    ximage.byte_order = LSBFirst;
                    ximage.bitmap_unit = 32;
                    ximage.bitmap_bit_order = LSBFirst;
                    ximage.bitmap_pad = 32;
                    ximage.depth = 32;
                    ximage.red_mask = 0;
                    ximage.green_mask = 0;
                    ximage.blue_mask = 0;
                    ximage.xoffset = 0;
                    ximage.bits_per_pixel = 32;
                    ximage.bytes_per_line = 800 * 4;
                    ximage.data = calloc (1, 800 * 480 * 4);

                    XInitImage (&ximage);

                    XPutImage (dpy, w, green_gc, &ximage, 0, 0, 0, 0,
                               800, 480);
                    free (ximage.data);
                  }
                  draw_rect (dpy, w, green_gc, &green_col, 100, 100);
                }
                else if (xev.type == ButtonRelease) {
                  XButtonEvent *e = (XButtonEvent*)&xev;
                  draw_rect (dpy, w, green_gc, &green_col, e->x, e->y);
                }
        }

        return 0;
}


