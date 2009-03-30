INCLUDES = @HD_INCS@ $(HD_CFLAGS) $(MB2_CFLAGS) -D_XOPEN_SOURCE=500

hdahome_h = 		hda-home-init.h \
			hda-home.h

hdahome_c =		hda-home-init.c \
			hda-home.c

noinst_LTLIBRARIES = 	libhdahome.la

libhdahome_la_SOURCES = $(hdahome_h) $(hdahome_c)
libhdahome_la_CFLAGS =  @HD_CFLAGS@				\
		        @HDA_CFLAGS@				\
			-DHD_DATADIR=\"$(datadir)/hildon-desktop\"
libhdahome_la_LIBS = 	@HD_LIBS@ @HDA_LIBS@

CLEANFILES = *~