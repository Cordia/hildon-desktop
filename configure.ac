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

CFLAGS="$CFLAGS -Wall -Wno-format-extra-args -ansi -Wmissing-prototypes -Wmissing-declarations -Werror -std=c99 -rdynamic"

AC_SUBST(CFLAGS)

# gettext
ALL_LINGUAS="en_GB"
AC_SUBST(ALL_LINGUAS)
AM_GLIB_GNU_GETTEXT
AC_DEFINE(GETTEXT_PACKAGE, ["maemo-af-desktop"], [Localisation domain])

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
		       libmatchbox2-0.1 dnl
		       gtk+-2.0 dnl
		       gconf-2.0 dnl
		       hildon-1 dnl
		       libhildondesktop-1 >= 2.1.6 dnl
		       libcanberra dnl
		       xft])

HD_INCS='-I$(top_srcdir)/src -I$(top_srcdir)/src/home -I$(top_srcdir)/src/mb -I$(top_srcdir)/src/util'

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
data/Makefile
data/icons/Makefile
data/icons/48x48/Makefile
data/desktop/Makefile
tests/Makefile
])

AC_OUTPUT
