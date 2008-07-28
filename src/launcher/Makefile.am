SUBDIRS = tidy

INCLUDES = \
	-I. \
	-I.. \
	-D_XOPEN_SOURCE=500 \
	$(HD_INCS) \
	$(HD_CFLAGS)

launcher_h =	hd-app-launcher.h		\
		hd-dummy-launcher.h		\
		hd-launcher-item.h		\
		hd-section-launcher.h		\
		hd-task-launcher.h		\
		hd-task-launcher-private.h

launcher_c =	hd-app-launcher.c		\
		hd-dummy-launcher.c		\
		hd-launcher-item.c		\
		hd-section-launcher.c		\
		hd-task-launcher.c

noinst_LIBRARIES = liblauncher.a

noinst_PROGRAMS = test-launcher

liblauncher_a_SOURCES = $(launcher_h) $(launcher_c)
liblauncher_a_CFLAGS =  $(HD_CFLAGS)
liblauncher_a_LIBS = $(HD_LIBS) $(top_builddir)/src/launcher/tidy/libtidy.la

test_launcher_SOURCES = main.c
test_launcher_CFLAGS = $(HD_CFLAGS)
test_launcher_LDADD = \
	$(HD_LIBS) \
	$(top_builddir)/src/launcher/tidy/libtidy.la \
	liblauncher.a

CLEANFILES = *~
