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
 * SECTION:eekboard-context-service
 * @short_description: base server implementation of eekboard input
 * context service
 *
 * The #EekboardService class provides a base server side
 * implementation of eekboard input context service.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  /* HAVE_CONFIG_H */

#include "eekboard/key-emitter.h"
#include "eekboard/eekboard-context-service.h"
//#include "eekboard/eekboard-xklutil.h"
//#include "eek/eek-xkl.h"

#define CSW 640
#define CSH 480

enum {
    PROP_0,
    PROP_OBJECT_PATH,
    PROP_CONNECTION,
    PROP_CLIENT_NAME,
    PROP_KEYBOARD,
    PROP_VISIBLE,
    PROP_FULLSCREEN,
    PROP_LAST
};

enum {
    ENABLED,
    DISABLED,
    DESTROYED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

#define EEKBOARD_CONTEXT_SERVICE_GET_PRIVATE(obj)                       \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EEKBOARD_TYPE_CONTEXT_SERVICE, EekboardContextServicePrivate))

struct _EekboardContextServicePrivate {
    GDBusConnection *connection;
    GDBusNodeInfo *introspection_data;

    guint registration_id;
    char *object_path;
    char *client_name;

    gboolean enabled;
    gboolean visible;
    gboolean fullscreen;

    EekKeyboard *keyboard;
    GHashTable *keyboard_hash;

    gulong key_pressed_handler;
    gulong key_released_handler;
    gulong key_cancelled_handler;

    EekKey *repeat_key;
    guint repeat_timeout_id;
    gboolean repeat_triggered;

    GSettings *settings;
};

G_DEFINE_TYPE (EekboardContextService, eekboard_context_service, G_TYPE_OBJECT);

static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='org.fedorahosted.Eekboard.Context'>"
    "    <method name='AddKeyboard'>"
    "      <arg direction='in' type='s' name='keyboard'/>"
    "      <arg direction='out' type='u' name='keyboard_id'/>"
    "    </method>"
    "    <method name='RemoveKeyboard'>"
    "      <arg direction='in' type='u' name='keyboard_id'/>"
    "    </method>"
    "    <method name='SetKeyboard'>"
    "      <arg type='u' name='keyboard_id'/>"
    "    </method>"
    "    <method name='SetFullscreen'>"
    "      <arg type='b' name='fullscreen'/>"
    "    </method>"
    "    <method name='ShowKeyboard'/>"
    "    <method name='HideKeyboard'/>"
    "    <method name='SetGroup'>"
    "      <arg type='i' name='group'/>"
    "    </method>"
    "    <method name='PressKeycode'>"
    "      <arg type='u' name='keycode'/>"
    "    </method>"
    "    <method name='ReleaseKeycode'>"
    "      <arg type='u' name='keycode'/>"
    "    </method>"
    /* signals */
    "    <signal name='Enabled'/>"
    "    <signal name='Disabled'/>"
    "    <signal name='Destroyed'/>"
    "    <signal name='KeyActivated'>"
    "      <arg type='u' name='keycode'/>"
    "      <arg type='v' name='symbol'/>"
    "      <arg type='u' name='modifiers'/>"
    "    </signal>"
    "    <signal name='VisibilityChanged'>"
    "      <arg type='b' name='visible'/>"
    "    </signal>"
    "    <signal name='KeyboardChanged'>"
    "      <arg type='u' name='keyboard_id'/>"
    "    </signal>"
    "    <signal name='GroupChanged'>"
    "      <arg type='i' name='group'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

static void connect_keyboard_signals (EekboardContextService *context);
static void disconnect_keyboard_signals
                                     (EekboardContextService *context);
static void emit_visibility_changed_signal
                                     (EekboardContextService *context,
                                      gboolean                visible);

static Display *display = NULL;

static EekKeyboard *
eekboard_context_service_real_create_keyboard (EekboardContextService *self,
                                               const gchar            *keyboard_type)
{
    EekKeyboard *keyboard;
    EekLayout *layout;
    GError *error;

    if (g_str_has_prefix (keyboard_type, "xkb:")) {
        /* TODO: Depends on xklavier
        XklConfigRec *rec =
            eekboard_xkl_config_rec_from_string (&keyboard_type[4]);

        if (display == NULL)
            //display = XOpenDisplay (NULL);
            return NULL; // FIXME: replace with wl display

        error = NULL;
        layout = eek_xkl_layout_new (display, &error);
        if (layout == NULL) {
            g_warning ("can't create keyboard %s: %s",
                       keyboard_type, error->message);
            g_error_free (error);
            return NULL;
        }

        if (!eek_xkl_layout_set_config (EEK_XKL_LAYOUT(layout), rec)) {
            g_object_unref (layout);
            return NULL;
        }
        */
        return NULL;
    } else {
        error = NULL;
        layout = eek_xml_layout_new (keyboard_type, &error);
        if (layout == NULL) {
            g_warning ("can't create keyboard %s: %s",
                       keyboard_type, error->message);
            g_error_free (error);
            error = NULL;
            layout = eek_xml_layout_new ("us", &error);
            if (layout == NULL) {
                g_error ("failed to create fallback layout: %s", error->message);
                g_error_free (error);
                return NULL;
            }
        }
    }
    keyboard = eek_keyboard_new (layout, CSW, CSH);
    g_object_unref (layout);

    return keyboard;
}

static void
eekboard_context_service_real_show_keyboard (EekboardContextService *self)
{
    gboolean visible = self->priv->visible;
    self->priv->visible = TRUE;
    if (visible != self->priv->visible)
        emit_visibility_changed_signal (self,
                                        self->priv->visible);
}

static void
eekboard_context_service_real_hide_keyboard (EekboardContextService *self)
{
    gboolean visible = self->priv->visible;
    self->priv->visible = FALSE;
    if (visible != self->priv->visible)
        emit_visibility_changed_signal (self,
                                        self->priv->visible);
}

static void
eekboard_context_service_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
    EekboardContextService *context = EEKBOARD_CONTEXT_SERVICE(object);
    GDBusConnection *connection;

    switch (prop_id) {
    case PROP_OBJECT_PATH:
        if (context->priv->object_path)
            g_free (context->priv->object_path);
        context->priv->object_path = g_value_dup_string (value);
        break;
    case PROP_CONNECTION:
        connection = g_value_get_object (value);
        if (context->priv->connection)
            g_object_unref (context->priv->connection);
        context->priv->connection = g_object_ref (connection);
        break;
    case PROP_CLIENT_NAME:
        if (context->priv->client_name)
            g_free (context->priv->client_name);
        context->priv->client_name = g_value_dup_string (value);
        break;
    case PROP_KEYBOARD:
        if (context->priv->keyboard)
            g_object_unref (context->priv->keyboard);
        context->priv->keyboard = g_value_get_object (value);
        break;
    case PROP_VISIBLE:
        /*if (context->priv->keyboard) {
            if (g_value_get_boolean (value))
                eekboard_context_service_show_keyboard (context);
            else
                eekboard_context_service_hide_keyboard (context);
        }*/
        break;
    case PROP_FULLSCREEN:
        context->priv->fullscreen = g_value_get_boolean (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eekboard_context_service_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
    EekboardContextService *context = EEKBOARD_CONTEXT_SERVICE(object);

    switch (prop_id) {
    case PROP_OBJECT_PATH:
        g_value_set_string (value, context->priv->object_path);
        break;
    case PROP_CONNECTION:
        g_value_set_object (value, context->priv->connection);
        break;
    case PROP_CLIENT_NAME:
        g_value_set_string (value, context->priv->client_name);
        break;
    case PROP_KEYBOARD:
        g_value_set_object (value, context->priv->keyboard);
        break;
    case PROP_VISIBLE:
        g_value_set_boolean (value, context->priv->visible);
        break;
    case PROP_FULLSCREEN:
        g_value_set_boolean (value, context->priv->fullscreen);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
eekboard_context_service_dispose (GObject *object)
{
    EekboardContextService *context = EEKBOARD_CONTEXT_SERVICE(object);

    if (context->priv->keyboard_hash) {
        g_hash_table_destroy (context->priv->keyboard_hash);
        context->priv->keyboard_hash = NULL;
    }

    if (context->priv->connection) {
        if (context->priv->registration_id > 0) {
            g_dbus_connection_unregister_object (context->priv->connection,
                                                 context->priv->registration_id);
            context->priv->registration_id = 0;
        }

        g_object_unref (context->priv->connection);
        context->priv->connection = NULL;
    }

    if (context->priv->introspection_data) {
        g_dbus_node_info_unref (context->priv->introspection_data);
        context->priv->introspection_data = NULL;
    }

    G_OBJECT_CLASS (eekboard_context_service_parent_class)->
        dispose (object);
}

static void
eekboard_context_service_finalize (GObject *object)
{
    EekboardContextService *context = EEKBOARD_CONTEXT_SERVICE(object);

    g_free (context->priv->object_path);
    g_free (context->priv->client_name);

    G_OBJECT_CLASS (eekboard_context_service_parent_class)->
        finalize (object);
}

static void
settings_get_layout(GSettings *settings, char **type, char **layout)
{
    GVariant *inputs = g_settings_get_value(settings, "sources");
    guint32 index;
    g_settings_get(settings, "current", "u", &index);

    GVariantIter *iter;
    g_variant_get(inputs, "a(ss)", &iter);

    for (unsigned i = 0;
         g_variant_iter_loop(iter, "(ss)", type, layout);
         i++) {
        if (i == index) {
            //printf("Found layout %s %s\n", *type, *layout);
            break;
        }
    }
    g_variant_iter_free(iter);
}


static void
settings_update_layout(EekboardContextService *context) {
    EekboardContextServiceClass *klass = EEKBOARD_CONTEXT_SERVICE_GET_CLASS(context);
    static guint keyboard_id = 0;
    g_autofree gchar *keyboard_type = NULL;
    g_autofree gchar *keyboard_layout = NULL;

    settings_get_layout(context->priv->settings, &keyboard_type, &keyboard_layout);

    if (!keyboard_type) {
        keyboard_type = g_strdup("us");
    }
    if (!keyboard_layout) {
        keyboard_layout = g_strdup("undefined");
    }

    EekKeyboard *keyboard = g_hash_table_lookup(context->priv->keyboard_hash,
                                                GUINT_TO_POINTER(keyboard_id));
// create a keyboard
    if (!keyboard) {
        keyboard = klass->create_keyboard (context, keyboard_layout);
        eek_keyboard_set_modifier_behavior (keyboard,
                                            EEK_MODIFIER_BEHAVIOR_LATCH);

        g_hash_table_insert (context->priv->keyboard_hash,
                             GUINT_TO_POINTER(keyboard_id),
                             keyboard);
        g_object_set_data (G_OBJECT(keyboard),
                           "keyboard-id",
                           GUINT_TO_POINTER(keyboard_id));
        keyboard_id++;
    }
// set as current
    if (context->priv->keyboard)
        disconnect_keyboard_signals (context);

    context->priv->keyboard = keyboard;
    connect_keyboard_signals (context);
    // TODO: this used to save the group, why?
    //group = eek_element_get_group (EEK_ELEMENT(context->priv->keyboard));

    g_object_notify (G_OBJECT(context), "keyboard");
}

static gboolean
settings_handle_layout_changed(GSettings *s,
                               gpointer keys, gint n_keys,
                               gpointer user_data) {
    EekboardContextService *context = user_data;
    settings_update_layout(context);
    return TRUE;
}


static void
eekboard_context_service_constructed (GObject *object)
{
    EekboardContextService *context = EEKBOARD_CONTEXT_SERVICE (object);
    settings_update_layout(context);
}

static void
eekboard_context_service_class_init (EekboardContextServiceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec *pspec;

    g_type_class_add_private (gobject_class,
                              sizeof (EekboardContextServicePrivate));

    klass->create_keyboard = eekboard_context_service_real_create_keyboard;
    klass->show_keyboard = eekboard_context_service_real_show_keyboard;
    klass->hide_keyboard = eekboard_context_service_real_hide_keyboard;

    gobject_class->constructed = eekboard_context_service_constructed;
    gobject_class->set_property = eekboard_context_service_set_property;
    gobject_class->get_property = eekboard_context_service_get_property;
    gobject_class->dispose = eekboard_context_service_dispose;
    gobject_class->finalize = eekboard_context_service_finalize;

    /**
     * EekboardContextService::enabled:
     * @context: an #EekboardContextService
     *
     * Emitted when @context is enabled.
     */
    signals[ENABLED] =
        g_signal_new (I_("enabled"),
                      G_TYPE_FROM_CLASS(gobject_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(EekboardContextServiceClass, enabled),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);

    /**
     * EekboardContextService::disabled:
     * @context: an #EekboardContextService
     *
     * Emitted when @context is enabled.
     */
    signals[DISABLED] =
        g_signal_new (I_("disabled"),
                      G_TYPE_FROM_CLASS(gobject_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(EekboardContextServiceClass, disabled),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);

    /**
     * EekboardContextService::destroyed:
     * @context: an #EekboardContextService
     *
     * Emitted when @context is destroyed.
     */
    signals[DESTROYED] =
        g_signal_new (I_("destroyed"),
                      G_TYPE_FROM_CLASS(gobject_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(EekboardContextServiceClass, destroyed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE,
                      0);

    /**
     * EekboardContextService:object-path:
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
     * EekboardContextService:connection:
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

    /**
     * EekboardContextService:client-name:
     *
     * Name of a client who created this context service.
     */
    pspec = g_param_spec_string ("client-name",
                                 "Client-name",
                                 "Client-name",
                                 NULL,
                                 G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class,
                                     PROP_CLIENT_NAME,
                                     pspec);

    /**
     * EekboardContextService:keyboard:
     *
     * An #EekKeyboard currently active in this context.
     */
    pspec = g_param_spec_object ("keyboard",
                                 "Keyboard",
                                 "Keyboard",
                                 EEK_TYPE_KEYBOARD,
                                 G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class,
                                     PROP_KEYBOARD,
                                     pspec);

    /**
     * EekboardContextService:visible:
     *
     * Flag to indicate if keyboard is visible or not.
     */
    pspec = g_param_spec_boolean ("visible",
                                  "Visible",
                                  "Visible",
                                  FALSE,
                                  G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class,
                                     PROP_VISIBLE,
                                     pspec);

    /**
     * EekboardContextService:fullscreen:
     *
     * Flag to indicate if keyboard is rendered in fullscreen mode.
     */
    pspec = g_param_spec_boolean ("fullscreen",
                                  "Fullscreen",
                                  "Fullscreen",
                                  FALSE,
                                  G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class,
                                     PROP_FULLSCREEN,
                                     pspec);
}

static void
eekboard_context_service_init (EekboardContextService *self)
{
    GError *error;

    self->priv = EEKBOARD_CONTEXT_SERVICE_GET_PRIVATE(self);
    error = NULL;
    self->priv->introspection_data =
        g_dbus_node_info_new_for_xml (introspection_xml, &error);
    if (self->priv->introspection_data == NULL) {
        g_warning ("failed to parse D-Bus XML: %s", error->message);
        g_error_free (error);
        g_assert_not_reached ();
    }

    self->priv->keyboard_hash =
        g_hash_table_new_full (g_direct_hash,
                               g_direct_equal,
                               NULL,
                               (GDestroyNotify)g_object_unref);

    self->priv->settings = g_settings_new ("org.gnome.desktop.input-sources");
    gulong conn_id = g_signal_connect(self->priv->settings, "change-event",
                                      G_CALLBACK(settings_handle_layout_changed),
                                      self);
    if (conn_id == 0) {
        g_warning ("Could not connect to gsettings updates, layout"
                   " changing unavailable");
    }
}

static void
disconnect_keyboard_signals (EekboardContextService *context)
{
    if (g_signal_handler_is_connected (context->priv->keyboard,
                                       context->priv->key_pressed_handler))
        g_signal_handler_disconnect (context->priv->keyboard,
                                     context->priv->key_pressed_handler);
    if (g_signal_handler_is_connected (context->priv->keyboard,
                                       context->priv->key_released_handler))
        g_signal_handler_disconnect (context->priv->keyboard,
                                     context->priv->key_released_handler);

    if (g_signal_handler_is_connected (context->priv->keyboard,
                                       context->priv->key_cancelled_handler)) {
        g_signal_handler_disconnect (context->priv->keyboard,
                                     context->priv->key_cancelled_handler);
    }
}

static void
emit_visibility_changed_signal (EekboardContextService *context,
                                gboolean                visible)
{
    return; // FIXME: update Visible property
    if (context->priv->connection && context->priv->enabled) {
        GError *error = NULL;
        gboolean retval;

        retval = g_dbus_connection_emit_signal (context->priv->connection,
                                                NULL,
                                                context->priv->object_path,
                                                EEKBOARD_CONTEXT_SERVICE_INTERFACE,
                                                "VisibilityChanged",
                                                g_variant_new ("(b)", visible),
                                                &error);
        if (!retval) {
            g_warning ("failed to emit VisibilityChanged signal: %s",
                       error->message);
            g_error_free (error);
            g_assert_not_reached ();
        }
    }
}

static gboolean on_repeat_timeout (EekboardContextService *context);

static gboolean
on_repeat_timeout (EekboardContextService *context)
{
    guint delay = 500; // ms

    // hardcoding; needs to connect to yet another settings path because
    // org.gnome.desktop.input-sources doesn't control repeating
    //g_settings_get (context->priv->settings, "repeat-interval", "u", &delay);

    context->priv->repeat_timeout_id =
        g_timeout_add (delay,
                       (GSourceFunc)on_repeat_timeout,
                       context);

    return FALSE;
}

static gboolean
on_repeat_timeout_init (EekboardContextService *context)
{
    /* FIXME: clear modifiers for further key repeat; better not
       depend on modifier behavior is LATCH */
    eek_keyboard_set_modifiers (context->priv->keyboard, 0);
    
    /* reschedule repeat timeout only when "repeat" option is set */
    /* TODO: org.gnome.desktop.input-sources doesn't have repeat info.
     * In addition, repeat is only useful when the keyboard is not in text
     * input mode */
    /*
    if (g_settings_get_boolean (context->priv->settings, "repeat")) {
        guint delay;

        g_settings_get (context->priv->settings, "repeat-interval", "u", &delay);
        context->priv->repeat_timeout_id =
            g_timeout_add (delay,
                           (GSourceFunc)on_repeat_timeout,
                           context);
    } else */
        context->priv->repeat_timeout_id = 0;

    return FALSE;
}

static void
on_key_activated(EekKeyboard *keyboard,
                 EekKey      *key,
                 EekboardContextService *context,
                 gboolean pressed)
{
    guint keycode = eek_key_get_keycode (key);
    EekSymbol *symbol = eek_key_get_symbol_with_fallback (key, 0, 0);

    guint modifiers = eek_keyboard_get_modifiers (context->priv->keyboard);

    // Insert
    EekboardContext ec = {0};
    Client c = {&ec, 0, {0}};

    emit_key_activated(&ec, keycode, symbol, modifiers, &c, pressed);
}

static void
on_key_pressed (EekKeyboard *keyboard,
                EekKey      *key,
                gpointer     user_data)
{
    EekboardContextService *context = user_data;
    guint delay = 500;

    // org.gnome.desktop.input-sources doesn't have delay info
    //g_settings_get (context->priv->settings, "repeat-delay", "u", &delay);

    if (context->priv->repeat_timeout_id) {
        g_source_remove (context->priv->repeat_timeout_id);
        context->priv->repeat_timeout_id = 0;
    }

    context->priv->repeat_key = key;
    context->priv->repeat_timeout_id =
        g_timeout_add (delay,
                       (GSourceFunc)on_repeat_timeout_init,
                       context);
    on_key_activated(keyboard, key, context, TRUE);
}

static void
on_key_released (EekKeyboard *keyboard,
                 EekKey      *key,
                 gpointer     user_data)
{
    EekboardContextService *context = user_data;

    if (context->priv->repeat_timeout_id > 0) {
        g_source_remove (context->priv->repeat_timeout_id);
        context->priv->repeat_timeout_id = 0;
    }
    on_key_activated(keyboard, key, context, FALSE);
}

static void
on_key_cancelled (EekKeyboard *keyboard,
                 EekKey      *key,
                 gpointer     user_data)
{
    on_key_released(keyboard, key, user_data);
    // TODO: special handling when composing text via text-input? text may not be committed until button released
}

static void
connect_keyboard_signals (EekboardContextService *context)
{
    context->priv->key_pressed_handler =
        g_signal_connect (context->priv->keyboard, "key-pressed",
                          G_CALLBACK(on_key_pressed),
                          context);
    context->priv->key_released_handler =
        g_signal_connect (context->priv->keyboard, "key-released",
                          G_CALLBACK(on_key_released),
                          context);
    context->priv->key_cancelled_handler =
        g_signal_connect (context->priv->keyboard, "key-cancelled",
                          G_CALLBACK(on_key_cancelled),
                          context);
}

/**
 * eekboard_context_service_enable:
 * @context: an #EekboardContextService
 *
 * Enable @context.  This function is called when @context is pushed
 * by eekboard_service_push_context().
 */
void
eekboard_context_service_enable (EekboardContextService *context)
{
    g_return_if_fail (EEKBOARD_IS_CONTEXT_SERVICE(context));
    g_return_if_fail (context->priv->connection);

    if (!context->priv->enabled) {
        context->priv->enabled = TRUE;
        g_signal_emit (context, signals[ENABLED], 0);
    }
}

/**
 * eekboard_context_service_disable:
 * @context: an #EekboardContextService
 *
 * Disable @context.  This function is called when @context is pushed
 * by eekboard_service_pop_context().
 */
void
eekboard_context_service_disable (EekboardContextService *context)
{
    GError *error;

    g_return_if_fail (EEKBOARD_IS_CONTEXT_SERVICE(context));
    g_return_if_fail (context->priv->connection);

    if (context->priv->enabled) {
        gboolean retval;

        context->priv->enabled = FALSE;

        error = NULL;
        retval = g_dbus_connection_emit_signal (context->priv->connection,
                                                NULL,
                                                context->priv->object_path,
                                                EEKBOARD_CONTEXT_SERVICE_INTERFACE,
                                                "Disabled",
                                                NULL,
                                                &error);
        if (!retval) {
            g_warning ("failed to emit Disabled signal: %s",
                       error->message);
            g_error_free (error);
            g_assert_not_reached ();
        }
        g_signal_emit (context, signals[DISABLED], 0);
    }
}

void
eekboard_context_service_show_keyboard (EekboardContextService *context)
{
    g_return_if_fail (EEKBOARD_IS_CONTEXT_SERVICE(context));
    g_return_if_fail (context->priv->connection);

    EEKBOARD_CONTEXT_SERVICE_GET_CLASS(context)->show_keyboard (context);
}

void
eekboard_context_service_hide_keyboard (EekboardContextService *context)
{
    g_return_if_fail (EEKBOARD_IS_CONTEXT_SERVICE(context));
    g_return_if_fail (context->priv->connection);

    EEKBOARD_CONTEXT_SERVICE_GET_CLASS(context)->hide_keyboard (context);
}

/**
 * eekboard_context_service_destroy:
 * @context: an #EekboardContextService
 *
 * Destroy @context.
 */
void
eekboard_context_service_destroy (EekboardContextService *context)
{
    gboolean retval;
    GError *error;

    g_return_if_fail (EEKBOARD_IS_CONTEXT_SERVICE(context));
    g_return_if_fail (context->priv->connection);

    if (context->priv->enabled) {
        eekboard_context_service_disable (context);
    }

    error = NULL;
    retval = g_dbus_connection_emit_signal (context->priv->connection,
                                            NULL,
                                            context->priv->object_path,
                                            EEKBOARD_CONTEXT_SERVICE_INTERFACE,
                                            "Destroyed",
                                            NULL,
                                            &error);
    if (!retval) {
        g_warning ("failed to emit Destroyed signal: %s",
                   error->message);
        g_error_free (error);
        g_assert_not_reached ();
    }
    g_signal_emit (context, signals[DESTROYED], 0);
}

/**
 * eekboard_context_service_get_keyboard:
 * @context: an #EekboardContextService
 *
 * Get keyboard currently active in @context.
 * Returns: (transfer none): an #EekKeyboard
 */
EekKeyboard *
eekboard_context_service_get_keyboard (EekboardContextService *context)
{
    return context->priv->keyboard;
}

/**
 * eekboard_context_service_get_fullscreen:
 * @context: an #EekboardContextService
 *
 * Check if keyboard is rendered in fullscreen mode in @context.
 * Returns: %TRUE or %FALSE
 */
gboolean
eekboard_context_service_get_fullscreen (EekboardContextService *context)
{
    return context->priv->fullscreen;
}

/**
 * eekboard_context_service_get_client_name:
 * @context: an #EekboardContextService
 *
 * Get the name of client which created @context.
 * Returns: (transfer none): a string
 */
const gchar *
eekboard_context_service_get_client_name (EekboardContextService *context)
{
    return context->priv->client_name;
}
