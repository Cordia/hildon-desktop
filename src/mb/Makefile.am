INCLUDES = @HD_INCS@ $(HD_CFLAGS) -D_XOPEN_SOURCE=500

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
		hd-applet-layout-manager.h

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
		hd-applet-layout-manager.c

noinst_LIBRARIES = libmb.a

libmb_a_SOURCES = $(mb_h) $(mb_c)
libmb_a_CFLAGS =  @HD_CFLAGS@
libmb_a_LIBS = @HD_LIBS@

CLEANFILES = *~
