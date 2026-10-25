/*
 * starter-web-ui - main.c
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app.h"
#include "wui-config.h"
#include "user-config.h"
#include "auth.h"
#include "bridge.h"
#include "guile-host.h"
#include "refresh.h"
#include "http.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define LOGI(...) g_log ("web-ui", G_LOG_LEVEL_INFO, __VA_ARGS__)
#define LOGE(...) g_log ("web-ui", G_LOG_LEVEL_ERROR, __VA_ARGS__)

static WuiApp *g_app = NULL;

static void
on_signal (int signo)
{
  (void) signo;
  if (g_app && g_app->loop)
    g_main_loop_quit (g_app->loop);
}

static void
app_free (WuiApp *app)
{
  if (!app)
    return;
  wui_refresh_unschedule_all (app);
  wui_http_stop (app);
  wui_bridge_shutdown (app);
  wui_auth_shutdown (app);
  if (app->widgets)
    g_ptr_array_free (app->widgets, TRUE);
  if (app->widget_types)
    g_hash_table_destroy (app->widget_types);
  g_free (app->config_path);
  g_free (app->http_address);
  g_free (app->data_dir);
  g_free (app->user_config_name);
  g_free (app->user_config_path);
  g_free (app->plugin_dir);
  g_free (app->scheme_dir);
  g_free (app->static_dir);
  g_free (app->bridge_base_url);
  g_free (app->bridge_token);
  g_free (app->mysql_host);
  g_free (app->mysql_database);
  g_free (app->mysql_user);
  g_free (app->mysql_password);
  g_free (app->auth_user);
  g_free (app->auth_password);
  g_free (app->cookie_name);
  if (app->loop)
    g_main_loop_unref (app->loop);
  if (app->guile_ready)
    g_mutex_clear (&app->guile_lock);
  g_free (app);
}

static void
usage (const char *argv0)
{
  g_printerr ("Usage: %s -c <web-ui.xml>\n", argv0);
}

int
main (int argc, char **argv)
{
  WuiApp *app;
  GError *err = NULL;
  const gchar *cfg = NULL;
  int i;

  for (i = 1; i < argc; i++)
    {
      if ((g_strcmp0 (argv[i], "-c") == 0 || g_strcmp0 (argv[i], "--config") == 0)
          && i + 1 < argc)
        cfg = argv[++i];
      else if (g_strcmp0 (argv[i], "-h") == 0 || g_strcmp0 (argv[i], "--help") == 0)
        {
          usage (argv[0]);
          return 0;
        }
    }
  if (!cfg)
    {
      usage (argv[0]);
      return 1;
    }

  app = g_new0 (WuiApp, 1);
  app->config_path = g_strdup (cfg);
  g_app = app;

  if (!wui_config_load (app, cfg, &err))
    {
      LOGE ("config: %s", err->message);
      g_clear_error (&err);
      app_free (app);
      return 1;
    }

  g_mkdir_with_parents (app->data_dir, 0755);

  if (!wui_user_config_load (app, &err))
    {
      LOGE ("user-config: %s", err->message);
      g_clear_error (&err);
      app_free (app);
      return 1;
    }

  wui_auth_init (app);
  wui_bridge_init (app);

  if (!wui_guile_init (app, &err))
    {
      LOGE ("guile: %s", err->message);
      g_clear_error (&err);
      app_free (app);
      return 1;
    }

  if (!wui_http_start (app, &err))
    {
      LOGE ("http: %s", err->message);
      g_clear_error (&err);
      app_free (app);
      return 1;
    }

  wui_refresh_schedule_all (app);

  signal (SIGINT, on_signal);
  signal (SIGTERM, on_signal);

  LOGI ("listening on %s:%u (auth=%s)", app->http_address, app->http_port,
        app->auth_method == WUI_AUTH_NONE ? "none" : "password");

  app->loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (app->loop);

  app_free (app);
  g_app = NULL;
  return 0;
}
