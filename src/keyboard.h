#ifndef __KEYBOARD_H
#define __KEYBOARD_H

#include "inttypes.h"
#include "stdbool.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"

struct squeek_key;

uint32_t squeek_key_is_pressed(struct squeek_key *key);
uint32_t squeek_key_is_locked(struct squeek_key *key);
#endif
