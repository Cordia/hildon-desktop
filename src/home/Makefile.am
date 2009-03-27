INCLUDES = -I $(top_srcdir)/src/launcher \
	-I $(top_srcdir)src/tidy \
	@HD_INCS@ $(MB2_CFLAGS) $(HD_CFLAGS) \
	-D_XOPEN_SOURCE=500

BUILT_SOURCES =	\
	hd-home-glue.h

hd-home-glue.h: hd-home-dbus.xml
	$(DBUS_BINDING_TOOL) --prefix=hd_home --mode=glib-server $< > xgen-$(@F)			\
	&& ( cmp -s xgen-$(@F) $@ || cp xgen-$(@F) $@ )							\
	&& rm -f xgen-$(@F)

home_h = 	hd-home.h		\
		hd-home-view.h		\
		hd-home-view-container.h\
		hd-home-view-layout.h   \
		hd-render-manager.h	\
		hd-scrollable-group.h	\
		hd-switcher.h		\
		hd-task-navigator.h	\
		hd-title-bar.h		\
		hd-clutter-cache.h

home_c = 	hd-home.c		\
		hd-home-view.c		\
		hd-home-view-container.c\
		hd-home-view-layout.c   \
		hd-render-manager.c	\		
		hd-scrollable-group.c	\
		hd-switcher.c		\
		hd-task-navigator.c	\
		hd-title-bar.c		\
		hd-clutter-cache.c

noinst_LTLIBRARIES = libhome.la

libhome_la_SOURCES = $(home_h) $(home_c) $(BUILT_SOURCES)
libhome_la_CFLAGS =  @HD_CFLAGS@						\
		     -DHD_DATADIR=\"$(datadir)/hildon-desktop\"
libhome_la_LIBADD = @HD_LIBS@ \
		    $(top_builddir)/src/launcher/liblauncher.la \
                    $(top_builddir)/src/util/libutil.la

EXTRA_DIST = hd-home-dbus.xml

CLEANFILES = *~ $(BUILT_SOURCES)
