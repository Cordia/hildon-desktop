INCLUDES = @HD_INCS@ $(MB2_CFLAGS) $(HD_CFLAGS) -D_XOPEN_SOURCE=500

mb_h =		hd-atoms.h			\
		hd-comp-mgr.h			\
		hd-wm.h				\
		hd-desktop.h			\
		hd-app.h			\
		hd-app-menu.h			\
		hd-note.h			\
		hd-status-area.h		\
		hd-status-menu.h		\
		hd-dialog.h			\
		hd-home-applet.h		\
		hd-theme.h			\
		hd-decor.h			\
		hd-decor-button.h		\
		hd-animation-actor.h		\
		hd-remote-texture.h

mb_c = 		hd-atoms.c			\
		hd-comp-mgr.c			\
		hd-wm.c				\
		hd-desktop.c			\
		hd-app.c			\
		hd-app-menu.c			\
		hd-note.c			\
		hd-status-area.c		\
		hd-status-menu.c		\
		hd-dialog.c			\
		hd-home-applet.c		\
		hd-theme.c			\
		hd-decor.c			\
		hd-decor-button.c		\
		hd-animation-actor.c		\
		hd-remote-texture.c

noinst_LTLIBRARIES = libmb.la

libmb_la_SOURCES = $(mb_h) $(mb_c)
libmb_la_CFLAGS =  @HD_CFLAGS@  \
		     -DHD_DATADIR=\"$(datadir)/hildon-desktop\"
libmb_la_LIBADD = @HD_LIBS@

CLEANFILES = *~
