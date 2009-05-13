
#include "hd-util.h"

#include <matchbox/core/mb-wm.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

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

/* Change the screen's orientation by rotating 90 degrees
 * (portrait mode) or going back to landscape.
 * Returns whether the orientation has actually changed. */
static gboolean
hd_util_change_screen_orientation_real (MBWindowManager *wm,
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

/* To be documented */
static struct
{
  MBWindowManager *wm;

  enum
  {
    GOTO_LANDSCAPE,
    GOTO_PORTRAIT,
  } direction, new_direction;

  enum
  {
    IDLE,
    FADE_OUT,
    WAITING,
    FADE_IN,
  } phase;
} Orientation_change;

static gboolean
hd_util_change_screen_orientation_cb (gpointer unused)
{
  g_debug ("%s: phase=%d, new_direction=%d, direction=%d",
           __FUNCTION__, Orientation_change.phase,
           Orientation_change.new_direction, Orientation_change.direction);
  switch (Orientation_change.phase)
    {
      case IDLE:
        /* Fade to black ((c) Metallica) */
        Orientation_change.phase = FADE_OUT;
        Orientation_change.direction = Orientation_change.new_direction;
        hd_transition_rotate_screen(
                TRUE, Orientation_change.direction == GOTO_PORTRAIT,
                G_CALLBACK(hd_util_change_screen_orientation_cb), NULL);
        break;
      case FADE_OUT:
        if (Orientation_change.direction == Orientation_change.new_direction)
          {
            /*
             * Wait for the screen change. During this period, blank the
             * screen by hiding hd_render_manager. Note that we could wait
             * until redraws have finished here, but currently X blanks us
             * for a set time period anyway - and this way it is easier
             * to get rotation speeds sorted.
             */
            Orientation_change.phase = WAITING;
            clutter_actor_hide(CLUTTER_ACTOR(hd_render_manager_get()));
            hd_util_change_screen_orientation_real(Orientation_change.wm,
                         Orientation_change.direction == GOTO_PORTRAIT);
            g_timeout_add(
              hd_transition_get_int("rotate", "duration_blanking", 500),
              hd_util_change_screen_orientation_cb, NULL);
            break;
          }
        else
          Orientation_change.direction = Orientation_change.new_direction;
        /* Fall through */
      case WAITING:
        if (Orientation_change.direction == Orientation_change.new_direction)
          { /* Fade back in */
            Orientation_change.phase = FADE_IN;
            clutter_actor_show(CLUTTER_ACTOR(hd_render_manager_get()));
            hd_transition_rotate_screen(
                    FALSE, Orientation_change.direction == GOTO_PORTRAIT,
                    G_CALLBACK(hd_util_change_screen_orientation_cb), NULL);
          }
        else
          {
            Orientation_change.direction = Orientation_change.new_direction;
            Orientation_change.phase = FADE_OUT;
            hd_util_change_screen_orientation_cb (NULL);
          }
        break;
      case FADE_IN:
        Orientation_change.phase = IDLE;
        if (Orientation_change.direction != Orientation_change.new_direction)
          hd_util_change_screen_orientation_cb (NULL);
        break;
    }

  return FALSE;
}

/* Start changing the screen's orientation by rotating 90 degrees
 * (portrait mode) or going back to landscape.  Returns FALSE if
 * orientation changing won't take place. */
gboolean
hd_util_change_screen_orientation (MBWindowManager *wm,
                                   gboolean goto_portrait)
{
  g_debug("%s(goto_portrait=%d)", __FUNCTION__, goto_portrait);

  Orientation_change.wm = wm;
  Orientation_change.new_direction = goto_portrait
    ? GOTO_PORTRAIT : GOTO_LANDSCAPE;

  if (Orientation_change.phase == IDLE)
    {
      if (goto_portrait == (HD_COMP_MGR_SCREEN_HEIGHT > HD_COMP_MGR_SCREEN_WIDTH))
        {
          g_warning("%s: already in %s mode", __FUNCTION__,
                    goto_portrait ? "portrait" : "landscape");
          return FALSE;
        }
      hd_util_change_screen_orientation_cb(NULL);
    }
  else
    g_debug ("divert");

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
