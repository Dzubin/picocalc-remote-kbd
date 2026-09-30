/*
 * relay.c - picocalc-remote-kbd desktop relay
 *
 * The program that runs on the PC (Windows or Linux) and connects to a PicoCalc
 * running a program built with the remote library. It works with ANY such
 * program: it does not know or care what the PicoCalc program does.
 *
 *   PC keyboard and mouse  -->  relay  -->  PicoCalc    (capture mode)
 *   PicoCalc text          -->  relay  -->  this console ("[PicoCalc] ...")
 *   PicoCalc's own keys    -->  relay  -->  typed into the PC (Windows, optional)
 *
 * How to use it:
 *
 *   picocalc-remote-kbd-relay [port]
 *
 * With no argument the PicoCalc's serial port is found by its USB ID. Or name
 * it: COM5 on Windows, /dev/ttyACM0 on Linux. Click the relay window to start
 * capturing: from then on every key press and mouse movement or button is sent
 * to the PicoCalc.
 *
 *   Ctrl+Alt+G   start or stop capturing (capturing also stops if the window
 *                loses focus). The hotkey itself is not sent.
 *   Ctrl+Alt+L   turn the console log of keys and mouse movement on or off (off at
 *                start). Works whenever the relay window has the focus.
 *   Ctrl+Alt+K   start or stop typing the PicoCalc's own keys into the PC
 *                (Windows only for now). Capture and typing into the PC never
 *                run together, see toggle_injection().
 *
 * If the link is lost (cable pulled out, PicoCalc reset or re-flashed) the relay
 * keeps running, turns the border red and looks for the PicoCalc again every
 * second, reconnecting by itself when it reappears.
 *
 * The window border shows the state: blue idle, green capturing, orange typing
 * into the PC, red serial link lost.
 *
 * -- Reading guide -----------------------------------------------------------
 *
 * The packet format is in remote/remote_protocol.h, shared with the PicoCalc
 * side. SDL2's key scancodes follow the USB HID key codes that the protocol
 * uses, so they go on the wire as they are. Everything OS-specific is kept out
 * of this file, in small pieces with their own headers:
 *
 *   relay_serial.c   opening and reading/writing the serial port, finding it by USB ID
 *   relay_inject.c   typing keys into the PC (the only other OS-specific part)
 *   relay_mirror.c   OPTIONAL: draws the cursor demo's screen in the window.
 *                    Compiled in only when RELAY_WITH_MIRROR is defined; a
 *                    relay for your own program does not need it.
 *
 * Author: Thomas Dzubin
 */

#include <stdio.h>
#include <string.h>

#include <SDL.h>

#include "../remote/remote_protocol.h"
#include "../remote/hid_keymap.h"
#include "relay_config.h"
#include "relay_serial.h"
#include "relay_inject.h"

#ifdef RELAY_WITH_MIRROR
#include "relay_mirror.h"
#endif

static int send_packet(const uint8_t *frame, uint8_t len);
static void send_key(uint8_t usage, bool pressed);
static void request_refresh(void);
static void set_capture(bool on);
static void toggle_injection(void);
static void handle_key(const SDL_KeyboardEvent *k);
static void handle_mouse_button(const SDL_MouseButtonEvent *b);
static void handle_mouse_motion(const SDL_MouseMotionEvent *m);
static void report_device_key(uint8_t usage, bool pressed);
static void log_key(const char *tag, uint8_t usage, bool pressed);
static void flush_mouse_motion(void);
static void shutdown_relay(SDL_Renderer *renderer);
static void pump_serial(void);
static void link_lost(const char *why);
static void try_reconnect(void);
static void draw(SDL_Renderer *r);

static bool capturing = false;      /* PC keyboard and mouse are being sent to the PicoCalc */
static bool injecting = false;      /* PicoCalc keys are being typed into the PC */
static bool link_ok = false;        /* the serial link is working */
static bool held[RELAY_KEY_COUNT];  /* keys we have sent as pressed and not yet as released */
static remote_proto_parser_t parser;
static SDL_Window *window;

static char port_name[RELAY_PORT_NAME_MAX];  /* the serial port in use */
static bool port_named_by_user = false;      /* given on the command line, so never searched for again */
static uint32_t last_reconnect_try_ms = 0;
static bool log_input = RELAY_LOG_INPUT_DEFAULT;  /* print the keys and mouse movement that go through (Ctrl+Alt+L) */
static int pending_dx = 0;  /* mouse movement collected since the last flush_mouse_motion() */
static int pending_dy = 0;
static bool redraw_needed = true;  /* something changed that the window must show */
static int last_status = -1;       /* status colour last drawn, see draw() */

int main(int argc, char **argv)
{
    SDL_Renderer *renderer;
    bool running = true;

    printf("picocalc-remote-kbd relay %s\n", REMOTE_KBD_VERSION);
    printf("Hotkeys:\n");
    printf("  Ctrl+Alt+G  start or stop capturing the PC keyboard and mouse (or click the window to start)\n");
    printf("  Ctrl+Alt+K  start or stop typing the PicoCalc's keys into the PC%s\n",
           relay_inject_available() ? "" : " (not supported on this system yet)");
    printf("  Ctrl+Alt+L  turn the console log of keys and mouse movement on or off (now %s)\n",
           log_input ? "ON" : "OFF");
    printf("\n");

    /* 1. Find and open the serial port. */
    if (argc >= 2)
    {
        snprintf(port_name, sizeof(port_name), "%s", argv[1]);
        port_named_by_user = true;
    }
    else if (relay_serial_find(RELAY_USB_VID, RELAY_USB_PID, port_name, sizeof(port_name)) != 0)
    {
        printf("No PicoCalc found (USB ID %04X:%04X). Is it running a program built with the remote library?\n",
               RELAY_USB_VID, RELAY_USB_PID);
        printf("Or give the port yourself: %s <serial port>   (e.g. COM5 or /dev/ttyACM0)\n", argv[0]);
        return 1;
    }
    else
    {
        printf("Found PicoCalc on %s\n", port_name);
    }

    if (relay_serial_open(port_name, RELAY_SERIAL_BAUD) != 0)
    {
        printf("Could not open serial port %s\n", port_name);
        return 1;
    }
    link_ok = true;
    remote_proto_parser_init(&parser);

    /* 2. Open the window. */
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        relay_serial_close();
        return 1;
    }
    window = SDL_CreateWindow(RELAY_WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              RELAY_WINDOW_WIDTH, RELAY_WINDOW_HEIGHT, 0);
    /* vsync keeps presenting at the screen's rate; fall back to no vsync if that is refused */
    renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC) : NULL;
    if (window && !renderer)
        renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer)
    {
        printf("Could not create the window: %s\n", SDL_GetError());
        shutdown_relay(NULL);
        return 1;
    }
#ifdef RELAY_WITH_MIRROR
    if (!relay_mirror_init(renderer))
    {
        printf("Could not create the screen mirror: %s\n", SDL_GetError());
        shutdown_relay(renderer);
        return 1;
    }
#endif

    printf("Connected to %s. Click the window to capture, Ctrl+Alt+G to release.\n", port_name);
    request_refresh(); /* ask the PicoCalc program to resend its state, in case it has been running a while */

    /* 3. Main loop: window events, hotkeys, PicoCalc data, redraw. */
    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                handle_key(&e.key);
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                handle_mouse_button(&e.button);
                break;
            case SDL_MOUSEMOTION:
                handle_mouse_motion(&e.motion);
                break;
            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                    set_capture(false);
                redraw_needed = true; /* exposed, resized, restored...: the picture may need repainting */
                break;
            }
        }

        flush_mouse_motion();

        if (relay_inject_hotkey_pressed())
            toggle_injection();

        if (!link_ok)
            try_reconnect();
        pump_serial();
        draw(renderer);
        SDL_Delay(RELAY_LOOP_DELAY_MS);
    }

    shutdown_relay(renderer);
    return 0;
}

/* Gives everything back: the mouse grab, held keys, the serial port, the
 * window and SDL. Safe to call with whatever has been set up so far. */
static void shutdown_relay(SDL_Renderer *renderer)
{
    set_capture(false);
    relay_inject_release_all();
    relay_serial_close();
#ifdef RELAY_WITH_MIRROR
    relay_mirror_shutdown();
#endif
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    SDL_Quit();
}

/* --- Sending to the PicoCalc -------------------------------------------- */

/* Sends one encoded frame. A failed write marks the link dead (window goes red). */
static int send_packet(const uint8_t *frame, uint8_t len)
{
    if (!link_ok || len == 0)
        return -1;
    int result = relay_serial_write(frame, len);
    if (result < 0)
    {
        link_lost("Serial write failed");
        return -1;
    }
    return result == 0 ? 0 : -1; /* 1: the PicoCalc was not taking data, this packet is dropped */
}

/* The serial link has failed (most likely the USB cable was pulled out, or the
 * PicoCalc was reset or re-flashed). Stop everything that depends on it, free
 * the port so it can be reopened, and let try_reconnect() wait for the PicoCalc
 * to come back. */
static void link_lost(const char *why)
{
    link_ok = false;
    printf("%s - link lost. Waiting for the PicoCalc to come back...\n", why);

    set_capture(false); /* gives the mouse back; release packets fail harmlessly as the link is already down */
    if (injecting)
    {
        injecting = false;
        printf("Keyboard injection turned OFF.\n");
    }
    relay_inject_release_all();
    memset(held, 0, sizeof(held));
    relay_serial_close();
    last_reconnect_try_ms = SDL_GetTicks();
}

/* Called every pass while the link is down. About once a second, looks for the
 * PicoCalc again (by USB ID, unless the port was named on the command line, in
 * which case that name is retried) and reopens it. The port can come back
 * under a different name after a replug, which is why it is searched for
 * again. */
static void try_reconnect(void)
{
    uint32_t now = SDL_GetTicks();

    if (now - last_reconnect_try_ms < RELAY_RECONNECT_MS)
        return;
    last_reconnect_try_ms = now;

    if (!port_named_by_user &&
        relay_serial_find(RELAY_USB_VID, RELAY_USB_PID, port_name, sizeof(port_name)) != 0)
        return; /* not plugged in yet */
    if (relay_serial_open(port_name, RELAY_SERIAL_BAUD) != 0)
        return; /* found, but not ready to open yet; try again next time */

    link_ok = true;
    remote_proto_parser_init(&parser);
    printf("Reconnected to %s. Click the window to capture again.\n", port_name);
    request_refresh(); /* the PicoCalc program resends its state */
}

static void send_key(uint8_t usage, bool pressed)
{
    uint8_t frame[REMOTE_PROTO_MAX_FRAME];
    if (log_input)
        log_key("[PC key]", usage, pressed);
    send_packet(frame, remote_proto_encode_key_event(usage, pressed, frame));
    held[usage] = pressed;
}

/* Asks the PicoCalc program to resend whatever state this side displays. */
static void request_refresh(void)
{
    uint8_t frame[REMOTE_PROTO_MAX_FRAME];
    send_packet(frame, remote_proto_encode_refresh(frame));
}

/* Turning capture off releases every key still held, so the PicoCalc side is
 * never left thinking a key is stuck down. */
static void set_capture(bool on)
{
    int i;
    if (on == capturing)
        return;
    if (on && injecting)
    {
        printf("Turn off keyboard injection (Ctrl+Alt+K) before capturing.\n");
        return;
    }
    capturing = on;
    pending_dx = pending_dy = 0; /* movement collected but not yet sent is discarded */
    redraw_needed = true;
    SDL_SetRelativeMouseMode(on ? SDL_TRUE : SDL_FALSE);
    if (on)
    {
        request_refresh(); /* cheap resync in case any packet was missed */
    }
    else
    {
        for (i = 0; i < RELAY_KEY_COUNT; i++)
            if (held[i])
                send_key((uint8_t)i, false);
    }
}

/* Ctrl+Alt+K: start or stop typing the PicoCalc's keys into the PC. Capture
 * and injection never run together: with both on, a key typed into the relay
 * window itself would be sent back to the PicoCalc, which would send it
 * again, round and round. */
static void toggle_injection(void)
{
    if (!relay_inject_available())
    {
        printf("Typing the PicoCalc's keys into the PC is not supported on this system yet.\n");
        return;
    }
    injecting = !injecting;
    if (injecting)
    {
        set_capture(false);
        printf("Keyboard injection ON: the PicoCalc's keys now type into the PC window that has the focus. Ctrl+Alt+K to stop.\n");
    }
    else
    {
        relay_inject_release_all();
        printf("Keyboard injection OFF.\n");
    }
}

/* --- Window events -------------------------------------------------------- */

static void handle_key(const SDL_KeyboardEvent *k)
{
    bool down = (k->type == SDL_KEYDOWN);
    int sc = (int)k->keysym.scancode;
    SDL_Keymod mod = SDL_GetModState();

    if (down && k->keysym.sym == SDLK_g && (mod & KMOD_CTRL) && (mod & KMOD_ALT))
    {
        set_capture(!capturing);
        return;
    }
    if (down && k->keysym.sym == SDLK_l && (mod & KMOD_CTRL) && (mod & KMOD_ALT))
    {
        log_input = !log_input;

        printf("Input logging %s (Ctrl+Alt+L to toggle).\n", log_input ? "ON" : "OFF");
        return;
    }
    if (k->keysym.sym == SDLK_k && (mod & KMOD_CTRL) && (mod & KMOD_ALT))
        return; /* the injection hotkey (seen by relay_inject_hotkey_pressed) is not sent on */
    if ((k->keysym.sym == SDLK_l || k->keysym.sym == SDLK_g) && (mod & KMOD_CTRL) && (mod & KMOD_ALT))
        return; /* the release of a hotkey's letter key is not sent on either */
    if (!capturing || k->repeat || sc <= 0 || sc >= RELAY_KEY_COUNT)
        return;
    send_key((uint8_t)sc, down);
}

static void handle_mouse_button(const SDL_MouseButtonEvent *b)
{
    uint8_t frame[REMOTE_PROTO_MAX_FRAME];
    uint8_t button;
    bool down = (b->type == SDL_MOUSEBUTTONDOWN);

    if (!capturing)
    {
        if (down && b->button == SDL_BUTTON_LEFT)
            set_capture(true); /* the click that starts capture is not forwarded */
        return;
    }

    flush_mouse_motion(); /* movement before this click goes out first, in order */

    switch (b->button)
    {
    case SDL_BUTTON_LEFT:   button = REMOTE_PROTO_MOUSE_LEFT; break;
    case SDL_BUTTON_RIGHT:  button = REMOTE_PROTO_MOUSE_RIGHT; break;
    case SDL_BUTTON_MIDDLE: button = REMOTE_PROTO_MOUSE_MIDDLE; break;
    default: return;
    }
    if (log_input)
    {
        printf("[PC mouse] button %s %s\n", button == REMOTE_PROTO_MOUSE_LEFT ? "left" :
               (button == REMOTE_PROTO_MOUSE_RIGHT ? "right" : "middle"), down ? "down" : "up");
        fflush(stdout);
    }
    send_packet(frame, remote_proto_encode_mouse_button(button, down, frame));
}

/* Mouse movement arrives in many tiny events. They are only added up here;
 * flush_mouse_motion() sends (and logs) the sum once per pass of the main loop,
 * instead of one serial write per event. */
static void handle_mouse_motion(const SDL_MouseMotionEvent *m)
{
    if (!capturing)
        return;
    pending_dx += m->xrel;
    pending_dy += m->yrel;
}

static void flush_mouse_motion(void)
{
    uint8_t frame[REMOTE_PROTO_MAX_FRAME];
    int dx = pending_dx, dy = pending_dy;

    if (dx == 0 && dy == 0)
        return;
    pending_dx = 0;
    pending_dy = 0;

    if (dx > RELAY_MOUSE_DELTA_MAX) dx = RELAY_MOUSE_DELTA_MAX;
    if (dx < -RELAY_MOUSE_DELTA_MAX) dx = -RELAY_MOUSE_DELTA_MAX;
    if (dy > RELAY_MOUSE_DELTA_MAX) dy = RELAY_MOUSE_DELTA_MAX;
    if (dy < -RELAY_MOUSE_DELTA_MAX) dy = -RELAY_MOUSE_DELTA_MAX;
    if (log_input)
    {
        printf("[PC mouse] dx=%d dy=%d\n", dx, dy);
        fflush(stdout);
    }
    send_packet(frame, remote_proto_encode_mouse_move((int16_t)dx, (int16_t)dy, frame));
}

/* Prints one key event on the console: the character it types, if it has
 * one, and its USB HID code. `tag` says where it came from. */
static void log_key(const char *tag, uint8_t usage, bool pressed)
{
    char c = hid_to_ascii(usage, false, false);

    if (c >= ' ' && c <= '~')
        printf("%s '%c' (HID 0x%02X) %s\n", tag, c, usage, pressed ? "down" : "up");
    else
        printf("%s HID 0x%02X %s\n", tag, usage, pressed ? "down" : "up");
    fflush(stdout);
}

/* --- Receiving from the PicoCalc ------------------------------------------ */

/* A key pressed or released on the PicoCalc's own keyboard: shown on the
 * console (the character it types, if it has one, and its HID code) and, if
 * injection is on, typed into the PC. */
static void report_device_key(uint8_t usage, bool pressed)
{
    if (log_input)
        log_key("[PicoCalc key]", usage, pressed);

    if (injecting)
        relay_inject_key(usage, pressed);
}

/* Reads whatever the PicoCalc has sent and acts on each packet. */
static void pump_serial(void)
{
    uint8_t buf[64];
    int n, i;
    remote_proto_packet_t pkt;

    if (!link_ok)
        return;
    while ((n = relay_serial_read(buf, sizeof(buf))) > 0)
    {
        for (i = 0; i < n; i++)
        {
            if (!remote_proto_parser_feed(&parser, buf[i], &pkt))
                continue;

            if (pkt.type == REMOTE_PROTO_TEXT_LOG)
            {
                printf("[PicoCalc] %.*s\n", (int)pkt.length, (const char *)pkt.payload);
                fflush(stdout);
            }
            else if (pkt.type == REMOTE_PROTO_DEVICE_KEY)
            {
                if (pkt.length == 2)
                    report_device_key(pkt.payload[0], (pkt.payload[1] & REMOTE_PROTO_KEY_PRESSED) != 0);
            }
#ifdef RELAY_WITH_MIRROR
            else if (relay_mirror_handle_packet(&pkt))
                redraw_needed = true;
#endif
        }
    }
    if (n < 0)
        link_lost("Serial read failed");
}

/* --- Drawing ---------------------------------------------------------------- */

/* The window background is the status colour (it shows as a frame around the
 * picture); the optional screen mirror is drawn scaled up on top. Redrawn only
 * when something changed: the status colour, a mirror packet, or a window
 * event (redraw_needed), not on every pass of the main loop. */
static void draw(SDL_Renderer *r)
{
#ifdef RELAY_WITH_MIRROR
    SDL_Rect dest = {RELAY_BORDER, RELAY_BORDER, RELAY_PICTURE_W * RELAY_SCALE, RELAY_PICTURE_H * RELAY_SCALE};
#endif
    int status = !link_ok ? 3 : (injecting ? 2 : (capturing ? 1 : 0));

    if (status != last_status)
        redraw_needed = true;
    if (!redraw_needed)
        return;
    last_status = status;
    redraw_needed = false;

    if (!link_ok)
        SDL_SetRenderDrawColor(r, RELAY_COLOR_NO_LINK, 255);
    else if (injecting)
        SDL_SetRenderDrawColor(r, RELAY_COLOR_INJECTING, 255);
    else if (capturing)
        SDL_SetRenderDrawColor(r, RELAY_COLOR_CAPTURING, 255);
    else
        SDL_SetRenderDrawColor(r, RELAY_COLOR_IDLE, 255);
    SDL_RenderClear(r);
#ifdef RELAY_WITH_MIRROR
    relay_mirror_draw(r, &dest);
#endif
    SDL_RenderPresent(r);
}
