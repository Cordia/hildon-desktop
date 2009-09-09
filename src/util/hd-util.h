
#ifndef __HD_UTIL_H__
#define __HD_UTIL_H__

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <glib.h>
#include <matchbox/core/mb-wm.h>

#include "hd-atoms.h"

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
gboolean hd_util_change_screen_orientation (MBWindowManager *wm,
                                            gboolean goto_portrait);
void hd_util_set_rotating_property(MBWindowManager *wm, gboolean is_rotating);

gboolean hd_util_get_cursor_position(gint *x, gint *y);
gboolean hd_util_client_has_video_overlay(MBWindowManagerClient *client);

void hd_util_click (const MBWindowManagerClient *c);

/* This is called on the MBWindowManagerSignalRootConfigure signal,
 * and is used for XRR rotation */
Bool hd_util_root_window_configured(MBWMObject *obj,
                                    int         mask,
                                    void       *userdata);

#endif
