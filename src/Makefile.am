SUBDIRS = mb tidy util launcher home

INCLUDES = @HD_INCS@ $(HD_CFLAGS) -D_XOPEN_SOURCE=500

MY_LIBS =	$(top_builddir)/src/mb/libmb.a			\
		$(top_builddir)/src/launcher/liblauncher.la 	\
		$(top_builddir)/src/home/libhome.la		\
		$(top_builddir)/src/util/libutil.la

bin_PROGRAMS = hildon-desktop

hildon_desktop_SOURCES = main.c hildon-desktop.h

hildon_desktop_LDADD = $(MY_LIBS) $(HD_LIBS)
hildon_desktop_CFLAGS = -std=c99 -fgnu89-inline

schemadir = /etc/gconf/schemas
schema_DATA = hildon-desktop.schemas

CLEANFILES = *~
