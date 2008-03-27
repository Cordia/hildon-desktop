INCLUDES = $(HD_CFLAGS)

bin_PROGRAMS = hildon-desktop

hildon_desktop_SOURCES = \
	hd-wm.c \
	hd-wm.h \
	hd-comp-mgr.c \
	hd-comp-mgr.h \
	hd-comp-window.c \
	hd-comp-window.h \
	hd-window-actor.c \
	hd-window-actor.h \
	hd-window-group.c \
	hd-window-group.h \
	hd-switcher.c \
	hd-switcher.h \
	hd-stage.c \
	hd-stage.h \
	hd-mb-wm-props.h \
	main.c

hildon_desktop_LDADD = $(HD_LIBS)
hildon_desktop_CFLAGS = -std=c99 -fgnu89-inline

CLEANFILES = *~
