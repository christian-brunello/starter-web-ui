/*
 * starter-web-ui - guile-host.c
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "guile-host.h"
#include "guile-api.h"

#include <libguile.h>

typedef struct {
  WuiApp *app;
  GError **error;
  gboolean ok;
} Boot;

static void
add_load_path (const char *dir)
{
  SCM var, path, entry;
  if (!dir || !*dir || !g_file_test (dir, G_FILE_TEST_IS_DIR))
    return;
  entry = scm_from_utf8_string (dir);
  var = scm_c_lookup ("%load-path");
  path = scm_variable_ref (var);
  scm_variable_set_x (var, scm_cons (entry, path));
}

static gchar *
resolve_scheme_dir (WuiApp *app)
{
  const gchar *cands[] = {
    app->scheme_dir,
    "scheme",
    "../scheme",
#ifdef WEB_UI_SCHEME_DIR
    WEB_UI_SCHEME_DIR,
#endif
    NULL
  };
  guint i;
  for (i = 0; cands[i]; i++)
    {
      g_autofree gchar *init = NULL;
      if (!cands[i] || !g_file_test (cands[i], G_FILE_TEST_IS_DIR))
        continue;
      init = g_build_filename (cands[i], "init.scm", NULL);
      if (g_file_test (init, G_FILE_TEST_IS_REGULAR))
        return g_canonicalize_filename (cands[i], NULL);
    }
  return NULL;
}

static void *
boot (void *data)
{
  Boot *b = data;
  g_autofree gchar *scheme_dir = NULL;
  g_autofree gchar *init_path = NULL;
  g_autofree gchar *core_path = NULL;
  g_autofree gchar *hooks_path = NULL;
  g_autofree gchar *plug_path = NULL;

  scheme_dir = resolve_scheme_dir (b->app);
  if (scheme_dir)
    add_load_path (scheme_dir);

  /* Start from guile-user; define-module forms in the Scheme files set up
   * (web-ui hooks) / (web-ui core). Inject the C primitive only after those
   * modules exist, then load plugins that call widget-type-register!. */
  scm_set_current_module (scm_c_resolve_module ("guile-user"));

  if (scheme_dir)
    {
      hooks_path = g_build_filename (scheme_dir, "web-ui", "hooks.scm", NULL);
      core_path = g_build_filename (scheme_dir, "web-ui", "core.scm", NULL);
      if (g_file_test (hooks_path, G_FILE_TEST_IS_REGULAR))
        scm_c_primitive_load (hooks_path);
      if (g_file_test (core_path, G_FILE_TEST_IS_REGULAR))
        scm_c_primitive_load (core_path);
    }

  wui_guile_api_register (b->app);

  if (scheme_dir)
    {
      init_path = g_build_filename (scheme_dir, "init.scm", NULL);
      if (g_file_test (init_path, G_FILE_TEST_IS_REGULAR))
        {
          scm_set_current_module (scm_c_resolve_module ("guile-user"));
          scm_c_primitive_load (init_path);
        }
      plug_path = g_build_filename (scheme_dir, "plugins", "builtins", "plugin.scm", NULL);
      if (g_file_test (plug_path, G_FILE_TEST_IS_REGULAR))
        {
          scm_set_current_module (scm_c_resolve_module ("guile-user"));
          scm_c_primitive_load (plug_path);
        }
    }

  b->app->guile_ready = TRUE;
  b->ok = TRUE;
  return NULL;
}

gboolean
wui_guile_init (WuiApp *app, GError **error)
{
  Boot b = { .app = app, .error = error, .ok = FALSE };
  g_mutex_init (&app->guile_lock);
  scm_with_guile (boot, &b);
  if (!b.ok)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Guile init failed");
      return FALSE;
    }
  return TRUE;
}
