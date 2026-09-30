#pragma once

/*
 * remote_input.h - picocalc-remote-kbd, the PicoCalc-side library
 *
 * Lets a PicoCalc program receive keyboard and mouse events from a desktop
 * computer (see desktop/relay.c) over USB, and send text and its own data
 * back. Use it to
 *   - add mouse control to a PicoCalc program, or
 *   - type on a proper PC keyboard instead of the PicoCalc's own keys.
 *
 * It needs no other PicoCalc code: nothing from picocalc-text-starter, no
 * display, no I2C. See INTEGRATION.md for the five-minute setup.
 *
 * -- How it fits into your program -----------------------------------------
 *
 *   #include "remote_input.h"
 *
 *   int main(void)
 *   {
 *       // ... your own setup ...
 *       remote_input_init();                 // once, at startup
 *
 *       while (1) {
 *           remote_input_task();             // EVERY pass of your main loop
 *
 *           remote_event_t ev;
 *           while (remote_input_get_event(&ev)) {
 *               switch (ev.type) {
 *               case REMOTE_EVENT_KEY:          // ev.key.code, ev.key.pressed
 *               case REMOTE_EVENT_MOUSE_MOVE:   // ev.mouse_move.dx, .dy
 *               case REMOTE_EVENT_MOUSE_BUTTON: // ev.mouse_button.button, .pressed
 *               case REMOTE_EVENT_REFRESH:      // the desktop wants your state resent
 *               }
 *           }
 *
 *           // ... the rest of your main loop ...
 *       }
 *   }
 *
 * Things to know:
 *
 *   * remote_input_task() runs the USB stack, so call it often (at least every
 *     few milliseconds). A long blocking call elsewhere in your loop, such as
 *     a slow screen redraw, delays USB and makes input feel laggy.
 *   * Events queue up (room for EVENT_QUEUE_SIZE in remote_input.c). If you do
 *     not read them fast enough the newest are dropped. Mouse movement arrives
 *     in many small pieces, so add the dx/dy values up and act once per loop.
 *   * Everything is non-blocking. If no desktop program is connected, nothing
 *     happens and nothing waits.
 *   * The library takes over the Pico's USB port. Do not also use the SDK's
 *     stdio over USB or another TinyUSB class in the same program.
 *
 * -- Key codes -------------------------------------------------------------
 *
 * ev.key.code is a USB HID keyboard usage ID (USB HID Usage Tables, page
 * 0x07), not ASCII and not a PC virtual-key code: 0x04 is 'A', 0x28 is Enter,
 * 0xE1 is Left Shift. Shift is reported as its own key. hid_keymap.h turns
 * codes into ASCII for a US keyboard (hid_to_ascii) and has named constants
 * for the common non-printing keys.
 *
 * -- Mouse -----------------------------------------------------------------
 *
 * Mouse movement is relative (dx right, dy down), like a real mouse. Keep
 * your own x, y and clamp them to your screen. Buttons use the
 * REMOTE_PROTO_MOUSE_LEFT / _RIGHT / _MIDDLE ids from remote_protocol.h.
 *
 * Author: Thomas Dzubin
 */

#include <stdint.h>
#include <stdbool.h>

#include "remote_protocol.h" /* for the REMOTE_PROTO_MOUSE_* button ids and USB HID packet constants */

typedef enum
{
    REMOTE_EVENT_KEY,          /* a key went down or up */
    REMOTE_EVENT_MOUSE_MOVE,   /* the mouse moved (relative) */
    REMOTE_EVENT_MOUSE_BUTTON, /* a mouse button went down or up */
    REMOTE_EVENT_REFRESH,      /* the desktop asks your program to resend its state (no fields) */
} remote_event_type_t;

typedef struct
{
    remote_event_type_t type;
    union
    {
        struct { uint8_t code; bool pressed; } key;             /* REMOTE_EVENT_KEY: code is a USB HID usage ID */
        struct { int16_t dx, dy; } mouse_move;                  /* REMOTE_EVENT_MOUSE_MOVE: movement since the last event */
        struct { uint8_t button; bool pressed; } mouse_button;  /* REMOTE_EVENT_MOUSE_BUTTON: REMOTE_PROTO_MOUSE_* */
    };
} remote_event_t;

/* Sets up the USB serial connection to the desktop. Call once at startup. */
void remote_input_init(void);

/* Runs the USB stack, sends any data queued by remote_send(), and decodes
 * whatever has arrived into the event queue. Call it every pass of your main
 * loop. Cheap and non-blocking when nothing is happening. */
void remote_input_task(void);

/* Takes the oldest queued event, if any. Returns false (and leaves *out
 * alone) when the queue is empty. Call it in a loop until it returns false. */
bool remote_input_get_event(remote_event_t *out);

/* Sends a line of text to the desktop relay, which prints it on its console
 * (prefixed "[PicoCalc]"). printf style. A handy debugging aid that needs no
 * serial adapter or display. Silently does nothing if no desktop program is
 * connected. */
void remote_log(const char *fmt, ...);

/* Sends one already-encoded frame to the desktop (build it with the
 * remote_proto_encode_* helpers in remote_protocol.h, into a buffer of
 * REMOTE_PROTO_MAX_FRAME bytes). Returns true if the whole frame was queued;
 * false if no desktop program is connected or there is not room for it right
 * now. Frames are never sent in part, so the desktop never sees a torn frame.
 * The caller decides whether to try again later or drop it. The data goes out
 * on the next remote_input_task(). */
bool remote_send(const uint8_t *frame, uint8_t len);

/* True while a desktop program has the serial port open. Use it to avoid
 * queueing data for a desktop that is not there (it would otherwise be sent,
 * late, when one connects). remote_log() and remote_send() already do nothing
 * when it is false. */
bool remote_input_connected(void);
