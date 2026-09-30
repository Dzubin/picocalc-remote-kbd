/*
 * hid_keymap.c - picocalc-remote-kbd, HID usage ID to ASCII (US layout)
 *
 * Author: Thomas Dzubin
 */

#include "hid_keymap.h"
#include "remote_config.h"


/* One entry per usage ID from HID_MAP_FIRST_USAGE to HID_MAP_LAST_USAGE. 0 = no character. */
static const char unshifted[HID_MAP_LAST_USAGE - HID_MAP_FIRST_USAGE + 1] = {
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    '\n', 0, '\b', '\t', ' ',
    '-', '=', '[', ']', '\\', 0,
    ';', '\'', '`', ',', '.', '/',
};

static const char shifted[HID_MAP_LAST_USAGE - HID_MAP_FIRST_USAGE + 1] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
    '\n', 0, '\b', '\t', ' ',
    '_', '+', '{', '}', '|', 0,
    ':', '"', '~', '<', '>', '?',
};

char hid_to_ascii(uint8_t usage, bool shift, bool caps)
{
    char c;
    bool is_letter;

    if (usage < HID_MAP_FIRST_USAGE || usage > HID_MAP_LAST_USAGE)
        return 0;

    c = unshifted[usage - HID_MAP_FIRST_USAGE];
    is_letter = (c >= 'a' && c <= 'z');

    /* Caps Lock only affects letters; Shift affects everything. */
    if (is_letter ? (shift != caps) : shift)
        c = shifted[usage - HID_MAP_FIRST_USAGE];
    return c;
}

bool hid_from_ascii(char c, uint8_t *usage, bool *shift)
{
    int i;

    if (c == 0)
        return false;
    for (i = 0; i <= HID_MAP_LAST_USAGE - HID_MAP_FIRST_USAGE; i++)
    {
        if (unshifted[i] == c)
        {
            *usage = (uint8_t)(i + HID_MAP_FIRST_USAGE);
            *shift = false;
            return true;
        }
        if (shifted[i] == c)
        {
            *usage = (uint8_t)(i + HID_MAP_FIRST_USAGE);
            *shift = true;
            return true;
        }
    }
    return false;
}
