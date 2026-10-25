/*
 * starter-web-ui - app.h
 *
 * Copyright (C) 2026 Christian Brunello <brncrs@gmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WUI_APP_H_INCLUDED
#define WUI_APP_H_INCLUDED

#include <glib.h>
#include <gio/gio.h>
#include <libsoup/soup.h>

G_BEGIN_DECLS

typedef enum {
  WUI_AUTH_NONE = 0,
  WUI_AUTH_PASSWORD
} WuiAuthMethod;

typedef enum {
  WUI_GROUP_USER = 0,
  WUI_GROUP_ADMIN
} WuiGroup;

typedef struct WuiWidget WuiWidget;
typedef struct WuiWidgetType WuiWidgetType;
typedef struct WuiApp WuiApp;

struct WuiWidget {
  gchar *id;
  gchar *type;          /* widget type name */
  gchar *title;
  gchar *io_kind;       /* digital | analog */
  gchar *entity_source; /* bridge | history */
  gchar *entity_domain; /* input | output */
  gchar *entity_name;   /* DigIn1 or label name */
  gchar *entity_label;  /* history label */
  /* Optional display feedback (typically bridge output DigOut/AnaOut). */
  gchar *feedback_source;
  gchar *feedback_domain;
  gchar *feedback_name;
  gchar *feedback_label;
  GHashTable *fields;   /* name -> string value */
  gchar *cached_view;   /* last Guile draw JSON */
  guint refresh_source; /* GSource id */
  guint usr1_clear_source; /* timeout to clear USR1 after UI set */
};

struct WuiApp {
  GMainLoop *loop;
  gchar *config_path;

  /* daemon config */
  gchar *http_address;
  guint http_port;
  gchar *data_dir;
  gchar *user_config_name; /* relative to data_dir */
  gchar *user_config_path; /* absolute */
  gchar *plugin_dir;
  gchar *scheme_dir;
  gchar *static_dir;
  gchar *bridge_base_url;
  gchar *bridge_token;
  guint usr1_clear_timeout_sec; /* clear USR1 after set; 0 = never */
  guint bridge_put_retries;     /* attempts for PUT (incl. first) */
  guint bridge_put_backoff_ms;  /* base delay; doubles each retry (bounce) */

  gchar *mysql_host;
  gchar *mysql_database;
  gchar *mysql_user;
  gchar *mysql_password;

  WuiAuthMethod auth_method;
  gchar *auth_user;
  gchar *auth_password;
  WuiGroup auth_group;
  gchar *cookie_name;
  guint session_days;

  /* runtime */
  SoupServer *soup;
  SoupSession *bridge_session;
  GPtrArray *widgets;           /* WuiWidget* */
  GHashTable *widget_types;     /* name -> WuiWidgetType* (C mirror) */
  GHashTable *sessions;         /* sid -> WuiSession* */
  GMutex guile_lock;
  gboolean guile_ready;
};

WuiWidget *wui_widget_new (void);
void wui_widget_free (WuiWidget *w);
gchar *wui_widget_field_get (WuiWidget *w, const gchar *name);
void wui_widget_field_set (WuiWidget *w, const gchar *name, const gchar *value);

G_END_DECLS

#endif /* WUI_APP_H_INCLUDED */
