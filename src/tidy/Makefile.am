NULL =

GLIB_GENMARSHAL = `pkg-config --variable=glib_genmarshal glib-2.0`
GLIB_MKENUMS = `pkg-config --variable=glib_mkenums glib-2.0`

INCLUDES = \
        -I$(top_srcdir)                 \
	-I$(top_srcdir)/src/		\
        -DPREFIX=\""$(prefix)"\"        \
        -DLIBDIR=\""$(libdir)"\"        \
        -DG_DISABLE_DEPRECATED          \
        -DG_LOG_DOMAIN=\"Tidy\"         \
	$(HD_CFLAGS)			\
	$(MB2_CFLAGS)			\
        $(NULL)
BUILT_SOURCES = \
        tidy-enum-types.h       \
        tidy-enum-types.c       \
        tidy-marshal.h          \
        tidy-marshal.c

STAMP_FILES = stamp-tidy-marshal.h stamp-tidy-enum-types.h

noinst_LTLIBRARIES = libtidy.la

source_h = \
	$(top_srcdir)/src/tidy/tidy-actor.h 		\
	$(top_srcdir)/src/tidy/tidy-adjustment.h	\
	$(top_srcdir)/src/tidy/tidy-blur-group.h 	\
	$(top_srcdir)/src/tidy/tidy-cached-group.h 	\
	$(top_srcdir)/src/tidy/tidy-finger-scroll.h	\
	$(top_srcdir)/src/tidy/tidy-frame.h	\
	$(top_srcdir)/src/tidy/tidy-highlight.h		\
	$(top_srcdir)/src/tidy/tidy-interval.h		\
	$(top_srcdir)/src/tidy/tidy-mem-texture.h	\
	$(top_srcdir)/src/tidy/tidy-scroll-bar.h	\
	$(top_srcdir)/src/tidy/tidy-scrollable.h	\
	$(top_srcdir)/src/tidy/tidy-scroll-view.h	\
	$(top_srcdir)/src/tidy/tidy-stylable.h		\
	$(top_srcdir)/src/tidy/tidy-style.h 		\
	$(top_srcdir)/src/tidy/tidy-sub-texture.h 	\
	$(top_srcdir)/src/tidy/tidy-types.h 		\
	$(NULL)

source_h_private = \
	tidy-debug.h \
	$(NULL)

source_c = \
	tidy-actor.c \
	tidy-adjustment.c \
	tidy-blur-group.c \
	tidy-cached-group.c \
	tidy-finger-scroll.c \
	tidy-frame.c \
	tidy-highlight.c \
	tidy-interval.c \
	tidy-mem-texture.c \
	tidy-scroll-bar.c \
	tidy-scrollable.c \
	tidy-scroll-view.c \
	tidy-stylable.c \
	tidy-style.c \
	tidy-sub-texture.c \
	$(NULL)

tidy-marshal.h: stamp-tidy-marshal.h
	@true
stamp-tidy-marshal.h: Makefile tidy-marshal.list
	$(GLIB_GENMARSHAL) \
		--prefix=_tidy_marshal \
		--header \
	$(srcdir)/tidy-marshal.list > xgen-tmh && \
	(cmp -s xgen-tmh tidy-marshal.h || cp -f xgen-tmh tidy-marshal.h) && \
	rm -f xgen-tmh && \
	echo timestamp > $(@F)

tidy-marshal.c: Makefile tidy-marshal.list
	(echo "#include \"tidy-marshal.h\"" ; \
	 $(GLIB_GENMARSHAL) \
		--prefix=_tidy_marshal \
		--body \
	 $(srcdir)/tidy-marshal.list ) > xgen-tmc && \
	cp -f xgen-tmc tidy-marshal.c && \
	rm -f xgen-tmc

tidy-enum-types.h: stamp-tidy-enum-types.h Makefile
	@true
stamp-tidy-enum-types.h: $(source_h) $(srcdir)/tidy-enum-types.h.in
	( $(GLIB_MKENUMS) \
	    --template $(srcdir)/tidy-enum-types.h.in \
	  $(source_h) ) >> xgen-teth && \
	(cmp xgen-teth tidy-enum-types.h || cp xgen-teth tidy-enum-types.h) && \
	rm -f xgen-teth && \
	echo timestamp > $(@F)

tidy-enum-types.c: stamp-tidy-enum-types.h $(srcdir)/tidy-enum-types.c.in
	( $(GLIB_MKENUMS) \
	    --template $(srcdir)/tidy-enum-types.c.in \
	  $(source_h) ) >> xgen-tetc && \
	cp xgen-tetc tidy-enum-types.c && \
	rm -f xgen-tetc

libtidy_la_LIBADD = $(HD_LIBS)
libtidy_la_SOURCES = \
	$(source_c) \
	$(source_h) \
	$(source_h_priv) \
	$(BUILT_SOURCES) \
	$(NULL)

CLEANFILES = $(STAMP_FILES) $(BUILT_SOURCES)

DISTCLEANFILES = tidy-version.h

EXTRA_DIST = \
	tidy-enum-types.h.in \
	tidy-enum-types.c.in \
	tidy-private.h \
	tidy-version.h.in \
	tidy.h \
	tidy-marshal.list
