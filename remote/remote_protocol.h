#pragma once

/*
 * remote_protocol.h - picocalc-remote-kbd wire protocol
 *
 * The packets that travel between the PicoCalc and the desktop relay program
 * over the USB serial link. This one header is shared, byte for byte, by the
 * PicoCalc firmware (remote_input.c) and the desktop relay (desktop/relay.c),
 * so the two ends can never disagree about the format.
 *
 * You only need to read this file if you are
 *   - sending your own kinds of packet (see "Adding your own packet types"), or
 *   - writing something other than the supplied relay to talk to the PicoCalc.
 * If you just want keyboard and mouse events in your PicoCalc program, use
 * remote_input.h and never touch this file. See INTEGRATION.md.
 *
 * -- Packets ---------------------------------------------------------------
 *
 *   type  name          direction            payload
 *   ----  ------------  -------------------  --------------------------------
 *   0x01  KEY_EVENT     desktop -> PicoCalc  HID usage ID (u8), flags (u8)
 *   0x02  MOUSE_MOVE    desktop -> PicoCalc  dx (i16 LE), dy (i16 LE)
 *   0x03  MOUSE_BUTTON  desktop -> PicoCalc  button id (u8), pressed (u8)
 *   0x04  TEXT_LOG      PicoCalc -> desktop  text, not NUL terminated
 *   0x05  REFRESH       desktop -> PicoCalc  (none) "send me your whole state"
 *   0x20  DEVICE_KEY    PicoCalc -> desktop  HID usage ID (u8), flags (u8)
 *
 * KEY_EVENT / DEVICE_KEY flags: bit 0 set = key pressed, clear = released.
 * Key codes are USB HID keyboard usage IDs (USB HID Usage Tables, page 0x07),
 * not ASCII: 0x04 is 'A', 0x28 is Enter, and so on. hid_keymap.h converts to
 * and from ASCII.
 *
 * MOUSE_MOVE carries the movement since the previous packet (relative, like a
 * real mouse); the receiving program keeps track of the pointer position.
 *
 * REFRESH is the desktop asking your program to resend whatever state the
 * desktop side displays. The library just passes it up as
 * REMOTE_EVENT_REFRESH; what to do about it is up to your program. The
 * desktop sends it when it connects.
 *
 * -- Adding your own packet types ------------------------------------------
 *
 * Types 0x10 to 0xFF are free for your own use (0x10-0x12 are taken by the
 * cursor demo, see demo/screen_mirror_protocol.h). Build a packet with
 * remote_proto_encode() and send it with remote_send() (PicoCalc side). The
 * library ignores packet types it does not know, so old and new programs can
 * mix.
 *
 * -- Frame layout ----------------------------------------------------------
 *
 *   +--------+--------+------+--------+-----------+----------+
 *   | SYNC0  | SYNC1  | TYPE | LENGTH |  PAYLOAD  | CHECKSUM |
 *   | 0xA5   | 0x5A   | u8   | u8     | LENGTH B  | u8       |
 *   +--------+--------+------+--------+-----------+----------+
 *
 * CHECKSUM is (TYPE + LENGTH + sum of PAYLOAD bytes) mod 256.
 *
 * Fields are packed and unpacked a byte at a time, never by copying a struct
 * onto the wire: the two ends are built by different compilers
 * (arm-none-eabi-gcc for the PicoCalc, MinGW/GCC/MSVC for the desktop), and
 * trusting them to agree on struct padding would be fragile.
 *
 * Author: Thomas Dzubin
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* USB vendor/product ID the PicoCalc firmware announces (see
 * usb_descriptors.c); the desktop relay uses them to find the right serial
 * port automatically. These are unregistered hobbyist values: if you publish
 * your own project built on this library, pick your own pair here, in one
 * place, and both ends follow. */
#define REMOTE_USB_VID 0xF00F
#define REMOTE_USB_PID 0xF00D

#define REMOTE_PROTO_SYNC0 ((uint8_t)0xA5)
#define REMOTE_PROTO_SYNC1 ((uint8_t)0x5A)

/* Key, mouse and refresh payloads are only 0-4 bytes, but TEXT_LOG carries
 * text, so this is sized for a typical remote_log() line to fit in one
 * packet. */
#define REMOTE_PROTO_MAX_PAYLOAD 60

/* Bytes needed to hold any encoded frame: sync(2) + type + length + payload + checksum. */
#define REMOTE_PROTO_MAX_FRAME (REMOTE_PROTO_MAX_PAYLOAD + 5)

typedef enum
{
    REMOTE_PROTO_KEY_EVENT = 0x01,    /* desktop -> PicoCalc */
    REMOTE_PROTO_MOUSE_MOVE = 0x02,   /* desktop -> PicoCalc */
    REMOTE_PROTO_MOUSE_BUTTON = 0x03, /* desktop -> PicoCalc */
    REMOTE_PROTO_TEXT_LOG = 0x04,     /* PicoCalc -> desktop */
    REMOTE_PROTO_REFRESH = 0x05,      /* desktop -> PicoCalc */
    REMOTE_PROTO_DEVICE_KEY = 0x20,   /* PicoCalc -> desktop: a key on the PicoCalc's own keyboard */
} remote_proto_type_t;

/* KEY_EVENT / DEVICE_KEY flags byte */
#define REMOTE_PROTO_KEY_PRESSED ((uint8_t)0x01)

/* MOUSE_BUTTON button IDs */
#define REMOTE_PROTO_MOUSE_LEFT ((uint8_t)0)
#define REMOTE_PROTO_MOUSE_RIGHT ((uint8_t)1)
#define REMOTE_PROTO_MOUSE_MIDDLE ((uint8_t)2)

/* A decoded packet, filled in by remote_proto_parser_feed() once a full,
 * checksum-valid frame has arrived. */
typedef struct
{
    uint8_t type;
    uint8_t length;
    uint8_t payload[REMOTE_PROTO_MAX_PAYLOAD];
} remote_proto_packet_t;

/* --- Encoding -----------------------------------------------------------
 *
 * Packs a frame into `out` (caller-owned; REMOTE_PROTO_MAX_FRAME bytes is
 * always enough) and returns the number of bytes written, or 0 if `length`
 * exceeds REMOTE_PROTO_MAX_PAYLOAD.
 */
static inline uint8_t remote_proto_encode(uint8_t type, const uint8_t *payload, uint8_t length, uint8_t *out)
{
    if (length > REMOTE_PROTO_MAX_PAYLOAD)
        return 0;

    uint8_t checksum = (uint8_t)(type + length);
    out[0] = REMOTE_PROTO_SYNC0;
    out[1] = REMOTE_PROTO_SYNC1;
    out[2] = type;
    out[3] = length;
    for (uint8_t i = 0; i < length; i++)
    {
        out[4 + i] = payload[i];
        checksum = (uint8_t)(checksum + payload[i]);
    }
    out[4 + length] = checksum;

    return (uint8_t)(5 + length);
}

/* Convenience encoders for each packet type: they size and shape the payload
 * correctly so callers never assemble byte offsets by hand. */

static inline uint8_t remote_proto_encode_key_event(uint8_t hid_usage, bool pressed, uint8_t *out)
{
    uint8_t payload[2] = {hid_usage, (uint8_t)(pressed ? REMOTE_PROTO_KEY_PRESSED : 0)};
    return remote_proto_encode(REMOTE_PROTO_KEY_EVENT, payload, sizeof(payload), out);
}

static inline uint8_t remote_proto_encode_device_key(uint8_t hid_usage, bool pressed, uint8_t *out)
{
    uint8_t payload[2] = {hid_usage, (uint8_t)(pressed ? REMOTE_PROTO_KEY_PRESSED : 0)};
    return remote_proto_encode(REMOTE_PROTO_DEVICE_KEY, payload, sizeof(payload), out);
}

static inline uint8_t remote_proto_encode_mouse_move(int16_t dx, int16_t dy, uint8_t *out)
{
    uint8_t payload[4] = {
        (uint8_t)((uint16_t)dx & 0xFF), (uint8_t)(((uint16_t)dx >> 8) & 0xFF),
        (uint8_t)((uint16_t)dy & 0xFF), (uint8_t)(((uint16_t)dy >> 8) & 0xFF),
    };
    return remote_proto_encode(REMOTE_PROTO_MOUSE_MOVE, payload, sizeof(payload), out);
}

static inline uint8_t remote_proto_encode_mouse_button(uint8_t button, bool pressed, uint8_t *out)
{
    uint8_t payload[2] = {button, (uint8_t)(pressed ? 1 : 0)};
    return remote_proto_encode(REMOTE_PROTO_MOUSE_BUTTON, payload, sizeof(payload), out);
}

static inline uint8_t remote_proto_encode_text_log(const char *text, uint8_t length, uint8_t *out)
{
    if (length > REMOTE_PROTO_MAX_PAYLOAD)
        length = REMOTE_PROTO_MAX_PAYLOAD; /* caller chunks longer text itself */
    return remote_proto_encode(REMOTE_PROTO_TEXT_LOG, (const uint8_t *)text, length, out);
}

static inline uint8_t remote_proto_encode_refresh(uint8_t *out)
{
    return remote_proto_encode(REMOTE_PROTO_REFRESH, NULL, 0, out);
}

/* Decode helper, mirroring remote_proto_encode_mouse_move(): pulls the signed
 * movement out of an already-validated MOUSE_MOVE packet. */
static inline void remote_proto_decode_mouse_move(const remote_proto_packet_t *pkt, int16_t *dx, int16_t *dy)
{
    *dx = (int16_t)((uint16_t)pkt->payload[0] | ((uint16_t)pkt->payload[1] << 8));
    *dy = (int16_t)((uint16_t)pkt->payload[2] | ((uint16_t)pkt->payload[3] << 8));
}

/* --- Decoding -------------------------------------------------------------
 *
 * A small incremental, byte-at-a-time parser. Both the firmware and the
 * desktop side feed it one byte at a time as it arrives off the wire;
 * remote_proto_parser_feed() returns true exactly when `out` has just been
 * filled in with a complete, checksum-valid packet.
 *
 * On any framing error (bad checksum, a stray byte where SYNC0/SYNC1 was
 * expected) the parser silently resets and starts scanning for the next
 * SYNC0/SYNC1 pair. That is what lets it resynchronise on its own after a
 * USB reconnect or a corrupted byte, with no separate "reset" call needed.
 */

typedef enum
{
    REMOTE_PROTO_STATE_SYNC0,
    REMOTE_PROTO_STATE_SYNC1,
    REMOTE_PROTO_STATE_TYPE,
    REMOTE_PROTO_STATE_LENGTH,
    REMOTE_PROTO_STATE_PAYLOAD,
    REMOTE_PROTO_STATE_CHECKSUM,
} remote_proto_parser_state_t;

typedef struct
{
    remote_proto_parser_state_t state;
    uint8_t type;
    uint8_t length;
    uint8_t payload[REMOTE_PROTO_MAX_PAYLOAD];
    uint8_t payload_index;
    uint8_t checksum;
} remote_proto_parser_t;

static inline void remote_proto_parser_init(remote_proto_parser_t *p)
{
    p->state = REMOTE_PROTO_STATE_SYNC0;
}

static inline bool remote_proto_parser_feed(remote_proto_parser_t *p, uint8_t byte, remote_proto_packet_t *out)
{
    switch (p->state)
    {
    case REMOTE_PROTO_STATE_SYNC0:
        if (byte == REMOTE_PROTO_SYNC0)
            p->state = REMOTE_PROTO_STATE_SYNC1;
        break;

    case REMOTE_PROTO_STATE_SYNC1:
        if (byte == REMOTE_PROTO_SYNC1)
            p->state = REMOTE_PROTO_STATE_TYPE;
        else if (byte != REMOTE_PROTO_SYNC0)
            p->state = REMOTE_PROTO_STATE_SYNC0;
        /* else: byte == SYNC0 again, stay here, it may start a real pair */
        break;

    case REMOTE_PROTO_STATE_TYPE:
        p->type = byte;
        p->checksum = byte;
        p->state = REMOTE_PROTO_STATE_LENGTH;
        break;

    case REMOTE_PROTO_STATE_LENGTH:
        if (byte > REMOTE_PROTO_MAX_PAYLOAD)
        {
            /* Cannot be a real frame of ours: resync instead of overrunning payload[]. */
            p->state = REMOTE_PROTO_STATE_SYNC0;
            break;
        }
        p->length = byte;
        p->checksum = (uint8_t)(p->checksum + byte);
        p->payload_index = 0;
        p->state = (byte == 0) ? REMOTE_PROTO_STATE_CHECKSUM : REMOTE_PROTO_STATE_PAYLOAD;
        break;

    case REMOTE_PROTO_STATE_PAYLOAD:
        p->payload[p->payload_index++] = byte;
        p->checksum = (uint8_t)(p->checksum + byte);
        if (p->payload_index == p->length)
            p->state = REMOTE_PROTO_STATE_CHECKSUM;
        break;

    case REMOTE_PROTO_STATE_CHECKSUM:
        p->state = REMOTE_PROTO_STATE_SYNC0;
        if (byte != p->checksum)
            break; /* bad frame: drop it and resync */

        out->type = p->type;
        out->length = p->length;
        memcpy(out->payload, p->payload, p->length);
        return true;
    }

    return false;
}
