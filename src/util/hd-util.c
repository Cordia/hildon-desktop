
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

static Bool
hd_util_modal_blocker_release_handler (XButtonEvent    *xev,
                                       void            *userdata)
{
  MBWindowManagerClient *c = userdata;

  g_debug ("%s: c %p", __FUNCTION__, c);
  mb_wm_client_deliver_delete (c);
  return False;
}

/* Creates a fullscreen modal blocker window for @client that closes it
 * when clicked.  Returns a matchbox callback ID you should deregister
 * when @client is destroyed. */
unsigned long
hd_util_modal_blocker_realize(MBWindowManagerClient *client)
{
  MBWindowManager *wm = client->wmref;

  /* Movedd from hd-dialog.c */
  if (!client->xwin_modal_blocker)
    {
      XSetWindowAttributes   attr;

      attr.override_redirect = True;
      attr.event_mask        = MBWMChildMask|ButtonPressMask|ButtonReleaseMask|
	                       ExposureMask;

      client->xwin_modal_blocker =
	XCreateWindow (wm->xdpy,
		       wm->root_win->xwindow,
		       0, 0,
		       wm->xdpy_width,
		       wm->xdpy_height,
		       0,
		       CopyFromParent,
		       InputOnly,
		       CopyFromParent,
		       CWOverrideRedirect|CWEventMask,
		       &attr);
      mb_wm_rename_window (wm, client->xwin_modal_blocker, "hdmodalblocker");
      g_debug ("%s: created modal blocker %lx", __FUNCTION__, client->xwin_modal_blocker);
    }
  else
    {
      /* make sure ButtonRelease is caught */
      XWindowAttributes attrs = { 0 };
      XGetWindowAttributes (wm->xdpy, client->xwin_modal_blocker, &attrs);
      attrs.your_event_mask |= ButtonReleaseMask;
      XSelectInput (wm->xdpy, client->xwin_modal_blocker, attrs.your_event_mask);
    }

    return mb_wm_main_context_x_event_handler_add (wm->main_ctx,
                  client->xwin_modal_blocker,
                  ButtonRelease,
                  (MBWMXEventFunc)hd_util_modal_blocker_release_handler,
                  client);
}

Bool
hd_util_is_client_system_modal (MBWindowManagerClient *c)
{
  return mb_wm_client_is_modal (c) &&
      !mb_wm_client_get_transient_for (c) &&
      mb_wm_get_modality_type (c->wmref) == MBWMModalitySystem;
}
