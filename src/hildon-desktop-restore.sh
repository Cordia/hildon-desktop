#! /bin/sh
HILDON_DESKTOP_DIR=$HOME/.config/hildon-desktop
HOME_USER_PLUGINS=$HILDON_DESKTOP_DIR/home.plugins
HOME_USER_PLUGINS_BACKUP=$HOME_USER_PLUGINS-backup

if [ "$1" ]; then
        if [ -z "`grep "$HOME_USER_PLUGINS_BACKUP" "$1"`" ]; then
                exit 0;
        fi

	if [ -f "$HOME_USER_PLUGINS_BACKUP" ] ; then
		mv -f "$HOME_USER_PLUGINS_BACKUP" "$HOME_USER_PLUGINS"
	fi
fi

kill `pidof hildon-home`
