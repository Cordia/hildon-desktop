AC_INIT(hildon-desktop, 2.9.0)

AM_INIT_AUTOMAKE

AM_CONFIG_HEADER(config.h)

AC_CANONICAL_HOST

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

AC_C_CONST
AC_HEADER_STDC

AC_HEADER_STDBOOL

AC_CHECK_HEADERS([ftw.h],
                 AC_DEFINE([HAVE_FTW_H], 1,
                           [Define to 1 if ftw.h is available]))
AC_CHECK_FUNCS([nftw])

AC_MSG_CHECKING([for GNU ftw extensions])
AC_TRY_COMPILE([#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#include <ftw.h>], [int flags = FTW_ACTIONRETVAL;], gtk_ok=yes, gtk_ok=no)
if test $gtk_ok = yes; then
  AC_MSG_RESULT([yes])
  AC_DEFINE(HAVE_GNU_FTW, 1, [Have GNU ftw])
else
  AC_MSG_RESULT([no])
fi

AM_PROG_CC_C_O

AC_PATH_PROG(DBUS_BINDING_TOOL, dbus-binding-tool)

#++++++++++++++++++
# Misc build flags
#++++++++++++++++++

CFLAGS="$CFLAGS -Wall -Wno-format-extra-args -ansi -Wmissing-prototypes -Wmissing-declarations -Wno-error -std=gnu99 -DG_DISABLE_CAST_CHECKS -fgnu89-inline"

AC_SUBST(CFLAGS)

# gettext
ALL_LINGUAS="en_GB"
AC_SUBST(ALL_LINGUAS)
AM_GLIB_GNU_GETTEXT
AC_DEFINE(GETTEXT_PACKAGE, ["maemo-af-desktop"], [Localisation domain])

# check if MB2 should be linked statically
AC_ARG_ENABLE(static_mb2,
             [  --enable-static-mb2  Link to MB2 statically [default=no]],
             [ac_cv_enable_static_mb2=$enableval],[ac_cv_enable_static_mb2=no])
AC_MSG_CHECKING([whether to statically link Matchbox2])
if test "$ac_cv_enable_static_mb2" = yes; then
       AC_MSG_RESULT(yes)
else
       AC_MSG_RESULT(no)
fi

# Use maemo-launcher?
AC_ARG_ENABLE(maemo-launcher,
  [  --enable-maemo-launcher compile with maemo-launcher [default=no]],,
  enable_maemo_launcher=no)

if test "x$enable_maemo_launcher" = "xyes"; then
  PKG_CHECK_MODULES(MAEMO_LAUNCHER, [maemo-launcher-app])
  AC_SUBST(MAEMO_LAUNCHER_CFLAGS)
  AC_SUBST(MAEMO_LAUNCHER_LIBS)
fi

# do we want a11y support?
AC_ARG_ENABLE(a11y,
             [  --enable-a11y   Enable Accessibility (requires cail) [default=no]],
             [ac_cv_enable_a11y=$enableval],[ac_cv_enable_a11y=no])

AC_MSG_CHECKING([whether to enable a11y])
AC_PATH_PROG(PKG_CONFIG, pkg-config, yes)
if test "$ac_cv_enable_a11y" = yes; then
       echo "ac_cv_enable_a11y: Forced off."
       ac_cv_enable_a11y=no
       if test "x$PKG_CONFIG" != "xno"; then
         echo "ac_cv_enable_a11y: Have pkg-config: $PKG_CONFIG"
         if $PKG_CONFIG --exists cail; then
           echo "ac_cv_enable_a11y: pkg-config says cail exists"
           ac_cv_enable_a11y=yes ;
           echo "ac_cv_enable_a11y: Turned on."
         else
           echo "ac_cv_enable_a11y: $PKG_CONFIG --exists cail failed"
         fi
       else
         echo "ac_cv_enable_a11y: Do not have pkg-config: $PKG_CONFIG"
       fi
fi
if test "$ac_cv_enable_a11y" = yes; then
       AC_MSG_RESULT(yes)
       PKG_CHECK_MODULES(HDA, cail)
       AC_SUBST(HDA_LIBS)
       AC_SUBST(HDA_CFLAGS)
       A11Y_DIR="a11y"
       AC_SUBST(A11Y_DIR)
       A11Y_LA="a11y/liba11y.la"
       AC_SUBST(A11Y_LA)
else
       AC_MSG_RESULT(no)
       AC_DEFINE(DISABLE_A11Y, [], ["Disabled a11y macro"])
fi

#+++++++++++++++++++++
# Dependencies checks
#+++++++++++++++++++++

if test "$ac_cv_enable_static_mb2" = yes; then
        PKG_CHECK_MODULES([MB2], [libmatchbox2-0.1 xext xrender libpng])
        MB2_STATIC_LIB='/usr/lib/libmatchbox2-0.1.a -lexpat'

        PKG_CHECK_MODULES(HD, [clutter-0.8 dnl
                       glib-2.0 >= 2.12 dnl
                       dbus-glib-1 dnl
                       gthread-2.0 dnl
                       dbus-1 dnl
                       x11 dnl
                       xcomposite dnl
                       xfixes dnl
                       xrandr dnl
                       xi dnl
                       gtk+-2.0 dnl
                       gconf-2.0 dnl
                       hildon-1 dnl
                       libhildondesktop-1 dnl
                       libcanberra dnl
                       libgnome-menu dnl
                       gdk-pixbuf-xlib-2.0 dnl
                       pangoxft dnl
                       xft dnl
                       dnl MAEGO_DISABLED dsme dnl TODO HAVE_DSME Convert MCE to DSME
                       ])
else
        MB2_CFLAGS=''
        MB2_STATIC_LIB=''
        PKG_CHECK_MODULES(HD, [clutter-0.8 dnl
                       glib-2.0 >= 2.12 dnl
                       dbus-glib-1 dnl
                       gthread-2.0 dnl
                       dbus-1 dnl
                       x11 dnl
                       xcomposite dnl
                       xfixes dnl
                       xrandr dnl
                       xi dnl
                       gtk+-2.0 dnl
                       gconf-2.0 dnl
                       hildon-1 dnl
                       libhildondesktop-1 dnl
                       libcanberra dnl
                       libgnome-menu dnl
                       gdk-pixbuf-xlib-2.0 dnl
                       xft dnl
                       libmatchbox2-0.1 dnl
                       dnl MAEGO_DISABLED dsme dnl TODO HAVE_DSME Convert MCE to DSME
                       ])
fi

HD_INCS='-I$(top_srcdir)/src -I$(top_srcdir)/src/home -I$(top_srcdir)/src/mb -I$(top_srcdir)/src/util -I$(top_srcdir)/src/a11y'

# only CFLAGS are needed from MB2 since we are statically linking
AC_SUBST(MB2_CFLAGS)
AC_SUBST(MB2_STATIC_LIB)

AC_SUBST(HD_LIBS)
AC_SUBST(HD_CFLAGS)
AC_SUBST(HD_INCS)

AC_CONFIG_FILES([
Makefile
src/Makefile
src/mb/Makefile
src/home/Makefile
src/launcher/Makefile
src/tidy/Makefile
src/util/Makefile
src/a11y/Makefile
src/a11y/launcher/Makefile
src/a11y/home/Makefile
src/a11y/tail/Makefile
data/Makefile
data/menu/Makefile
data/applications/Makefile
tests/Makefile
])

AC_OUTPUT
