/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>

#include <nm-utils.h>
#include <nm-device-wifi.h>

#include "net-connection-editor.h"
#include "net-connection-editor-resources.h"
#include "ce-page-details.h"
#include "ce-page-wifi.h"
#include "ce-page-ip4.h"
#include "ce-page-ip6.h"
#include "ce-page-security.h"
#include "ce-page-reset.h"

#include "egg-list-box/egg-list-box.h"

enum {
        DONE,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (NetConnectionEditor, net_connection_editor, G_TYPE_OBJECT)

static void
selection_changed (GtkTreeSelection *selection, NetConnectionEditor *editor)
{
        GtkWidget *widget;
        GtkTreeModel *model;
        GtkTreeIter iter;
        gint page;

        gtk_tree_selection_get_selected (selection, &model, &iter);
        gtk_tree_model_get (model, &iter, 1, &page, -1);

        widget = GTK_WIDGET (gtk_builder_get_object (editor->builder,
                                                     "details_notebook"));
        gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), page);
}

static void
cancel_editing (NetConnectionEditor *editor)
{
        gtk_widget_hide (editor->window);
        g_signal_emit (editor, signals[DONE], 0, FALSE);
}

static void
update_connection (NetConnectionEditor *editor)
{
        GHashTable *settings;

        settings = nm_connection_to_hash (editor->connection, NM_SETTING_HASH_FLAG_ALL);
        nm_connection_replace_settings (editor->orig_connection, settings, NULL);
        g_hash_table_destroy (settings);
}

static void
update_complete (NetConnectionEditor *editor, GError *error)
{
        gtk_widget_hide (editor->window);
        g_signal_emit (editor, signals[DONE], 0, !error);
}

static void
updated_connection_cb (NMRemoteConnection *connection, GError *error, gpointer data)
{
        NetConnectionEditor *editor = data;

        nm_connection_clear_secrets (NM_CONNECTION (connection));

        update_complete (editor, error);
}

static void
apply_edits (NetConnectionEditor *editor)
{
        update_connection (editor);
        nm_remote_connection_commit_changes (NM_REMOTE_CONNECTION (editor->orig_connection),
                                             updated_connection_cb, editor);
}

static void
net_connection_editor_init (NetConnectionEditor *editor)
{
        GtkTreeSelection *selection;
        GError *error = NULL;
        GtkWidget *button;

        editor->builder = gtk_builder_new ();

        gtk_builder_add_from_resource (editor->builder,
                                       "/org/gnome/control-center/network/connection-editor.ui",
                                       &error);
        if (error != NULL) {
                g_warning ("Could not load ui file: %s", error->message);
                g_error_free (error);
                return;
        }

        /* set up widgets */

        editor->window = GTK_WIDGET (gtk_builder_get_object (editor->builder, "details_dialog"));
        selection = GTK_TREE_SELECTION (gtk_builder_get_object (editor->builder,
                                                                "details_page_list_selection"));
        g_signal_connect (selection, "changed",
                          G_CALLBACK (selection_changed), editor);

        button = GTK_WIDGET (gtk_builder_get_object (editor->builder, "details_cancel_button"));
        g_signal_connect_swapped (button, "clicked",
                                  G_CALLBACK (cancel_editing), editor);
        g_signal_connect_swapped (editor->window, "delete-event",
                                  G_CALLBACK (cancel_editing), editor);

        button = GTK_WIDGET (gtk_builder_get_object (editor->builder, "details_apply_button"));
        g_signal_connect_swapped (button, "clicked",
                                  G_CALLBACK (apply_edits), editor);
}

static void
net_connection_editor_finalize (GObject *object)
{
        NetConnectionEditor *editor = NET_CONNECTION_EDITOR (object);

        g_clear_object (&editor->connection);
        g_clear_object (&editor->orig_connection);
        if (editor->window) {
                gtk_widget_destroy (editor->window);
                editor->window = NULL;
        }
        g_clear_object (&editor->parent_window);
        g_clear_object (&editor->builder);
        g_clear_object (&editor->device);
        g_clear_object (&editor->settings);
        g_clear_object (&editor->client);
        g_clear_object (&editor->ap);

        G_OBJECT_CLASS (net_connection_editor_parent_class)->finalize (object);
}

static void
net_connection_editor_class_init (NetConnectionEditorClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);

        g_resources_register (net_connection_editor_get_resource ());

        object_class->finalize = net_connection_editor_finalize;

        signals[DONE] = g_signal_new ("done",
                                      G_OBJECT_CLASS_TYPE (object_class),
                                      G_SIGNAL_RUN_FIRST,
                                      G_STRUCT_OFFSET (NetConnectionEditorClass, done),
                                      NULL, NULL,
                                      NULL,
                                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static void
net_connection_editor_update_title (NetConnectionEditor *editor)
{
        NMSettingWireless *sw;
        const GByteArray *ssid;
        gchar *id;

        sw = nm_connection_get_setting_wireless (editor->connection);
        ssid = nm_setting_wireless_get_ssid (sw);
        id = nm_utils_ssid_to_utf8 (ssid);
        gtk_window_set_title (GTK_WINDOW (editor->window), id);
        g_free (id);
}

static gboolean
editor_is_initialized (NetConnectionEditor *editor)
{
        return editor->initializing_pages == NULL;
}

static void
validate (NetConnectionEditor *editor)
{
        gboolean valid = FALSE;
        GSList *l;

        if (!editor_is_initialized (editor))
                goto done;

        valid = TRUE;
        for (l = editor->pages; l; l = l->next) {
                GError *error = NULL;

                if (!ce_page_validate (CE_PAGE (l->data), editor->connection, &error)) {
                        valid = FALSE;
                        if (error) {
                                g_warning ("Invalid setting %s: %s", ce_page_get_title (CE_PAGE (l->data)), error->message);
                                g_error_free (error);
                        } else {
                                g_warning ("Invalid setting %s", ce_page_get_title (CE_PAGE (l->data)));
                        }
                }
        }

done:
        gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (editor->builder, "details_apply_button")), valid);
}

static void
page_changed (CEPage *page, gpointer user_data)
{
        NetConnectionEditor *editor= user_data;

        validate (editor);
}

static gboolean
idle_validate (gpointer user_data)
{
        validate (NET_CONNECTION_EDITOR (user_data));

        return G_SOURCE_REMOVE;
}

static void
recheck_initialization (NetConnectionEditor *editor)
{
        GtkNotebook *notebook;

        if (!editor_is_initialized (editor))
                return;

        notebook = GTK_NOTEBOOK (gtk_builder_get_object (editor->builder, "details_notebook"));
        gtk_notebook_set_current_page (notebook, 0);

        g_idle_add (idle_validate, editor);
}

static void
page_initialized (CEPage *page, GError *error, NetConnectionEditor *editor)
{
        GtkNotebook *notebook;
        GtkWidget *widget;
        gint position;
        GList *children, *l;
        gint i;

        notebook = GTK_NOTEBOOK (gtk_builder_get_object (editor->builder, "details_notebook"));
        widget = ce_page_get_page (page);
        position = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (page), "position"));
        g_object_set_data (G_OBJECT (widget), "position", GINT_TO_POINTER (position));
        children = gtk_container_get_children (GTK_CONTAINER (notebook));
        for (l = children, i = 0; l; l = l->next, i++) {
                gint pos = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (l->data), "position"));
                if (pos > position)
                        break;
        }
        g_list_free (children);
        gtk_notebook_insert_page (notebook, widget, NULL, i);

        editor->initializing_pages = g_slist_remove (editor->initializing_pages, page);
        editor->pages = g_slist_append (editor->pages, page);

        recheck_initialization (editor);
}

typedef struct {
        NetConnectionEditor *editor;
        CEPage *page;
        const gchar *setting_name;
        gboolean canceled;
} GetSecretsInfo;

static void
get_secrets_cb (NMRemoteConnection *connection,
                GHashTable         *secrets,
                GError             *error,
                gpointer            user_data)
{
        GetSecretsInfo *info = user_data;

        if (info->canceled) {
                g_free (info);
                return;
        }

        ce_page_complete_init (info->page, info->setting_name, secrets, error);
        g_free (info);
}

static void
get_secrets_for_page (NetConnectionEditor *editor,
                      CEPage              *page,
                      const gchar         *setting_name)
{
        GetSecretsInfo *info;

        info = g_new0 (GetSecretsInfo, 1);
        info->editor = editor;
        info->page = page;
        info->setting_name = setting_name;

        nm_remote_connection_get_secrets (NM_REMOTE_CONNECTION (editor->orig_connection),
                                          setting_name,
                                          get_secrets_cb,
                                          info);
}

static void
add_page (NetConnectionEditor *editor, CEPage *page)
{
        GtkListStore *store;
        GtkTreeIter iter;
        const gchar *title;
        gint position;

        store = GTK_LIST_STORE (gtk_builder_get_object (editor->builder,
                                                "details_store"));
        title = ce_page_get_title (page);
        position = g_slist_length (editor->initializing_pages);
        g_object_set_data (G_OBJECT (page), "position", GINT_TO_POINTER (position));
        gtk_list_store_insert_with_values (store, &iter, -1,
                                           0, title,
                                           1, position,
                                           -1);
        editor->initializing_pages = g_slist_append (editor->initializing_pages, page);

        g_signal_connect (page, "changed", G_CALLBACK (page_changed), editor);
        g_signal_connect (page, "initialized", G_CALLBACK (page_initialized), editor);
}

static void
net_connection_editor_set_connection (NetConnectionEditor *editor,
                              NMConnection        *connection)
{
        GSList *pages, *l;

        editor->connection = nm_connection_duplicate (connection);
        editor->orig_connection = g_object_ref (connection);

        net_connection_editor_update_title (editor);

        add_page (editor, ce_page_details_new (editor->connection, editor->client, editor->settings, editor->device, editor->ap));
        add_page (editor, ce_page_wifi_new (editor->connection, editor->client, editor->settings));
        add_page (editor, ce_page_ip4_new (editor->connection, editor->client, editor->settings));
        add_page (editor, ce_page_ip6_new (editor->connection, editor->client, editor->settings));
        add_page (editor, ce_page_security_new (editor->connection, editor->client, editor->settings));
        add_page (editor, ce_page_reset_new (editor->connection, editor->client, editor->settings));

        pages = g_slist_copy (editor->initializing_pages);
        for (l = pages; l; l = l->next) {
                CEPage *page = l->data;
                const gchar *security_setting;

                security_setting = ce_page_get_security_setting (page);
                if (!security_setting) {
                        ce_page_complete_init (page, NULL, NULL, NULL);
                } else {
                        get_secrets_for_page (editor, page, security_setting);
                }
        }
        g_slist_free (pages);
}

NetConnectionEditor *
net_connection_editor_new (GtkWindow        *parent_window,
                           NMConnection     *connection,
                           NMDevice         *device,
                           NMAccessPoint    *ap,
                           NMClient         *client,
                           NMRemoteSettings *settings)
{
        NetConnectionEditor *editor;

        editor = g_object_new (NET_TYPE_CONNECTION_EDITOR, NULL);

        if (parent_window) {
                editor->parent_window = g_object_ref (parent_window);
                gtk_window_set_transient_for (GTK_WINDOW (editor->window),
                                              parent_window);
        }
        if (ap)
                editor->ap = g_object_ref (ap);
        editor->device = g_object_ref (device);
        editor->client = g_object_ref (client);
        editor->settings = g_object_ref (settings);

        net_connection_editor_set_connection (editor, connection);

        return editor;
}

void
net_connection_editor_present (NetConnectionEditor *editor)
{
        gtk_window_present (GTK_WINDOW (editor->window));
}