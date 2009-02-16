INCLUDES = \
	-I. \
	-I.. \
	-D_GNU_SOURCE \
	-D_XOPEN_SOURCE=500 \
	-DPREFIX=\""$(prefix)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	@HD_INCS@

BUILT_SOURCES =	\
	hd-app-mgr-glue.h

hd-app-mgr-glue.h: hd-app-mgr-dbus.xml
	$(DBUS_BINDING_TOOL) --prefix=hd_app_mgr --mode=glib-server $< > xgen-$(@F) \
	&& ( cmp -s xgen-$(@F) $@ || cp xgen-$(@F) $@ )                              \
	&& rm -f xgen-$(@F)

launcher_h = \
	hd-app-mgr.h      \
	hd-launcher-tree.h		\
	hd-launcher-item.h		\
	hd-launcher-cat.h		\
	hd-launcher-app.h		\
	hd-launcher-tile.h		\
	hd-launcher-grid.h		\
	hd-launcher-page.h		\
	hd-launcher.h

launcher_c = \
	hd-app-mgr.c      \
	hd-launcher-tree.c		\
	hd-launcher-item.c		\
	hd-launcher-cat.c		\
	hd-launcher-app.c		\
	hd-launcher-tile.c		\
	hd-launcher-grid.c		\
	hd-launcher-page.c		\
	hd-launcher.c

noinst_LTLIBRARIES = liblauncher.la

liblauncher_la_SOURCES = $(launcher_h) $(launcher_c) $(BUILT_SOURCES)
liblauncher_la_CFLAGS =  $(HD_CFLAGS)
liblauncher_la_LIBADD = $(HD_LIBS) \
                        $(top_builddir)/src/util/libutil.la \
                        $(top_builddir)/src/tidy/libtidy.la

EXTRA_DIST = hd-app-mgr-dbus.xml

CLEANFILES = *~
