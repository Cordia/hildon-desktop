SUBDIRS = $(A11Y_DIR)

INCLUDES = @HD_INCS@ $(MB2_CFLAGS) $(HD_CFLAGS) -D_XOPEN_SOURCE=500

MY_LIBS = $(top_builddir)/desktop/libhildondesktop.la

bin_PROGRAMS = hildon-desktop-3

hildon_desktop_3_SOURCES = main.c hildon-desktop.h

UNWANTED = -Wl,--export-dynamic -export-dynamic -rdynamic
hildon_desktop_3_LDADD = \
	$(filter-out $(UNWANTED),$(MY_LIBS) $(HD_LIBS) $(A11Y_LA))
hildon_desktop_3_LDFLAGS = -rdynamic -Wl,--version-script=export.map \
	$(filter-out $(UNWANTED),$(MAEMO_LAUNCHER_LIBS))
hildon_desktop_3_CFLAGS = -std=c99 -fgnu89-inline $(MAEMO_LAUNCHER_CFLAGS)

schemadir = ${datadir}/gconf/schemas
schema_DATA = \ 
	hildon-desktop-views.schemas

CLEANFILES = *~
