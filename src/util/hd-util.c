
#include "hd-util.h"

#include <matchbox/core/mb-wm.h>
#include <clutter/x11/clutter-x11.h>
#include <cogl/cogl.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>

#include "hd-comp-mgr.h"
#include "hd-wm.h"
#include "hd-note.h"
#include "hd-transition.h"
#include "hd-render-manager.h"

#include <gdk/gdk.h>

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

  /* We don't care about X errors here, because they will be reported
   * in the return value. */
  mb_wm_util_async_trap_x_errors (xdpy);

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

  mb_wm_util_async_untrap_x_errors ();

  if (status != Success || prop_data == NULL)
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

  g_debug ("%s: c %p\n", __FUNCTION__, c);
  if (mb_wm_client_is_map_confirmed (c) && c->cm_client)
    {
      ClutterActor *actor = mb_wm_comp_mgr_clutter_client_get_actor(
                               MB_WM_COMP_MGR_CLUTTER_CLIENT(c->cm_client));
      if (actor)
        {
          int x, y;
          unsigned int h, w;

          w = h = x = y = 0;
          clutter_actor_get_size (actor, &w, &h);
          clutter_actor_get_position (actor, &x, &y);

          if (xev->x < x || xev->x > x + w || xev->y < y || xev->y > y + h)
            mb_wm_client_deliver_delete (c);
          else
            g_debug ("%s: ignoring ButtonRelease because "
                     "it happened on top of the window\n", __func__);
        }
    }
  else
    g_debug ("%s: ignoring ButtonRelease because "
             "window for this blocker is not mapped yet\n", __func__);
}

static void
hd_util_modal_blocker_release_handler_for_ping (XButtonEvent    *xev,
                                                void            *userdata)
{
  MBWindowManagerClient *c = userdata;

  g_debug ("%s: c %p", __FUNCTION__, c);
  mb_wm_client_ping_start (c);
}

/* Creates a fullscreen modal blocker window for @client that closes it
 * when clicked.  Returns a matchbox callback ID you should deregister
 * when @client is destroyed. */
unsigned long
hd_util_modal_blocker_realize(MBWindowManagerClient *client,
                              gboolean ping_only)
{
  MBWindowManager *wm = client->wmref;
  unsigned long handler;

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

  if (ping_only)
    handler = mb_wm_main_context_x_event_handler_add (wm->main_ctx,
                  client->xwin_modal_blocker,
                  ButtonRelease,
                  (MBWMXEventFunc)
                  hd_util_modal_blocker_release_handler_for_ping,
                  client);
  else
    handler = mb_wm_main_context_x_event_handler_add (wm->main_ctx,
                  client->xwin_modal_blocker,
                  ButtonRelease,
                  (MBWMXEventFunc)hd_util_modal_blocker_release_handler,
                  client);
  return handler;
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
        HD_NOTE (c)->note_type != HdNoteTypeIncomingEventPreview &&
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
  Window win, clwin;
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

  win = mb_wm_comp_mgr_clutter_get_overlay_window(
              MB_WM_COMP_MGR_CLUTTER(wm->comp_mgr));
  if (win != None)
    hd_comp_mgr_set_input_viewport_for_window (wm->xdpy, win, region);
  hd_comp_mgr_set_input_viewport_for_window (wm->xdpy, clwin, region);
  XFixesDestroyRegion (wm->xdpy, region);
}

static RRCrtc
get_primary_crtc (MBWindowManager *wm, XRRScreenResources *res)
{
  int i;
  XRROutputInfo *output;
  RRCrtc ret = ~0UL;
  Atom rr_connector_type, rr_connector_panel;
  unsigned char *contype;
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;

  if (res->ncrtc == 1)
    return res->crtcs[0];

  rr_connector_type = hd_comp_mgr_get_atom (HD_COMP_MGR (wm->comp_mgr),
                                            HD_ATOM_RANDR_CONNECTOR_TYPE);
  rr_connector_panel = hd_comp_mgr_get_atom (HD_COMP_MGR (wm->comp_mgr),
                                             HD_ATOM_RANDR_CONNECTOR_TYPE_PANEL);

  for (i = 0; i < res->noutput; i++)
    {
      output = XRRGetOutputInfo (wm->xdpy, res, res->outputs[i]);
      if (!output)
          continue;

      if (XRRGetOutputProperty (wm->xdpy, res->outputs[i], rr_connector_type,
                                0, 1, False, False, AnyPropertyType, &actual_type,
                                &actual_format, &nitems, &bytes_after,
                                &contype) == Success)
        {
          if (actual_type == XA_ATOM && actual_format == 32 && nitems == 1 &&
              *(Atom *)contype == rr_connector_panel)
            {
              ret = output->crtc;
              XRRFreeOutputInfo (output);
              break;
            }
        }
      XRRFreeOutputInfo (output);
    }

  return ret;
}

/* Set a property on the root window to tell others whether we are doing
 * rotation or not, so they can do certain things like increasing our
 * process priority. */
void hd_util_set_rotating_property(MBWindowManager *wm, gboolean is_rotating)
{
  HdCompMgr *hmgr = HD_COMP_MGR(wm->comp_mgr);
  guint value = is_rotating ? 1 : 0;
  XChangeProperty(wm->xdpy, wm->root_win->xwindow,
      hd_comp_mgr_get_atom (hmgr, HD_ATOM_MAEMO_ROTATION_TRANSITION),
                  XA_CARDINAL, 32, PropModeReplace,
                  (unsigned char *)&value, 1);
}

void hd_util_set_screen_size_properties(MBWindowManager *wm,
                                        guint width, guint height)
{
  long value[2];
  HdCompMgr *hmgr = HD_COMP_MGR(wm->comp_mgr);

  value[0] = width;
  value[1] = height;
  XChangeProperty(wm->xdpy, wm->root_win->hidden_window,
        hd_comp_mgr_get_atom (hmgr, HD_ATOM_MAEMO_SCREEN_SIZE),
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)value, 2);
}

/* Change the screen's orientation by rotating 90 degrees
 * (portrait mode) or going back to landscape.
 * Returns whether the orientation has actually changed. */
gboolean
hd_util_change_screen_orientation (MBWindowManager *wm,
                                   gboolean goto_portrait)
{
  int rr_major, rr_minor;
  static RRCrtc crtc = ~0UL; /* cache to avoid potentially lots of roundtrips */
  static int randr_supported = -1;
  XRRScreenResources *res;
  XRRCrtcInfo *crtc_info;
  Rotation want;
  Status ret;
  int width, height, width_mm, height_mm;
  unsigned long one = 1;

  if (randr_supported == -1)
    {
      ret = XRRQueryVersion (wm->xdpy, &rr_major, &rr_minor);
      if (ret == True && (rr_major > 1 || (rr_major == 1 && rr_minor >= 3)))
          randr_supported = 1;
      else
          randr_supported = 0;
    }
  if (!randr_supported)
    {
      g_debug ("Server does not support RandR 1.3\n");
      return FALSE;
    }

  res = XRRGetScreenResources (wm->xdpy, wm->root_win->xwindow);
  if (!res)
    {
      g_warning ("Couldn't get RandR screen resources\n");
      return FALSE;
    }

  if (crtc == ~0UL)
      crtc = get_primary_crtc (wm, res);
  if (crtc == ~0UL)
    {
      g_warning ("Couldn't find CRTC to rotate\n");
      return FALSE;
    }
  crtc_info = XRRGetCrtcInfo (wm->xdpy, res, crtc);
  if (!crtc_info)
    {
      g_warning ("Couldn't find CRTC info\n");
      return FALSE;
    }

  if (goto_portrait)
    {
      g_debug ("Entering portrait mode");
      want = RR_Rotate_90;
      width = MIN(DisplayWidth (wm->xdpy, DefaultScreen (wm->xdpy)),
		  DisplayHeight (wm->xdpy, DefaultScreen (wm->xdpy)));
      height = MAX(DisplayWidth (wm->xdpy, DefaultScreen (wm->xdpy)),
 		   DisplayHeight (wm->xdpy, DefaultScreen (wm->xdpy)));
      width_mm = MIN(DisplayWidthMM (wm->xdpy, DefaultScreen (wm->xdpy)),
		     DisplayHeightMM (wm->xdpy, DefaultScreen (wm->xdpy)));
      height_mm = MAX(DisplayWidthMM (wm->xdpy, DefaultScreen (wm->xdpy)),
 		      DisplayHeightMM (wm->xdpy, DefaultScreen (wm->xdpy)));
    }
  else
    {
      g_debug ("Leaving portrait mode");
      want = RR_Rotate_0;
      width = MAX(DisplayWidth (wm->xdpy, DefaultScreen (wm->xdpy)),
		  DisplayHeight (wm->xdpy, DefaultScreen (wm->xdpy)));
      height = MIN(DisplayWidth (wm->xdpy, DefaultScreen (wm->xdpy)),
		   DisplayHeight (wm->xdpy, DefaultScreen (wm->xdpy)));
      width_mm = MAX(DisplayWidthMM (wm->xdpy, DefaultScreen (wm->xdpy)),
		     DisplayHeightMM (wm->xdpy, DefaultScreen (wm->xdpy)));
      height_mm = MIN(DisplayWidthMM (wm->xdpy, DefaultScreen (wm->xdpy)),
 		      DisplayHeightMM (wm->xdpy, DefaultScreen (wm->xdpy)));
    }

  if (!(crtc_info->rotations & want))
    {
      g_warning ("CRTC does not support rotation (0x%.8X vs. 0x%.8X)",
		 crtc_info->rotations, want);
      return FALSE;
    }

  if (crtc_info->rotation == want)
    {
      g_debug ("Requested rotation already active");
      return FALSE;
    }

  /* We must call glFinish here in order to be sure that OpenGL won't be
   * trying to render stuff while we do the transition - as this sometimes
   * causes rubbish to be displayed. */
  glFinish();

  /* Grab the server around rotation to prevent clients attempting to
   * draw at inopportune times. */
  XGrabServer (wm->xdpy);
  /* Stop windows being reconfigured */
  XChangeProperty(wm->xdpy, wm->root_win->xwindow,
                  wm->atoms[MBWM_ATOM_MAEMO_SUPPRESS_ROOT_RECONFIGURATION],
                  XA_CARDINAL, 32, PropModeReplace,
                  (unsigned char *)&one, 1);

  /* Disable the CRTC first, as it doesn't fit within our existing screen. */
  XRRSetCrtcConfig (wm->xdpy, res, crtc, crtc_info->timestamp, 0, 0, None,
		    RR_Rotate_0, NULL, 0);
  /* Then change the screen size to accommodate our glorious new CRTC. */
  XRRSetScreenSize (wm->xdpy, wm->root_win->xwindow, width, height,
		    width_mm, height_mm);
  /* And now rotate. */
  ret = XRRSetCrtcConfig (wm->xdpy, res, crtc, crtc_info->timestamp,
                          crtc_info->x, crtc_info->y, crtc_info->mode, want,
                          crtc_info->outputs, crtc_info->noutput);

  /* hd_util_root_window_configured will be called directly after this root
   * window has been reconfigured */

  /* Allow clients to redraw. */
  XUngrabServer (wm->xdpy);

  XRRFreeCrtcInfo (crtc_info);
  XRRFreeScreenResources (res);

  if (ret != Success)
    {
      g_warning ("XRRSetCrtcConfig() failed: %d", ret);
      return FALSE;
    }

  /* Maybe we needn't bother with errors. */
  mb_wm_util_async_trap_x_errors (wm->xdpy);
  hd_util_flip_input_viewport (wm);
  mb_wm_util_async_untrap_x_errors ();

  return TRUE;
}

/* This is the finishing counterpart of hd_util_change_screen_orientation(),
 * whicih must be called after the root window has been reconfigured. */
void
hd_util_root_window_configured(MBWindowManager *wm)
{
  /* Deleting this property allows other windows to be reconfigured again. */
  mb_wm_util_async_trap_x_errors (wm->xdpy);
  XDeleteProperty(wm->xdpy, wm->root_win->xwindow,
                  wm->atoms[MBWM_ATOM_MAEMO_SUPPRESS_ROOT_RECONFIGURATION]);
  XSync(wm->xdpy, False);
  mb_wm_util_async_untrap_x_errors ();
}

/* Map a portrait @geo to landscape screen or vica versa.
 * Returns if it mapped to ladnscape. */
gboolean
hd_util_rotate_geometry (ClutterGeometry *geo, guint scrw, guint scrh)
{
  gint tmp;

  tmp = geo->width;
  geo->width = geo->height;
  geo->height = tmp;

  /* It is very interesting to observe the dualism here. */
  if (scrw > scrh)
    { /* Map from from portrait to landscape. */
      tmp = geo->x;
      geo->x = geo->y;
      geo->y = scrh - (tmp + geo->height);
      return TRUE;
    }
  else
    { /* Map from landscape to portrait. */
      tmp = geo->y;
      geo->y = geo->x;
      geo->x = scrw - (tmp + geo->width);
      return FALSE;
    }
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

gboolean hd_util_client_has_video_overlay(MBWindowManagerClient *client)
{
  MBWindowManager *wm;
  HdCompMgr *hmgr;
  Atom atom;
  Atom actual_type_return;
  int actual_format_return;
  unsigned long nitems_return;
  unsigned long bytes_after_return;
  unsigned char* prop_return = NULL;
  int result = 0;

  wm = client->wmref;
  hmgr = HD_COMP_MGR (wm->comp_mgr);
  atom = hd_comp_mgr_get_atom (hmgr, HD_ATOM_OMAP_VIDEO_OVERLAY);

  mb_wm_util_async_trap_x_errors (wm->xdpy);
  XGetWindowProperty (wm->xdpy, client->window->xwindow,
                      atom,
                      0, G_MAXLONG,
                      False,
                      AnyPropertyType,
                      &actual_type_return,
                      &actual_format_return,
                      &nitems_return,
                      &bytes_after_return,
                      &prop_return);
  if (prop_return)
    {
      result = prop_return[0];
      XFree (prop_return);
    }
  mb_wm_util_async_untrap_x_errors();

  return result;
}

/* Send a synthetic %ButtonPress to @c. */
void
hd_util_click (const MBWindowManagerClient *c)
{
  Window xwin;
  Display *xdpy;
  XButtonEvent ev;

  xwin = c->window->xwindow;
  xdpy = c->wmref->xdpy;

  memset (&ev, 0, sizeof (ev));
  ev.type         = ButtonPress;
  ev.send_event   = True;
  ev.display      = xdpy;
  ev.window       = xwin;
  ev.root         = DefaultRootWindow (xdpy);
  ev.time         = CurrentTime;
  ev.button       = Button1;
  ev.same_screen  = True;

  XSendEvent(xdpy, xwin, False, ButtonPressMask, (XEvent *)&ev);
}

/* Try and get the translated bounds for an actor (the actual pixel position
 * of it on the screen). If geo is 0 or width/height are 0, this func will
 * use the full bounds of the actor. Otherwise we translate the bounds given
 * in geo (eg. for updating an area of an actor). Returns false if it failed
 * (because the actor or its parents were rotated) */
static gboolean
hd_util_get_actor_bounds(ClutterActor *actor, ClutterGeometry *geo, gboolean *is_visible)
{
  gdouble x, y;
  gdouble width, height;
  ClutterActor *it = actor;
  ClutterActor *stage = clutter_actor_get_stage(actor);
  gboolean visible = TRUE;
  gboolean valid = TRUE;

  if (geo && geo->width && geo->height)
    {
      x = geo->x;
      y = geo->y;
      width = geo->width;
      height = geo->height;
    }
  else
    {
      guint w,h;
      clutter_actor_get_size(actor, &w, &h);
      x = 0;
      y = 0;
      width = w;
      height = h;
    }

  while (it && it != stage)
    {
      ClutterFixed px,py;
      gdouble scalex, scaley;
      ClutterUnit anchorx, anchory;

      /* Big safety check here - don't attempt to work out bounds if anything
       * is rotated, as we'll probably get it wrong. */
      clutter_actor_get_scale(it, &scalex, &scaley);
      clutter_actor_get_anchor_pointu(it, &anchorx, &anchory);
      if (clutter_actor_get_rotationu(it, CLUTTER_X_AXIS, 0, 0, 0)!=0 ||
          clutter_actor_get_rotationu(it, CLUTTER_Y_AXIS, 0, 0, 0)!=0 ||
          clutter_actor_get_rotationu(it, CLUTTER_Z_AXIS, 0, 0, 0)!=0)
        valid = FALSE;

      clutter_actor_get_positionu(it, &px, &py);
      x = ((x - CLUTTER_FIXED_TO_DOUBLE(anchorx))*scalex)
          + CLUTTER_FIXED_TO_DOUBLE(px);
      y = ((y - CLUTTER_FIXED_TO_DOUBLE(anchory))*scaley)
          + CLUTTER_FIXED_TO_DOUBLE(py);
      width *= scalex;
      height *= scaley;

      it = clutter_actor_get_parent(it);
    }

  if (geo)
    {
      /* Do some simple rounding */
      geo->x = (int)(x + 0.5);
      geo->y = (int)(y + 0.5);
      geo->width = (int)(width + 0.5);
      geo->height = (int)(height + 0.5);
    }
  if (is_visible)
    {
      *is_visible = visible;
    }
  return valid;
}

/* Call this after an actor is updated, and it will ask the stage to redraw
 * in whatever way is best (a small area if it can manage, or the whole
 * screen if not). NOTE: This takes account of *current* visibility (so
 * it won't update if an actor goes from visible->invisible). It also won't
 * Update correctly if an actor is moved/scaled. For that, you'll have to call
 * it once before and once after.
 * clutter_actor_set_allow_redraw(actor, false) should be called before using
 * this, or the actor will cause a full screen redraw regardless.*/
void
hd_util_partial_redraw_if_possible(ClutterActor *actor, ClutterGeometry *bounds)
{
  ClutterGeometry area = {0,0,0,0};
  ClutterActor *stage = clutter_stage_get_default();
  gboolean visible, valid;

  if (bounds)
    area = *bounds;

  valid = hd_util_get_actor_bounds(actor, &area, &visible);
  if (!visible) return;
  if (valid)
    {
      /* Queue a redraw, but without updating the whole area */
      clutter_stage_set_damaged_area(stage, area);
      clutter_actor_queue_redraw_damage(stage);
    }
  else
    {
      clutter_actor_queue_redraw(stage);
    }
}

/* Check to see whether clients above this one totally obscure it */
gboolean hd_util_client_obscured(MBWindowManagerClient *client)
{
  GdkRegion *region;
  MBWindowManagerClient *obscurer;
  gboolean empty;

  /* Get region representing the current client */
  region = gdk_region_rectangle((GdkRectangle*)&client->window->geometry);

  /* Subtract the region of all clients above */
  for (obscurer = client->stacked_above;
       obscurer && !gdk_region_empty(region);
       obscurer = obscurer->stacked_above)
    {
      GdkRegion *obscure_region =
          gdk_region_rectangle((GdkRectangle*)&obscurer->window->geometry);
      gdk_region_subtract(region, obscure_region);
      gdk_region_destroy(obscure_region);
    }

  /* If there is nothing left, then this can't
   * be visible */
  empty = gdk_region_empty(region);
  gdk_region_destroy(region);
  return empty;
}


/* Structure holding a list of keyframes that will be linearly interpolated
 * between to produce animation*/
struct _HdKeyFrameList {
  float *keyframes;
  int count;
};

/* Create a keyframe list from a comma-separated list of floating point values */
HdKeyFrameList *hd_key_frame_list_create(const char *keys)
{
  char *key_copy = 0;
  char *p;
  int i=0;
  HdKeyFrameList *k = (HdKeyFrameList*)g_malloc0(sizeof(HdKeyFrameList));
  /* Fail nicely by returning a straight ramp */
  if (!keys || strlen(keys)<=1)
    goto fail;
  key_copy = g_strdup(keys);
  /* Scan for how many elements we need */
  k->count = 0;
  for (p=key_copy;*p;p++)
    if (*p==',') k->count++;
  if (key_copy[strlen(key_copy)-1]!=',')
    k->count++;
  if (k->count<2)
    goto fail;
  k->keyframes = (float*)g_malloc0(sizeof(float) * k->count);
  /* read in individual keys */
  for (p=key_copy;*p;)
    {
      char *comma = p;
      char old_comma;
      /* Find comma and replace with string end character */
      while (*comma && *comma!=',')
        comma++;
      old_comma = *comma;
      *comma = 0;
      /* Read the data value */
      k->keyframes[i++] = atof(p);
      /* Set up for next iteration. If we hit the end, don't skip over it! */
      if (old_comma)
        p = comma+1;
      else
        p = comma;
    }
  k->count = i;
  g_free(key_copy);
  return k;
fail:
  /* On failure, free memory, and return a simple
   * linear ramp */
  if (key_copy) g_free(key_copy);
  /* k is already allocated */
  k->count = 2;
  if (k->keyframes) g_free(k->keyframes);
  k->keyframes = (float*)g_malloc(sizeof(float) * k->count);
  k->keyframes[0] = 0.0f;
  k->keyframes[1] = 1.0f;
  return k;
}

void hd_key_frame_list_free(HdKeyFrameList *k)
{
  if (k)
    {
      g_free(k->keyframes);
      g_free(k);
    }
}

/* As X goes between 0 and 1, interpolate into the HdKeyFrameList */
float hd_key_frame_interpolate(HdKeyFrameList *k, float x)
{
  float v,n;
  int idx;

  if (!k || k->count < 2)
    return x;

  v = x * (k->count-1);
  idx = (int)v;
  n = v - idx;

  if (idx >= k->count-1)
    {
      idx = k->count-2;
      n = 1;
    }
  if (idx<0)
    {
      idx = 0;
      n = 0;
    }
  return k->keyframes[idx]*(1-n) + k->keyframes[idx+1]*n;
}
