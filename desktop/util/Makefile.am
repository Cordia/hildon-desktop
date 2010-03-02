INCLUDES = @HD_INCS@ $(MB2_CFLAGS) $(HD_CFLAGS) -D_XOPEN_SOURCE=500

util_h = 	hd-util.h		\
		hd-dbus.h         \
		hd-theme-config.h		\
		hd-gtk-utils.h		\
		hd-transition.h

util_c = 	hd-util.c		\
		hd-dbus.c         \
		hd-theme-config.c		\
		hd-gtk-utils.c		\
		hd-transition.c

noinst_LTLIBRARIES = libutil.la

libutil_la_SOURCES = $(util_h) $(util_c)
libutil_la_CFLAGS =  @HD_CFLAGS@					\
		     -DHD_DATADIR=\"$(datadir)/hildon-desktop\"
libutil_la_LIBS = @HD_LIBS@

CLEANFILES = *~
