/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
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

#ifndef __NET_CONNECTION_EDITOR_H
#define __NET_CONNECTION_EDITOR_H

#include <glib-object.h>

#include <gtk/gtk.h>
#include <nm-client.h>
#include <nm-access-point.h>
#include <nm-remote-settings.h>

G_BEGIN_DECLS

#define NET_TYPE_CONNECTION_EDITOR         (net_connection_editor_get_type ())
#define NET_CONNECTION_EDITOR(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), NET_TYPE_CONNECTION_EDITOR, NetConnectionEditor))
#define NET_CONNECTION_EDITOR_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), NET_TYPE_CONNECTION_EDITOR, NetConnectionEditorClass))
#define NET_IS_CONNECTION_EDITOR(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), NET_TYPE_CONNECTION_EDITOR))
#define NET_IS_CONNECTION_EDITOR_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), NET_TYPE_CONNECTION_EDITOR))
#define NET_CONNECTION_EDITOR_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), NET_TYPE_CONNECTION_EDITOR, NetConnectionEditorClass))

typedef struct _NetConnectionEditor          NetConnectionEditor;
typedef struct _NetConnectionEditorClass     NetConnectionEditorClass;

struct _NetConnectionEditor
{
         GObject parent;

        GtkWidget        *parent_window;
        NMClient         *client;
        NMDevice         *device;
        NMRemoteSettings *settings;

        NMConnection     *connection;
        NMConnection     *orig_connection;
        NMAccessPoint    *ap;

        GtkBuilder       *builder;
        GtkWidget        *window;

        GSList *initializing_pages;
        GSList *pages;
};

struct _NetConnectionEditorClass
{
        GObjectClass parent_class;

        void (*done) (NetConnectionEditor *details, gboolean success);
};

GType                net_connection_editor_get_type (void);
NetConnectionEditor *net_connection_editor_new      (GtkWindow        *parent_window,
                                                     NMConnection     *connection,
                                                     NMDevice         *device,
                                                     NMAccessPoint    *ap,
                                                     NMClient         *client,
                                                     NMRemoteSettings *settings);
void                 net_connection_editor_present  (NetConnectionEditor   *details);

G_END_DECLS

#endif /* __NET_CONNECTION_EDITOR_H */
