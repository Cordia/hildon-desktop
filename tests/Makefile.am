noinst_PROGRAMS = test-hung-process test-applet test-launcher test-portrait

test_hung_process_SOURCES = test-hung-process.c
test_hung_process_CFLAGS = `pkg-config --cflags gtk+-2.0`
test_hung_process_LDFLAGS = `pkg-config --libs gtk+-2.0`

test_applet_SOURCES = test-applet.c
test_applet_CFLAGS = `pkg-config --cflags gtk+-2.0`
test_applet_LDFLAGS = `pkg-config --libs gtk+-2.0`

test_portrait_SOURCES = test-portrait.c
test_portrait_CFLAGS = `pkg-config --cflags gtk+-2.0`
test_portrait_LDFLAGS = `pkg-config --libs gtk+-2.0`

test_launcher_SOURCES = test-launcher.c
test_launcher_CFLAGS = $(HD_CFLAGS) -I$(top_srcdir)/src/launcher
test_launcher_LDADD = \
        $(HD_LIBS) \
        $(top_builddir)/src/launcher/tidy/libtidy.la \
        $(top_builddir)/src/launcher/liblauncher.la

