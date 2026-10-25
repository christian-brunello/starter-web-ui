/*
 * starter-web-ui - http.h
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WUI_HTTP_H_INCLUDED
#define WUI_HTTP_H_INCLUDED

#include "app.h"

G_BEGIN_DECLS

gboolean wui_http_start (WuiApp *app, GError **error);
void wui_http_stop (WuiApp *app);

G_END_DECLS

#endif
