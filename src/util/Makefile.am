INCLUDES = @HD_INCS@ $(HD_CFLAGS) -D_XOPEN_SOURCE=500

util_h = 	hd-util.h		\
		hd-dbus.h

util_c = 	hd-util.c		\
		hd-dbus.c

noinst_LIBRARIES = libutil.a

libutil_a_SOURCES = $(util_h) $(util_c)
libutil_a_CFLAGS =  @HD_CFLAGS@
libutil_a_LIBS = @HD_LIBS@

CLEANFILES = *~
