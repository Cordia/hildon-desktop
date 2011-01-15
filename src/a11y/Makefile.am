SUBDIRS = 		launcher home

INCLUDES = 		@HD_INCS@ \
			@HD_CFLAGS@ -D_XOPEN_SOURCE=500

MY_LIBS =		$(top_builddir)/src/a11y/launcher/libhdalauncher.la 	\
			$(top_builddir)/src/a11y/home/libhdahome.la

# flat a11y
a11y_h =		hildon-desktop-a11y.h \
			hda-factory.h

a11y_c =		hildon-desktop-a11y.c

noinst_LTLIBRARIES = 	liba11y.la

liba11y_la_SOURCES = 	$(a11y_h) $(a11y_c)

liba11y_la_CFLAGS =  	@HD_CFLAGS@						\
		     	-DHD_DATADIR=\"$(datadir)/hildon-desktop\" -fno-strict-aliasing
liba11y_la_LIBS = 	@HD_LIBS@

# hda
module_LTLIBRARIES = 	libhda.la

hda_c_sources = 	hda-module.c

libhda_la_SOURCES = 	$(hda_c_sources)

libhda_la_CFLAGS =	@HDA_CFLAGS@
libhda_la_LIBS =	@HDA_LIBS@

libhda_la_LIBADD =     	$(MY_LIBS)

#moduledir should be the gtk one?
moduledir = 		$(libdir)/gtk-2.0/modules

libhda_la_LDFLAGS =    	-rpath $(moduledir) -module -avoid-version \
			@LDFLAGS@

CLEANFILES = *~
