/*
 * remote_test.c - picocalc-remote-kbd, the smallest useful example
 *
 * A complete PicoCalc program that uses the remote library and nothing else:
 * no display, no other drivers. It echoes every event the desktop relay sends
 * it straight back as text, so you can see on the PC that the link works:
 *
 *     [PicoCalc] KEY   usage=0x04 down
 *     [PicoCalc] MOUSE dx=3 dy=-1
 *     [PicoCalc] MOUSE button=0 down
 *     [PicoCalc] heartbeat 12
 *
 * Read this file top to bottom to see how to use the library in your own
 * program. The steps marked "library" are the ones you copy. The "optional"
 * block is a convenience for development, not part of the library proper.
 *
 * Build it with the rest of the project (see README.md); output
 * picocalc-remote-kbd-example-<chip>.uf2.
 *
 * Author: Thomas Dzubin
 */

#include "pico/stdlib.h"

#include "remote_input.h" /* library */

/* optional: the PicoCalc's own keyboard, here only for the BOOTSEL shortcut */
#include "pico/bootrom.h"
#include "picocalc_keys.h"
#include "remote_test_config.h"


int main(void)
{
    uint32_t last_heartbeat = to_ms_since_boot(get_absolute_time());
    uint32_t heartbeat_count = 0;

    remote_input_init(); /* library: once, at startup */

    picocalc_keys_init(); /* optional */

    while (1)
    {
        /* optional: pressing '~' (Shift + backtick) on the PicoCalc reboots
         * it into BOOTSEL mode, so you can flash a new build without
         * touching the BOOTSEL button. */
        picocalc_keys_poll();
        if (picocalc_keys_bootsel_requested())
            reset_usb_boot(0, 0); /* does not return */

        remote_input_task(); /* library: every pass of the main loop */

        /* library: read every event that has arrived since last time. */
        remote_event_t ev;
        while (remote_input_get_event(&ev))
        {
            switch (ev.type)
            {
            case REMOTE_EVENT_KEY:
                /* ev.key.code is a USB HID usage ID (0x04 = 'A'), see hid_keymap.h */
                remote_log("KEY   usage=0x%02X %s", ev.key.code, ev.key.pressed ? "down" : "up");
                break;
            case REMOTE_EVENT_MOUSE_MOVE:
                /* relative movement since the last event; your program keeps the position */
                remote_log("MOUSE dx=%d dy=%d", ev.mouse_move.dx, ev.mouse_move.dy);
                break;
            case REMOTE_EVENT_MOUSE_BUTTON:
                remote_log("MOUSE button=%u %s", ev.mouse_button.button, ev.mouse_button.pressed ? "down" : "up");
                break;
            case REMOTE_EVENT_REFRESH:
                /* the desktop asks for your state to be resent; this test has none */
                break;
            }
        }

        /* library: remote_log() sends text to the relay's console. */
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_heartbeat >= HEARTBEAT_MS)
        {
            last_heartbeat = now;
            remote_log("heartbeat %lu", (unsigned long)heartbeat_count++);
        }
    }
}
