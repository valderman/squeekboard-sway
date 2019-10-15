#ifndef __LAYOUT_H
#define __LAYOUT_H

#include <inttypes.h>
#include <glib.h>
#include "eek/eek-element.h"
#include "eek/eek-gtk-keyboard.h"
#include "eek/eek-types.h"
#include "src/keyboard.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"

enum layout_type {
    LAYOUT_TYPE_BASE = 0,
    LAYOUT_TYPE_WIDE = 1,
};

struct squeek_button;
struct squeek_row;
struct squeek_view;
struct squeek_layout;

/// Represents the path to the button within a view
struct button_place {
    const struct squeek_row *row;
    const struct squeek_button *button;
};

int32_t squeek_row_get_angle(const struct squeek_row*);

EekBounds squeek_row_get_bounds(const struct squeek_row*);

typedef void (*ButtonCallback) (struct squeek_button *button, gpointer user_data);
void squeek_row_foreach(struct squeek_row*,
                            ButtonCallback   callback,
                            gpointer      user_data);

EekBounds squeek_button_get_bounds(const struct squeek_button*);
const char *squeek_button_get_label(const struct squeek_button*);
const char *squeek_button_get_icon_name(const struct squeek_button*);
const char *squeek_button_get_name(const struct squeek_button*);
const char *squeek_button_get_outline_name(const struct squeek_button*);

struct squeek_key *squeek_button_get_key(const struct squeek_button*);
uint32_t *squeek_button_has_key(const struct squeek_button* button,
                                const struct squeek_key *key);
void squeek_button_print(const struct squeek_button* button);


EekBounds squeek_view_get_bounds(const struct squeek_view*);

typedef void (*RowCallback) (struct squeek_row *row, gpointer user_data);
void squeek_view_foreach(struct squeek_view*,
                            RowCallback   callback,
                            gpointer      user_data);

void
squeek_layout_place_contents(struct squeek_layout*);
struct squeek_view *squeek_layout_get_current_view(struct squeek_layout*);

struct squeek_layout *squeek_load_layout(const char *name, uint32_t type);
const char *squeek_layout_get_keymap(const struct squeek_layout*);
void squeek_layout_free(struct squeek_layout*);

void squeek_layout_release(struct squeek_layout *layout, struct zwp_virtual_keyboard_v1 *virtual_keyboard, uint32_t timestamp, EekGtkKeyboard *ui_keyboard);
void squeek_layout_release_all_only(struct squeek_layout *layout, struct zwp_virtual_keyboard_v1 *virtual_keyboard, uint32_t timestamp);
void squeek_layout_depress(struct squeek_layout *layout, struct zwp_virtual_keyboard_v1 *virtual_keyboard,
                           double x_widget, double y_widget,
                           struct transformation widget_to_layout,
                           uint32_t timestamp, EekGtkKeyboard *ui_keyboard);
void squeek_layout_drag(struct squeek_layout *layout, struct zwp_virtual_keyboard_v1 *virtual_keyboard,
                        double x_widget, double y_widget,
                        struct transformation widget_to_layout,
                        uint32_t timestamp, EekGtkKeyboard *ui_keyboard);
void squeek_layout_draw_all_changed(struct squeek_layout *layout, EekGtkKeyboard *ui_keyboard);
#endif
