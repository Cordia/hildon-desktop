SUBDIRS = mb tidy launcher home util

INCLUDES = @HD_INCS@ $(HD_CFLAGS) -D_XOPEN_SOURCE=500

MY_LIBS =	$(top_builddir)/src/mb/libmb.a			\
		$(top_builddir)/src/launcher/liblauncher.la 	\
		$(top_builddir)/src/home/libhome.la		\
		$(top_builddir)/src/util/libutil.a

bin_PROGRAMS = hildon-desktop

hildon_desktop_SOURCES = main.c hildon-desktop.h

hildon_desktop_LDADD = $(MY_LIBS) $(HD_LIBS)
hildon_desktop_CFLAGS = -std=c99 -fgnu89-inline

# The sed is because GCONF_SCHEMA_FILE_DIR is based on the prefix.
# At this point, the prefix is the /usr directory of the scratchbox.
# Hence we have to rewrite so we don't get /usr/etc/...
schemadir = `echo @GCONF_SCHEMA_FILE_DIR@|$(SED) -e s^usr/etc^etc^`
schema_DATA = hildon-desktop.schemas

CLEANFILES = *~
