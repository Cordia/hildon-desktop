
#include "hd-atoms.h"

void
hd_atoms_init (Display * xdpy, Atom * atoms)
{
  /*
   *   The list below *MUST* be kept in the same order as the corresponding
   *   emun in hd-atoms.h or *everything* will break.
   *   Doing it like this avoids a mass of round trips on startup.
   */

  char *atom_names[] = {
    "_HILDON_APP_KILLABLE",	/* Hildon only props */
    "_HILDON_ABLE_TO_HIBERNATE",/* alias for the above */

    "_HILDON_HOME_VIEW",
    "_HILDON_STACKABLE_WINDOW",

    "_HILDON_WM_WINDOW_TYPE_HOME_APPLET",

    "_HILDON_CLIENT_MESSAGE_PAN",
    "_HILDON_CLIENT_MESSAGE_SHOW_SETTINGS",

    "WM_WINDOW_ROLE",
  };

  XInternAtoms (xdpy,
		atom_names,
		_HD_ATOM_LAST,
                False,
		atoms);
}
