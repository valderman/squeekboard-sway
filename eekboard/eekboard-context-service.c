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

#include "config.h"

#include <fcntl.h>
#include <stdio.h>
#define _XOPEN_SOURCE 500
#include <string.h>
#include <sys/mman.h>
#include <sys/random.h> // TODO: this is Linux-specific
#include <xkbcommon/xkbcommon.h>

#include <gio/gio.h>

#include "eekboard/key-emitter.h"
#include "wayland.h"

#include "eek/eek-xml-layout.h"
#include "src/server-context-service.h"

#include "eekboard/eekboard-context-service.h"

enum {
    PROP_0, // Magic: without this, keyboard is not useable in g_object_notify
    PROP_KEYBOARD,
    PROP_VISIBLE,
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
    gboolean enabled;
    gboolean visible;

    LevelKeyboard *keyboard; // currently used keyboard
    GHashTable *keyboard_hash; // a table of available keyboards, per layout

    char *overlay;

    GSettings *settings;
    uint32_t hint;
    uint32_t purpose;
};

G_DEFINE_TYPE_WITH_PRIVATE (EekboardContextService, eekboard_context_service, G_TYPE_OBJECT);

static LevelKeyboard *
eekboard_context_service_real_create_keyboard (EekboardContextService *self,
                                               const gchar            *keyboard_type,
                                               enum squeek_arrangement_kind t)
{
    LevelKeyboard *keyboard = eek_xml_layout_real_create_keyboard(keyboard_type, self, t);
    if (!keyboard) {
        g_error("Failed to create a keyboard");
    }

    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!context) {
        g_error("No context created");
    }

    const gchar *keymap_str = squeek_layout_get_keymap(keyboard->layout);

    struct xkb_keymap *keymap = xkb_keymap_new_from_string(context, keymap_str,
        XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);

    if (!keymap)
        g_error("Bad keymap:\n%s", keymap_str);

    xkb_context_unref(context);
    keyboard->keymap = keymap;

    keymap_str = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
    keyboard->keymap_len = strlen(keymap_str) + 1;

    g_autofree char *path = strdup("/eek_keymap-XXXXXX");
    char *r = &path[strlen(path) - 6];
    getrandom(r, 6, GRND_NONBLOCK);
    for (unsigned i = 0; i < 6; i++) {
        r[i] = (r[i] & 0b1111111) | 0b1000000; // A-z
        r[i] = r[i] > 'z' ? '?' : r[i]; // The randomizer doesn't need to be good...
    }
    int keymap_fd = shm_open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (keymap_fd < 0) {
        g_error("Failed to set up keymap fd");
    }
    keyboard->keymap_fd = keymap_fd;
    shm_unlink(path);
    if (ftruncate(keymap_fd, (off_t)keyboard->keymap_len)) {
        g_error("Failed to increase keymap fd size");
    }
    char *ptr = mmap(NULL, keyboard->keymap_len, PROT_WRITE, MAP_SHARED,
        keymap_fd, 0);
    if ((void*)ptr == (void*)-1) {
        g_error("Failed to set up mmap");
    }
    strncpy(ptr, keymap_str, keyboard->keymap_len);
    munmap(ptr, keyboard->keymap_len);
    return keyboard;
}

static void
eekboard_context_service_real_show_keyboard (EekboardContextService *self)
{
    self->priv->visible = TRUE;
}

static void
eekboard_context_service_real_hide_keyboard (EekboardContextService *self)
{
    self->priv->visible = FALSE;
}

static void
eekboard_context_service_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
    EekboardContextService *context = EEKBOARD_CONTEXT_SERVICE(object);

    switch (prop_id) {
    case PROP_KEYBOARD:
        if (context->priv->keyboard)
            g_object_unref (context->priv->keyboard);
        context->priv->keyboard = g_value_get_object (value);
        break;
    case PROP_VISIBLE:
        context->priv->visible = g_value_get_boolean (value);
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
    case PROP_KEYBOARD:
        g_value_set_object (value, context->priv->keyboard);
        break;
    case PROP_VISIBLE:
        g_value_set_boolean (value, context->priv->visible);
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
        keyboard_layout = g_strdup("number");
        break;
    default:
        ;
    }

    // generic part follows
    LevelKeyboard *keyboard = eekboard_context_service_real_create_keyboard(context, keyboard_layout, t);
    // set as current
    LevelKeyboard *previous_keyboard = context->priv->keyboard;
    context->priv->keyboard = keyboard;

    g_object_notify (G_OBJECT(context), "keyboard");

    // replacing the keyboard above will cause the previous keyboard to get destroyed from the UI side (eek_gtk_keyboard_dispose)
    if (previous_keyboard) {
        level_keyboard_free(previous_keyboard);
    }
}

static void update_layout_and_type(EekboardContextService *context) {
    eekboard_context_service_update_layout(context, server_context_service_get_layout_type(context));
}

static gboolean
settings_handle_layout_changed(GSettings *s,
                               gpointer keys, gint n_keys,
                               gpointer user_data) {
    (void)s;
    (void)keys;
    (void)n_keys;
    EekboardContextService *context = user_data;
    free(context->priv->overlay);
    context->priv->overlay = NULL;
    update_layout_and_type(context);
    return TRUE;
}

static void
eekboard_context_service_constructed (GObject *object)
{
    EekboardContextService *context = EEKBOARD_CONTEXT_SERVICE (object);
    context->virtual_keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(
                squeek_wayland->virtual_keyboard_manager,
                squeek_wayland->seat);
    if (!context->virtual_keyboard) {
        g_error("Programmer error: Failed to receive a virtual keyboard instance");
    }
    update_layout_and_type(context);
}

static void
eekboard_context_service_class_init (EekboardContextServiceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec *pspec;

    klass->show_keyboard = eekboard_context_service_real_show_keyboard;
    klass->hide_keyboard = eekboard_context_service_real_hide_keyboard;

    gobject_class->constructed = eekboard_context_service_constructed;
    gobject_class->set_property = eekboard_context_service_set_property;
    gobject_class->get_property = eekboard_context_service_get_property;
    gobject_class->dispose = eekboard_context_service_dispose;

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
     * EekboardContextService:keyboard:
     *
     * An #EekKeyboard currently active in this context.
     */
    pspec = g_param_spec_pointer("keyboard",
                                 "Keyboard",
                                 "Keyboard",
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
    g_return_if_fail (EEKBOARD_IS_CONTEXT_SERVICE(context));

    if (context->priv->enabled) {
        context->priv->enabled = FALSE;
        g_signal_emit (context, signals[DISABLED], 0);
    }
}

void
eekboard_context_service_show_keyboard (EekboardContextService *context)
{
    g_return_if_fail (EEKBOARD_IS_CONTEXT_SERVICE(context));

    if (!context->priv->visible) {
        EEKBOARD_CONTEXT_SERVICE_GET_CLASS(context)->show_keyboard (context);
    }
}

void
eekboard_context_service_hide_keyboard (EekboardContextService *context)
{
    g_return_if_fail (EEKBOARD_IS_CONTEXT_SERVICE(context));

    if (context->priv->visible) {
        EEKBOARD_CONTEXT_SERVICE_GET_CLASS(context)->hide_keyboard (context);
    }
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

    if (context->priv->enabled) {
        eekboard_context_service_disable (context);
    }
    free(context->priv->overlay);
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

void eekboard_context_service_set_keymap(EekboardContextService *context,
                                         const LevelKeyboard *keyboard)
{
    zwp_virtual_keyboard_v1_keymap(context->virtual_keyboard,
        WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
        keyboard->keymap_fd, keyboard->keymap_len);
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
    context->priv->overlay = strdup(name);
    update_layout_and_type(context);
}
