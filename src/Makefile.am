INCLUDES = $(HD_CFLAGS) -D_XOPEN_SOURCE=500

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
	hd-home-view.c \
	hd-home-view.h \
	hd-dbus.c \
	hd-dbus.h \
	hd-atoms.c \
	hd-atoms.h \
	hd-util.c \
	hd-util.h \
	hd-home-applet.c \
	hd-home-applet.h \
	hd-desktop.h \
	hd-desktop.c \
	main.c

hildon_desktop_LDADD = $(HD_LIBS)
hildon_desktop_CFLAGS = -std=c99 -fgnu89-inline

CLEANFILES = *~
