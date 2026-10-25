/*
 * starter-web-ui - guile-api.h
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WUI_GUILE_API_H_INCLUDED
#define WUI_GUILE_API_H_INCLUDED

#include "app.h"
#include <libguile.h>

G_BEGIN_DECLS

struct WuiWidgetType {
  gchar *name;
  gchar *label;
  GPtrArray *tags; /* gchar* */
  gchar *fields_json; /* schema as JSON string for editor */
  SCM prepare; /* may be SCM_BOOL_F */
  SCM draw;    /* required */
};

void wui_guile_api_register (WuiApp *app);
WuiWidgetType *wui_widget_type_lookup (WuiApp *app, const gchar *name);
gchar *wui_widget_types_json (WuiApp *app);

/* Call Guile prepare → string (SQL or empty); caller frees */
gchar *wui_guile_prepare (WuiApp *app, WuiWidget *w, GError **error);

/* Call Guile draw(raw_json) → view JSON; caller frees */
gchar *wui_guile_draw (WuiApp *app, WuiWidget *w, const gchar *raw_json, GError **error);

void wui_guile_invoke_hook (WuiApp *app, const char *name, WuiWidget *w, const gchar *extra_json);

G_END_DECLS

#endif
