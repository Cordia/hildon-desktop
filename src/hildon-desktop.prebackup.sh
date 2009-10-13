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

[ -z `gconftool-2 --ignore-schema-defaults -g /apps/osso/hildon-home/task-shortcuts` ] && touch /tmp/hildon-home-task-shortcuts.default
[ -z `gconftool-2 --ignore-schema-defaults -g /apps/osso/hildon-home/bookmark-shortcuts` ] && touch /tmp/hildon-home-bookmark-shortcuts.default
[ -z `gconftool-2 --ignore-schema-defaults -g /apps/osso/hildon-desktop/views/1/bg-image` ] && touch /tmp/hildon-desktop-bg-image-1.default
[ -z `gconftool-2 --ignore-schema-defaults -g /apps/osso/hildon-desktop/views/2/bg-image` ] && touch /tmp/hildon-desktop-bg-image-2.default
[ -z `gconftool-2 --ignore-schema-defaults -g /apps/osso/hildon-desktop/views/3/bg-image` ] && touch /tmp/hildon-desktop-bg-image-3.default
[ -z `gconftool-2 --ignore-schema-defaults -g /apps/osso/hildon-desktop/views/4/bg-image` ] && touch /tmp/hildon-desktop-bg-image-4.default

