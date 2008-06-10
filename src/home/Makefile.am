INCLUDES = @HD_INCS@ $(HD_CFLAGS) -D_XOPEN_SOURCE=500

home_h = 	hd-home.h		\
		hd-home-view.h		\
		hd-switcher.h		\
		hd-switcher-group.h

home_c = 	hd-home.c		\
		hd-home-view.c		\
		hd-switcher.c		\
		hd-switcher-group.c

noinst_LIBRARIES = libhome.a

libhome_a_SOURCES = $(home_h) $(home_c)
libhome_a_CFLAGS =  @HD_CFLAGS@
libhome_a_LIBS = @HD_LIBS@

CLEANFILES = *~
