/*
 * starter-web-ui - mysql-db.c
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mysql-db.h"

#include <string.h>
#include <ctype.h>
#include <mysql.h>
#include <json-glib/json-glib.h>

static gboolean
is_select_only (const gchar *sql)
{
  const gchar *p = sql;
  while (*p && isspace ((unsigned char) *p))
    p++;
  if (g_ascii_strncasecmp (p, "SELECT", 6) != 0)
    return FALSE;
  /* reject multi-statement */
  if (strchr (p, ';') != NULL && strchr (p, ';')[1] != '\0'
      && strspn (strchr (p, ';') + 1, " \t\r\n") != strlen (strchr (p, ';') + 1))
    return FALSE;
  return TRUE;
}

static MYSQL *
connect_db (WuiApp *app, GError **error)
{
  MYSQL *m = mysql_init (NULL);
  if (!m)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "mysql_init failed");
      return NULL;
    }
  if (!mysql_real_connect (m, app->mysql_host, app->mysql_user, app->mysql_password,
                           app->mysql_database, 0, NULL, 0))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "mysql: %s",
                   mysql_error (m));
      mysql_close (m);
      return NULL;
    }
  return m;
}

gchar *
wui_mysql_query_json (WuiApp *app, const gchar *sql, GError **error)
{
  MYSQL *m;
  MYSQL_RES *res;
  MYSQL_ROW row;
  MYSQL_FIELD *fields;
  unsigned int nfields, i;
  JsonBuilder *b;
  JsonGenerator *gen;
  gchar *out;
  JsonNode *root;

  if (sql == NULL || !is_select_only (sql))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "only single SELECT allowed");
      return NULL;
    }

  m = connect_db (app, error);
  if (!m)
    return NULL;

  if (mysql_query (m, sql) != 0)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "mysql: %s", mysql_error (m));
      mysql_close (m);
      return NULL;
    }

  res = mysql_store_result (m);
  if (!res)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "no result set");
      mysql_close (m);
      return NULL;
    }

  nfields = mysql_num_fields (res);
  fields = mysql_fetch_fields (res);
  b = json_builder_new ();
  json_builder_begin_array (b);
  while ((row = mysql_fetch_row (res)) != NULL)
    {
      json_builder_begin_object (b);
      for (i = 0; i < nfields; i++)
        {
          json_builder_set_member_name (b, fields[i].name);
          if (row[i] == NULL)
            json_builder_add_null_value (b);
          else
            json_builder_add_string_value (b, row[i]);
        }
      json_builder_end_object (b);
    }
  json_builder_end_array (b);
  root = json_builder_get_root (b);
  gen = json_generator_new ();
  json_generator_set_root (gen, root);
  out = json_generator_to_data (gen, NULL);
  g_object_unref (gen);
  json_node_unref (root);
  g_object_unref (b);
  mysql_free_result (res);
  mysql_close (m);
  return out;
}

gchar *
wui_mysql_labels_json (WuiApp *app, GError **error)
{
  return wui_mysql_query_json (
      app,
      "SELECT name, description, type, unit, min, max, step FROM labels ORDER BY name",
      error);
}
