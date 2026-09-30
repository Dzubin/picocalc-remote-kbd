#pragma once

/*
 * cursor_demo.h - picocalc-remote-kbd, cursor demo firmware constants
 *
 * The picture itself (grid size, colours, arrow shape) lives in
 * screen_model.h so the desktop relay can share it.
 *
 * Author: Thomas Dzubin
 */

#include "screen_model.h"
#include "../version.h"

#define TAB_WIDTH           4

/* Key auto-repeat (the relay sends one event per physical press). */
#define REPEAT_DELAY_MS     400
#define REPEAT_RATE_MS      40

/* Largest per-frame arrow movement redrawn as one combined block; bigger
 * jumps repaint the old and new spots separately. */
#define CURSOR_MAX_STEP     8

/* Largest redraw area: one full text row (the whole screen is redrawn a row at a time). */
#define DRAW_BUF_PIXELS     (SCREEN_W * CELL_H)

/* PicoCalc key events waiting to be sent to the relay. */
#define KEY_OUT_QUEUE_SIZE  32

typedef struct
{
    uint8_t usage;
    bool pressed;
} key_out_t;

/* Title line shown on the first row of the screen (at most TEXT_COLS characters). */
#define DEMO_BANNER         "Remote KBD demo " REMOTE_KBD_VERSION
