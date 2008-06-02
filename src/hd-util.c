
#include "hd-util.h"

#include <matchbox/core/mb-wm.h>

#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>

void *
hd_util_get_win_prop_data_and_validate (Display   *xdpy,
					Window     xwin,
					Atom       prop,
					Atom       type,
					gint       expected_format,
					gint       expected_n_items,
					gint      *n_items_ret)
{
  Atom           type_ret;
  int            format_ret;
  unsigned long  items_ret;
  unsigned long  after_ret;
  unsigned char *prop_data;
  int            status;

  prop_data = NULL;

  mb_wm_util_trap_x_errors ();

  status = XGetWindowProperty (xdpy,
			       xwin,
			       prop,
			       0, G_MAXLONG,
			       False,
			       type,
			       &type_ret,
			       &format_ret,
			       &items_ret,
			       &after_ret,
			       &prop_data);


  if (mb_wm_util_untrap_x_errors () || status != Success || prop_data == NULL)
    goto fail;

  if (expected_format && format_ret != expected_format)
    goto fail;

  if (expected_n_items && items_ret != expected_n_items)
    goto fail;

  if (n_items_ret)
    *n_items_ret = items_ret;

  return prop_data;

 fail:

  if (prop_data)
    XFree(prop_data);

  return NULL;
}

/*
 * The following function is based on XTestFakeButtonEvent from libxtst6.
 *
 * Copyright 1990, 1991 by UniSoft Group Limited
 * Copyright 1992, 1993, 1998  The Open Group
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of The Open Group shall
 * not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization
 * from The Open Group.
 *
 */

#define NEED_REPLIES
#include <X11/Xlibint.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/xteststr.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/extutil.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XIproto.h>

static int close_display (Display *dpy, XExtCodes *codes);

static XExtensionInfo  _xtest_info_data;
static XExtensionInfo *xtest_info = &_xtest_info_data;
static char           *xtest_extension_name = XTestExtensionName;
static XExtensionHooks xtest_extension_hooks =
  {
    NULL,				/* create_gc */
    NULL,				/* copy_gc */
    NULL,				/* flush_gc */
    NULL,				/* free_gc */
    NULL,				/* create_font */
    NULL,				/* free_font */
    close_display,			/* close_display */
    NULL,				/* wire_to_event */
    NULL,				/* event_to_wire */
    NULL,				/* error */
    NULL				/* error_string */
};

static XPointer
get_xinput_base(Display *dpy)
{
    int major_opcode, first_event, first_error;
    first_event = 0;

    XQueryExtension(dpy, INAME, &major_opcode, &first_event, &first_error);
    return (XPointer)(long)first_event;
}

#define XTestCheckExtension(dpy,i,val) \
  XextCheckExtension (dpy, i, xtest_extension_name, val)

#define XTestICheckExtension(dpy,i,val) \
  XextCheckExtension (dpy, i, xtest_extension_name, val); \
  if (!i->data) return val

static XEXT_GENERATE_FIND_DISPLAY (find_display, xtest_info,
				   xtest_extension_name,
				   &xtest_extension_hooks, XTestNumberEvents,
				   get_xinput_base(dpy))

static XEXT_GENERATE_CLOSE_DISPLAY (close_display, xtest_info)

void
hd_util_fake_button_event (Display *dpy,
			   guint    button,
			   gboolean is_press,
			   gint16   x,
			   gint16   y)
{
    XExtDisplayInfo *info = find_display (dpy);
    xXTestFakeInputReq *req;

    LockDisplay(dpy);
    GetReq(XTestFakeInput, req);
    req->reqType = info->codes->major_opcode;
    req->xtReqType = X_XTestFakeInput;
    req->type = is_press ? ButtonPress : ButtonRelease;
    req->detail = button;
    req->time = CurrentTime;
    req->rootX = x;
    req->rootY = y;
    UnlockDisplay(dpy);
    SyncHandle();
}

gint
hd_util_grab_pointer ()
{
  ClutterActor  *stage = clutter_stage_get_default();
  Window         clutter_window;
  Display       *dpy = clutter_x11_get_default_display ();
  gint           status;

  clutter_window = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));

  status = XGrabPointer (dpy,
			 clutter_window,
			 False,
			 ButtonPressMask   |
			 ButtonReleaseMask |
			 PointerMotionMask,
			 GrabModeAsync,
			 GrabModeAsync,
			 None,
			 None,
			 CurrentTime);

  g_debug ("Doing pointer grab (status %d)!!!", status);

  return status;
}

void
hd_util_ungrab_pointer ()
{
  Display * dpy = clutter_x11_get_default_display ();

  g_debug ("Doing pointer ungrab !!!");

  XUngrabPointer (dpy, CurrentTime);
}
