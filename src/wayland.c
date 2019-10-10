#include "wayland.h"

struct squeek_wayland *squeek_wayland = NULL;

// The following functions only exist
// to create linkable symbols out of inline functions,
// because those are not directly callable from Rust

void
eek_virtual_keyboard_v1_key(struct zwp_virtual_keyboard_v1 *zwp_virtual_keyboard_v1, uint32_t time, uint32_t key, uint32_t state) {
    zwp_virtual_keyboard_v1_key(zwp_virtual_keyboard_v1, time, key, state);
}

int squeek_output_add_listener(struct wl_output *wl_output,
                                const struct wl_output_listener *listener, void *data) {
    return wl_output_add_listener(wl_output, listener, data);
}
