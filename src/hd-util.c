
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

  g_debug ("Doing pointer grab on 0x%x (status %d)!!!",
	   (unsigned int) clutter_window, status);

  return status;
}

gint
hd_util_ungrab_pointer ()
{
  Display * dpy = clutter_x11_get_default_display ();
  gint      status;


  status = XUngrabPointer (dpy, CurrentTime);

  g_debug ("Doing pointer ungrab (status %d)!!!", status);

  return status;
}
