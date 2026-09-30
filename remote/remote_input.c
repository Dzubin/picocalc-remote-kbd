/*
 * remote_input.c - picocalc-remote-kbd
 *
 * See remote_input.h for the public API and how to use it; this file is the
 * implementation, on top of TinyUSB's device-mode CDC-ACM (USB serial port)
 * class driver running on the RP2040/RP2350's native USB controller. It is a
 * channel of its own, completely separate from picocalc-text-starter's UART0
 * debug console, so the two never interact.
 *
 * remote_input_task() polls tud_task() rather than driving USB from an
 * interrupt, so the application must call it often from its main loop. A
 * long blocking call elsewhere in the loop delays USB servicing too.
 *
 * Author: Thomas Dzubin
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "tusb.h"

#include "remote_input.h"
#include "remote_protocol.h"
#include "remote_config.h"

/* Defined in usb_descriptors.c; fills in the chip's unique-ID-derived USB
 * serial number string before enumeration starts. */
void usb_descriptors_init(void);

static remote_event_t event_queue[EVENT_QUEUE_SIZE];
static uint8_t event_head = 0;
static uint8_t event_tail = 0;

static remote_proto_parser_t parser;

/* Removes the oldest queued mouse-move event to make room. Returns false if
 * there is none. */
static bool event_queue_drop_one_move(void)
{
    uint8_t i = event_tail;
    while (i != event_head)
    {
        if (event_queue[i].type == REMOTE_EVENT_MOUSE_MOVE)
        {
            /* close the gap by moving everything after it one place towards the tail */
            uint8_t next = (uint8_t)((i + 1) % EVENT_QUEUE_SIZE);
            while (next != event_head)
            {
                event_queue[i] = event_queue[next];
                i = next;
                next = (uint8_t)((next + 1) % EVENT_QUEUE_SIZE);
            }
            event_head = (uint8_t)((event_head + EVENT_QUEUE_SIZE - 1) % EVENT_QUEUE_SIZE);
            return true;
        }
        i = (uint8_t)((i + 1) % EVENT_QUEUE_SIZE);
    }
    return false;
}

/* Adds an event. Mouse movement is merged into a mouse-move event already at
 * the back of the queue, so a fast mouse cannot fill the queue. When the
 * queue is still full, a queued mouse move is given up to make room for a key
 * or button event (losing a key-up or button-up would leave that key or
 * button stuck down); if there is no mouse move to give up, the new event is
 * dropped. */
static bool event_queue_push(const remote_event_t *ev)
{
    if (ev->type == REMOTE_EVENT_MOUSE_MOVE && event_head != event_tail)
    {
        remote_event_t *last = &event_queue[(event_head + EVENT_QUEUE_SIZE - 1) % EVENT_QUEUE_SIZE];
        if (last->type == REMOTE_EVENT_MOUSE_MOVE)
        {
            int dx = last->mouse_move.dx + ev->mouse_move.dx;
            int dy = last->mouse_move.dy + ev->mouse_move.dy;
            last->mouse_move.dx = (int16_t)(dx > INT16_MAX ? INT16_MAX : (dx < INT16_MIN ? INT16_MIN : dx));
            last->mouse_move.dy = (int16_t)(dy > INT16_MAX ? INT16_MAX : (dy < INT16_MIN ? INT16_MIN : dy));
            return true;
        }
    }

    uint8_t next_head = (uint8_t)((event_head + 1) % EVENT_QUEUE_SIZE);
    if (next_head == event_tail)
    {
        if (ev->type == REMOTE_EVENT_MOUSE_MOVE || !event_queue_drop_one_move())
            return false; /* queue full: drop the new event rather than overwrite an older one */
        next_head = (uint8_t)((event_head + 1) % EVENT_QUEUE_SIZE);
    }

    event_queue[event_head] = *ev;
    event_head = next_head;
    return true;
}

static void handle_packet(const remote_proto_packet_t *pkt)
{
    remote_event_t ev;

    switch (pkt->type)
    {
    case REMOTE_PROTO_KEY_EVENT:
        if (pkt->length != 2)
            return;
        ev.type = REMOTE_EVENT_KEY;
        ev.key.code = pkt->payload[0];
        ev.key.pressed = (pkt->payload[1] & REMOTE_PROTO_KEY_PRESSED) != 0;
        event_queue_push(&ev);
        break;

    case REMOTE_PROTO_MOUSE_MOVE:
        if (pkt->length != 4)
            return;
        ev.type = REMOTE_EVENT_MOUSE_MOVE;
        remote_proto_decode_mouse_move(pkt, &ev.mouse_move.dx, &ev.mouse_move.dy);
        event_queue_push(&ev);
        break;

    case REMOTE_PROTO_MOUSE_BUTTON:
        if (pkt->length != 2)
            return;
        ev.type = REMOTE_EVENT_MOUSE_BUTTON;
        ev.mouse_button.button = pkt->payload[0];
        ev.mouse_button.pressed = pkt->payload[1] != 0;
        event_queue_push(&ev);
        break;

    case REMOTE_PROTO_REFRESH:
        ev.type = REMOTE_EVENT_REFRESH;
        event_queue_push(&ev);
        break;

    default:
        break; /* unknown, or a desktop-bound type like TEXT_LOG -- ignore */
    }
}

void remote_input_init(void)
{
    remote_proto_parser_init(&parser);
    usb_descriptors_init();
    tud_init(BOARD_TUD_RHPORT);
}

void remote_input_task(void)
{
    tud_task();

    if (!tud_cdc_connected())
        return;

    tud_cdc_write_flush(); /* push out anything remote_send() queued since last time */

    uint8_t buf[64];
    uint32_t count;
    while ((count = tud_cdc_read(buf, sizeof(buf))) > 0)
    {
        for (uint32_t i = 0; i < count; i++)
        {
            remote_proto_packet_t pkt;
            if (remote_proto_parser_feed(&parser, buf[i], &pkt))
                handle_packet(&pkt);
        }
    }
}

bool remote_input_get_event(remote_event_t *out)
{
    if (event_head == event_tail)
        return false;

    *out = event_queue[event_tail];
    event_tail = (uint8_t)((event_tail + 1) % EVENT_QUEUE_SIZE);
    return true;
}

void remote_log(const char *fmt, ...)
{
    if (!tud_cdc_connected())
        return;

    char text[128];
    va_list args;
    va_start(args, fmt);
    int length = vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    if (length <= 0)
        return;
    if (length > (int)sizeof(text) - 1)
        length = (int)sizeof(text) - 1;

    /* Chunked into REMOTE_PROTO_MAX_PAYLOAD-sized pieces -- only matters
     * for a line longer than that; most remote_log() calls fit in one. */
    uint8_t frame[REMOTE_PROTO_MAX_PAYLOAD + 5];
    int offset = 0;
    while (offset < length)
    {
        int remaining = length - offset;
        uint8_t chunk = (uint8_t)(remaining > REMOTE_PROTO_MAX_PAYLOAD ? REMOTE_PROTO_MAX_PAYLOAD : remaining);
        uint8_t frame_len = remote_proto_encode_text_log(text + offset, chunk, frame);
        if (!remote_send(frame, frame_len))
            break; /* no room: drop the rest of the line rather than send a torn frame */
        offset += chunk;
    }
    tud_cdc_write_flush();
}

bool remote_input_connected(void)
{
    return tud_cdc_connected();
}

bool remote_send(const uint8_t *frame, uint8_t len)
{
    if (!tud_cdc_connected() || tud_cdc_write_available() < len)
        return false;

    tud_cdc_write(frame, len);
    return true;
}

/* --------------------------------------------------------------------
 * TinyUSB device callbacks.
 *
 * Defined unconditionally (even as empty stubs) rather than relying on
 * them being weak symbols in this TinyUSB version -- matches convention
 * in every pico-sdk TinyUSB example. RX is drained by remote_input_task()
 * via tud_cdc_read() instead of reacting to tud_cdc_rx_cb() directly.
 * -------------------------------------------------------------------- */

void tud_cdc_rx_cb(uint8_t itf)
{
    (void)itf;
}

void tud_mount_cb(void) {}
void tud_umount_cb(void) {}
void tud_suspend_cb(bool remote_wakeup_en) { (void)remote_wakeup_en; }
void tud_resume_cb(void) {}
