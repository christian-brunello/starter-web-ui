/*
 * starter-web-ui - bridge.h
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WUI_BRIDGE_H_INCLUDED
#define WUI_BRIDGE_H_INCLUDED

#include "app.h"

G_BEGIN_DECLS

/* Same bit as ST_IO_FLAG_USR1 in starter-core. */
#define WUI_ST_IO_FLAG_USR1 0x1000000000000000ULL

void wui_bridge_init (WuiApp *app);
void wui_bridge_shutdown (WuiApp *app);

/* GET path (e.g. /io or /input/DigIn1) → body string (caller frees) */
gchar *wui_bridge_get (WuiApp *app, const gchar *path, GError **error);

/* PUT JSON body; retries with bouncing backoff (daemon config). */
gboolean wui_bridge_put (WuiApp *app, const gchar *path, const gchar *json_body,
                         GError **error);

/*
 * Set input val and OR USR1 into flags (GET current flags first).
 * Schedules usr1-clear-timeout-sec to clear USR1 on that widget's entity.
 */
gboolean wui_bridge_set_input_val_usr1 (WuiApp *app, WuiWidget *w, gdouble val,
                                        GError **error);

G_END_DECLS

#endif
