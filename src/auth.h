/*
 * starter-web-ui - auth.h
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WUI_AUTH_H_INCLUDED
#define WUI_AUTH_H_INCLUDED

#include "app.h"
#include <libsoup/soup.h>

G_BEGIN_DECLS

typedef struct {
  gchar *id;
  gchar *user_id;
  WuiGroup group;
  gint64 expires_unix;
} WuiSession;

void wui_auth_init (WuiApp *app);
void wui_auth_shutdown (WuiApp *app);

/* NULL session + auth none => allowed as admin. */
WuiSession *wui_auth_session_from_message (WuiApp *app, SoupServerMessage *msg);
gboolean wui_auth_require (WuiApp *app, SoupServerMessage *msg, WuiGroup min_group,
                           WuiSession **out_session);

gboolean wui_auth_login (WuiApp *app, const gchar *user, const gchar *password,
                         gchar **set_cookie, GError **error);
void wui_auth_logout (WuiApp *app, SoupServerMessage *msg, gchar **set_cookie);

const gchar *wui_group_name (WuiGroup g);

G_END_DECLS

#endif
