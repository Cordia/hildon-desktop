
#ifndef __HD_UTIL_H__
#define __HD_UTIL_H__

#include <X11/Xlib.h>
#include <X11/Xatom.h>

void *
hd_util_get_win_prop_data_and_validate (Display   *xpdy,
					Window     xwin,
					Atom       prop,
					Atom       type,
					int        expected_format,
					int        expected_n_items,
					int       *n_items_ret);

#endif
