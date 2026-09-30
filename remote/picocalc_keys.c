/*
 * picocalc_keys.c - picocalc-remote-kbd, PicoCalc keyboard to HID events
 *
 * See picocalc_keys.h. The south bridge hands back a 16-bit value per FIFO
 * entry: state in the high byte (pressed, held, released) and a key code in
 * the low byte, which is either an ASCII character or one of the starter's
 * KEY_* codes (keyboard.h, by Blair Leduc). Letters arrive lowercase with
 * Shift reported as its own modifier key; symbols arrive already shifted.
 *
 * Translation to HID: modifiers and special keys map directly. A printable
 * character that needs Shift to type on a PC keyboard ('A', '!', '~'...) is
 * sent as Shift down, the key, and (on release) Shift up again, unless the
 * PicoCalc's own Shift key is already being held. Caps Lock is left out: the
 * south bridge has already applied it to the letters.
 *
 * Author: Thomas Dzubin
 */

#include "pico/stdlib.h"

#include "keyboard.h"
#include "southbridge.h"
#include "hid_keymap.h"
#include "picocalc_keys.h"
#include "picocalc_keys_config.h"

typedef struct
{
    uint8_t usage;
    bool pressed;
} key_event_t;

static key_event_t queue[KEY_QUEUE_SIZE];
static uint8_t queue_head = 0;
static uint8_t queue_tail = 0;

static bool bootsel_requested = false;
static uint32_t last_poll_ms = 0;

/* Shift handling. shift_physical: a Shift key really held down on the PicoCalc.
 * shift_added: a Shift we sent ourselves because a key needs it. Several keys
 * can be down at once (fast typing rolls one key into the next), so we count
 * the held keys that were pressed with the added Shift and let it go only when
 * the last of them is released. */
static bool shift_physical = false;
static bool shift_added = false;
static bool key_pressed_shifted[256]; /* held keys that were pressed with the added Shift */
static uint8_t shifted_keys_held = 0;

static void push(uint8_t usage, bool pressed)
{
    uint8_t next = (uint8_t)((queue_head + 1) % KEY_QUEUE_SIZE);
    if (next == queue_tail)
        return; /* full: drop rather than overwrite an older event */
    queue[queue_head].usage = usage;
    queue[queue_head].pressed = pressed;
    queue_head = next;
}

/* Modifier and special-key codes to HID usage; 0 if `code` is not one. */
static uint8_t special_to_hid(uint8_t code)
{
    switch (code)
    {
    case KEY_MOD_SHL:   return HID_KEY_LSHIFT;
    case KEY_MOD_SHR:   return HID_KEY_RSHIFT;
    case KEY_MOD_CTRL:  return HID_KEY_LCTRL;
    case KEY_MOD_ALT:   return HID_KEY_LALT;
    case KEY_BACKSPACE: return HID_KEY_BACKSPACE;
    case KEY_TAB:       return HID_KEY_TAB;
    case KEY_ENTER:
    case KEY_RETURN:    return HID_KEY_ENTER;
    case KEY_ESC:       return HID_KEY_ESCAPE;
    case KEY_UP:        return HID_KEY_UP;
    case KEY_DOWN:      return HID_KEY_DOWN;
    case KEY_LEFT:      return HID_KEY_LEFT;
    case KEY_RIGHT:     return HID_KEY_RIGHT;
    case KEY_INSERT:    return HID_KEY_INSERT;
    case KEY_HOME:      return HID_KEY_HOME;
    case KEY_END:       return HID_KEY_END;
    case KEY_DEL:       return HID_KEY_DELETE;
    case KEY_PAGE_UP:   return HID_KEY_PAGE_UP;
    case KEY_PAGE_DOWN: return HID_KEY_PAGE_DOWN;
    default:
        break;
    }
    if (code >= KEY_F1 && code <= KEY_F9)
        return (uint8_t)(HID_KEY_F1 + (code - KEY_F1));
    if (code == KEY_F10)
        return (uint8_t)(HID_KEY_F1 + 9);
    return 0;
}

static void handle_raw(uint8_t state, uint8_t code)
{
    uint8_t usage;
    bool needs_shift = false;
    bool pressed = (state == KEY_STATE_PRESSED);

    if (state != KEY_STATE_PRESSED && state != KEY_STATE_RELEASED)
        return; /* HOLD is auto-repeat; the receiving program does its own */

    if (code == '~')
    {
        if (pressed)
            bootsel_requested = true;
        return;
    }
    if (code == KEY_CAPS_LOCK || code == KEY_MOD_SYM || code == KEY_BREAK)
        return;

    usage = special_to_hid(code);
    if (usage)
    {
        if (usage == HID_KEY_LSHIFT || usage == HID_KEY_RSHIFT)
            shift_physical = pressed;
        push(usage, pressed);
        return;
    }

    if (!hid_from_ascii((char)code, &usage, &needs_shift))
        return;

    if (pressed)
    {
        /* Make the Shift state right for THIS key before it goes down: add
         * Shift if it needs one, drop our added Shift if it must not have one. */
        if (needs_shift && !shift_physical && !shift_added)
        {
            push(HID_KEY_LSHIFT, true);
            shift_added = true;
        }
        else if (!needs_shift && shift_added)
        {
            push(HID_KEY_LSHIFT, false);
            shift_added = false;
        }
        push(usage, true);
        if (shift_added && !key_pressed_shifted[usage])
        {
            key_pressed_shifted[usage] = true;
            shifted_keys_held++;
        }
    }
    else
    {
        push(usage, false);
        if (key_pressed_shifted[usage])
        {
            key_pressed_shifted[usage] = false;
            if (shifted_keys_held > 0)
                shifted_keys_held--;
            if (shifted_keys_held == 0 && shift_added)
            {
                push(HID_KEY_LSHIFT, false);
                shift_added = false;
            }
        }
    }
}

void picocalc_keys_init(void)
{
    sb_init();
}

void picocalc_keys_poll(void)
{
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_poll_ms < KEY_POLL_MS || !sb_available())
        return;
    last_poll_ms = now;

    /* Drain the FIFO, but never spend more than a few reads per pass: each
     * read is a slow I2C transaction. */
    for (int i = 0; i < KEY_READS_PER_POLL; i++)
    {
        uint16_t raw = sb_read_keyboard();
        uint8_t state = (uint8_t)(raw >> 8);
        if (state == 0)
            break;
        handle_raw(state, (uint8_t)(raw & 0xFF));
    }
}

bool picocalc_keys_get(uint8_t *usage, bool *pressed)
{
    if (queue_head == queue_tail)
        return false;
    *usage = queue[queue_tail].usage;
    *pressed = queue[queue_tail].pressed;
    queue_tail = (uint8_t)((queue_tail + 1) % KEY_QUEUE_SIZE);
    return true;
}

bool picocalc_keys_bootsel_requested(void)
{
    return bootsel_requested;
}
