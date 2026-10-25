/*
 * starter-web-ui - user-config.h
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WUI_USER_CONFIG_H_INCLUDED
#define WUI_USER_CONFIG_H_INCLUDED

#include "app.h"

G_BEGIN_DECLS

gboolean wui_user_config_load (WuiApp *app, GError **error);
gboolean wui_user_config_save (WuiApp *app, GError **error);
WuiWidget *wui_app_find_widget (WuiApp *app, const gchar *id);

G_END_DECLS

#endif
