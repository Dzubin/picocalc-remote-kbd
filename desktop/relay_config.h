#pragma once

/*
 * relay_config.h - picocalc-remote-kbd desktop relay constants
 *
 * Author: Thomas Dzubin
 */

#include "../remote/remote_protocol.h"
#include "../version.h"

#define RELAY_WINDOW_TITLE      "PicoCalc Remote Keyboard " REMOTE_KBD_VERSION " - HOTKEY: CTRL-ALT-G"

/* The window shows a picture (the demo's screen mirror, when it is built in)
 * drawn RELAY_SCALE times larger inside a RELAY_BORDER pixel frame. The frame
 * is the status colour. RELAY_PICTURE_* is the size of the picture before
 * scaling; relay_mirror.c checks it matches the demo's screen. Without the
 * mirror the window is just the same size and shows only the status colour. */
#define RELAY_PICTURE_W         320
#define RELAY_PICTURE_H         320
#define RELAY_SCALE             2
#define RELAY_BORDER            10
#define RELAY_WINDOW_WIDTH      (RELAY_PICTURE_W * RELAY_SCALE + 2 * RELAY_BORDER)
#define RELAY_WINDOW_HEIGHT     (RELAY_PICTURE_H * RELAY_SCALE + 2 * RELAY_BORDER)

/* Whether the relay prints every key, mouse movement and mouse button it sends to the
 * PicoCalc ("[PC key] ...", "[PC mouse] ...") and every key typed on the PicoCalc
 * ("[PicoCalc key] ...") on the console when it starts. 0 = quiet; Ctrl+Alt+L turns it
 * on and off while the relay runs. */
#define RELAY_LOG_INPUT_DEFAULT 0

/* How long (ms) a write to the PicoCalc may wait for room before it is given up, so
 * a stalled PicoCalc cannot freeze the relay window. */
#define RELAY_WRITE_TIMEOUT_MS  50

/* How often (ms) the relay looks for the PicoCalc again after the link is lost. */
#define RELAY_RECONNECT_MS      1000

/* Main loop pause, in milliseconds, so the relay does not spin a whole CPU. */
#define RELAY_LOOP_DELAY_MS     1

/* Window colours (R, G, B). Bright on purpose, never dim grey. */
#define RELAY_COLOR_IDLE        0, 0, 160      /* blue: not capturing */
#define RELAY_COLOR_CAPTURING   0, 160, 0      /* green: input is being sent */
#define RELAY_COLOR_NO_LINK     160, 0, 0      /* red: serial port not open */
#define RELAY_COLOR_INJECTING   255, 140, 0    /* orange: PicoCalc keys are typing into the PC */

/* USB vendor/product ID of the PicoCalc firmware, used to find its serial
 * port automatically. Defined once for both ends in remote_protocol.h. */
#define RELAY_USB_VID           REMOTE_USB_VID
#define RELAY_USB_PID           REMOTE_USB_PID

/* Room for a port name such as "COM12" or "/dev/ttyACM0". */
#define RELAY_PORT_NAME_MAX     64

/* Serial port settings. The link is USB CDC, so the baud rate is ignored by
 * the device, but the OS still wants a value. */
#define RELAY_SERIAL_BAUD       115200

/* Largest mouse delta that fits in one MOUSE_MOVE packet (int16_t). */
#define RELAY_MOUSE_DELTA_MAX   32767

/* Number of HID keyboard usage IDs tracked for held-key release (one byte). */
#define RELAY_KEY_COUNT         256
