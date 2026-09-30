/*
 * relay_inject.c - picocalc-remote-kbd desktop relay, keystroke injection layer
 *
 * See relay_inject.h. Windows: keys are sent with SendInput as hardware scan
 * codes (layout independent, like the HID usages they come from). Anything
 * else: a stub that reports the feature as unavailable.
 *
 * Author: Thomas Dzubin
 */

#include "relay_inject.h"

#ifdef _WIN32

#include <string.h>
#include <windows.h>

#include "relay_inject_keys.h"

static bool key_down[256]; /* usages we have pressed and not yet released */

bool relay_inject_available(void)
{
    return true;
}

/* Finds the scan code for a usage, or returns false if there is none. */
static bool lookup(uint8_t usage, scan_entry_t *out)
{
    if (usage >= INJECT_FIRST_USAGE && usage <= INJECT_LAST_USAGE)
        *out = inject_table[usage - INJECT_FIRST_USAGE];
    else if (usage >= INJECT_MOD_FIRST && usage <= INJECT_MOD_LAST)
        *out = inject_mod_table[usage - INJECT_MOD_FIRST];
    else
        return false;
    return out->scan != 0;
}

void relay_inject_key(uint8_t usage, bool pressed)
{
    scan_entry_t entry;
    INPUT in;

    if (!lookup(usage, &entry))
        return;

    memset(&in, 0, sizeof(in));
    in.type = INPUT_KEYBOARD;
    in.ki.wScan = entry.scan;
    in.ki.dwFlags = KEYEVENTF_SCANCODE;
    if (entry.ext)
        in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    if (!pressed)
        in.ki.dwFlags |= KEYEVENTF_KEYUP;
    SendInput(1, &in, sizeof(in));
    key_down[usage] = pressed;
}

void relay_inject_release_all(void)
{
    int i;
    for (i = 0; i < 256; i++)
        if (key_down[i])
            relay_inject_key((uint8_t)i, false);
}

/* Polled, not a registered hotkey: works whichever window has the focus and
 * needs no message-loop hookup. Fires once on the transition to "all down". */
bool relay_inject_hotkey_pressed(void)
{
    static bool was_down = false;
    bool down = (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_MENU) & 0x8000) &&
                (GetAsyncKeyState('K') & 0x8000);
    bool fire = down && !was_down;

    was_down = down;
    return fire;
}

#else /* not Windows: injection not supported yet */

bool relay_inject_available(void)
{
    return false;
}

void relay_inject_key(uint8_t usage, bool pressed)
{
    (void)usage;
    (void)pressed;
}

void relay_inject_release_all(void)
{
}

bool relay_inject_hotkey_pressed(void)
{
    return false;
}

#endif
