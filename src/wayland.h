#ifndef WAYLAND_H
#define WAYLAND_H

#include <gmodule.h>

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"
#include "input-method-unstable-v2-client-protocol.h"

#include "outputs.h"

struct squeek_wayland {
    struct zwlr_layer_shell_v1 *layer_shell;
    struct zwp_virtual_keyboard_manager_v1 *virtual_keyboard_manager;
    struct zwp_input_method_manager_v2 *input_method_manager;
    struct squeek_outputs *outputs;
    struct wl_seat *seat;
};


extern struct squeek_wayland *squeek_wayland;


static inline void squeek_wayland_init(struct squeek_wayland *wayland) {
    wayland->outputs = squeek_outputs_new();
}

static inline void squeek_wayland_set_global(struct squeek_wayland *wayland) {
    squeek_wayland = wayland;
}

static inline void squeek_wayland_deinit(struct squeek_wayland *wayland) {
    squeek_outputs_free(wayland->outputs);
}

#endif // WAYLAND_H
