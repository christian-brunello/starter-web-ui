/*
 * starter-web-ui - http.c
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "http.h"
#include "auth.h"
#include "user-config.h"
#include "guile-api.h"
#include "refresh.h"
#include "bridge.h"
#include "mysql-db.h"

#include <string.h>
#include <json-glib/json-glib.h>

static gchar *
resolve_static_dir (WuiApp *app)
{
  const gchar *cands[] = {
    app->static_dir,
    "static",
    "../static",
#ifdef WEB_UI_STATIC_DIR
    WEB_UI_STATIC_DIR,
#endif
    NULL
  };
  guint i;
  for (i = 0; cands[i]; i++)
    {
      g_autofree gchar *idx = NULL;
      if (!cands[i] || !g_file_test (cands[i], G_FILE_TEST_IS_DIR))
        continue;
      idx = g_build_filename (cands[i], "index.html", NULL);
      if (g_file_test (idx, G_FILE_TEST_IS_REGULAR))
        return g_strdup (cands[i]);
    }
  return NULL;
}

static void
json_reply (SoupServerMessage *msg, guint status, const gchar *body)
{
  const char *b = body ? body : "{}";
  soup_server_message_set_status (msg, status, NULL);
  soup_server_message_set_response (msg, "application/json", SOUP_MEMORY_COPY,
                                    b, strlen (b));
}

static void
add_set_cookie (SoupServerMessage *msg, const gchar *cookie)
{
  if (cookie)
    soup_message_headers_append (soup_server_message_get_response_headers (msg),
                                 "Set-Cookie", cookie);
}

static gchar *
read_body (SoupServerMessage *msg)
{
  SoupMessageBody *body = soup_server_message_get_request_body (msg);
  if (!body)
    return g_strdup ("");
  soup_message_body_flatten (body);
  return g_strndup (body->data ? body->data : "", body->length);
}

static gboolean
serve_file (SoupServerMessage *msg, const gchar *dir, const gchar *rel, const char *ctype)
{
  g_autofree gchar *path = g_build_filename (dir, rel, NULL);
  g_autofree gchar *contents = NULL;
  gsize len;
  if (!g_file_get_contents (path, &contents, &len, NULL))
    return FALSE;
  soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
  soup_server_message_set_response (msg, ctype, SOUP_MEMORY_TAKE,
                                    g_steal_pointer (&contents), len);
  return TRUE;
}

static gchar *
dashboard_json (WuiApp *app)
{
  JsonBuilder *b = json_builder_new ();
  JsonGenerator *gen;
  JsonNode *root;
  gchar *out;
  guint i;

  json_builder_begin_object (b);
  json_builder_set_member_name (b, "version");
  json_builder_add_int_value (b, 1);
  json_builder_set_member_name (b, "widgets");
  json_builder_begin_array (b);
  for (i = 0; i < app->widgets->len; i++)
    {
      WuiWidget *w = g_ptr_array_index (app->widgets, i);
      GHashTableIter iter;
      gpointer k, v;
      json_builder_begin_object (b);
      json_builder_set_member_name (b, "id");
      json_builder_add_string_value (b, w->id);
      json_builder_set_member_name (b, "type");
      json_builder_add_string_value (b, w->type);
      json_builder_set_member_name (b, "title");
      json_builder_add_string_value (b, w->title ? w->title : "");
      json_builder_set_member_name (b, "io_kind");
      json_builder_add_string_value (b, w->io_kind ? w->io_kind : "");
      json_builder_set_member_name (b, "entity");
      json_builder_begin_object (b);
      json_builder_set_member_name (b, "source");
      json_builder_add_string_value (b, w->entity_source ? w->entity_source : "");
      json_builder_set_member_name (b, "domain");
      json_builder_add_string_value (b, w->entity_domain ? w->entity_domain : "");
      json_builder_set_member_name (b, "name");
      json_builder_add_string_value (b, w->entity_name ? w->entity_name : "");
      json_builder_set_member_name (b, "label");
      json_builder_add_string_value (b, w->entity_label ? w->entity_label : "");
      json_builder_end_object (b);
      json_builder_set_member_name (b, "fields");
      json_builder_begin_object (b);
      g_hash_table_iter_init (&iter, w->fields);
      while (g_hash_table_iter_next (&iter, &k, &v))
        {
          json_builder_set_member_name (b, (const char *) k);
          json_builder_add_string_value (b, (const char *) v);
        }
      json_builder_end_object (b);
      if (w->cached_view)
        {
          JsonParser *p = json_parser_new ();
          if (json_parser_load_from_data (p, w->cached_view, -1, NULL))
            {
              json_builder_set_member_name (b, "view");
              json_builder_add_value (b, json_node_ref (json_parser_get_root (p)));
            }
          g_object_unref (p);
        }
      json_builder_end_object (b);
    }
  json_builder_end_array (b);
  json_builder_end_object (b);
  root = json_builder_get_root (b);
  gen = json_generator_new ();
  json_generator_set_root (gen, root);
  out = json_generator_to_data (gen, NULL);
  g_object_unref (gen);
  json_node_unref (root);
  g_object_unref (b);
  return out;
}

static WuiWidget *
widget_from_json (JsonObject *o)
{
  WuiWidget *w = wui_widget_new ();
  JsonObject *ent, *fields;
  GList *keys, *l;

  w->id = g_strdup (json_object_get_string_member_with_default (o, "id", ""));
  w->type = g_strdup (json_object_get_string_member_with_default (o, "type", ""));
  w->title = g_strdup (json_object_get_string_member_with_default (o, "title", ""));
  w->io_kind = g_strdup (json_object_get_string_member_with_default (o, "io_kind", ""));
  if (json_object_has_member (o, "entity"))
    {
      ent = json_object_get_object_member (o, "entity");
      w->entity_source = g_strdup (json_object_get_string_member_with_default (ent, "source", ""));
      w->entity_domain = g_strdup (json_object_get_string_member_with_default (ent, "domain", ""));
      w->entity_name = g_strdup (json_object_get_string_member_with_default (ent, "name", ""));
      w->entity_label = g_strdup (json_object_get_string_member_with_default (ent, "label", ""));
    }
  if (json_object_has_member (o, "fields"))
    {
      fields = json_object_get_object_member (o, "fields");
      keys = json_object_get_members (fields);
      for (l = keys; l; l = l->next)
        {
          const char *k = l->data;
          JsonNode *n = json_object_get_member (fields, k);
          if (JSON_NODE_HOLDS_VALUE (n))
            {
              if (json_node_get_value_type (n) == G_TYPE_STRING)
                wui_widget_field_set (w, k, json_node_get_string (n));
              else
                {
                  g_autofree gchar *s = json_node_get_value_type (n) == G_TYPE_BOOLEAN
                                           ? g_strdup (json_node_get_boolean (n) ? "true" : "false")
                                           : g_strdup_printf ("%g", json_node_get_double (n));
                  wui_widget_field_set (w, k, s);
                }
            }
        }
      g_list_free (keys);
    }
  return w;
}

static void
handle_request (SoupServer *server, SoupServerMessage *msg, const char *path,
                GHashTable *query, gpointer user_data)
{
  WuiApp *app = user_data;
  const char *method = soup_server_message_get_method (msg);
  g_autofree gchar *static_dir = resolve_static_dir (app);

  (void) server;
  (void) query;

  if (g_strcmp0 (path, "/health") == 0 && g_strcmp0 (method, "GET") == 0)
    {
      json_reply (msg, 200, "{\"status\":\"ok\"}");
      return;
    }

  /* static / login pages */
  if (g_strcmp0 (method, "GET") == 0 && static_dir)
    {
      if (g_strcmp0 (path, "/") == 0 || g_strcmp0 (path, "/index.html") == 0)
        {
          if (app->auth_method != WUI_AUTH_NONE
              && wui_auth_session_from_message (app, msg) == NULL)
            {
              soup_server_message_set_redirect (msg, SOUP_STATUS_FOUND, "/login");
              return;
            }
          if (serve_file (msg, static_dir, "index.html", "text/html; charset=utf-8"))
            return;
        }
      if (g_strcmp0 (path, "/login") == 0 || g_strcmp0 (path, "/login.html") == 0)
        {
          if (serve_file (msg, static_dir, "login.html", "text/html; charset=utf-8"))
            return;
        }
      if (g_str_has_prefix (path, "/static/"))
        {
          const char *rel = path + strlen ("/static/");
          const char *ctype = "application/octet-stream";
          if (g_str_has_suffix (rel, ".js"))
            ctype = "application/javascript";
          else if (g_str_has_suffix (rel, ".css"))
            ctype = "text/css";
          if (serve_file (msg, static_dir, rel, ctype))
            return;
        }
      /* also serve app.js / style.css from root for convenience */
      if (g_strcmp0 (path, "/app.js") == 0
          && serve_file (msg, static_dir, "app.js", "application/javascript"))
        return;
      if (g_strcmp0 (path, "/style.css") == 0
          && serve_file (msg, static_dir, "style.css", "text/css"))
        return;
    }

  if (g_strcmp0 (path, "/login") == 0 && g_strcmp0 (method, "POST") == 0)
    {
      g_autofree gchar *body = read_body (msg);
      JsonParser *p = json_parser_new ();
      g_autofree gchar *cookie = NULL;
      GError *err = NULL;
      const char *user = NULL, *pass = NULL;

      if (!json_parser_load_from_data (p, body, -1, NULL))
        {
          g_object_unref (p);
          json_reply (msg, 400, "{\"error\":\"bad json\"}");
          return;
        }
      {
        JsonObject *o = json_node_get_object (json_parser_get_root (p));
        user = json_object_get_string_member_with_default (o, "user", "");
        pass = json_object_get_string_member_with_default (o, "password", "");
      }
      if (!wui_auth_login (app, user, pass, &cookie, &err))
        {
          json_reply (msg, 401, "{\"error\":\"invalid credentials\"}");
          g_clear_error (&err);
          g_object_unref (p);
          return;
        }
      add_set_cookie (msg, cookie);
      json_reply (msg, 200, "{\"ok\":true}");
      g_object_unref (p);
      return;
    }

  if (g_strcmp0 (path, "/logout") == 0 && g_strcmp0 (method, "POST") == 0)
    {
      g_autofree gchar *cookie = NULL;
      wui_auth_logout (app, msg, &cookie);
      add_set_cookie (msg, cookie);
      json_reply (msg, 200, "{\"ok\":true}");
      return;
    }

  /* API below requires auth when enabled */
  if (g_str_has_prefix (path, "/api/") || g_str_has_prefix (path, "/dashboard")
      || g_str_has_prefix (path, "/widgets") || g_str_has_prefix (path, "/widget-types")
      || g_str_has_prefix (path, "/labels") || g_str_has_prefix (path, "/session"))
    {
      WuiGroup need = WUI_GROUP_USER;
      if (g_strcmp0 (method, "POST") == 0 || g_strcmp0 (method, "PUT") == 0
          || g_strcmp0 (method, "DELETE") == 0)
        {
          if (g_str_has_prefix (path, "/dashboard")
              || (g_str_has_prefix (path, "/widgets")
                  && !g_str_has_suffix (path, "/action")))
            need = WUI_GROUP_ADMIN;
        }
      if (!wui_auth_require (app, msg, need, NULL))
        return;
    }

  if (g_strcmp0 (path, "/session") == 0 && g_strcmp0 (method, "GET") == 0)
    {
      WuiSession *s = wui_auth_session_from_message (app, msg);
      if (app->auth_method == WUI_AUTH_NONE)
        json_reply (msg, 200, "{\"auth\":false,\"group\":\"admin\"}");
      else if (!s)
        json_reply (msg, 401, "{\"error\":\"unauthorized\"}");
      else
        {
          g_autofree gchar *j = g_strdup_printf (
              "{\"auth\":true,\"user\":\"%s\",\"group\":\"%s\"}", s->user_id,
              wui_group_name (s->group));
          json_reply (msg, 200, j);
        }
      return;
    }

  if (g_strcmp0 (path, "/widget-types") == 0 && g_strcmp0 (method, "GET") == 0)
    {
      g_autofree gchar *j = wui_widget_types_json (app);
      json_reply (msg, 200, j);
      return;
    }

  if (g_strcmp0 (path, "/dashboard") == 0 && g_strcmp0 (method, "GET") == 0)
    {
      g_autofree gchar *j = dashboard_json (app);
      json_reply (msg, 200, j);
      return;
    }

  if (g_strcmp0 (path, "/labels") == 0 && g_strcmp0 (method, "GET") == 0)
    {
      GError *err = NULL;
      g_autofree gchar *j = wui_mysql_labels_json (app, &err);
      if (!j)
        {
          json_reply (msg, 502, "{\"error\":\"mysql unavailable\"}");
          g_clear_error (&err);
          return;
        }
      json_reply (msg, 200, j);
      return;
    }

  if (g_strcmp0 (path, "/widgets") == 0 && g_strcmp0 (method, "POST") == 0)
    {
      g_autofree gchar *body = read_body (msg);
      JsonParser *p = json_parser_new ();
      WuiWidget *w;
      GError *err = NULL;

      if (!json_parser_load_from_data (p, body, -1, NULL))
        {
          g_object_unref (p);
          json_reply (msg, 400, "{\"error\":\"bad json\"}");
          return;
        }
      w = widget_from_json (json_node_get_object (json_parser_get_root (p)));
      g_object_unref (p);
      if (!w->id || !*w->id || !w->type || !*w->type)
        {
          wui_widget_free (w);
          json_reply (msg, 400, "{\"error\":\"id and type required\"}");
          return;
        }
      if (wui_app_find_widget (app, w->id))
        {
          wui_widget_free (w);
          json_reply (msg, 409, "{\"error\":\"exists\"}");
          return;
        }
      g_ptr_array_add (app->widgets, w);
      if (!wui_user_config_save (app, &err))
        {
          json_reply (msg, 500, "{\"error\":\"save failed\"}");
          g_clear_error (&err);
          return;
        }
      wui_refresh_schedule_all (app);
      json_reply (msg, 200, "{\"ok\":true}");
      return;
    }

  if (g_str_has_prefix (path, "/widgets/"))
    {
      const char *rest = path + strlen ("/widgets/");
      g_autofree gchar *id = NULL;
      const char *sub = NULL;
      WuiWidget *w;
      char *slash = strchr (rest, '/');

      if (slash)
        {
          id = g_strndup (rest, slash - rest);
          sub = slash + 1;
        }
      else
        id = g_strdup (rest);

      w = wui_app_find_widget (app, id);
      if (!w)
        {
          json_reply (msg, 404, "{\"error\":\"not found\"}");
          return;
        }

      if (sub == NULL && g_strcmp0 (method, "DELETE") == 0)
        {
          GError *err = NULL;
          g_ptr_array_remove (app->widgets, w);
          if (!wui_user_config_save (app, &err))
            {
              json_reply (msg, 500, "{\"error\":\"save failed\"}");
              g_clear_error (&err);
              return;
            }
          wui_refresh_schedule_all (app);
          json_reply (msg, 200, "{\"ok\":true}");
          return;
        }

      if (sub == NULL && (g_strcmp0 (method, "PUT") == 0 || g_strcmp0 (method, "POST") == 0)
          && (sub == NULL))
        {
          /* update widget */
          g_autofree gchar *body = read_body (msg);
          JsonParser *p = json_parser_new ();
          WuiWidget *nw;
          GError *err = NULL;
          guint i;

          if (!json_parser_load_from_data (p, body, -1, NULL))
            {
              g_object_unref (p);
              json_reply (msg, 400, "{\"error\":\"bad json\"}");
              return;
            }
          nw = widget_from_json (json_node_get_object (json_parser_get_root (p)));
          g_object_unref (p);
          g_free (nw->id);
          nw->id = g_strdup (w->id);
          for (i = 0; i < app->widgets->len; i++)
            {
              if (g_ptr_array_index (app->widgets, i) == w)
                {
                  g_ptr_array_remove_index (app->widgets, i);
                  break;
                }
            }
          g_ptr_array_add (app->widgets, nw);
          if (!wui_user_config_save (app, &err))
            {
              json_reply (msg, 500, "{\"error\":\"save failed\"}");
              g_clear_error (&err);
              return;
            }
          wui_refresh_schedule_all (app);
          json_reply (msg, 200, "{\"ok\":true}");
          return;
        }

      if (sub && g_strcmp0 (sub, "data") == 0 && g_strcmp0 (method, "GET") == 0)
        {
          if (!w->cached_view)
            wui_refresh_widget (app, w, NULL);
          json_reply (msg, 200, w->cached_view ? w->cached_view : "{}");
          return;
        }

      if (sub && g_strcmp0 (sub, "action") == 0 && g_strcmp0 (method, "POST") == 0)
        {
          g_autofree gchar *body = read_body (msg);
          GError *err = NULL;
          JsonParser *p;
          gdouble val = 0;

          if (g_strcmp0 (w->entity_source, "history") == 0)
            {
              json_reply (msg, 405, "{\"error\":\"history read-only\"}");
              return;
            }
          p = json_parser_new ();
          if (!json_parser_load_from_data (p, body, -1, NULL))
            {
              g_object_unref (p);
              json_reply (msg, 400, "{\"error\":\"bad json\"}");
              return;
            }
          {
            JsonNode *root = json_parser_get_root (p);
            if (JSON_NODE_HOLDS_VALUE (root))
              val = json_node_get_double (root);
            else if (JSON_NODE_HOLDS_OBJECT (root))
              {
                JsonObject *o = json_node_get_object (root);
                if (json_object_has_member (o, "val"))
                  val = json_object_get_double_member (o, "val");
              }
          }
          g_object_unref (p);

          /* Always OR USR1; schedule clear from daemon usr1-clear-timeout-sec */
          if (!wui_bridge_set_input_val_usr1 (app, w, val, &err))
            {
              json_reply (msg, 502, "{\"error\":\"bridge write failed\"}");
              g_clear_error (&err);
              return;
            }
          wui_refresh_widget (app, w, NULL);
          json_reply (msg, 200, w->cached_view ? w->cached_view : "{\"ok\":true}");
          return;
        }
    }

  if (g_str_has_prefix (path, "/api/"))
    {
      const char *br = path + 4; /* keep leading / of /api → /io via skip "api" */
      /* path is /api/io → bridge /io */
      g_autofree gchar *bpath = g_strdup (path + 4);
      GError *err = NULL;
      if (g_strcmp0 (method, "GET") == 0)
        {
          g_autofree gchar *body = wui_bridge_get (app, bpath, &err);
          if (!body)
            {
              json_reply (msg, 502, "{\"error\":\"bridge\"}");
              g_clear_error (&err);
              return;
            }
          json_reply (msg, 200, body);
          return;
        }
      if (g_strcmp0 (method, "PUT") == 0 || g_strcmp0 (method, "POST") == 0)
        {
          g_autofree gchar *req = read_body (msg);
          if (!wui_bridge_put (app, bpath, req, &err))
            {
              json_reply (msg, 502, "{\"error\":\"bridge\"}");
              g_clear_error (&err);
              return;
            }
          json_reply (msg, 200, "{\"ok\":true}");
          return;
        }
      (void) br;
    }

  json_reply (msg, 404, "{\"error\":\"not found\"}");
}

gboolean
wui_http_start (WuiApp *app, GError **error)
{
  GSocketAddress *addr;
  GError *local = NULL;

  app->soup = soup_server_new ("server-header", "starter-web-ui", NULL);
  soup_server_add_handler (app->soup, NULL, handle_request, app, NULL);

  addr = g_inet_socket_address_new_from_string (app->http_address, app->http_port);
  if (!addr)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, "bad address");
      return FALSE;
    }
  if (!soup_server_listen (app->soup, addr, 0, &local))
    {
      g_propagate_error (error, local);
      g_object_unref (addr);
      return FALSE;
    }
  g_object_unref (addr);
  return TRUE;
}

void
wui_http_stop (WuiApp *app)
{
  if (app->soup)
    {
      soup_server_disconnect (app->soup);
      g_clear_object (&app->soup);
    }
}
