INCLUDES = @HD_INCS@ $(HD_CFLAGS) $(MB2_CFLAGS) -D_XOPEN_SOURCE=500

hdalauncher_h = 	hda-launcher.h		\
			hda-launcher-grid.h	\
			hda-launcher-page.h
			hda-launcher-tile.h

hdalauncher_c =		hda-launcher.c		\
			hda-launcher-grid.c	\
			hda-launcher-page.c	\
			hda-launcher-tile.c

noinst_LTLIBRARIES = libhdalauncher.la

libhdalauncher_la_SOURCES = $(hdalauncher_h) $(hdalauncher_c)
libhdalauncher_la_CFLAGS =  @HD_CFLAGS@					\
		            @HDA_CFLAGS@				\
			     -DHD_DATADIR=\"$(datadir)/hildon-desktop\"
libhdalauncher_la_LIBS = @HD_LIBS@ @HDA_LIBS@

CLEANFILES = *~