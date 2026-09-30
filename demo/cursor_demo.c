/*
 * cursor_demo.c - picocalc-remote-kbd cursor and typing demo
 *
 * Shows an arrow pointer on the PicoCalc's screen that follows the mouse of
 * the desktop relay program (desktop/relay.c), and a text area you type into
 * with the relay's keyboard. Run the relay, click its window to capture, then
 * move the mouse and type. The relay window shows the same picture.
 *
 *   Mouse          moves the arrow; holding a button changes its fill colour
 *                  (left = yellow, right = green, middle = magenta)
 *   Typing         letters, digits, punctuation, Shift, Caps Lock
 *   Enter, Tab     new line / next tab stop
 *   Backspace      erase the character before the text cursor
 *   Arrow keys     move the text cursor
 *
 * The picture (screen_model.c, shared with the relay) is a 40x32 grid of 8x10
 * pixel cells (the starter's 8x10 font, by Blair Leduc), a block text cursor
 * and the arrow. Every LCD update goes through draw_area(), which rebuilds a
 * rectangle from that model and sends it to the LCD in a single block, so the
 * arrow can move over text without erasing it and nothing flickers.
 *
 * Whenever the picture changes, the new state is also sent to the relay
 * (changed text rows, text cursor, arrow), a little at a time so the USB
 * buffer never overflows. A REFRESH request from the relay marks everything
 * as changed so a late-starting relay gets the whole screen.
 *
 * The PicoCalc's own keyboard (picocalc_keys.c) also types into the text area
 * and is passed on to the relay as USB HID key events. Pressing '~' on it
 * reboots into BOOTSEL, as in the other firmware here.
 *
 * Author: Thomas Dzubin
 */

#include <string.h>

#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include "lcd.h"
#include "remote_input.h"
#include "remote_protocol.h"
#include "screen_mirror_protocol.h"
#include "hid_keymap.h"
#include "picocalc_keys.h"
#include "screen_model.h"
#include "cursor_demo.h"

static void draw_area(int x, int y, int w, int h);
static void draw_cell(int col, int row);
static void redraw_all(void);
static void move_arrow(int new_x, int new_y);
static void handle_button(const remote_event_t *ev);
static void handle_key_event(uint8_t usage, bool pressed);
static void do_key(uint8_t usage);
static void put_char(char c);
static void newline(void);
static void backspace(void);
static void scroll_up(void);
static void move_text_cursor(int new_col, int new_row);
static bool is_repeatable(uint8_t usage);
static void mark_all_dirty(void);
static void mirror_send(void);
static void queue_device_key(uint8_t usage, bool pressed);
static int clamp(int v, int lo, int hi);

static screen_state_t scr;

static uint8_t buttons_down = 0; /* bit per REMOTE_PROTO_MOUSE_* id */

static bool shift_down = false;
static bool caps_lock = false;
static uint8_t repeat_usage = 0; /* 0 = nothing repeating */
static uint32_t repeat_next_ms = 0;

/* What still has to be sent to the relay. */
static bool row_dirty[TEXT_ROWS];
static bool cursor_dirty = false;
static bool arrow_dirty = false;

/* PicoCalc key presses waiting to go to the relay, in order. */
static key_out_t key_out[KEY_OUT_QUEUE_SIZE];
static uint8_t key_out_head = 0;
static uint8_t key_out_tail = 0;

int main(void)
{
    stdio_init_all();

    picocalc_keys_init();

    lcd_init();
    lcd_enable_cursor(false); /* the starter blinks a text cursor by default */

    scr.arrow_x = (SCREEN_W - CURSOR_W) / 2;
    scr.arrow_y = (SCREEN_H - CURSOR_H) / 2;

    /* A title line with the version, on the first row; typing starts below it. */
    memcpy(scr.text[0], DEMO_BANNER, sizeof(DEMO_BANNER) - 1);
    scr.text_row = 2;

    remote_input_init();
    redraw_all();

    while (1)
    {
        /* The PicoCalc's own keys: '~' reboots to BOOTSEL; everything else
         * types into the text area and is passed on to the relay. */
        picocalc_keys_poll();
        if (picocalc_keys_bootsel_requested())
            reset_usb_boot(0, 0); /* does not return */
        uint8_t key_usage;
        bool key_pressed;
        while (picocalc_keys_get(&key_usage, &key_pressed))
        {
            handle_key_event(key_usage, key_pressed);
            queue_device_key(key_usage, key_pressed);
        }

        remote_input_task();

        /* Fold every queued mouse move into one arrow redraw. */
        int dx = 0, dy = 0;
        bool arrow_changed = false;
        remote_event_t ev;
        while (remote_input_get_event(&ev))
        {
            switch (ev.type)
            {
            case REMOTE_EVENT_MOUSE_MOVE:
                dx += ev.mouse_move.dx;
                dy += ev.mouse_move.dy;
                arrow_changed = true;
                break;
            case REMOTE_EVENT_MOUSE_BUTTON:
                handle_button(&ev);
                arrow_changed = true; /* the fill colour may have changed */
                break;
            case REMOTE_EVENT_KEY:
                handle_key_event(ev.key.code, ev.key.pressed);
                break;
            case REMOTE_EVENT_REFRESH:
                /* The relay has just connected or (re)started capturing, so it
                 * holds no keys or buttons: forget any we still think are down
                 * (a lost key-up would otherwise repeat forever), and resend
                 * the whole picture. */
                repeat_usage = 0;
                buttons_down = 0;
                scr.fill = FILL_IDLE;
                arrow_changed = true;
                mark_all_dirty();
                break;
            }
        }
        if (arrow_changed)
            move_arrow(scr.arrow_x + dx, scr.arrow_y + dy);

        /* Auto-repeat for a key that is being held. */
        if (repeat_usage)
        {
            uint32_t now = to_ms_since_boot(get_absolute_time());
            if ((int32_t)(now - repeat_next_ms) >= 0)
            {
                do_key(repeat_usage);
                repeat_next_ms = now + REPEAT_RATE_MS;
            }
        }

        mirror_send();
    }
}

static int clamp(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* --- Sending the picture to the relay ------------------------------------ */

static void mark_all_dirty(void)
{
    for (int row = 0; row < TEXT_ROWS; row++)
        row_dirty[row] = true;
    cursor_dirty = true;
    arrow_dirty = true;
}

static void queue_device_key(uint8_t usage, bool pressed)
{
    if (!remote_input_connected())
        return; /* nobody to send it to; do not keep it for later (it would be typed late) */

    uint8_t next = (uint8_t)((key_out_head + 1) % KEY_OUT_QUEUE_SIZE);
    if (next == key_out_tail)
        return; /* full: the relay is not keeping up, drop this one */
    key_out[key_out_head].usage = usage;
    key_out[key_out_head].pressed = pressed;
    key_out_head = next;
}

/* Sends whatever changed, stopping at the first packet that does not fit in
 * the USB buffer; the rest goes out on a later pass. Key presses go first
 * and in order, so a press is never overtaken by its own release. */
static void mirror_send(void)
{
    uint8_t frame[REMOTE_PROTO_MAX_FRAME];

    if (!remote_input_connected())
    {
        key_out_tail = key_out_head; /* the relay went away: forget keys that were waiting for it */
        return;
    }

    while (key_out_tail != key_out_head)
    {
        uint8_t len = remote_proto_encode_device_key(key_out[key_out_tail].usage, key_out[key_out_tail].pressed, frame);
        if (!remote_send(frame, len))
            return;
        key_out_tail = (uint8_t)((key_out_tail + 1) % KEY_OUT_QUEUE_SIZE);
    }

    for (int row = 0; row < TEXT_ROWS; row++)
    {
        if (!row_dirty[row])
            continue;
        uint8_t len = screen_proto_encode_row((uint8_t)row, scr.text[row], TEXT_COLS, frame);
        if (!remote_send(frame, len))
            return;
        row_dirty[row] = false;
    }
    if (cursor_dirty)
    {
        uint8_t len = screen_proto_encode_text_cursor((uint8_t)scr.text_col, (uint8_t)scr.text_row, frame);
        if (!remote_send(frame, len))
            return;
        cursor_dirty = false;
    }
    if (arrow_dirty)
    {
        uint8_t len = screen_proto_encode_arrow((uint16_t)scr.arrow_x, (uint16_t)scr.arrow_y, scr.fill, frame);
        if (!remote_send(frame, len))
            return;
        arrow_dirty = false;
    }
}

/* --- Drawing on the LCD ------------------------------------------------- */

/* Rebuilds the w*h rectangle at x, y from the model and sends it to the LCD
 * as one block. */
static void draw_area(int x, int y, int w, int h)
{
    static uint16_t buf[DRAW_BUF_PIXELS];

    for (int row = 0; row < h; row++)
        for (int col = 0; col < w; col++)
            buf[row * w + col] = screen_pixel(&scr, x + col, y + row);
    lcd_blit(buf, (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h);
}

static void draw_cell(int col, int row)
{
    draw_area(col * CELL_W, row * CELL_H, CELL_W, CELL_H);
}

/* The whole screen, one text row at a time (keeps the buffer small). */
static void redraw_all(void)
{
    for (int row = 0; row < TEXT_ROWS; row++)
        draw_area(0, row * CELL_H, SCREEN_W, CELL_H);
}

/* --- Mouse -------------------------------------------------------------- */

static void handle_button(const remote_event_t *ev)
{
    if (ev->mouse_button.button > REMOTE_PROTO_MOUSE_MIDDLE)
        return;

    uint8_t bit = (uint8_t)(1u << ev->mouse_button.button);
    if (ev->mouse_button.pressed)
        buttons_down |= bit;
    else
        buttons_down &= (uint8_t)~bit;

    if (buttons_down & (1u << REMOTE_PROTO_MOUSE_LEFT))
        scr.fill = FILL_LEFT;
    else if (buttons_down & (1u << REMOTE_PROTO_MOUSE_RIGHT))
        scr.fill = FILL_RIGHT;
    else if (buttons_down & (1u << REMOTE_PROTO_MOUSE_MIDDLE))
        scr.fill = FILL_MIDDLE;
    else
        scr.fill = FILL_IDLE;
}

static void move_arrow(int new_x, int new_y)
{
    int old_x = scr.arrow_x, old_y = scr.arrow_y;
    new_x = clamp(new_x, 0, SCREEN_W - CURSOR_W);
    new_y = clamp(new_y, 0, SCREEN_H - CURSOR_H);

    int dx = new_x - old_x, dy = new_y - old_y;
    int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;

    scr.arrow_x = new_x;
    scr.arrow_y = new_y;
    arrow_dirty = true;

    if (adx <= CURSOR_MAX_STEP && ady <= CURSOR_MAX_STEP)
    {
        /* One block covering both the old and new arrow. */
        int ux = old_x < new_x ? old_x : new_x;
        int uy = old_y < new_y ? old_y : new_y;
        draw_area(ux, uy, CURSOR_W + adx, CURSOR_H + ady);
    }
    else
    {
        /* Big jump: repaint the old spot (arrow is gone from it), then the new one. */
        draw_area(old_x, old_y, CURSOR_W, CURSOR_H);
        draw_area(new_x, new_y, CURSOR_W, CURSOR_H);
    }
}

/* --- Keyboard and text -------------------------------------------------- */

static bool is_repeatable(uint8_t usage)
{
    if (usage == HID_KEY_CAPS_LOCK || usage == HID_KEY_LSHIFT || usage == HID_KEY_RSHIFT)
        return false;
    if (usage >= HID_KEY_RIGHT && usage <= HID_KEY_UP)
        return true;
    return hid_to_ascii(usage, false, false) != 0;
}

static void handle_key_event(uint8_t usage, bool pressed)
{
    if (usage == HID_KEY_LSHIFT || usage == HID_KEY_RSHIFT)
    {
        shift_down = pressed;
        return;
    }
    if (usage == HID_KEY_CAPS_LOCK)
    {
        if (pressed)
            caps_lock = !caps_lock;
        return;
    }

    if (!pressed)
    {
        if (usage == repeat_usage)
            repeat_usage = 0;
        return;
    }

    do_key(usage);
    if (is_repeatable(usage))
    {
        repeat_usage = usage;
        repeat_next_ms = to_ms_since_boot(get_absolute_time()) + REPEAT_DELAY_MS;
    }
}

/* Performs the action for one key press (or one auto-repeat). */
static void do_key(uint8_t usage)
{
    switch (usage)
    {
    case HID_KEY_LEFT:  move_text_cursor(scr.text_col - 1, scr.text_row); return;
    case HID_KEY_RIGHT: move_text_cursor(scr.text_col + 1, scr.text_row); return;
    case HID_KEY_UP:    move_text_cursor(scr.text_col, scr.text_row - 1); return;
    case HID_KEY_DOWN:  move_text_cursor(scr.text_col, scr.text_row + 1); return;
    }

    char c = hid_to_ascii(usage, shift_down, caps_lock);
    if (c == '\n')
        newline();
    else if (c == '\b')
        backspace();
    else if (c == '\t')
    {
        int stop = (scr.text_col / TAB_WIDTH + 1) * TAB_WIDTH;
        while (scr.text_col < stop && scr.text_col < TEXT_COLS - 1)
            put_char(' ');
    }
    else if (c)
        put_char(c);
}

/* Moves the text cursor, repainting the two cells it touches. */
static void move_text_cursor(int new_col, int new_row)
{
    int old_col = scr.text_col, old_row = scr.text_row;
    scr.text_col = clamp(new_col, 0, TEXT_COLS - 1);
    scr.text_row = clamp(new_row, 0, TEXT_ROWS - 1);
    cursor_dirty = true;
    draw_cell(old_col, old_row);
    draw_cell(scr.text_col, scr.text_row);
}

/* Scrolls the text up one row and repaints the whole screen. */
static void scroll_up(void)
{
    memmove(scr.text[0], scr.text[1], sizeof(scr.text) - sizeof(scr.text[0]));
    memset(scr.text[TEXT_ROWS - 1], 0, sizeof(scr.text[0]));
    mark_all_dirty();
    redraw_all();
}

static void put_char(char c)
{
    int col = scr.text_col, row = scr.text_row;

    scr.text[row][col] = c;
    row_dirty[row] = true;
    cursor_dirty = true;
    if (++scr.text_col >= TEXT_COLS)
    {
        scr.text_col = 0;
        if (scr.text_row < TEXT_ROWS - 1)
            scr.text_row++;
        else
        {
            scroll_up();
            return;
        }
    }
    draw_cell(col, row);
    draw_cell(scr.text_col, scr.text_row); /* the block cursor moved on */
}

static void newline(void)
{
    int old_col = scr.text_col, old_row = scr.text_row;

    scr.text_col = 0;
    cursor_dirty = true;
    if (scr.text_row < TEXT_ROWS - 1)
    {
        scr.text_row++;
        draw_cell(old_col, old_row);
        draw_cell(scr.text_col, scr.text_row);
    }
    else
        scroll_up();
}

static void backspace(void)
{
    int old_col = scr.text_col, old_row = scr.text_row;

    if (scr.text_col > 0)
        scr.text_col--;
    else if (scr.text_row > 0)
    {
        scr.text_row--;
        scr.text_col = TEXT_COLS - 1;
    }
    else
        return;

    scr.text[scr.text_row][scr.text_col] = 0;
    row_dirty[scr.text_row] = true;
    cursor_dirty = true;
    draw_cell(old_col, old_row);
    draw_cell(scr.text_col, scr.text_row);
}
