/*
 * starter-web-ui - config.c
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "wui-config.h"

#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

static gchar *
xml_child_text (xmlNode *parent, const char *name)
{
  xmlNode *n;
  for (n = parent->children; n; n = n->next)
    {
      if (n->type == XML_ELEMENT_NODE && xmlStrcmp (n->name, (const xmlChar *) name) == 0)
        {
          xmlChar *c = xmlNodeGetContent (n);
          gchar *out = g_strdup (c ? (const char *) c : "");
          if (c)
            xmlFree (c);
          return out;
        }
    }
  return NULL;
}

static xmlNode *
xml_child (xmlNode *parent, const char *name)
{
  xmlNode *n;
  for (n = parent->children; n; n = n->next)
    {
      if (n->type == XML_ELEMENT_NODE && xmlStrcmp (n->name, (const xmlChar *) name) == 0)
        return n;
    }
  return NULL;
}

gboolean
wui_config_load (WuiApp *app, const gchar *path, GError **error)
{
  xmlDoc *doc;
  xmlNode *root, *n;
  gchar *tmp;

  g_return_val_if_fail (app != NULL && path != NULL, FALSE);

  doc = xmlReadFile (path, NULL, 0);
  if (doc == NULL)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   "failed to parse config %s", path);
      return FALSE;
    }

  root = xmlDocGetRootElement (doc);
  if (root == NULL || xmlStrcmp (root->name, (const xmlChar *) "web-ui") != 0)
    {
      xmlFreeDoc (doc);
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "root must be <web-ui>");
      return FALSE;
    }

  tmp = xml_child_text (root, "http-address");
  app->http_address = tmp ? tmp : g_strdup ("127.0.0.1");

  tmp = xml_child_text (root, "http-port");
  app->http_port = tmp ? (guint) atoi (tmp) : 18767;
  g_free (tmp);

  tmp = xml_child_text (root, "data-dir");
  app->data_dir = tmp ? tmp : g_strdup ("./data");

  tmp = xml_child_text (root, "user-config");
  app->user_config_name = tmp ? tmp : g_strdup ("user-config.xml");
  app->user_config_path = g_build_filename (app->data_dir, app->user_config_name, NULL);

  tmp = xml_child_text (root, "plugin-dir");
  app->plugin_dir = tmp;

  tmp = xml_child_text (root, "scheme-dir");
  app->scheme_dir = tmp;

  tmp = xml_child_text (root, "static-dir");
  app->static_dir = tmp;

  tmp = xml_child_text (root, "bridge-base-url");
  app->bridge_base_url = tmp ? tmp : g_strdup ("http://127.0.0.1:18766");

  tmp = xml_child_text (root, "bridge-token");
  app->bridge_token = tmp;

  app->usr1_clear_timeout_sec = 30;
  tmp = xml_child_text (root, "usr1-clear-timeout-sec");
  if (tmp)
    {
      app->usr1_clear_timeout_sec = (guint) atoi (tmp);
      g_free (tmp);
    }

  app->bridge_put_retries = 5;
  tmp = xml_child_text (root, "bridge-put-retries");
  if (tmp)
    {
      int ntry = atoi (tmp);
      if (ntry > 0)
        app->bridge_put_retries = (guint) ntry;
      g_free (tmp);
    }

  app->bridge_put_backoff_ms = 100;
  tmp = xml_child_text (root, "bridge-put-backoff-ms");
  if (tmp)
    {
      int ms = atoi (tmp);
      if (ms > 0)
        app->bridge_put_backoff_ms = (guint) ms;
      g_free (tmp);
    }

  n = xml_child (root, "mysql");
  if (n)
    {
      gchar *cred_file;

      app->mysql_host = xml_child_text (n, "host");
      app->mysql_database = xml_child_text (n, "database");
      app->mysql_user = xml_child_text (n, "user");
      app->mysql_password = xml_child_text (n, "password");
      cred_file = xml_child_text (n, "credentials-file");
      if (cred_file && *cred_file && g_file_test (cred_file, G_FILE_TEST_IS_REGULAR))
        {
          g_autofree gchar *contents = NULL;
          gchar **lines;
          guint i;

          if (g_file_get_contents (cred_file, &contents, NULL, NULL))
            {
              lines = g_strsplit (contents, "\n", -1);
              for (i = 0; lines[i]; i++)
                {
                  gchar *line = g_strstrip (lines[i]);
                  if (g_str_has_prefix (line, "DB_HOST="))
                    {
                      g_free (app->mysql_host);
                      app->mysql_host = g_strdup (line + strlen ("DB_HOST="));
                    }
                  else if (g_str_has_prefix (line, "DB_READER_USER="))
                    {
                      g_free (app->mysql_user);
                      app->mysql_user = g_strdup (line + strlen ("DB_READER_USER="));
                    }
                  else if (g_str_has_prefix (line, "DB_READER_PASS="))
                    {
                      g_free (app->mysql_password);
                      app->mysql_password = g_strdup (line + strlen ("DB_READER_PASS="));
                    }
                }
              g_strfreev (lines);
            }
        }
      g_free (cred_file);
    }
  if (app->mysql_host == NULL)
    app->mysql_host = g_strdup ("127.0.0.1");
  if (app->mysql_database == NULL)
    app->mysql_database = g_strdup ("STARTER");
  if (app->mysql_user == NULL)
    app->mysql_user = g_strdup ("STReader");

  n = xml_child (root, "auth");
  app->auth_method = WUI_AUTH_NONE;
  app->auth_group = WUI_GROUP_ADMIN;
  app->cookie_name = g_strdup ("starter_web_ui");
  app->session_days = 30;
  if (n)
    {
      xmlChar *method = xmlGetProp (n, (const xmlChar *) "method");
      if (method)
        {
          if (xmlStrcmp (method, (const xmlChar *) "password") == 0)
            app->auth_method = WUI_AUTH_PASSWORD;
          xmlFree (method);
        }
      app->auth_user = xml_child_text (n, "user");
      g_free (app->auth_password);
      app->auth_password = xml_child_text (n, "password");
      tmp = xml_child_text (n, "group");
      if (tmp)
        {
          if (g_strcmp0 (tmp, "user") == 0)
            app->auth_group = WUI_GROUP_USER;
          else
            app->auth_group = WUI_GROUP_ADMIN;
          g_free (tmp);
        }
      tmp = xml_child_text (n, "cookie-name");
      if (tmp)
        {
          g_free (app->cookie_name);
          app->cookie_name = tmp;
        }
      tmp = xml_child_text (n, "session-days");
      if (tmp)
        {
          app->session_days = (guint) atoi (tmp);
          if (app->session_days == 0)
            app->session_days = 30;
          g_free (tmp);
        }
    }

  xmlFreeDoc (doc);
  return TRUE;
}
