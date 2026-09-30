#pragma once

/*
 * remote_config.h - picocalc-remote-kbd library constants
 *
 * Tunables of the library in one place. Part of the library: copy it along
 * with the rest of the remote/ folder.
 *
 * Author: Thomas Dzubin
 */

/* Events waiting for the application to read them (remote_input.c). When the
 * queue is full, queued mouse movement is given up first, so key and button
 * events (whose release must never be lost) survive; see event_queue_push(). */
#define EVENT_QUEUE_SIZE        32

/* The range of USB HID usage IDs the hid_keymap.c tables cover, 'a' to '/'. */
#define HID_MAP_FIRST_USAGE     0x04
#define HID_MAP_LAST_USAGE      0x38
