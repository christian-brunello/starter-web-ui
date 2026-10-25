/*
 * starter-web-ui - auth.c
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "auth.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static void
session_free (gpointer p)
{
  WuiSession *s = p;
  if (!s)
    return;
  g_free (s->id);
  g_free (s->user_id);
  g_free (s);
}

void
wui_auth_init (WuiApp *app)
{
  app->sessions = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, session_free);
}

void
wui_auth_shutdown (WuiApp *app)
{
  if (app->sessions)
    {
      g_hash_table_destroy (app->sessions);
      app->sessions = NULL;
    }
}

const gchar *
wui_group_name (WuiGroup g)
{
  return g == WUI_GROUP_ADMIN ? "admin" : "user";
}

static gchar *
random_sid (void)
{
  guint8 buf[16];
  guint i;
  GString *s = g_string_new (NULL);
  for (i = 0; i < sizeof (buf); i++)
    buf[i] = (guint8) g_random_int_range (0, 256);
  for (i = 0; i < sizeof (buf); i++)
    g_string_append_printf (s, "%02x", buf[i]);
  return g_string_free (s, FALSE);
}

static gchar *
cookie_value_from_headers (SoupMessageHeaders *headers, const gchar *name)
{
  const char *cookie = soup_message_headers_get_list (headers, "Cookie");
  gchar **parts;
  guint i;
  gchar *found = NULL;

  if (cookie == NULL)
    return NULL;
  parts = g_strsplit (cookie, ";", -1);
  for (i = 0; parts[i]; i++)
    {
      gchar *p = g_strstrip (parts[i]);
      gchar *eq = strchr (p, '=');
      if (!eq)
        continue;
      *eq = '\0';
      if (g_strcmp0 (g_strstrip (p), name) == 0)
        {
          found = g_strdup (g_strstrip (eq + 1));
          break;
        }
    }
  g_strfreev (parts);
  return found;
}

WuiSession *
wui_auth_session_from_message (WuiApp *app, SoupServerMessage *msg)
{
  gchar *sid;
  WuiSession *s;
  gint64 now;

  if (app->auth_method == WUI_AUTH_NONE)
    return NULL;

  sid = cookie_value_from_headers (soup_server_message_get_request_headers (msg),
                                   app->cookie_name);
  if (sid == NULL)
    return NULL;
  s = g_hash_table_lookup (app->sessions, sid);
  g_free (sid);
  if (s == NULL)
    return NULL;
  now = g_get_real_time () / G_USEC_PER_SEC;
  if (s->expires_unix < now)
    {
      g_hash_table_remove (app->sessions, s->id);
      return NULL;
    }
  return s;
}

gboolean
wui_auth_require (WuiApp *app, SoupServerMessage *msg, WuiGroup min_group,
                  WuiSession **out_session)
{
  WuiSession *s;

  if (out_session)
    *out_session = NULL;

  if (app->auth_method == WUI_AUTH_NONE)
    return TRUE;

  s = wui_auth_session_from_message (app, msg);
  if (s == NULL)
    {
      soup_server_message_set_status (msg, SOUP_STATUS_UNAUTHORIZED, NULL);
      soup_server_message_set_response (msg, "application/json", SOUP_MEMORY_COPY,
                                        "{\"error\":\"unauthorized\"}", -1);
      return FALSE;
    }
  if (min_group == WUI_GROUP_ADMIN && s->group != WUI_GROUP_ADMIN)
    {
      soup_server_message_set_status (msg, SOUP_STATUS_FORBIDDEN, NULL);
      soup_server_message_set_response (msg, "application/json", SOUP_MEMORY_COPY,
                                        "{\"error\":\"forbidden\"}", -1);
      return FALSE;
    }
  if (out_session)
    *out_session = s;
  return TRUE;
}

gboolean
wui_auth_login (WuiApp *app, const gchar *user, const gchar *password,
                gchar **set_cookie, GError **error)
{
  WuiSession *s;
  gint64 now;

  if (app->auth_method != WUI_AUTH_PASSWORD)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "auth disabled");
      return FALSE;
    }
  if (g_strcmp0 (user, app->auth_user) != 0
      || g_strcmp0 (password, app->auth_password) != 0)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "invalid credentials");
      return FALSE;
    }

  s = g_new0 (WuiSession, 1);
  s->id = random_sid ();
  s->user_id = g_strdup (user);
  s->group = app->auth_group;
  now = g_get_real_time () / G_USEC_PER_SEC;
  s->expires_unix = now + (gint64) app->session_days * 86400;
  g_hash_table_insert (app->sessions, s->id, s);

  if (set_cookie)
    {
      *set_cookie = g_strdup_printf (
          "%s=%s; Path=/; Max-Age=%u; HttpOnly; SameSite=Lax",
          app->cookie_name, s->id, app->session_days * 86400);
    }
  return TRUE;
}

void
wui_auth_logout (WuiApp *app, SoupServerMessage *msg, gchar **set_cookie)
{
  WuiSession *s = wui_auth_session_from_message (app, msg);
  if (s)
    g_hash_table_remove (app->sessions, s->id);
  if (set_cookie)
    {
      *set_cookie = g_strdup_printf (
          "%s=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax", app->cookie_name);
    }
}
