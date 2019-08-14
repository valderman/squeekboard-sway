#ifndef __LAYOUT_H
#define __LAYOUT_H

#include "inttypes.h"

#include "eek/eek-container.h"
#include "src/keyboard.h"

struct squeek_button;

/*
struct squeek_buttons;

typedef void (*ButtonCallback) (struct squeek_button *button, gpointer user_data);

struct squeek_buttons *squeek_buttons_new();
void squeek_buttons_free(struct squeek_buttons*);
void squeek_buttons_foreach(const struct squeek_buttons*,
                            ButtonCallback   callback,
                            gpointer      user_data);
struct squeek_button *squeek_buttons_find_by_position(
    const struct squeek_buttons *buttons,
    double x, double y,
    double origin_x, double origin_y,
    double angle);
void squeek_buttons_add(struct squeek_buttons*, const struct squeek_button* button);
void squeek_buttons_remove_key(struct squeek_buttons*, const struct squeek_key* key);
*/
struct squeek_button *squeek_button_new(uint32_t keycode, uint32_t oref);
struct squeek_button *squeek_button_new_with_state(const struct squeek_button* source);
uint32_t squeek_button_get_oref(const struct squeek_button*);
EekBounds squeek_button_get_bounds(const struct squeek_button*);
void squeek_button_set_bounds(struct squeek_button* button, EekBounds bounds);

struct squeek_symbol *squeek_button_get_symbol (
    const struct squeek_button *button);
struct squeek_key *squeek_button_get_key(struct squeek_button*);
uint32_t *squeek_button_has_key(const struct squeek_button* button,
                                const struct squeek_key *key);
#endif
