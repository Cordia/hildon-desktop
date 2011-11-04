#!/bin/sh

export HILDON_DESKTOP_APPS_PRESTART=auto

# sadly, we have to use dsmetool here, so we cant respawn session..
/usr/sbin/dsmetool -c 3 -T 180 -m -17 -f /usr/bin/hildon-desktop

