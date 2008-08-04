
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

#endif
