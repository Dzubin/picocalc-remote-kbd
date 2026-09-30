#pragma once

/*
 * relay_inject.h - picocalc-remote-kbd desktop relay, keystroke injection layer
 *
 * Types keys into the PC on behalf of the PicoCalc's own keyboard. This is
 * the only OS-specific part of that feature, so relay.c stays portable ISO C
 * and sees just these functions. Implemented in relay_inject.c for Windows
 * (SendInput); on other systems (Linux for now) it is a stub that reports the
 * feature as unavailable, and everything else in the relay, including sending
 * the PC's keyboard and mouse to the PicoCalc, works as before.
 *
 * Author: Thomas Dzubin
 */

#include <stdint.h>
#include <stdbool.h>

/* True if this build can inject keystrokes. */
bool relay_inject_available(void);

/* Presses or releases the key with this USB HID usage ID on the PC. */
void relay_inject_key(uint8_t usage, bool pressed);

/* Releases every key this module still holds down (call when turning
 * injection off, so no key is left stuck). */
void relay_inject_release_all(void);

/* True exactly once each time the on/off hotkey (Ctrl+Alt+K) is pressed,
 * whichever window has the focus. */
bool relay_inject_hotkey_pressed(void);
