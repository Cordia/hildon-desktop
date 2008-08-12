
INCLUDES = \
	-I. \
	-I.. \
	-D_GNU_SOURCE \
	-D_XOPEN_SOURCE=500 \
	-DPREFIX=\""$(prefix)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	@HD_INCS@

launcher_h = \
	hd-app-launcher.h		\
	hd-dummy-launcher.h		\
	hd-launcher-item.h		\
	hd-task-launcher.h		\
	hd-task-launcher-private.h	\
	hd-launcher-tree.h		\
	hd-launcher-utils.h

launcher_c = \
	hd-app-launcher.c		\
	hd-dummy-launcher.c		\
	hd-launcher-item.c		\
	hd-task-launcher.c		\
	hd-launcher-tree.c		\
	hd-launcher-utils.c

noinst_LTLIBRARIES = liblauncher.la

liblauncher_la_SOURCES = $(launcher_h) $(launcher_c)
liblauncher_la_CFLAGS =  $(HD_CFLAGS)
liblauncher_la_LIBADD = $(HD_LIBS) $(top_builddir)/src/tidy/libtidy.la

CLEANFILES = *~
