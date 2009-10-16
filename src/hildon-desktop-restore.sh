#! /bin/sh
HILDON_DESKTOP_DIR=$HOME/.config/hildon-desktop
HOME_USER_PLUGINS=$HILDON_DESKTOP_DIR/home.plugins
HOME_USER_PLUGINS_BACKUP=$HOME_USER_PLUGINS-backup

if [ "$1" ]; then
        if [ ! -z "`grep "$HOME_USER_PLUGINS_BACKUP" "$1"`" ]; then
		if [ -f "$HOME_USER_PLUGINS_BACKUP" ] ; then
			mv -f "$HOME_USER_PLUGINS_BACKUP" "$HOME_USER_PLUGINS"
		fi
	fi
        if [ ! -z "`grep "/tmp/hildon-home-task-shortcuts.default" "$1"`" ]; then
		if [ -f "/tmp/hildon-home-task-shortcuts.default" ] ; then
			gconftool-2 --unset /apps/osso/hildon-home/task-shortcuts
		fi
	fi
        if [ ! -z "`grep "/tmp/hildon-home-bookmark-shortcuts.default" "$1"`" ]; then
		if [ -f "/tmp/hildon-home-bookmark-shortcuts.default" ] ; then
			gconftool-2 --unset /apps/osso/hildon-home/bookmark-shortcuts
		fi
	fi
        if [ ! -z "`grep "/tmp/hildon-desktop-bg-image-1.default" "$1"`" ]; then
		if [ -f "/tmp/hildon-desktop-bg-image-1.default" ] ; then
			gconftool-2 --unset /apps/osso/hildon-desktop/views/1/bg-image
		fi
	fi
        if [ ! -z "`grep "/tmp/hildon-desktop-bg-image-2.default" "$1"`" ]; then
		if [ -f "/tmp/hildon-desktop-bg-image-2.default" ] ; then
			gconftool-2 --unset /apps/osso/hildon-desktop/views/2/bg-image
		fi
	fi
        if [ ! -z "`grep "/tmp/hildon-desktop-bg-image-3.default" "$1"`" ]; then
		if [ -f "/tmp/hildon-desktop-bg-image-3.default" ] ; then
			gconftool-2 --unset /apps/osso/hildon-desktop/views/3/bg-image
		fi
	fi
        if [ ! -z "`grep "/tmp/hildon-desktop-bg-image-4.default" "$1"`" ]; then
		if [ -f "/tmp/hildon-desktop-bg-image-4.default" ] ; then
			gconftool-2 --unset /apps/osso/hildon-desktop/views/4/bg-image
		fi
	fi
fi

# remove application loading screenshots
if [ -d $HOME/.cache/launch ]; then
        rm -rf $HOME/.cache/launch/*
fi

kill `pidof hildon-home`
