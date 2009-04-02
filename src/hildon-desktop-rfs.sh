#!/bin/sh

CFGDIR=$HOME/.config/hildon-desktop

for file in $(ls -A $CFGDIR); do
  if ! echo $file | grep -q home.plugins; then
    rm -rf $file
  fi
done

if ! rmdir $CFGDIR; then
  echo -n '' # NOOP
fi
