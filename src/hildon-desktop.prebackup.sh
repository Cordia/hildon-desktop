#! /bin/sh
HILDON_DESKTOP_DIR=$HOME/.config/hildon-desktop
HOME_USER_PLUGINS=$HILDON_DESKTOP_DIR/home.plugins
HOME_USER_PLUGINS_BACKUP=$HOME_USER_PLUGINS-backup
HOME_SYSTEM_PLUGINS=/etc/hildon-desktop/home.plugins

if [ ! -d "$HILDON_DESKTOP_DIR" ] ; then
  mkdir -p "$HILDON_DESKTOP_DIR";
fi

if [ ! -f "$HOME_USER_PLUGINS" ] ; then
  cp "$HOME_SYSTEM_PLUGINS" "$HOME_USER_PLUGINS"
fi

cp -f "$HOME_USER_PLUGINS" "$HOME_USER_PLUGINS_BACKUP"
