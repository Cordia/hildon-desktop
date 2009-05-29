
#include "hd-util.h"

#include <matchbox/core/mb-wm.h>
#include <clutter/x11/clutter-x11.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>

#include "hd-comp-mgr.h"
#include "hd-wm.h"
#include "hd-note.h"
#include "hd-transition.h"
#include "hd-render-manager.h"

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

/* Returns the value of a #HdWm string property of @xwin or %NULL
 * if the window doesn't have such property or it can't be retrieved.
 * If the return value is not %NULL it must be XFree()d by the caller. */
char *
hd_util_get_x_window_string_property (MBWindowManager *wm, Window xwin,
                                      HdAtoms atom_id)
{
  Atom type;
  int format, ret;
  unsigned char *value;
  unsigned long items, left;

  /* The return @type is %None if the property is missing. */
  ret = XGetWindowProperty (wm->xdpy, xwin,
                            hd_comp_mgr_get_atom (HD_COMP_MGR (wm->comp_mgr),
                                                  atom_id),
                            0, 999, False, XA_STRING, &type, &format,
                            &items, &left, &value);
  if (ret != Success)
    g_warning ("%s: XGetWindowProperty(0x%lx, 0x%x): failed (%d)",
               __FUNCTION__, xwin, atom_id, ret);
  return ret != Success || type == None ? NULL : (char *)value;
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

/* Queries the current input viewport and flips the coordinates
 * of the rectangles. */
static void
hd_util_flip_input_viewport (MBWindowManager *wm)
{
  Window clwin;
  XserverRegion region;
  XRectangle *inputshape;
  int i, ninputshapes, unused;

  clwin = clutter_x11_get_stage_window (
                          CLUTTER_STAGE (clutter_stage_get_default ()));
  inputshape = XShapeGetRectangles(wm->xdpy, clwin,
                                   ShapeInput, &ninputshapes, &unused);
  for (i = 0; i < ninputshapes; i++)
    {
      XRectangle tmp;

      tmp = inputshape[i];
      inputshape[i].x = tmp.y;
      inputshape[i].y = tmp.x;
      inputshape[i].width = tmp.height;
      inputshape[i].height = tmp.width;
    }

  region = XFixesCreateRegion (wm->xdpy, inputshape, ninputshapes);
  XFree(inputshape);

  hd_comp_mgr_set_input_viewport_for_window (wm->xdpy,
    XCompositeGetOverlayWindow (wm->xdpy, wm->root_win->xwindow),
    region);
  hd_comp_mgr_set_input_viewport_for_window (wm->xdpy, clwin, region);
  XFixesDestroyRegion (wm->xdpy, region);
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

  /* Maybe we needn't bother with errors. */
  mb_wm_util_trap_x_errors ();
  hd_util_flip_input_viewport (wm);
  mb_wm_util_untrap_x_errors ();

  return TRUE;
}

/* Get the current cursor position, return true (and updates the parameters)
 * on success, otherwise leaves them as they were */
gboolean hd_util_get_cursor_position(gint *x, gint *y)
{
  Window root, child;
  int root_x, root_y;
  int pos_x, pos_y;
  unsigned int keys_buttons;
  MBWindowManager *wm = mb_wm_root_window_get (NULL)->wm;

  if (!wm->root_win)
    return FALSE;

  if (!XQueryPointer(wm->xdpy, wm->root_win->xwindow, &root, &child, &root_x, &root_y,
      &pos_x, &pos_y, &keys_buttons))
    return FALSE;

  *x = pos_x;
  *y = pos_y;
  return TRUE;
}
