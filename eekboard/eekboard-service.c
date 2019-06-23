/* 
 * Copyright (C) 2010-2011 Daiki Ueno <ueno@unixuser.org>
 * Copyright (C) 2010-2011 Red Hat, Inc.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:eekboard-service
 * @short_description: base implementation of eekboard service
 *
 * Provides a dbus object, and contains the context.
 *
 * The #EekboardService class provides a base server side
 * implementation of eekboard service.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  /* HAVE_CONFIG_H */

#include "eekboard/eekboard-service.h"

#include "sm.puri.OSK0.h"

#include <stdio.h>

enum {
    PROP_0,
    PROP_OBJECT_PATH,
    PROP_CONNECTION,
    PROP_LAST
};

enum {
    DESTROYED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

#define EEKBOARD_SERVICE_GET_PRIVATE(obj)                               \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EEKBOARD_TYPE_SERVICE, EekboardServicePrivate))

struct _EekboardServicePrivate {
    GDBusConnection *connection;
    SmPuriOSK0 *dbus_interface;
    GDBusNodeInfo *introspection_data;
    guint registration_id;
    char *object_path;

    EekboardContextService *context;
};

G_DEFINE_TYPE (EekboardService, eekboard_service, G_TYPE_OBJECT);

static void
eekboard_service_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    EekboardService *service = EEKBOARD_SERVICE(object);
    GDBusConnection *connection;

    switch (prop_id) {
    case PROP_OBJECT_PATH:
        if (service->priv->object_path)
            g_free (service->priv->object_path);
        service->priv->object_path = g_value_dup_string (value);
        break;
    case PROP_CONNECTION:
        connection = g_value_get_object (value);
        if (service->priv->connection)
            g_object_unref (service->priv->connection);
        service->priv->connection = g_object_ref (connection);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eekboard_service_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
    EekboardService *service = EEKBOARD_SERVICE(object);

    switch (prop_id) {
    case PROP_OBJECT_PATH:
        g_value_set_string (value, service->priv->object_path);
        break;
    case PROP_CONNECTION:
        g_value_set_object (value, service->priv->connection);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eekboard_service_dispose (GObject *object)
{
    EekboardService *service = EEKBOARD_SERVICE(object);
    if (service->priv->connection) {
        if (service->priv->registration_id > 0) {
            g_dbus_connection_unregister_object (service->priv->connection,
                                                 service->priv->registration_id);
            service->priv->registration_id = 0;
        }

        g_object_unref (service->priv->connection);
        service->priv->connection = NULL;
    }

    if (service->priv->introspection_data) {
        g_dbus_node_info_unref (service->priv->introspection_data);
        service->priv->introspection_data = NULL;
    }

    G_OBJECT_CLASS (eekboard_service_parent_class)->dispose (object);
}

static void
eekboard_service_finalize (GObject *object)
{
    EekboardService *service = EEKBOARD_SERVICE(object);

    g_free (service->priv->object_path);

    G_OBJECT_CLASS (eekboard_service_parent_class)->finalize (object);
}

static gboolean
handle_set_visible(SmPuriOSK0 *object, GDBusMethodInvocation *invocation,
                   gboolean arg_visible, gpointer user_data) {
    EekboardService *service = user_data;
    if (arg_visible) {
        eekboard_context_service_show_keyboard (service->priv->context);
    } else {
        eekboard_context_service_hide_keyboard (service->priv->context);
    }
    sm_puri_osk0_complete_set_visible(object, invocation);
    return TRUE;
}

static void
eekboard_service_constructed (GObject *object)
{
    EekboardService *service = EEKBOARD_SERVICE(object);

    service->priv->dbus_interface = sm_puri_osk0_skeleton_new();
    sm_puri_osk0_set_visible(service->priv->dbus_interface, FALSE); // TODO: use actual value
    g_signal_connect(service->priv->dbus_interface, "handle-set-visible",
                     G_CALLBACK(handle_set_visible), service);

    if (service->priv->connection && service->priv->object_path) {
        GError *error = NULL;

        if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(service->priv->dbus_interface),
                                              service->priv->connection,
                                              service->priv->object_path,
                                              &error)) {
            g_warning("Error registering dbus object: %s\n", error->message);
            g_clear_error(&error);
        }
    }

    // CreateContext
    EekboardServiceClass *klass = EEKBOARD_SERVICE_GET_CLASS(service);

    EekboardContextService *context;

    g_assert (klass->create_context);
    context = klass->create_context (service);
    g_object_set_data_full (G_OBJECT(context),
                            "owner", g_strdup ("sender"),
                            (GDestroyNotify)g_free);
    service->priv->context = context;
    eekboard_context_service_enable (context);
}

static void
eekboard_service_class_init (EekboardServiceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec *pspec;

    g_type_class_add_private (gobject_class,
                              sizeof (EekboardServicePrivate));

    klass->create_context = NULL;

    gobject_class->constructed = eekboard_service_constructed;
    gobject_class->set_property = eekboard_service_set_property;
    gobject_class->get_property = eekboard_service_get_property;
    gobject_class->dispose = eekboard_service_dispose;
    gobject_class->finalize = eekboard_service_finalize;

    /**
     * EekboardService::destroyed:
     * @service: an #EekboardService
     *
     * The ::destroyed signal is emitted when the service is vanished.
     */
    signals[DESTROYED] =
        g_signal_new (I_("destroyed"),
                      G_TYPE_FROM_CLASS(gobject_class),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);

    /**
     * EekboardService:object-path:
     *
     * D-Bus object path.
     */
    pspec = g_param_spec_string ("object-path",
                                 "Object-path",
                                 "Object-path",
                                 NULL,
                                 G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class,
                                     PROP_OBJECT_PATH,
                                     pspec);

    /**
     * EekboardService:connection:
     *
     * D-Bus connection.
     */
    pspec = g_param_spec_object ("connection",
                                 "Connection",
                                 "Connection",
                                 G_TYPE_DBUS_CONNECTION,
                                 G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class,
                                     PROP_CONNECTION,
                                     pspec);
}

static void
eekboard_service_init (EekboardService *self)
{
    self->priv = EEKBOARD_SERVICE_GET_PRIVATE(self);
}

/**
 * eekboard_service_new:
 * @connection: a #GDBusConnection
 * @object_path: object path
 */
EekboardService *
eekboard_service_new (GDBusConnection *connection,
                      const gchar     *object_path)
{
    return g_object_new (EEKBOARD_TYPE_SERVICE,
                         "object-path", object_path,
                         "connection", connection,
                         NULL);
}
