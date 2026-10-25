/*
 * starter-web-ui - guile-host.h
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WUI_GUILE_HOST_H_INCLUDED
#define WUI_GUILE_HOST_H_INCLUDED

#include "app.h"

G_BEGIN_DECLS

gboolean wui_guile_init (WuiApp *app, GError **error);

G_END_DECLS

#endif
