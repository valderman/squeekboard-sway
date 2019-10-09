#ifndef __KEYBOARD_H
#define __KEYBOARD_H

#include "inttypes.h"
#include "stdbool.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"

struct squeek_key;

uint32_t squeek_key_is_pressed(struct squeek_key *key);
uint32_t squeek_key_is_locked(struct squeek_key *key);
void squeek_key_set_locked(struct squeek_key *key, uint32_t pressed);
uint32_t squeek_key_equal(struct squeek_key* key, struct squeek_key* key1);

enum key_press {
    KEY_RELEASE = 0,
    KEY_PRESS = 1,
};

void squeek_key_press(struct squeek_key *key, struct zwp_virtual_keyboard_v1*, enum key_press press, uint32_t timestamp);
#endif
