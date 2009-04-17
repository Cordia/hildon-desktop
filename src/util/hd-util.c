
#include "hd-util.h"

#include <matchbox/core/mb-wm.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "hd-comp-mgr.h"
#include "hd-wm.h"
#include "hd-note.h"

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

static void
hd_util_modal_blocker_release_handler (XButtonEvent    *xev,
                                       void            *userdata)
{
  MBWindowManagerClient *c = userdata;

  g_debug ("%s: c %p", __FUNCTION__, c);
  mb_wm_client_deliver_delete (c);
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

      /* Create a WIDTHxWIDTH large blocker because we may enter
       * portrait mode unexpectedly. */
      client->xwin_modal_blocker =
	XCreateWindow (wm->xdpy,
		       wm->root_win->xwindow,
		       0, 0,
                       HD_COMP_MGR_LANDSCAPE_WIDTH,
                       HD_COMP_MGR_LANDSCAPE_WIDTH,
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
hd_util_client_has_modal_blocker (MBWindowManagerClient *c)
{
  /* This is *almost* a system modal check, but we actually
   * care if the client has modal blocker that means that
   * h-d shouldn't allow the top-left buttons to work.
   *
   * Other clients exist that are not transient to anything
   * but are not system modal (for example the Status Area)
   */
  MBWMClientType c_type = MB_WM_CLIENT_CLIENT_TYPE(c);
  return
      ((c_type == MBWMClientTypeDialog) ||
       (c_type == MBWMClientTypeMenu) ||
       (c_type == HdWmClientTypeAppMenu) ||
       (c_type == HdWmClientTypeStatusMenu) ||
       (c_type == MBWMClientTypeNote &&
        HD_NOTE (c)->note_type != HdNoteTypeIncomingEvent &&
        HD_NOTE (c)->note_type != HdNoteTypeBanner)) &&
      !mb_wm_client_get_transient_for (c) &&
      mb_wm_get_modality_type (c->wmref) == MBWMModalitySystem;
}

/* Change the screen's orientation by rotating 90 degrees
 * (portrait mode) or going back to landscape.
 * Returns whether the orientation has actually changed. */
gboolean
hd_util_change_screen_orientation (MBWindowManager *wm,
                                   gboolean goto_portrait)
{
  Status ret;
  Time cfgtime;
  SizeID sizeid;
  Rotation current, want;
  XRRScreenConfiguration *scrcfg;
  static Rotation can;

  if (goto_portrait)
    {
      g_debug ("Entering portrait mode");
      want = RR_Rotate_90;
    }
  else
    {
      g_debug ("Leaving portrait mode");
      want = RR_Rotate_0;
    }

  if (can == 0)
    can = XRRRotations (wm->xdpy, 0, &current);

  if (!(can & want))
    {
      g_warning ("Server is incapable (0x%.8X vs. 0x%.8X)", can, want);
      return FALSE;
    }

  scrcfg = XRRGetScreenInfo(wm->xdpy, wm->root_win->xwindow);
  XRRConfigTimes(scrcfg, &cfgtime);
  sizeid = XRRConfigCurrentConfiguration(scrcfg, &current);
  if (current == want)
    {
      g_warning ("Already in that mode");
      return FALSE;
    }

  ret = XRRSetScreenConfig(wm->xdpy, scrcfg, wm->root_win->xwindow, sizeid,
                           want, cfgtime);
  XRRFreeScreenConfigInfo(scrcfg);

  if (ret != Success)
    {
      g_warning("XRRSetScreenConfig() failed: %d", ret);
      return FALSE;
    }
  else
    return TRUE;
}
