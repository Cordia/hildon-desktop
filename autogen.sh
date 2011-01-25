#!/bin/sh
( cd clutter_0_8  ; autoreconf -v --install ) || exit 1
( cd libmatchbox2 ; autoreconf -v --install ) || exit 1
autoreconf -v --install || exit 1
