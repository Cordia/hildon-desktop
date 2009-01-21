
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
    "_HILDON_WM_WINDOW_TYPE_APP_MENU",
    "_HILDON_WM_WINDOW_TYPE_STATUS_AREA",
    "_HILDON_WM_WINDOW_TYPE_STATUS_MENU",

    "_HILDON_NOTIFICATION_TYPE",
    "_HILDON_NOTIFICATION_TYPE_BANNER",
    "_HILDON_NOTIFICATION_TYPE_INFO",
    "_HILDON_NOTIFICATION_TYPE_CONFIRMATION",

    "_HILDON_INCOMING_EVENT_NOTIFICATION_DESTINATION",
    "_HILDON_INCOMING_EVENT_NOTIFICATION_SUMMARY",
    "_HILDON_INCOMING_EVENT_NOTIFICATION_ICON",

    "_HILDON_CLIENT_MESSAGE_PAN",
    "_HILDON_CLIENT_MESSAGE_SHOW_SETTINGS",

    "_HILDON_APPLET_ID",
    "_HILDON_APPLET_SETTINGS",
    "_HILDON_APPLET_SHOW_SETTINGS",

    "_HILDON_WM_WINDOW_PROGRESS_INDICATOR",

    "WM_WINDOW_ROLE",

    "UTF8_STRING",
  };

  XInternAtoms (xdpy,
		atom_names,
		_HD_ATOM_LAST,
                False,
		atoms);
}
