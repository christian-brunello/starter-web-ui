/*
 * starter-web-ui - guile-api.c
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "guile-api.h"

#include <stdlib.h>
#include <string.h>
#include <json-glib/json-glib.h>

static WuiApp *g_api_app = NULL;

static void
wtype_free (gpointer p)
{
  WuiWidgetType *t = p;
  if (!t)
    return;
  g_free (t->name);
  g_free (t->label);
  g_free (t->fields_json);
  if (t->tags)
    g_ptr_array_free (t->tags, TRUE);
  if (t->prepare != SCM_BOOL_F && scm_is_true (t->prepare))
    scm_gc_unprotect_object (t->prepare);
  if (t->draw != SCM_BOOL_F && scm_is_true (t->draw))
    scm_gc_unprotect_object (t->draw);
  g_free (t);
}

static SCM
widget_to_scm_alist (WuiWidget *w)
{
  SCM alist = SCM_EOL;
  GHashTableIter iter;
  gpointer k, v;

  alist = scm_acons (scm_from_utf8_symbol ("id"),
                     scm_from_utf8_string (w->id ? w->id : ""), alist);
  alist = scm_acons (scm_from_utf8_symbol ("type"),
                     scm_from_utf8_string (w->type ? w->type : ""), alist);
  alist = scm_acons (scm_from_utf8_symbol ("title"),
                     scm_from_utf8_string (w->title ? w->title : ""), alist);
  alist = scm_acons (scm_from_utf8_symbol ("io-kind"),
                     scm_from_utf8_string (w->io_kind ? w->io_kind : ""), alist);
  alist = scm_acons (scm_from_utf8_symbol ("entity-source"),
                     scm_from_utf8_string (w->entity_source ? w->entity_source : ""), alist);
  alist = scm_acons (scm_from_utf8_symbol ("entity-domain"),
                     scm_from_utf8_string (w->entity_domain ? w->entity_domain : ""), alist);
  alist = scm_acons (scm_from_utf8_symbol ("entity-name"),
                     scm_from_utf8_string (w->entity_name ? w->entity_name : ""), alist);
  alist = scm_acons (scm_from_utf8_symbol ("entity-label"),
                     scm_from_utf8_string (w->entity_label ? w->entity_label : ""), alist);

  g_hash_table_iter_init (&iter, w->fields);
  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      alist = scm_acons (scm_from_utf8_symbol ((const char *) k),
                         scm_from_utf8_string ((const char *) v), alist);
    }
  return alist;
}

static SCM
scm_register_widget_type_x (SCM name, SCM label, SCM tags, SCM fields_json,
                            SCM prepare, SCM draw)
{
  WuiWidgetType *t;
  gchar *n;

  if (g_api_app == NULL)
    scm_misc_error ("register-widget-type!", "no app", SCM_EOL);

  {
    char *tmp;
    if (scm_is_symbol (name))
      tmp = scm_to_utf8_string (scm_symbol_to_string (name));
    else if (scm_is_string (name))
      tmp = scm_to_utf8_string (name);
    else
      tmp = strdup ("unknown");
    n = g_strdup (tmp);
    free (tmp);
  }
  t = g_new0 (WuiWidgetType, 1);
  t->prepare = SCM_BOOL_F;
  t->draw = SCM_BOOL_F;
  t->name = n;
  if (scm_is_string (label))
    {
      char *tmp = scm_to_utf8_string (label);
      t->label = g_strdup (tmp);
      free (tmp);
    }
  else
    t->label = g_strdup (n);
  t->tags = g_ptr_array_new_with_free_func (g_free);
  for (SCM p = tags; scm_is_pair (p); p = scm_cdr (p))
    {
      SCM x = scm_car (p);
      char *tmp = NULL;
      if (scm_is_symbol (x))
        tmp = scm_to_utf8_string (scm_symbol_to_string (x));
      else if (scm_is_string (x))
        tmp = scm_to_utf8_string (x);
      if (tmp)
        {
          g_ptr_array_add (t->tags, g_strdup (tmp));
          free (tmp);
        }
    }
  if (scm_is_string (fields_json))
    {
      char *tmp = scm_to_utf8_string (fields_json);
      t->fields_json = g_strdup (tmp);
      free (tmp);
    }
  else
    t->fields_json = g_strdup ("[]");
  t->prepare = prepare;
  t->draw = draw;
  if (scm_is_true (prepare))
    scm_gc_protect_object (prepare);
  if (scm_is_true (draw))
    scm_gc_protect_object (draw);

  g_hash_table_replace (g_api_app->widget_types, g_strdup (t->name), t);
  return SCM_UNSPECIFIED;
}

void
wui_guile_api_register (WuiApp *app)
{
  SCM mod;
  SCM prim;

  g_api_app = app;
  if (app->widget_types == NULL)
    app->widget_types = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, wtype_free);

  mod = scm_c_resolve_module ("web-ui core");
  scm_set_current_module (mod);
  prim = scm_c_make_gsubr ("%register-widget-type!", 6, 0, 0,
                           (scm_t_subr) scm_register_widget_type_x);
  scm_c_module_define (mod, "%register-widget-type!", prim);
  scm_c_export ("%register-widget-type!", NULL);
}

WuiWidgetType *
wui_widget_type_lookup (WuiApp *app, const gchar *name)
{
  if (!app || !app->widget_types || !name)
    return NULL;
  return g_hash_table_lookup (app->widget_types, name);
}

gchar *
wui_widget_types_json (WuiApp *app)
{
  JsonBuilder *b = json_builder_new ();
  GHashTableIter iter;
  gpointer k, v;
  JsonGenerator *gen;
  JsonNode *root;
  gchar *out;

  json_builder_begin_array (b);
  g_hash_table_iter_init (&iter, app->widget_types);
  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      WuiWidgetType *t = v;
      guint i;
      JsonParser *fp;
      JsonNode *fnode;

      json_builder_begin_object (b);
      json_builder_set_member_name (b, "type");
      json_builder_add_string_value (b, t->name);
      json_builder_set_member_name (b, "label");
      json_builder_add_string_value (b, t->label);
      json_builder_set_member_name (b, "tags");
      json_builder_begin_array (b);
      for (i = 0; i < t->tags->len; i++)
        json_builder_add_string_value (b, g_ptr_array_index (t->tags, i));
      json_builder_end_array (b);
      json_builder_set_member_name (b, "fields");
      fp = json_parser_new ();
      if (json_parser_load_from_data (fp, t->fields_json, -1, NULL))
        {
          fnode = json_parser_get_root (fp);
          json_builder_add_value (b, json_node_ref (fnode));
        }
      else
        {
          json_builder_begin_array (b);
          json_builder_end_array (b);
        }
      g_object_unref (fp);
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
  return out;
}

typedef struct {
  WuiApp *app;
  WuiWidget *w;
  const gchar *raw_json;
  gchar *result;
  GError **error;
  gboolean ok;
  const char *hook_name;
  const gchar *extra_json;
} GuileCall;

static void *
call_prepare (void *data)
{
  GuileCall *c = data;
  WuiWidgetType *t = wui_widget_type_lookup (c->app, c->w->type);
  SCM res;

  c->ok = FALSE;
  if (!t || !scm_is_true (t->prepare))
    {
      c->result = g_strdup ("");
      c->ok = TRUE;
      return NULL;
    }
  res = scm_call_1 (t->prepare, widget_to_scm_alist (c->w));
  if (scm_is_string (res))
    c->result = scm_to_utf8_stringn (res, NULL);
  else
    c->result = g_strdup ("");
  c->ok = TRUE;
  return NULL;
}

static void *
call_draw (void *data)
{
  GuileCall *c = data;
  WuiWidgetType *t = wui_widget_type_lookup (c->app, c->w->type);
  SCM res;
  SCM raw;

  c->ok = FALSE;
  if (!t || !scm_is_true (t->draw))
    {
      g_set_error (c->error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "no draw for type %s",
                   c->w->type ? c->w->type : "?");
      return NULL;
    }
  raw = scm_from_utf8_string (c->raw_json ? c->raw_json : "null");
  res = scm_call_2 (t->draw, widget_to_scm_alist (c->w), raw);
  if (!scm_is_string (res))
    {
      g_set_error (c->error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "draw must return JSON string");
      return NULL;
    }
  c->result = scm_to_utf8_stringn (res, NULL);
  c->ok = TRUE;
  return NULL;
}

static void *
call_hook (void *data)
{
  GuileCall *c = data;
  SCM invoke;

  invoke = scm_c_public_ref ("web-ui hooks", "invoke-hook");
  scm_call_3 (invoke,
              scm_from_utf8_symbol (c->hook_name),
              widget_to_scm_alist (c->w),
              scm_from_utf8_string (c->extra_json ? c->extra_json : "null"));
  c->ok = TRUE;
  return NULL;
}

gchar *
wui_guile_prepare (WuiApp *app, WuiWidget *w, GError **error)
{
  GuileCall c = { .app = app, .w = w, .error = error, .ok = FALSE };
  g_mutex_lock (&app->guile_lock);
  scm_with_guile (call_prepare, &c);
  g_mutex_unlock (&app->guile_lock);
  if (!c.ok)
    return NULL;
  return c.result;
}

gchar *
wui_guile_draw (WuiApp *app, WuiWidget *w, const gchar *raw_json, GError **error)
{
  GuileCall c = { .app = app, .w = w, .raw_json = raw_json, .error = error, .ok = FALSE };
  g_mutex_lock (&app->guile_lock);
  scm_with_guile (call_draw, &c);
  g_mutex_unlock (&app->guile_lock);
  if (!c.ok)
    return NULL;
  return c.result;
}

void
wui_guile_invoke_hook (WuiApp *app, const char *name, WuiWidget *w, const gchar *extra_json)
{
  GuileCall c = {
    .app = app, .w = w, .hook_name = name, .extra_json = extra_json, .ok = FALSE
  };
  g_mutex_lock (&app->guile_lock);
  scm_with_guile (call_hook, &c);
  g_mutex_unlock (&app->guile_lock);
}
