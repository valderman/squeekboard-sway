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

#include "config.h"

#include <stdio.h>

#include <gio/gio.h>

#include "wayland.h"

#include "eek/eek-keyboard.h"
#include "src/server-context-service.h"

#include "eekboard/eekboard-context-service.h"

enum {
    PROP_0, // Magic: without this, keyboard is not useable in g_object_notify
    PROP_KEYBOARD,
    PROP_LAST
};

enum {
    DESTROYED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

#define EEKBOARD_CONTEXT_SERVICE_GET_PRIVATE(obj)                       \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EEKBOARD_TYPE_CONTEXT_SERVICE, EekboardContextServicePrivate))

struct _EekboardContextServicePrivate {
    LevelKeyboard *keyboard; // currently used keyboard
    GHashTable *keyboard_hash; // a table of available keyboards, per layout

    char *overlay;

    GSettings *settings; // Owned reference
    uint32_t hint;
    uint32_t purpose;

    // Maybe TODO: it's used only for fetching layout type.
    // Maybe let UI push the type to this structure?
    ServerContextService *ui; // unowned reference
    /// Needed for keymap changes after keyboard updates
    struct submission *submission; // unowned
};

G_DEFINE_TYPE_WITH_PRIVATE (EekboardContextService, eekboard_context_service, G_TYPE_OBJECT);

static void
eekboard_context_service_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
    (void)value;
    switch (prop_id) {
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
    case PROP_KEYBOARD:
        g_value_set_object (value, context->priv->keyboard);
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

    G_OBJECT_CLASS (eekboard_context_service_parent_class)->
        dispose (object);
}

static void
settings_get_layout(GSettings *settings, char **type, char **layout)
{
    GVariant *inputs = g_settings_get_value(settings, "sources");
    // current layout is always first
    g_variant_get_child(inputs, 0, "(ss)", type, layout);
    g_variant_unref(inputs);
}

void
eekboard_context_service_update_layout(EekboardContextService *context, enum squeek_arrangement_kind t)
{
    g_autofree gchar *keyboard_layout = NULL;
    if (context->priv->overlay) {
        keyboard_layout = g_strdup(context->priv->overlay);
    } else {
        g_autofree gchar *keyboard_type = NULL;
        settings_get_layout(context->priv->settings,
                            &keyboard_type, &keyboard_layout);
    }

    if (!keyboard_layout) {
        keyboard_layout = g_strdup("us");
    }

    EekboardContextServicePrivate *priv = EEKBOARD_CONTEXT_SERVICE_GET_PRIVATE(context);

    switch (priv->purpose) {
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NUMBER:
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PHONE:
        g_free(keyboard_layout);
        keyboard_layout = g_strdup("number");
        break;
    case ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_TERMINAL:
        g_free(keyboard_layout);
        keyboard_layout = g_strdup("terminal");
        break;
    default:
        ;
    }

    // generic part follows
    LevelKeyboard *keyboard = level_keyboard_new(keyboard_layout, t);
    // set as current
    LevelKeyboard *previous_keyboard = context->priv->keyboard;
    context->priv->keyboard = keyboard;
    // Update the keymap if necessary.
    // TODO: Update submission on change event
    if (context->priv->submission) {
        submission_set_keyboard(context->priv->submission, keyboard);
    }

    g_object_notify (G_OBJECT(context), "keyboard");

    // replacing the keyboard above will cause the previous keyboard to get destroyed from the UI side (eek_gtk_keyboard_dispose)
    if (previous_keyboard) {
        level_keyboard_free(previous_keyboard);
    }
}

static void update_layout_and_type(EekboardContextService *context) {
    EekboardContextServicePrivate *priv = EEKBOARD_CONTEXT_SERVICE_GET_PRIVATE(context);
    enum squeek_arrangement_kind layout_kind = ARRANGEMENT_KIND_BASE;
    if (priv->ui) {
        layout_kind = server_context_service_get_layout_type(priv->ui);
    }
    eekboard_context_service_update_layout(context, layout_kind);
}

static gboolean
settings_handle_layout_changed(GSettings *s,
                               gpointer keys, gint n_keys,
                               gpointer user_data) {
    (void)s;
    (void)keys;
    (void)n_keys;
    EekboardContextService *context = user_data;
    g_free(context->priv->overlay);
    context->priv->overlay = NULL;
    update_layout_and_type(context);
    return TRUE;
}

static void
eekboard_context_service_constructed (GObject *object)
{
    EekboardContextService *context = EEKBOARD_CONTEXT_SERVICE (object);
    update_layout_and_type(context);
}

static void
eekboard_context_service_class_init (EekboardContextServiceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec *pspec;

    gobject_class->constructed = eekboard_context_service_constructed;
    gobject_class->set_property = eekboard_context_service_set_property;
    gobject_class->get_property = eekboard_context_service_get_property;
    gobject_class->dispose = eekboard_context_service_dispose;
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
     * EekboardContextService:keyboard:
     *
     * An #EekKeyboard currently active in this context.
     */
    pspec = g_param_spec_pointer("keyboard",
                                 "Keyboard",
                                 "Keyboard",
                                 G_PARAM_READABLE);
    g_object_class_install_property (gobject_class,
                                     PROP_KEYBOARD,
                                     pspec);
}

static void
eekboard_context_service_init (EekboardContextService *self)
{
    self->priv = EEKBOARD_CONTEXT_SERVICE_GET_PRIVATE(self);

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

    self->priv->overlay = NULL;
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
    g_return_if_fail (EEKBOARD_IS_CONTEXT_SERVICE(context));

    g_free(context->priv->overlay);
    g_signal_emit (context, signals[DESTROYED], 0);
}

/**
 * eekboard_context_service_get_keyboard:
 * @context: an #EekboardContextService
 *
 * Get keyboard currently active in @context.
 * Returns: (transfer none): an #EekKeyboard
 */
LevelKeyboard *
eekboard_context_service_get_keyboard (EekboardContextService *context)
{
    return context->priv->keyboard;
}

void eekboard_context_service_set_hint_purpose(EekboardContextService *context,
                                               uint32_t hint, uint32_t purpose)
{
    EekboardContextServicePrivate *priv = EEKBOARD_CONTEXT_SERVICE_GET_PRIVATE(context);

    if (priv->hint != hint || priv->purpose != purpose) {
        priv->hint = hint;
        priv->purpose = purpose;
        update_layout_and_type(context);
    }
}

void
eekboard_context_service_set_overlay(EekboardContextService *context, const char* name) {
    context->priv->overlay = g_strdup(name);
    update_layout_and_type(context);
}

const char*
eekboard_context_service_get_overlay(EekboardContextService *context) {
    return context->priv->overlay;
}

EekboardContextService *eekboard_context_service_new(void)
{
    return g_object_new (EEKBOARD_TYPE_CONTEXT_SERVICE, NULL);
}

void eekboard_context_service_set_submission(EekboardContextService *context, struct submission *submission) {
    context->priv->submission = submission;
    if (context->priv->submission) {
        submission_set_keyboard(context->priv->submission, context->priv->keyboard);
    }
}

void eekboard_context_service_set_ui(EekboardContextService *context, ServerContextService *ui) {
    context->priv->ui = ui;
}
