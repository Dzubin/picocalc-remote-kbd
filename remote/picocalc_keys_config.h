#pragma once

/*
 * picocalc_keys_config.h - picocalc-remote-kbd, PicoCalc keyboard reader constants
 *
 * Author: Thomas Dzubin
 */

/* Minimum time between keyboard FIFO polls; each poll is a slow (10 kHz) I2C exchange. */
#define KEY_POLL_MS         25

/* FIFO entries read per poll. */
#define KEY_READS_PER_POLL  4

/* Translated HID events waiting to be consumed. */
#define KEY_QUEUE_SIZE      32
