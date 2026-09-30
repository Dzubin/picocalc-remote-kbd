#pragma once

/*
 * screen_mirror_protocol.h - picocalc-remote-kbd cursor demo
 *
 * The extra packets the cursor demo sends so the desktop relay window can show
 * a live copy of the PicoCalc's screen. This is demo code, not part of the
 * reusable library: it is an example of "adding your own packet types" (see
 * remote/remote_protocol.h), using the library's remote_proto_encode() and
 * remote_send().
 *
 *   type  name         direction            payload
 *   ----  -----------  -------------------  ---------------------------------
 *   0x10  SCREEN_ROW   PicoCalc -> desktop  row (u8), then that row's chars
 *                                           (0 = empty cell)
 *   0x11  TEXT_CURSOR  PicoCalc -> desktop  column (u8), row (u8)
 *   0x12  ARROW        PicoCalc -> desktop  x (u16 LE), y (u16 LE), fill (u8)
 *
 * Author: Thomas Dzubin
 */

#include "remote_protocol.h"

#define SCREEN_PROTO_ROW         0x10
#define SCREEN_PROTO_TEXT_CURSOR 0x11
#define SCREEN_PROTO_ARROW       0x12

/* One row of the text screen: `count` characters (0 = empty cell). */
static inline uint8_t screen_proto_encode_row(uint8_t row, const char *chars, uint8_t count, uint8_t *out)
{
    uint8_t payload[REMOTE_PROTO_MAX_PAYLOAD];
    if (count > REMOTE_PROTO_MAX_PAYLOAD - 1)
        count = REMOTE_PROTO_MAX_PAYLOAD - 1;
    payload[0] = row;
    memcpy(payload + 1, chars, count);
    return remote_proto_encode(SCREEN_PROTO_ROW, payload, (uint8_t)(count + 1), out);
}

static inline uint8_t screen_proto_encode_text_cursor(uint8_t col, uint8_t row, uint8_t *out)
{
    uint8_t payload[2] = {col, row};
    return remote_proto_encode(SCREEN_PROTO_TEXT_CURSOR, payload, sizeof(payload), out);
}

static inline uint8_t screen_proto_encode_arrow(uint16_t x, uint16_t y, uint8_t fill, uint8_t *out)
{
    uint8_t payload[5] = {(uint8_t)(x & 0xFF), (uint8_t)(x >> 8), (uint8_t)(y & 0xFF), (uint8_t)(y >> 8), fill};
    return remote_proto_encode(SCREEN_PROTO_ARROW, payload, sizeof(payload), out);
}

static inline void screen_proto_decode_arrow(const remote_proto_packet_t *pkt, uint16_t *x, uint16_t *y, uint8_t *fill)
{
    *x = (uint16_t)((uint16_t)pkt->payload[0] | ((uint16_t)pkt->payload[1] << 8));
    *y = (uint16_t)((uint16_t)pkt->payload[2] | ((uint16_t)pkt->payload[3] << 8));
    *fill = pkt->payload[4];
}
