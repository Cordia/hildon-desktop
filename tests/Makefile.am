noinst_PROGRAMS = test-hung-process test-applet test-launcher test-portrait-win \
		  test-portrait-dlg test-signals

test_hung_process_SOURCES = test-hung-process.c
test_hung_process_CFLAGS = `pkg-config --cflags gtk+-2.0`
test_hung_process_LDFLAGS = `pkg-config --libs gtk+-2.0`

test_applet_SOURCES = test-applet.c
test_applet_CFLAGS = `pkg-config --cflags gtk+-2.0`
test_applet_LDFLAGS = `pkg-config --libs gtk+-2.0`

test_portrait_win_SOURCES = test-portrait-win.c
test_portrait_win_CFLAGS = `pkg-config --cflags hildon-1` -D_XOPEN_SOURCE=500
test_portrait_win_LDFLAGS = `pkg-config --libs hildon-1`

test_portrait_dlg_SOURCES = test-portrait-dlg.c
test_portrait_dlg_CFLAGS = `pkg-config --cflags hildon-1` -D_XOPEN_SOURCE=500
test_portrait_dlg_LDFLAGS = `pkg-config --libs hildon-1`

test_launcher_SOURCES = test-launcher.c
test_launcher_CFLAGS = $(HD_CFLAGS) -I$(top_srcdir)/src/launcher \
	-I$(top_srcdir)/src
test_launcher_LDADD = \
        $(HD_LIBS) \
        $(top_builddir)/src/tidy/libtidy.la \
        $(top_builddir)/src/util/libutil.la  \
        $(top_builddir)/src/launcher/liblauncher.la

test_signals_SOURCES = test-signals.c
test_signals_CFLAGS = `pkg-config --cflags gtk+-2.0 dbus-1`
test_signals_LDFLAGS = `pkg-config --libs gtk+-2.0 dbus-1`

