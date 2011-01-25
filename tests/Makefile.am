noinst_PROGRAMS = test-hung-process test-applet test-app-launch \
		  test-do-not-disturb test-large-note \
		  test-portrait-win test-portrait-dlg test-signals \
		  test-speed test-winstack test-non-compositing \
		  test-no-gtk test-live-bg

test_hung_process_SOURCES = test-hung-process.c
test_hung_process_CFLAGS = `pkg-config --cflags gtk+-2.0`
test_hung_process_LDFLAGS = `pkg-config --libs gtk+-2.0`

test_app_launch_SOURCES = test-app-launch.c
test_app_launch_CFLAGS = `pkg-config --cflags gtk+-2.0 dbus-glib-1`
test_app_launch_LDFLAGS = `pkg-config --libs gtk+-2.0 dbus-glib-1`

test_applet_SOURCES = test-applet.c
test_applet_CFLAGS = `pkg-config --cflags gtk+-2.0`
test_applet_LDFLAGS = `pkg-config --libs gtk+-2.0 --libs x11 `

test_do_not_disturb_SOURCES = test-do-not-disturb.c
test_do_not_disturb_CFLAGS = `pkg-config --cflags gtk+-2.0`
test_do_not_disturb_LDFLAGS = `pkg-config --libs gtk+-2.0`

test_large_note_SOURCES = test-large-note.c
test_large_note_CFLAGS = `pkg-config --cflags gtk+-2.0 hildon-1`
test_large_note_LDFLAGS = `pkg-config --libs gtk+-2.0 hildon-1`

test_portrait_win_SOURCES = test-portrait-win.c
test_portrait_win_CFLAGS = `pkg-config --cflags hildon-1`
test_portrait_win_LDFLAGS = `pkg-config --libs hildon-1`

test_portrait_dlg_SOURCES = test-portrait-dlg.c
test_portrait_dlg_CFLAGS = `pkg-config --cflags hildon-1`
test_portrait_dlg_LDFLAGS = `pkg-config --libs hildon-1`

test_signals_SOURCES = test-signals.c
test_signals_CFLAGS = `pkg-config --cflags gtk+-2.0 dbus-1`
test_signals_LDFLAGS = `pkg-config --libs gtk+-2.0 dbus-1`

test_speed_SOURCES = test-speed.c
test_speed_CFLAGS = `pkg-config --cflags gtk+-2.0`
test_speed_LDFLAGS = `pkg-config --libs gtk+-2.0 --libs x11`

test_live_bg_SOURCES = test-live-bg.c
test_live_bg_CFLAGS = `pkg-config --cflags x11 xrender`
test_live_bg_LDFLAGS = `pkg-config --libs x11 xrender`

test_winstack_SOURCES = test-large-window-stack.c
test_winstack_CFLAGS = `pkg-config --cflags hildon-1`
test_winstack_LDFLAGS = `pkg-config --libs hildon-1`

test_non_compositing_SOURCES = test-non-compositing.c
test_non_compositing_CFLAGS = `pkg-config --cflags hildon-1`
test_non_compositing_LDFLAGS = `pkg-config --libs hildon-1 --libs x11`

test_no_gtk_SOURCES = test-no-gtk.c
test_no_gtk_CFLAGS = `pkg-config --cflags x11` 
test_no_gtk_LDFLAGS = `pkg-config --libs x11`
