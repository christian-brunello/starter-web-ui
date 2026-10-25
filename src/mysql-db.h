/*
 * starter-web-ui - mysql-db.h
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WUI_MYSQL_DB_H_INCLUDED
#define WUI_MYSQL_DB_H_INCLUDED

#include "app.h"

G_BEGIN_DECLS

/* Run SELECT; returns JSON array of row objects, or NULL on error.
 * Only SELECT allowed. */
gchar *wui_mysql_query_json (WuiApp *app, const gchar *sql, GError **error);

/* Labels catalog as JSON array [{name,description,type,...}, ...] */
gchar *wui_mysql_labels_json (WuiApp *app, GError **error);

G_END_DECLS

#endif
