INCLUDES = @HD_INCS@ $(HD_CFLAGS) -D_XOPEN_SOURCE=500

tail_h = 		tail.h		\
			tail-sub-texture.h

tail_c =		tail.c		\
			tail-sub-texture.c

noinst_LTLIBRARIES = libtail.la

libtail_la_SOURCES = $(tail_h) $(tail_c)
libtail_la_CFLAGS =  	@HD_CFLAGS@					\
		      	@HDA_CFLAGS@				\
			     -DHD_DATADIR=\"$(datadir)/hildon-desktop\"
libtail_la_LIBS = @HD_LIBS@ @HDA_LIBS@

CLEANFILES = *~