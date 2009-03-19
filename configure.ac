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

CFLAGS="$CFLAGS -Wall -Wno-format-extra-args -ansi -Wmissing-prototypes -Wmissing-declarations -Werror -std=c99 -rdynamic -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS"

AC_SUBST(CFLAGS)

# gettext
ALL_LINGUAS="en_GB"
AC_SUBST(ALL_LINGUAS)
AM_GLIB_GNU_GETTEXT
AC_DEFINE(GETTEXT_PACKAGE, ["maemo-af-desktop"], [Localisation domain])


# do we want a11y support?
AC_ARG_ENABLE(a11y,
             [ --enable-a11y   Enable Accessibility (requires cail) [default=no]],
             [ac_cv_enable_a11y=$enableval],[ac_cv_enable_a11y=no])
AC_MSG_CHECKING([whether to enable a11y])
if test -z "$PKG_CONFIG"; then
       echo "Going to check for pkg-config ..."
       AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
       echo "Checked for pkg-config: $PKG_CONFIG"
fi
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

PKG_CHECK_MODULES(HD, [clutter-0.8 dnl
                       glib-2.0 >= 2.12 dnl
                       gthread-2.0 dnl
		       dbus-1 dnl
		       x11 dnl
		       xcomposite dnl
		       xfixes dnl
		       xrandr dnl
		       libmatchbox2-0.1 dnl
		       gtk+-2.0 dnl
		       gconf-2.0 dnl
		       hildon-1 dnl
		       libcanberra dnl
		       gnome-vfs-2.0 dnl
		       gdk-pixbuf-xlib-2.0 dnl
		       xft dnl
                       mce])

HD_INCS='-I$(top_srcdir)/src -I$(top_srcdir)/src/home -I$(top_srcdir)/src/mb -I$(top_srcdir)/src/util -I$(top_srcdir)/src/a11y'

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
data/desktop/Makefile
tests/Makefile
])

AC_OUTPUT
