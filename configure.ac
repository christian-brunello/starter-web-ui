#
# starter-web-ui - configure.ac
#
# Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
#
# Process this file with autoconf to produce configure.

AC_INIT([starter-web-ui], [1.0.0], [startercontroller@gmail.com])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_MACRO_DIRS([m4])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_PROG_CC
AM_PROG_AR
LT_INIT

PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.44 gobject-2.0 >= 2.44 gio-2.0 >= 2.44])
PKG_CHECK_MODULES([LIBXML2], [libxml-2.0 >= 2.9.0])
PKG_CHECK_MODULES([JSON_GLIB], [json-glib-1.0])
PKG_CHECK_MODULES([LIBSOUP], [libsoup-3.0])
PKG_CHECK_MODULES([GUILE], [guile-2.2])
PKG_CHECK_MODULES([MYSQL], [mysqlclient])

AC_DEFINE([HAVE_LIBSOUP_3], [1], [Building against libsoup 3.x])

ST_WUI_CFLAGS="$GLIB_CFLAGS $LIBXML2_CFLAGS $JSON_GLIB_CFLAGS $LIBSOUP_CFLAGS $GUILE_CFLAGS $MYSQL_CFLAGS"
ST_WUI_LIBS="$GLIB_LIBS $LIBXML2_LIBS $JSON_GLIB_LIBS $LIBSOUP_LIBS $GUILE_LIBS $MYSQL_LIBS"

AC_SUBST(ST_WUI_CFLAGS)
AC_SUBST(ST_WUI_LIBS)

schemedir='${datadir}/starter-web-ui/scheme'
staticdir='${datadir}/starter-web-ui/static'
AC_SUBST(schemedir)
AC_SUBST(staticdir)

AC_CONFIG_FILES([
  Makefile
  src/Makefile
  examples/Makefile
  doc/Makefile
  scheme/Makefile
  static/Makefile
])

AC_OUTPUT
