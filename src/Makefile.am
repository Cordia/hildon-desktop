INCLUDES = $(HD_CFLAGS)

bin_PROGRAMS = hildon-desktop

hildon_desktop_SOURCES = \
	hd-wm.c \
	hd-wm.h \
	hd-comp-mgr.c \
	hd-comp-mgr.h \
	hd-switcher-group.c \
	hd-switcher-group.h \
	hd-switcher.c \
	hd-switcher.h \
	hd-home.c \
	hd-home.h \
	hd-dbus.c \
	hd-dbus.h \
	hd-atoms.c \
	hd-atoms.h \
	hd-util.c \
	hd-util.h \
	main.c

hildon_desktop_LDADD = $(HD_LIBS)
hildon_desktop_CFLAGS = -std=c99 -fgnu89-inline

CLEANFILES = *~
