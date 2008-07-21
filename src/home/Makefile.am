INCLUDES = @HD_INCS@ $(HD_CFLAGS) -D_XOPEN_SOURCE=500

home_h = 	hd-home.h		\
		hd-home-view.h		\
		hd-edit-menu.h		\
		hd-layout-dialog.h	\
		hd-switcher.h		\
		hd-switcher-group.h	\
		hd-gtk-style.h		\
		hd-gtk-utils.h

home_c = 	hd-home.c		\
		hd-home-view.c		\
		hd-layout-dialog.c	\
		hd-edit-menu.c		\
		hd-switcher.c		\
		hd-switcher-group.c	\
		hd-gtk-style.c		\
		hd-gtk-utils.c

noinst_LIBRARIES = libhome.a

libhome_a_SOURCES = $(home_h) $(home_c)
libhome_a_CFLAGS =  @HD_CFLAGS@ -DHD_DATADIR=\"$(datadir)/hildon-desktop\"
libhome_a_LIBS = @HD_LIBS@

CLEANFILES = *~
