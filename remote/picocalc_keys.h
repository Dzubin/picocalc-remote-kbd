#pragma once

/*
 * picocalc_keys.h - picocalc-remote-kbd
 *
 * Reads the PicoCalc's own keyboard and turns it into USB HID key events
 * (usage ID + pressed/released), the same vocabulary the relay uses in the
 * other direction.
 *
 * The starter's keyboard driver (keyboard.c, by Blair Leduc) hides key
 * releases, so this reads the south bridge's raw key FIFO directly through
 * its sb_read_keyboard() call instead; keyboard.c is not used alongside it
 * (they would compete for the same FIFO).
 *
 * The one key this module keeps for itself is '~' (Shift + backtick), the
 * BOOTSEL shortcut: it is never passed on as an event, see
 * picocalc_keys_bootsel_requested().
 *
 * Author: Thomas Dzubin
 */

#include <stdint.h>
#include <stdbool.h>

/* Brings up the south bridge. Call once at startup. */
void picocalc_keys_init(void);

/* Polls the keyboard FIFO (rate-limited internally, cheap to call every
 * loop) and queues any resulting HID events. */
void picocalc_keys_poll(void);

/* Pops one queued HID event. Returns false if there is none. */
bool picocalc_keys_get(uint8_t *usage, bool *pressed);

/* True once the '~' key has been pressed on the PicoCalc. */
bool picocalc_keys_bootsel_requested(void);
