#ifndef __KEYBOARD_H
#define __KEYBOARD_H

#include "stdbool.h"
#include "inttypes.h"

struct squeek_key;

uint32_t squeek_key_is_pressed(struct squeek_key *key);
void squeek_key_set_pressed(struct squeek_key *key, uint32_t pressed);
uint32_t squeek_key_is_locked(struct squeek_key *key);
void squeek_key_set_locked(struct squeek_key *key, uint32_t pressed);
uint32_t squeek_key_get_keycode(struct squeek_key *key);
uint32_t squeek_key_equal(struct squeek_key* key, struct squeek_key* key1);
#endif
