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


/* Reading position of applets. */

static Atom win_type_atom, on_current_desktop_atom;

static char *get_atom_prop(Display *dpy, Window w, Atom atom)
{ 
      Atom type;
      int format, rc;
      unsigned long items;
      unsigned long left;
	  unsigned char *value; // will point to Atom, actually
  	  char *copy;

      rc = XGetWindowProperty (dpy, w, atom, 0, 1, False,
                          XA_ATOM, &type, &format,
                          &items, &left, &value);
      if (type != XA_ATOM || format == 0 || rc != Success) {
	    copy = strdup("");
	  } else {
	    char *s = XGetAtomName(dpy, *((Atom*)value));
	    copy = strdup(s);
	    XFree(s);
      }
      return copy;
}

static unsigned long get_card_prop(Display *dpy, Window w, Atom atom)
{ 
      Atom type;
      int format, rc;
      unsigned long items;
      unsigned long left;
      unsigned char *value; // will point to 'unsigned long', actually

      rc = XGetWindowProperty (dpy, w, atom, 0, 1, False,
                          XA_CARDINAL, &type, &format,
                          &items, &left, &value);
      if (type == None || rc != Success)
        return 0;
      else
      {
        return *value;
      }
}

static void show_winbox(Display *dpy, Window w, Window drawwin, GC gc,
						XColor *col)
{
	Window qroot;
	unsigned int bw, d, width, height;
	int x, y;
	int on_desktop = get_card_prop(dpy, w, on_current_desktop_atom);
	XGetGeometry(dpy, w, &qroot, &x, &y, &width, &height, &bw, &d);
	printf(
		"Applet %lu at x=%3d y=%3d w=%3d h=%3d, on current desktop - %s\n",
		w, x, y, width, height,	(on_desktop? "YES": "NO"));
	if (on_desktop)
	{
    	XSetForeground (dpy, gc, col->pixel);
    	XFillRectangle (dpy, drawwin, gc, x, y, width, height);
	}
}

static int is_applet(Display *dpy, Window w)
{
	char *wmtype;
	int ret = 0;
	wmtype = get_atom_prop(dpy, w, win_type_atom);
	if (!strcmp(wmtype, "_HILDON_WM_WINDOW_TYPE_HOME_APPLET"))
	{
		ret = 1;
	}
	free(wmtype);
	return ret;
}

static void check_all_windows(Display *dpy, Window w, Window drawwin, GC gc, 
							  XColor *col)
{
	unsigned int n_children = 0;
	Window *child_l = NULL;
	Window root_ret, parent_ret;
	unsigned int i;

	XQueryTree(dpy, w, &root_ret, &parent_ret, &child_l, &n_children);

	if (is_applet(dpy, w))
	{
		XSelectInput(dpy, w, PropertyChangeMask); // listen for changes
		show_winbox(dpy, w, drawwin, gc, col);
	}
	for (i = 0; i < n_children; ++i) {
		check_all_windows(dpy, child_l[i], drawwin, gc, col); // do recursive
	}
	XFree(child_l);
}

static void read_applet_positions(Display *dpy, Window drawwin, GC gc, 
								  XColor *col)
{
    win_type_atom = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    on_current_desktop_atom = XInternAtom(dpy, 
								"_HILDON_APPLET_ON_CURRENT_DESKTOP",
                                False);
	printf("READING APPLET POSITIONS\n");
	check_all_windows(dpy, XDefaultRootWindow(dpy), drawwin, gc, col);
	printf("DONE\n");
}

/* End of applets position reading. */

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
        GC green_gc, red_gc;
        XColor green_col, red_col;
        Colormap colormap;
        char green[] = "#00ff00";
        char red[] = "#ff0000";
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

        red_gc = XCreateGC (dpy, w, 0, NULL);
        XParseColor (dpy, colormap, red, &red_col);
        XAllocColor (dpy, colormap, &red_col);

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
                  printf("Expose\n");

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
			      read_applet_positions(dpy, w, red_gc, &red_col);
                }
				else if (xev.type == PropertyNotify) {
                  printf("PropertyNotify\n");
				  if (is_applet(dpy, xev.xproperty.window)) {
					char color[16];
					sprintf(color, "#%02x0000",
						(unsigned int)(xev.xproperty.window & 0xff)/2+64);
					XParseColor (dpy, colormap, color, &red_col);
					XAllocColor (dpy, colormap, &red_col);
			      	show_winbox(dpy, xev.xproperty.window, w,
						red_gc, &red_col);
				  }
				}
                else if (xev.type == ButtonRelease) {
                  XButtonEvent *e = (XButtonEvent*)&xev;
                  draw_rect (dpy, w, green_gc, &green_col, e->x, e->y);
                }
        }

        return 0;
}


