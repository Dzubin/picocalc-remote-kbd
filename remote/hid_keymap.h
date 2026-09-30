#pragma once

/*
 * hid_keymap.h - picocalc-remote-kbd
 *
 * Turns USB HID keyboard usage IDs (what the relay sends, see
 * remote_protocol.h) into ASCII for a US keyboard layout. Pure portable C,
 * no hardware dependencies.
 *
 * Author: Thomas Dzubin
 */

#include <stdint.h>
#include <stdbool.h>

/* USB HID Usage Tables, keyboard page (0x07): the non-printing keys the
 * programs here care about. */
#define HID_KEY_CAPS_LOCK   0x39
#define HID_KEY_DELETE      0x4C
#define HID_KEY_RIGHT       0x4F
#define HID_KEY_LEFT        0x50
#define HID_KEY_DOWN        0x51
#define HID_KEY_UP          0x52
#define HID_KEY_HOME        0x4A
#define HID_KEY_END         0x4D
#define HID_KEY_ENTER       0x28
#define HID_KEY_ESCAPE      0x29
#define HID_KEY_BACKSPACE   0x2A
#define HID_KEY_TAB         0x2B
#define HID_KEY_F1          0x3A   /* F1..F12 run 0x3A..0x45 */
#define HID_KEY_INSERT      0x49
#define HID_KEY_PAGE_UP     0x4B
#define HID_KEY_PAGE_DOWN   0x4E
#define HID_KEY_LCTRL       0xE0
#define HID_KEY_LSHIFT      0xE1
#define HID_KEY_LALT        0xE2
#define HID_KEY_RSHIFT      0xE5

/* Returns the ASCII code for a key, or 0 if it has none (arrows, function
 * keys, modifiers...). Enter gives '\n', Backspace '\b', Tab '\t'. `caps`
 * (Caps Lock) flips the case of letters only. */
char hid_to_ascii(uint8_t usage, bool shift, bool caps);

/* The reverse: finds the usage ID that types ASCII character `c` and whether
 * Shift must be held for it. Returns false if the character has no key. */
bool hid_from_ascii(char c, uint8_t *usage, bool *shift);
