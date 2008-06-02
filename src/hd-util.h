
#ifndef __HD_UTIL_H__
#define __HD_UTIL_H__

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <glib.h>

void * hd_util_get_win_prop_data_and_validate (Display   *xpdy,
					       Window     xwin,
					       Atom       prop,
					       Atom       type,
					       gint       expected_format,
					       gint       expected_n_items,
					       gint      *n_items_ret);

void hd_util_fake_button_event (Display *dpy,
				guint    button,
				gboolean is_press,
				gint16   x,
				gint16   y);

gint hd_util_grab_pointer (void);

void hd_util_ungrab_pointer (void);

#endif
