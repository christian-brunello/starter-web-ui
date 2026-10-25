/*
 * starter-web-ui - refresh.c
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "refresh.h"
#include "user-config.h"
#include "guile-api.h"
#include "bridge.h"
#include "mysql-db.h"

#include <string.h>
#include <stdlib.h>

static guint
refresh_sec_for (WuiWidget *w)
{
  gchar *s = wui_widget_field_get (w, "refresh-sec");
  guint v = 5;
  if (s)
    {
      int n = atoi (s);
      if (n > 0)
        v = (guint) n;
      g_free (s);
    }
  return v;
}

gboolean
wui_refresh_widget (WuiApp *app, WuiWidget *w, GError **error)
{
  g_autofree gchar *prepare = NULL;
  g_autofree gchar *raw = NULL;
  g_autofree gchar *view = NULL;
  GError *local = NULL;

  wui_guile_invoke_hook (app, "before-widget-refresh", w, NULL);

  prepare = wui_guile_prepare (app, w, &local);
  if (local)
    {
      g_propagate_error (error, local);
      return FALSE;
    }

  if (g_strcmp0 (w->entity_source, "history") == 0
      || (prepare && *prepare && g_ascii_strncasecmp (prepare, "SELECT", 6) == 0))
    {
      if (prepare == NULL || !*prepare)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "history widget needs SQL");
          return FALSE;
        }
      raw = wui_mysql_query_json (app, prepare, &local);
    }
  else
    {
      /* live bridge */
      g_autofree gchar *path = NULL;
      if (w->entity_domain && w->entity_name)
        path = g_strdup_printf ("/%s/%s", w->entity_domain, w->entity_name);
      else
        path = g_strdup ("/io");
      raw = wui_bridge_get (app, path, &local);
      if (raw == NULL && local)
        {
          /* soft-fail: draw with error object */
          g_clear_error (&local);
          raw = g_strdup ("{\"error\":\"bridge unreachable\"}");
        }
    }

  if (raw == NULL)
    {
      if (local)
        g_propagate_error (error, local);
      else
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "no raw data");
      return FALSE;
    }

  view = wui_guile_draw (app, w, raw, &local);
  if (view == NULL)
    {
      g_propagate_error (error, local);
      return FALSE;
    }

  wui_guile_invoke_hook (app, "after-widget-refresh", w, view);

  g_free (w->cached_view);
  w->cached_view = g_steal_pointer (&view);
  return TRUE;
}

typedef struct {
  WuiApp *app;
  gchar *widget_id;
} RefreshData;

static gboolean
on_timeout (gpointer user_data)
{
  RefreshData *rd = user_data;
  WuiWidget *w = wui_app_find_widget (rd->app, rd->widget_id);
  if (w)
    wui_refresh_widget (rd->app, w, NULL);
  return G_SOURCE_CONTINUE;
}

static void
rd_free (gpointer p)
{
  RefreshData *rd = p;
  g_free (rd->widget_id);
  g_free (rd);
}

void
wui_refresh_unschedule_all (WuiApp *app)
{
  guint i;
  if (!app->widgets)
    return;
  for (i = 0; i < app->widgets->len; i++)
    {
      WuiWidget *w = g_ptr_array_index (app->widgets, i);
      if (w->refresh_source)
        {
          g_source_remove (w->refresh_source);
          w->refresh_source = 0;
        }
    }
}

void
wui_refresh_schedule_all (WuiApp *app)
{
  guint i;
  wui_refresh_unschedule_all (app);
  for (i = 0; i < app->widgets->len; i++)
    {
      WuiWidget *w = g_ptr_array_index (app->widgets, i);
      RefreshData *rd = g_new0 (RefreshData, 1);
      guint sec = refresh_sec_for (w);
      rd->app = app;
      rd->widget_id = g_strdup (w->id);
      w->refresh_source = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT, sec, on_timeout,
                                                      rd, rd_free);
      wui_refresh_widget (app, w, NULL);
    }
}
