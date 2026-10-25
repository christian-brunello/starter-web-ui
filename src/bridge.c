/*
 * starter-web-ui - bridge.c
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "bridge.h"
#include "user-config.h"

#include <string.h>
#include <stdlib.h>
#include <json-glib/json-glib.h>

void
wui_bridge_init (WuiApp *app)
{
  app->bridge_session = soup_session_new ();
}

void
wui_bridge_shutdown (WuiApp *app)
{
  g_clear_object (&app->bridge_session);
}

static SoupMessage *
make_msg (WuiApp *app, const char *method, const gchar *path)
{
  g_autofree gchar *url = NULL;
  SoupMessage *msg;

  if (path == NULL || path[0] != '/')
    return NULL;
  url = g_strdup_printf ("%s%s", app->bridge_base_url, path);
  msg = soup_message_new (method, url);
  if (msg && app->bridge_token && *app->bridge_token)
    {
      g_autofree gchar *auth = g_strdup_printf ("Bearer %s", app->bridge_token);
      soup_message_headers_replace (soup_message_get_request_headers (msg),
                                    "Authorization", auth);
    }
  return msg;
}

static gboolean
put_once (WuiApp *app, const gchar *path, const gchar *json_body, GError **error)
{
  SoupMessage *msg;
  GBytes *body;
  GBytes *resp;
  gboolean ok;

  msg = make_msg (app, "PUT", path);
  if (!msg)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "bad path");
      return FALSE;
    }
  body = g_bytes_new (json_body ? json_body : "", json_body ? strlen (json_body) : 0);
  soup_message_set_request_body_from_bytes (msg, "application/json", body);
  g_bytes_unref (body);
  resp = soup_session_send_and_read (app->bridge_session, msg, NULL, error);
  ok = resp != NULL && soup_message_get_status (msg) < 400;
  if (resp == NULL)
    {
      /* error already set */
    }
  else if (!ok)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "bridge HTTP %u",
                   soup_message_get_status (msg));
    }
  if (resp)
    g_bytes_unref (resp);
  g_object_unref (msg);
  return ok;
}

gchar *
wui_bridge_get (WuiApp *app, const gchar *path, GError **error)
{
  SoupMessage *msg;
  GBytes *bytes;
  gconstpointer data;
  gsize len;
  gchar *out;

  msg = make_msg (app, "GET", path);
  if (!msg)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "bad path");
      return NULL;
    }
  bytes = soup_session_send_and_read (app->bridge_session, msg, NULL, error);
  if (bytes == NULL)
    {
      g_object_unref (msg);
      return NULL;
    }
  if (soup_message_get_status (msg) >= 400)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "bridge HTTP %u",
                   soup_message_get_status (msg));
      g_bytes_unref (bytes);
      g_object_unref (msg);
      return NULL;
    }
  data = g_bytes_get_data (bytes, &len);
  out = g_strndup (data, len);
  g_bytes_unref (bytes);
  g_object_unref (msg);
  return out;
}

gboolean
wui_bridge_put (WuiApp *app, const gchar *path, const gchar *json_body, GError **error)
{
  guint attempts = app->bridge_put_retries ? app->bridge_put_retries : 1;
  guint base_ms = app->bridge_put_backoff_ms ? app->bridge_put_backoff_ms : 100;
  guint i;
  GError *last = NULL;

  for (i = 0; i < attempts; i++)
    {
      g_clear_error (&last);
      if (put_once (app, path, json_body, &last))
        {
          g_clear_error (&last);
          return TRUE;
        }
      if (i + 1 < attempts)
        {
          guint delay = base_ms << i; /* bounce: 100, 200, 400, … */
          if (delay > 5000)
            delay = 5000;
          g_usleep ((gulong) delay * 1000UL);
        }
    }
  if (last)
    g_propagate_error (error, last);
  else
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "bridge PUT failed");
  return FALSE;
}

static gboolean
read_input_flags (WuiApp *app, const gchar *domain, const gchar *name, guint64 *flags_out,
                  GError **error)
{
  g_autofree gchar *path = NULL;
  g_autofree gchar *json = NULL;
  JsonParser *p;
  JsonObject *o;

  path = g_strdup_printf ("/%s/%s", domain, name);
  json = wui_bridge_get (app, path, error);
  if (!json)
    return FALSE;
  p = json_parser_new ();
  if (!json_parser_load_from_data (p, json, -1, error))
    {
      g_object_unref (p);
      return FALSE;
    }
  o = json_node_get_object (json_parser_get_root (p));
  if (!json_object_has_member (o, "flags"))
    *flags_out = 0;
  else
    *flags_out = (guint64) json_object_get_int_member (o, "flags");
  g_object_unref (p);
  return TRUE;
}

typedef struct {
  WuiApp *app;
  gchar *domain;
  gchar *name;
  gchar *widget_id;
} Usr1Clear;

static void
usr1_clear_free (gpointer p)
{
  Usr1Clear *c = p;
  if (!c)
    return;
  g_free (c->domain);
  g_free (c->name);
  g_free (c->widget_id);
  g_free (c);
}

static gboolean
on_usr1_clear (gpointer user_data)
{
  Usr1Clear *c = user_data;
  WuiWidget *w;
  guint64 flags = 0;
  GError *err = NULL;
  g_autofree gchar *path = NULL;
  g_autofree gchar *body = NULL;

  w = wui_app_find_widget (c->app, c->widget_id);
  if (w)
    w->usr1_clear_source = 0;

  if (!read_input_flags (c->app, c->domain, c->name, &flags, &err))
    {
      g_warning ("web-ui: USR1 clear GET failed: %s", err ? err->message : "?");
      g_clear_error (&err);
      return G_SOURCE_REMOVE;
    }
  flags &= ~WUI_ST_IO_FLAG_USR1;
  path = g_strdup_printf ("/%s/%s/flags", c->domain, c->name);
  body = g_strdup_printf ("%" G_GUINT64_FORMAT, flags);
  if (!wui_bridge_put (c->app, path, body, &err))
    {
      g_warning ("web-ui: USR1 clear PUT failed: %s", err ? err->message : "?");
      g_clear_error (&err);
    }
  return G_SOURCE_REMOVE;
}

static void
schedule_usr1_clear (WuiApp *app, WuiWidget *w)
{
  Usr1Clear *c;

  if (app->usr1_clear_timeout_sec == 0 || w == NULL)
    return;

  if (w->usr1_clear_source)
    {
      g_source_remove (w->usr1_clear_source);
      w->usr1_clear_source = 0;
    }

  c = g_new0 (Usr1Clear, 1);
  c->app = app;
  c->domain = g_strdup (w->entity_domain ? w->entity_domain : "input");
  c->name = g_strdup (w->entity_name ? w->entity_name : "");
  c->widget_id = g_strdup (w->id);
  w->usr1_clear_source = g_timeout_add_seconds_full (
      G_PRIORITY_DEFAULT, app->usr1_clear_timeout_sec, on_usr1_clear, c, usr1_clear_free);
}

gboolean
wui_bridge_set_input_val_usr1 (WuiApp *app, WuiWidget *w, gdouble val, GError **error)
{
  const gchar *domain;
  const gchar *name;
  guint64 flags = 0;
  g_autofree gchar *path = NULL;
  g_autofree gchar *body = NULL;

  g_return_val_if_fail (app != NULL && w != NULL, FALSE);

  domain = w->entity_domain && *w->entity_domain ? w->entity_domain : "input";
  name = w->entity_name;
  if (name == NULL || *name == '\0')
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "no entity name");
      return FALSE;
    }

  if (!read_input_flags (app, domain, name, &flags, error))
    return FALSE;

  flags |= WUI_ST_IO_FLAG_USR1;
  path = g_strdup_printf ("/%s/%s", domain, name);
  /* JSON number for flags: print as unsigned 64-bit decimal */
  body = g_strdup_printf ("{\"val\":%.10g,\"flags\":%" G_GUINT64_FORMAT "}", val, flags);
  if (!wui_bridge_put (app, path, body, error))
    return FALSE;

  schedule_usr1_clear (app, w);
  return TRUE;
}
