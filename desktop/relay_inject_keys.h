#pragma once

/*
 * relay_inject_keys.h - picocalc-remote-kbd desktop relay
 *
 * USB HID keyboard usage ID to PC keyboard scan code (set 1) table, used by
 * the Windows keystroke injection. Scan codes are physical key positions, like
 * HID usages, so the PC's own keyboard layout decides what gets typed.
 * `ext` marks the "extended" keys (arrows, Home/End, right Ctrl/Alt...).
 * A scan of 0 means the usage has no PC key here.
 *
 * Author: Thomas Dzubin
 */

#include <stdint.h>

typedef struct
{
    uint8_t scan;
    uint8_t ext;
} scan_entry_t;

/* Usages 0x04 (A) to 0x52 (Up arrow). */
#define INJECT_FIRST_USAGE  0x04
#define INJECT_LAST_USAGE   0x52

static const scan_entry_t inject_table[INJECT_LAST_USAGE - INJECT_FIRST_USAGE + 1] = {
    /* 0x04-0x0D  a b c d e f g h i j */
    {0x1E, 0}, {0x30, 0}, {0x2E, 0}, {0x20, 0}, {0x12, 0}, {0x21, 0}, {0x22, 0}, {0x23, 0}, {0x17, 0}, {0x24, 0},
    /* 0x0E-0x17  k l m n o p q r s t */
    {0x25, 0}, {0x26, 0}, {0x32, 0}, {0x31, 0}, {0x18, 0}, {0x19, 0}, {0x10, 0}, {0x13, 0}, {0x1F, 0}, {0x14, 0},
    /* 0x18-0x1D  u v w x y z */
    {0x16, 0}, {0x2F, 0}, {0x11, 0}, {0x2D, 0}, {0x15, 0}, {0x2C, 0},
    /* 0x1E-0x27  1 2 3 4 5 6 7 8 9 0 */
    {0x02, 0}, {0x03, 0}, {0x04, 0}, {0x05, 0}, {0x06, 0}, {0x07, 0}, {0x08, 0}, {0x09, 0}, {0x0A, 0}, {0x0B, 0},
    /* 0x28-0x2C  Enter Esc Backspace Tab Space */
    {0x1C, 0}, {0x01, 0}, {0x0E, 0}, {0x0F, 0}, {0x39, 0},
    /* 0x2D-0x32  - = [ ] \ non-US-# */
    {0x0C, 0}, {0x0D, 0}, {0x1A, 0}, {0x1B, 0}, {0x2B, 0}, {0x2B, 0},
    /* 0x33-0x38  ; ' ` , . / */
    {0x27, 0}, {0x28, 0}, {0x29, 0}, {0x33, 0}, {0x34, 0}, {0x35, 0},
    /* 0x39  Caps Lock */
    {0x3A, 0},
    /* 0x3A-0x43  F1 to F10 */
    {0x3B, 0}, {0x3C, 0}, {0x3D, 0}, {0x3E, 0}, {0x3F, 0}, {0x40, 0}, {0x41, 0}, {0x42, 0}, {0x43, 0}, {0x44, 0},
    /* 0x44-0x45  F11 F12 */
    {0x57, 0}, {0x58, 0},
    /* 0x46-0x48  Print Screen, Scroll Lock, Pause (not supported except Scroll Lock) */
    {0, 0}, {0x46, 0}, {0, 0},
    /* 0x49-0x4E  Insert Home PageUp Delete End PageDown */
    {0x52, 1}, {0x47, 1}, {0x49, 1}, {0x53, 1}, {0x4F, 1}, {0x51, 1},
    /* 0x4F-0x52  Right Left Down Up */
    {0x4D, 1}, {0x4B, 1}, {0x50, 1}, {0x48, 1},
};

/* Modifiers, usages 0xE0 to 0xE7, in order: LCtrl LShift LAlt LGui RCtrl RShift RAlt RGui. */
#define INJECT_MOD_FIRST    0xE0
#define INJECT_MOD_LAST     0xE7

static const scan_entry_t inject_mod_table[INJECT_MOD_LAST - INJECT_MOD_FIRST + 1] = {
    {0x1D, 0}, {0x2A, 0}, {0x38, 0}, {0x5B, 1}, {0x1D, 1}, {0x36, 0}, {0x38, 1}, {0x5C, 1},
};
