#include "imservice.h"

#include <glib.h>

static const struct zwp_input_method_v2_listener input_method_listener = {
    .activate = imservice_handle_input_method_activate,
    .deactivate = imservice_handle_input_method_deactivate,
    .surrounding_text = imservice_handle_surrounding_text,
    .text_change_cause = imservice_handle_text_change_cause,
    .content_type = imservice_handle_content_type,
    .done = imservice_handle_commit_state,
    .unavailable = imservice_handle_unavailable,
};

struct imservice* get_imservice(struct zwp_input_method_manager_v2 *manager,
                                struct wl_seat *seat,
                                EekboardContextService *state) {
    struct zwp_input_method_v2 *im = zwp_input_method_manager_v2_get_input_method(manager, seat);
    struct imservice *imservice = imservice_new(im, state);

    /* Add a listener, passing the imservice instance to make it available to
       callbacks. */
    zwp_input_method_v2_add_listener(im, &input_method_listener, imservice);

    return imservice;
}

/// Declared explicitly because _destroy is inline,
/// making it unavailable in Rust
void imservice_destroy_im(struct zwp_input_method_v2 *im) {
    zwp_input_method_v2_destroy(im);
}
