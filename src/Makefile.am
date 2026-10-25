#
# starter-web-ui - src/Makefile.am
#
# Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>

bin_PROGRAMS = starter-web-ui

starter_web_ui_SOURCES = \
	main.c \
	app.h \
	wui-config.c \
	wui-config.h \
	user-config.c \
	user-config.h \
	auth.c \
	auth.h \
	bridge.c \
	bridge.h \
	mysql-db.c \
	mysql-db.h \
	guile-host.c \
	guile-host.h \
	guile-api.c \
	guile-api.h \
	refresh.c \
	refresh.h \
	http.c \
	http.h

starter_web_ui_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(ST_WUI_CFLAGS) \
	-I$(builddir) -I$(srcdir) \
	-DWEB_UI_SCHEME_DIR=\"$(schemedir)\" \
	-DWEB_UI_STATIC_DIR=\"$(staticdir)\" \
	-DHAVE_LIBSOUP_3

starter_web_ui_LDADD = $(ST_WUI_LIBS)
starter_web_ui_CFLAGS = $(AM_CFLAGS)
