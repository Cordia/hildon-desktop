SUBDIRS = tidy

INCLUDES = \
	-I. \
	-I.. \
	-D_XOPEN_SOURCE=500 \
	@HD_INCS@

launcher_h = \
	hd-app-launcher.h		\
	hd-dummy-launcher.h		\
	hd-launcher-item.h		\
	hd-task-launcher.h		\
	hd-task-launcher-private.h	\
	hd-launcher-utils.h

launcher_c = \
	hd-app-launcher.c		\
	hd-dummy-launcher.c		\
	hd-launcher-item.c		\
	hd-task-launcher.c		\
	hd-launcher-utils.c

noinst_LTLIBRARIES = liblauncher.la

liblauncher_la_SOURCES = $(launcher_h) $(launcher_c)
liblauncher_la_CFLAGS =  $(HD_CFLAGS)
liblauncher_la_LIBADD = $(HD_LIBS) $(top_builddir)/src/launcher/tidy/libtidy.la

CLEANFILES = *~
