
#ifndef __HD_UTIL_H__
#define __HD_UTIL_H__

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <glib.h>
#include <clutter/clutter.h>
#include <matchbox/core/mb-wm.h>
#include <clutter/clutter.h>

#include "mb/hd-atoms.h"

void * hd_util_get_win_prop_data_and_validate (Display   *xpdy,
					       Window     xwin,
					       Atom       prop,
					       Atom       type,
					       gint       expected_format,
					       gint       expected_n_items,
					       gint      *n_items_ret);
char * hd_util_get_x_window_string_property (MBWindowManager  *wm,
                                             Window            xwin,
                                             HdAtoms           atom_id);
unsigned long hd_util_modal_blocker_realize(MBWindowManagerClient *client,
                                            gboolean ping_only);
Bool hd_util_client_has_modal_blocker (MBWindowManagerClient *c);

void hd_util_set_rotating_property(MBWindowManager *wm, gboolean is_rotating);
void hd_util_set_screen_size_property(MBWindowManager *wm, gboolean is_portrait);
gboolean hd_util_change_screen_orientation (MBWindowManager *wm,
                                            gboolean goto_portrait);
void hd_util_root_window_configured(MBWindowManager *wm);
gboolean hd_util_rotate_geometry (ClutterGeometry *geo, guint scrh, guint scrw);

gboolean hd_util_get_cursor_position(gint *x, gint *y);
gboolean hd_util_client_has_video_overlay(MBWindowManagerClient *client);

void hd_util_click (const MBWindowManagerClient *c);

void
hd_util_partial_redraw_if_possible(ClutterActor *actor, ClutterGeometry *bounds);

gboolean hd_util_client_obscured(MBWindowManagerClient *client);

/* Functions for loading and interpolating from a list of keyframes */
typedef struct _HdKeyFrameList HdKeyFrameList;
HdKeyFrameList *hd_key_frame_list_create(const char *keys);
void hd_key_frame_list_free(HdKeyFrameList *k);
float hd_key_frame_interpolate(HdKeyFrameList *k, float x);

#endif
