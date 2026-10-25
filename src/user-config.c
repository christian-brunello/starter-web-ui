/*
 * starter-web-ui - user-config.c
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "user-config.h"

#include <errno.h>
#include <string.h>
#include <glib/gstdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

WuiWidget *
wui_widget_new (void)
{
  WuiWidget *w = g_new0 (WuiWidget, 1);
  w->fields = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  return w;
}

void
wui_widget_free (WuiWidget *w)
{
  if (w == NULL)
    return;
  if (w->refresh_source)
    g_source_remove (w->refresh_source);
  if (w->usr1_clear_source)
    g_source_remove (w->usr1_clear_source);
  g_free (w->id);
  g_free (w->type);
  g_free (w->title);
  g_free (w->io_kind);
  g_free (w->entity_source);
  g_free (w->entity_domain);
  g_free (w->entity_name);
  g_free (w->entity_label);
  g_free (w->cached_view);
  if (w->fields)
    g_hash_table_destroy (w->fields);
  g_free (w);
}

gchar *
wui_widget_field_get (WuiWidget *w, const gchar *name)
{
  if (w == NULL || w->fields == NULL || name == NULL)
    return NULL;
  return g_strdup (g_hash_table_lookup (w->fields, name));
}

void
wui_widget_field_set (WuiWidget *w, const gchar *name, const gchar *value)
{
  g_return_if_fail (w != NULL && name != NULL);
  g_hash_table_insert (w->fields, g_strdup (name), g_strdup (value ? value : ""));
}

WuiWidget *
wui_app_find_widget (WuiApp *app, const gchar *id)
{
  guint i;
  if (app == NULL || app->widgets == NULL || id == NULL)
    return NULL;
  for (i = 0; i < app->widgets->len; i++)
    {
      WuiWidget *w = g_ptr_array_index (app->widgets, i);
      if (g_strcmp0 (w->id, id) == 0)
        return w;
    }
  return NULL;
}

static gchar *
attr (xmlNode *n, const char *name)
{
  xmlChar *v = xmlGetProp (n, (const xmlChar *) name);
  gchar *out = v ? g_strdup ((const char *) v) : NULL;
  if (v)
    xmlFree (v);
  return out;
}

gboolean
wui_user_config_load (WuiApp *app, GError **error)
{
  xmlDoc *doc;
  xmlNode *root, *n;

  g_return_val_if_fail (app != NULL, FALSE);

  if (app->widgets == NULL)
    app->widgets = g_ptr_array_new_with_free_func ((GDestroyNotify) wui_widget_free);
  else
    g_ptr_array_set_size (app->widgets, 0);

  if (!g_file_test (app->user_config_path, G_FILE_TEST_IS_REGULAR))
    return TRUE; /* empty dashboard */

  doc = xmlReadFile (app->user_config_path, NULL, 0);
  if (doc == NULL)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   "failed to parse user-config %s", app->user_config_path);
      return FALSE;
    }

  root = xmlDocGetRootElement (doc);
  if (root == NULL || xmlStrcmp (root->name, (const xmlChar *) "user-config") != 0)
    {
      xmlFreeDoc (doc);
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "root must be <user-config>");
      return FALSE;
    }

  for (n = root->children; n; n = n->next)
    {
      xmlNode *c;
      WuiWidget *w;

      if (n->type != XML_ELEMENT_NODE || xmlStrcmp (n->name, (const xmlChar *) "widget") != 0)
        continue;

      w = wui_widget_new ();
      w->id = attr (n, "id");
      w->type = attr (n, "type");
      w->title = attr (n, "title");
      w->io_kind = attr (n, "io-kind");

      for (c = n->children; c; c = c->next)
        {
          if (c->type != XML_ELEMENT_NODE)
            continue;
          if (xmlStrcmp (c->name, (const xmlChar *) "entity") == 0)
            {
              w->entity_source = attr (c, "source");
              w->entity_domain = attr (c, "domain");
              w->entity_name = attr (c, "name");
              w->entity_label = attr (c, "label");
            }
          else if (xmlStrcmp (c->name, (const xmlChar *) "fields") == 0)
            {
              xmlNode *f;
              for (f = c->children; f; f = f->next)
                {
                  if (f->type == XML_ELEMENT_NODE
                      && xmlStrcmp (f->name, (const xmlChar *) "field") == 0)
                    {
                      gchar *fname = attr (f, "name");
                      xmlChar *content = xmlNodeGetContent (f);
                      if (fname)
                        wui_widget_field_set (w, fname,
                                              content ? (const char *) content : "");
                      g_free (fname);
                      if (content)
                        xmlFree (content);
                    }
                }
            }
        }

      if (w->id && w->type)
        g_ptr_array_add (app->widgets, w);
      else
        wui_widget_free (w);
    }

  xmlFreeDoc (doc);
  return TRUE;
}

gboolean
wui_user_config_save (WuiApp *app, GError **error)
{
  xmlDoc *doc;
  xmlNode *root;
  gchar *tmp_path = NULL;
  guint i;
  int rc;

  g_return_val_if_fail (app != NULL, FALSE);

  if (g_mkdir_with_parents (app->data_dir, 0755) != 0)
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   "cannot create data-dir %s", app->data_dir);
      return FALSE;
    }

  doc = xmlNewDoc ((const xmlChar *) "1.0");
  root = xmlNewNode (NULL, (const xmlChar *) "user-config");
  xmlNewProp (root, (const xmlChar *) "version", (const xmlChar *) "1");
  xmlDocSetRootElement (doc, root);

  for (i = 0; i < app->widgets->len; i++)
    {
      WuiWidget *w = g_ptr_array_index (app->widgets, i);
      xmlNode *wn = xmlNewChild (root, NULL, (const xmlChar *) "widget", NULL);
      xmlNode *ent;
      xmlNode *fields;
      GHashTableIter iter;
      gpointer k, v;

      xmlNewProp (wn, (const xmlChar *) "id", (const xmlChar *) w->id);
      xmlNewProp (wn, (const xmlChar *) "type", (const xmlChar *) w->type);
      if (w->title)
        xmlNewProp (wn, (const xmlChar *) "title", (const xmlChar *) w->title);
      if (w->io_kind)
        xmlNewProp (wn, (const xmlChar *) "io-kind", (const xmlChar *) w->io_kind);

      ent = xmlNewChild (wn, NULL, (const xmlChar *) "entity", NULL);
      if (w->entity_source)
        xmlNewProp (ent, (const xmlChar *) "source", (const xmlChar *) w->entity_source);
      if (w->entity_domain)
        xmlNewProp (ent, (const xmlChar *) "domain", (const xmlChar *) w->entity_domain);
      if (w->entity_name)
        xmlNewProp (ent, (const xmlChar *) "name", (const xmlChar *) w->entity_name);
      if (w->entity_label)
        xmlNewProp (ent, (const xmlChar *) "label", (const xmlChar *) w->entity_label);

      fields = xmlNewChild (wn, NULL, (const xmlChar *) "fields", NULL);
      g_hash_table_iter_init (&iter, w->fields);
      while (g_hash_table_iter_next (&iter, &k, &v))
        {
          xmlNode *fn = xmlNewChild (fields, NULL, (const xmlChar *) "field",
                                     (const xmlChar *) (v ? v : ""));
          xmlNewProp (fn, (const xmlChar *) "name", (const xmlChar *) k);
        }
    }

  tmp_path = g_strdup_printf ("%s.tmp", app->user_config_path);
  rc = xmlSaveFormatFileEnc (tmp_path, doc, "UTF-8", 1);
  xmlFreeDoc (doc);
  if (rc < 0)
    {
      g_free (tmp_path);
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "failed to write %s",
                   app->user_config_path);
      return FALSE;
    }
  if (g_rename (tmp_path, app->user_config_path) != 0)
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   "failed to replace %s", app->user_config_path);
      g_free (tmp_path);
      return FALSE;
    }
  g_free (tmp_path);
  return TRUE;
}
