#pragma once

/*
 * tusb_config.h - picocalc-remote-kbd
 *
 * TinyUSB configuration for the library: a single device-mode CDC-ACM (USB
 * serial) interface on the RP2040/RP2350's native USB controller. It is
 * separate from, and does not touch, picocalc-text-starter's UART0 debug
 * console.
 *
 * Part of the library: you normally do not need to edit this file. If your
 * program already has its own tusb_config.h (because it uses USB for something
 * else), the two must be merged by hand: the library needs CFG_TUD_CDC 1.
 * CFG_TUD_CDC_TX_BUFSIZE is the room for data waiting to go to the desktop;
 * raise it if you send a lot with remote_send().
 *
 * CFG_TUSB_MCU is defined by the Pico SDK's own build (based on
 * PICO_PLATFORM) when linking the tinyusb_device target, so it is not set here.
 *
 * Author: Thomas Dzubin
 */

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined (expected to come from the Pico SDK tinyusb_device target)
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_PICO
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 0
#endif

/* Device mode only -- this project never acts as a USB host. */
#define CFG_TUD_ENABLED 1
#define CFG_TUH_ENABLED 0

#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT 0
#endif

#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED OPT_MODE_DEFAULT_SPEED
#endif

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE 64
#endif

/* -------- Class drivers -------- */
/* Only CDC is used; every other class driver stays off. */
#define CFG_TUD_CDC 1
#define CFG_TUD_MSC 0
#define CFG_TUD_HID 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0

/* CDC FIFO sizes (RAM-side ring buffers TinyUSB manages internally). */
#define CFG_TUD_CDC_RX_BUFSIZE 256
#define CFG_TUD_CDC_TX_BUFSIZE 1024

/* CDC endpoint transfer buffer size -- matches the endpoint size used in
 * usb_descriptors.c's TUD_CDC_DESCRIPTOR(). */
#define CFG_TUD_CDC_EP_BUFSIZE 64

#ifdef __cplusplus
}
#endif
