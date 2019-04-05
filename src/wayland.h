#ifndef WAYLAND_H
#define WAYLAND_H

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <gmodule.h>


struct squeak_wayland {
    struct zwlr_layer_shell_v1 *layer_shell;
    GPtrArray *outputs; // *wl_output
    struct wl_seat *seat;
};


extern struct squeak_wayland *squeak_wayland;


static inline void squeak_wayland_init(struct squeak_wayland *wayland) {
    wayland->outputs = g_ptr_array_new();
}

static inline void squeak_wayland_set_global(struct squeak_wayland *wayland) {
    squeak_wayland = wayland;
}

static inline void squeak_wayland_deinit(struct squeak_wayland *wayland) {
    g_ptr_array_free(wayland->outputs, TRUE);
}

#endif // WAYLAND_H
