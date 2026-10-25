/*
 * starter-web-ui - config.h
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WUI_CONFIG_H_INCLUDED
#define WUI_CONFIG_H_INCLUDED

#include "app.h"

G_BEGIN_DECLS

gboolean wui_config_load (WuiApp *app, const gchar *path, GError **error);

G_END_DECLS

#endif
