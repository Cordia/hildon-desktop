AC_INIT(hildon-desktop, 2.9.0)

AM_INIT_AUTOMAKE

AM_CONFIG_HEADER(config.h)

AC_CANONICAL_HOST

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_PROG_LIBTOOL

AC_C_CONST
AC_HEADER_STDC

AC_HEADER_STDBOOL


#++++++++++++++++++
# Misc build flags
#++++++++++++++++++

CFLAGS="$CFLAGS -Wall -ansi -Wmissing-prototypes -Wmissing-declarations -Werror -std=c99 -rdynamic"

AC_SUBST(CFLAGS)

#+++++++++++++++++++++
# Dependencies checks
#+++++++++++++++++++++

PKG_CHECK_MODULES(HD, clutter-0.7 \
                      glib-2.0 >= 2.12 \
		      dbus-1 \
		      x11 \
		      xcomposite \
		      xfixes \
		      libmatchbox2-0.1 \
		      gtk+-2.0 \
		      xft)

HD_INCS='-I$(top_srcdir)/src -I$(top_srcdir)/src/home -I$(top_srcdir)/src/mb -I$(top_srcdir)/src/util'

AC_SUBST(HD_LIBS)
AC_SUBST(HD_CFLAGS)
AC_SUBST(HD_INCS)

AC_CONFIG_FILES([
Makefile
src/Makefile
src/mb/Makefile
src/home/Makefile
src/util/Makefile
data/Makefile
data/icons/Makefile
data/icons/48x48/Makefile
])

AC_OUTPUT
