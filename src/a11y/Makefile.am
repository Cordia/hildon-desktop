SUBDIRS = launcher home

INCLUDES = @HD_INCS@ $(HD_CFLAGS) -D_XOPEN_SOURCE=500

MY_LIBS =	$(top_builddir)/src/a11y/launcher/libhdalauncher.la \
		$(top_builddir)/src/a11y/home/libhdahome.la


a11y_h =	hildon-desktop-a11y.h \
		hda-factory.h

a11y_c =	hildon-desktop-a11y.c

noinst_LTLIBRARIES = liba11y.la

liba11y_la_SOURCES = $(a11y_h) $(a11y_c)

liba11y_la_LIBADD  = $(MY_LIBS)
liba11y_la_CFLAGS =  @HD_CFLAGS@						\
		     @HDA_CFLAGS@						\
		     -DHD_DATADIR=\"$(datadir)/hildon-desktop\" -fno-strict-aliasing
liba11y_la_LIBS = @HD_LIBS@ @HDA_LIBS@

CLEANFILES = *~
