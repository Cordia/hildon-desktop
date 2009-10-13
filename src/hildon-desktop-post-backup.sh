#!/bin/sh
HILDON_DESKTOP_DIR=$HOME/.config/hildon-desktop
HOME_USER_PLUGINS=$HILDON_DESKTOP_DIR/home.plugins
HOME_USER_PLUGINS_BACKUP=$HOME_USER_PLUGINS-backup

rm "$HOME_USER_PLUGINS_BACKUP"
rm /tmp/hildon-home-task-shortcuts.default
rm /tmp/hildon-home-bookmark-shortcuts.default
rm /tmp/hildon-desktop-bg-image-1.default
rm /tmp/hildon-desktop-bg-image-2.default
rm /tmp/hildon-desktop-bg-image-3.default
rm /tmp/hildon-desktop-bg-image-4.default
