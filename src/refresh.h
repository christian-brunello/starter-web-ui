/*
 * starter-web-ui - refresh.h
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WUI_REFRESH_H_INCLUDED
#define WUI_REFRESH_H_INCLUDED

#include "app.h"

G_BEGIN_DECLS

gboolean wui_refresh_widget (WuiApp *app, WuiWidget *w, GError **error);
void wui_refresh_schedule_all (WuiApp *app);
void wui_refresh_unschedule_all (WuiApp *app);

G_END_DECLS

#endif
