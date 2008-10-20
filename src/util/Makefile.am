INCLUDES = @HD_INCS@ $(HD_CFLAGS) -D_XOPEN_SOURCE=500

util_h = 	hd-util.h		\
		hd-dbus.h         \
		hd-gtk-style.h		\
		hd-gtk-utils.h

util_c = 	hd-util.c		\
		hd-dbus.c         \
		hd-gtk-style.c		\
		hd-gtk-utils.c

noinst_LTLIBRARIES = libutil.la

libutil_la_SOURCES = $(util_h) $(util_c)
libutil_la_CFLAGS =  @HD_CFLAGS@
libutil_la_LIBS = @HD_LIBS@

CLEANFILES = *~
