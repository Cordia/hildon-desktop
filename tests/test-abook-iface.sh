#!/bin/sh

gconftool-2 -t list --list-type=string \
  --set /apps/osso/hildon-desktop/addressbook_dbus_interface '[a,b,c]'
