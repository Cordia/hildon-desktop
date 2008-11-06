INCLUDES = -I $(top_srcdir)/src/launcher \
	-I $(top_srcdir)src/tidy \
	@HD_INCS@ $(HD_CFLAGS) -D_XOPEN_SOURCE=500

home_h = 	hd-home.h		\
		hd-home-view.h		\
		hd-edit-menu.h		\
		hd-background-dialog.h	\
		hd-layout-dialog.h	\
		hd-scrollable-group.h	\
		hd-switcher.h		\
		hd-task-navigator.h	\
		hd-applet-manager.h	\
		hd-add-applet-dialog.h	\
		hd-notification.h

home_c = 	hd-home.c		\
		hd-home-view.c		\
		hd-background-dialog.c	\
		hd-layout-dialog.c	\
		hd-edit-menu.c		\
		hd-scrollable-group.c	\
		hd-switcher.c		\
		hd-task-navigator.c	\
		hd-applet-manager.c	\
		hd-add-applet-dialog.c	\
		hd-notification.c

noinst_LTLIBRARIES = libhome.la

libhome_la_SOURCES = $(home_h) $(home_c)
libhome_la_CFLAGS =  @HD_CFLAGS@ -DHD_DATADIR=\"$(datadir)/hildon-desktop\"
libhome_la_LIBADD = @HD_LIBS@ \
                    $(top_builddir)/src/launcher/liblauncher.la \
                    $(top_builddir)/src/util/libutil.la

CLEANFILES = *~
