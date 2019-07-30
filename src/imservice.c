#include "imservice.h"

#include <glib.h>

#include "eekboard/eekboard-context-service.h"


static const struct zwp_input_method_v2_listener input_method_listener = {
    .activate = imservice_handle_input_method_activate,
    .deactivate = imservice_handle_input_method_deactivate,
    .surrounding_text = imservice_handle_surrounding_text,
    .text_change_cause = imservice_handle_text_change_cause,
    .content_type = imservice_handle_content_type,
    .done = imservice_handle_commit_state,
    .unavailable = imservice_handle_unavailable,
};

struct imservice* get_imservice(EekboardContextService *context,
                                struct zwp_input_method_manager_v2 *manager,
                                struct wl_seat *seat) {
    struct zwp_input_method_v2 *im = zwp_input_method_manager_v2_get_input_method(manager, seat);
    struct imservice *imservice = imservice_new(im, context);

    /* Add a listener, passing the imservice instance to make it available to
       callbacks. */
    zwp_input_method_v2_add_listener(im, &input_method_listener, imservice);

    return imservice;
}

void imservice_make_visible(EekboardContextService *context) {
    eekboard_context_service_show_keyboard (context);
}

void imservice_try_hide(EekboardContextService *context) {
    eekboard_context_service_hide_keyboard (context);
}

/// Declared explicitly because _destroy is inline,
/// making it unavailable in Rust
void imservice_destroy_im(struct zwp_input_method_v2 *im) {
    zwp_input_method_v2_destroy(im);
}
