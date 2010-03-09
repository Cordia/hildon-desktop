SUBDIRS = $(A11Y_DIR)

INCLUDES = @HD_INCS@ $(MB2_CFLAGS) $(HD_CFLAGS) -D_XOPEN_SOURCE=500

MY_LIBS = $(top_builddir)/desktop/libhildondesktop.la

bin_PROGRAMS = hildon-desktop

hildon_desktop_SOURCES = main.c hildon-desktop.h shell-recorder.c shell-recorder.h shell-recorder-src.c shell-recorder-src.h

UNWANTED = -Wl,--export-dynamic -export-dynamic -rdynamic
hildon_desktop_LDADD = \
	$(filter-out $(UNWANTED),$(MY_LIBS) $(HD_LIBS) $(A11Y_LA))
hildon_desktop_LDFLAGS = -rdynamic -Wl,--version-script=export.map \
	$(filter-out $(UNWANTED),$(MAEMO_LAUNCHER_LIBS))
hildon_desktop_CFLAGS = -std=c99 -fgnu89-inline $(MAEMO_LAUNCHER_CFLAGS)

schemadir = ${datadir}/gconf/schemas
schema_DATA = \ 
	hildon-desktop-views.schemas

CLEANFILES = *~
