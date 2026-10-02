# Adding remote keyboard and mouse to your PicoCalc program

*Guide for people who want to use this library in their own PicoCalc project. Author: Thomas Dzubin.*

The library lets a **PC** send keyboard and mouse events to your PicoCalc
program over the USB cable. There are two common reasons to want that:

1. **Mouse control.** Your program gets a real mouse pointer to work with.
   Games, paint programs, menus, anything that wants pointing.
2. **A proper keyboard.** You (or your users) type on the PC keyboard instead
   of the PicoCalc's small keys.

It works the other way too: your program can print text lines on the PC
(a debugging aid that needs no serial adapter), and send anything else you
like.

```
  PC keyboard and mouse --> relay program --USB--> your PicoCalc program
                                          <-USB--  text, your own data
```

There are two pieces, and you only write code for one of them:

| Piece | What it is | Do you change it? |
|-------|------------|-------------------|
| **`remote/` library** | C code that goes into *your PicoCalc program* | You copy it and call 4 functions |
| **Relay** (`desktop/`) | A ready-made program you run on the PC (Windows or Linux) | No. It works with any program that uses the library |

The cursor demo in `demo/` is **only a proof of concept** that shows the
library working. You do not need any of it.

---

## Quick start

### 1. Copy the library

Copy the whole **`remote/`** folder into your project. It contains:

| File | What it does |
|------|--------------|
| `remote_input.c/.h` | **The library.** `remote_input_init()`, `remote_input_task()`, `remote_input_get_event()`, `remote_log()`, `remote_send()` |
| `remote_protocol.h` | The packet format, shared with the relay. You rarely need to read it |
| `usb_descriptors.c`, `tusb_config.h` | The USB setup that makes the PicoCalc appear as a serial port |
| `hid_keymap.c/.h` | Turns key codes into ASCII characters (and back), with names for the special keys |
| `remote_config.h`, `usb_descriptors_config.h` | The library's constants (queue size, USB layout). Normally left alone |
| `picocalc_keys.c/.h`, `picocalc_keys_config.h` | *Optional.* Reads the PicoCalc's own keyboard as the same kind of key events. **Do not add it if your program already uses the text starter's `keyboard.c`**: the two would compete for the same keystrokes (see *Things to watch out for*) |
| `remote.cmake` | Two-line CMake helper |

The library needs **only the Pico SDK**. It does not need picocalc-text-starter,
a display, or any other driver.

### 2. Add two lines to your `CMakeLists.txt`

After `project(...)` and `pico_sdk_init()`:

```cmake
include(remote/remote.cmake)              # path to the folder you copied
remote_add_to_target(my_program)          # your existing target name
```

That adds the sources, the include path and the libraries. It also switches off
the SDK's own `stdio` over USB (see *Things to watch out for* below).

### 3. Use it in your code

```c
#include "remote_input.h"

int main(void)
{
    // ... your own setup ...
    remote_input_init();                      // once, at startup

    while (1)
    {
        remote_input_task();                  // EVERY pass of your main loop

        remote_event_t ev;
        while (remote_input_get_event(&ev))   // read everything that has arrived
        {
            switch (ev.type)
            {
            case REMOTE_EVENT_KEY:
                // ev.key.code (USB HID key code), ev.key.pressed
                break;
            case REMOTE_EVENT_MOUSE_MOVE:
                // ev.mouse_move.dx, ev.mouse_move.dy (movement since last event)
                break;
            case REMOTE_EVENT_MOUSE_BUTTON:
                // ev.mouse_button.button (REMOTE_PROTO_MOUSE_LEFT/RIGHT/MIDDLE), .pressed
                break;
            case REMOTE_EVENT_REFRESH:
                // the PC asks you to resend what it displays; ignore if you send nothing
                break;
            }
        }

        // ... the rest of your main loop ...
    }
}
```

### 4. Build, flash, run the relay

Build and flash as usual. Then on the PC, run the relay:

```
remote-kbd-relay-Windows.exe
```

It finds the PicoCalc by itself. **Click its window**, and your keyboard and
mouse now go to the PicoCalc. **Ctrl+Alt+G** releases them again.

A complete, working program that does exactly this is
[`examples/remote_test.c`](examples/remote_test.c): about 80 lines with comments, it prints
every event it receives back on the PC. Start there.

---

## Use case 1: add mouse control to a program

The mouse arrives as **relative movement**, like any mouse: `dx` to the right,
`dy` down, in PC screen pixels since the previous event. Your program keeps the
pointer position and stops it at the screen edges.

Movement comes in many small pieces (often several per loop), so add them up and
update **once per pass of your loop**:

```c
#include "remote_input.h"

#define SCREEN_W 320
#define SCREEN_H 320

static int mouse_x = SCREEN_W / 2;
static int mouse_y = SCREEN_H / 2;
static bool left_down = false;

static int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* call once per main-loop pass, after remote_input_task() */
static void read_mouse(void)
{
    int dx = 0, dy = 0;
    remote_event_t ev;

    while (remote_input_get_event(&ev))
    {
        if (ev.type == REMOTE_EVENT_MOUSE_MOVE)
        {
            dx += ev.mouse_move.dx;
            dy += ev.mouse_move.dy;
        }
        else if (ev.type == REMOTE_EVENT_MOUSE_BUTTON && ev.mouse_button.button == REMOTE_PROTO_MOUSE_LEFT)
        {
            left_down = ev.mouse_button.pressed;
        }
    }

    mouse_x = clamp(mouse_x + dx, 0, SCREEN_W - 1);
    mouse_y = clamp(mouse_y + dy, 0, SCREEN_H - 1);
}
```

Notes:

- **Speed.** A PC mouse moves fast compared to the PicoCalc's 320x320 screen. If
  the pointer feels too quick, divide `dx` and `dy` (for example `dx / 2`), or
  keep fractional position in a fixed-point variable.
- **Clicks vs. held buttons.** You get a separate event when a button goes down
  and another when it is released, so dragging is easy: remember the state.
- **Drawing the pointer.** Erasing and redrawing a pointer without flicker over
  changing content is the fiddly part. `demo/cursor_demo.c` shows one way
  (rebuild the small rectangle around the old and new position in memory and
  send it to the screen as one block).

---

## Use case 2: type on the PC keyboard instead of the PicoCalc's

Key events carry a **USB HID key code** in `ev.key.code`, not an ASCII
character. The codes describe *physical keys*: `0x04` is the A key, `0x28` is
Enter, `0xE1` is Left Shift. Shift and Caps Lock are reported as keys of their
own, so your program decides what they mean.

`hid_keymap.h` turns a key code into a character for a US keyboard:

```c
#include "remote_input.h"
#include "hid_keymap.h"

static bool shift_down = false;
static bool caps_lock = false;

static void handle_key(uint8_t code, bool pressed)
{
    if (code == HID_KEY_LSHIFT || code == HID_KEY_RSHIFT)
    {
        shift_down = pressed;                       // Shift is a key of its own
        return;
    }
    if (code == HID_KEY_CAPS_LOCK)
    {
        if (pressed)
            caps_lock = !caps_lock;
        return;
    }
    if (!pressed)
        return;                                     // act on key down only

    char c = hid_to_ascii(code, shift_down, caps_lock);
    if (c == '\n')       { /* Enter */ }
    else if (c == '\b')  { /* Backspace */ }
    else if (c == '\t')  { /* Tab */ }
    else if (c)          { my_program_got_character(c); }   // letters, digits, punctuation
    else
    {
        switch (code)    // keys with no character
        {
        case HID_KEY_LEFT:  /* ... */ break;
        case HID_KEY_RIGHT: /* ... */ break;
        case HID_KEY_UP:    /* ... */ break;
        case HID_KEY_DOWN:  /* ... */ break;
        }
    }
}
```

Notes:

- **Both keyboards at once.** Nothing stops your program from also reading the
  PicoCalc's own keys the way it always did. Feed both into the same handler.
- **Key repeat.** The PC sends one event when a key goes down and one when it
  comes up, nothing in between. If you want auto-repeat, remember which key is
  held and repeat it yourself from a timer. `demo/cursor_demo.c` does exactly
  this (400 ms delay, then every 40 ms).
- **Other layouts.** `hid_keymap.c` is a US layout. The codes are physical keys,
  so for another layout you replace the two small tables in that file.
- **The other way round.** `picocalc_keys.c` (optional) reads the PicoCalc's own
  keyboard and produces the same kind of key codes, which the relay can even
  type into the PC (Windows). See *The relay*, below.

---

## Sending text and your own data back to the PC

**Text lines.** `remote_log()` works like `printf` and shows up on the relay's
console as `[PicoCalc] ...`. It needs no display or serial adapter:

```c
remote_log("score: %d  x=%d y=%d", score, mouse_x, mouse_y);
```

**Your own data.** To send something else (the demo sends its screen contents),
build a packet and send it:

```c
uint8_t frame[REMOTE_PROTO_MAX_FRAME];
uint8_t payload[2] = { high_score_hi, high_score_lo };
uint8_t len = remote_proto_encode(0x40, payload, sizeof payload, frame);   // 0x40 = your own type
remote_send(frame, len);                                                  // false = no room or no PC
```

Packet types 0x10 to 0xFF are free for your own use. The relay ignores types it
does not know, so you need to add matching code to the relay to do something with
yours (see `desktop/relay_mirror.c` for an example). `remote_send()` never sends
half a packet: it returns `false` if there is no room, and you can try again on
a later pass.

---

## Function reference

| Function | What it does |
|----------|--------------|
| `void remote_input_init(void)` | Sets up the USB connection. Call once, at startup |
| `void remote_input_task(void)` | Runs the USB stack and sends/receives data. Call every pass of the main loop |
| `bool remote_input_get_event(remote_event_t *out)` | Takes the next event, if any. Returns `false` when there are none |
| `void remote_log(const char *fmt, ...)` | Sends a line of text to the PC's console (printf style) |
| `bool remote_send(const uint8_t *frame, uint8_t len)` | Sends one whole encoded packet. `false` if no PC is connected or there is no room |
| `bool remote_input_connected(void)` | `true` while a desktop program has the port open. Use it to avoid queueing data for a desktop that is not there |

Event types (`remote_event_t`, field `type`):

| Type | Fields | Meaning |
|------|--------|---------|
| `REMOTE_EVENT_KEY` | `key.code`, `key.pressed` | A key went down or up. `code` is a USB HID key code |
| `REMOTE_EVENT_MOUSE_MOVE` | `mouse_move.dx`, `.dy` | The mouse moved; relative, PC pixels |
| `REMOTE_EVENT_MOUSE_BUTTON` | `mouse_button.button`, `.pressed` | A button went down or up: `REMOTE_PROTO_MOUSE_LEFT`, `_RIGHT`, `_MIDDLE` |
| `REMOTE_EVENT_REFRESH` | none | The PC asks you to resend whatever state it displays |

### Common key codes

USB HID keyboard usage IDs (page 0x07). `hid_keymap.h` has named constants for the
non-printing ones and `hid_to_ascii()` for everything printable.

| Keys | Codes |
|------|-------|
| A to Z | 0x04 to 0x1D |
| 1 to 9, then 0 | 0x1E to 0x27 |
| Enter, Esc, Backspace, Tab, Space | 0x28, 0x29, 0x2A, 0x2B, 0x2C |
| `- = [ ] \ ; ' ` , . /` | 0x2D to 0x38 (leaving out the non-US key at 0x32) |
| Caps Lock | 0x39 |
| F1 to F12 | 0x3A to 0x45 |
| Insert, Home, Page Up, Delete, End, Page Down | 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E |
| Right, Left, Down, Up | 0x4F, 0x50, 0x51, 0x52 |
| Left Ctrl, Shift, Alt | 0xE0, 0xE1, 0xE2 |
| Right Shift | 0xE5 |

---

## Things to watch out for

- **Call `remote_input_task()` often.** It runs the USB stack. Call it every pass
  of your main loop, at least every few milliseconds. A long blocking call
  elsewhere, such as a slow full-screen redraw, makes input feel laggy or drops
  the connection. If you have long operations, call `remote_input_task()` in the
  middle of them.
- **The library owns the USB port.** Do not also use the Pico SDK's `stdio` over
  USB or another TinyUSB class in the same program. (`remote_add_to_target`
  switches the SDK's USB stdio off for you.) If your program already has its own
  `tusb_config.h`, merge the two by hand: the library needs `CFG_TUD_CDC` set to 1.
  The text starter's UART0 console is unaffected and keeps working.
- **Events can be dropped.** The queue holds 32 events. Mouse movement is merged into the previous mouse-move event, and if the queue is still full, queued mouse movement is given up before any key or button event, so a key-up or button-up is lost only if the queue is full of key and button events. If your loop is too slow
  to drain it, the newest events are lost. Reading all events every pass, and adding
  up mouse movement, avoids this.
- **Your PicoCalc keyboard code is untouched.** The library only drives the USB serial
  port. It never reads the PicoCalc's keyboard, the I2C bus or the south bridge, so the
  keyboard handling your program already has keeps working exactly as before, with or
  without a PC connected.
- **Do not combine `picocalc_keys` with the starter's keyboard driver.** The optional
  `picocalc_keys` module reads the keyboard controller's key queue directly, and so does
  the text starter's `keyboard.c` (by Blair Leduc). Using both, each takes some of the
  keystrokes and the other never sees them. Pick one: keep the starter's driver (and do
  not add `picocalc_keys`), or use `picocalc_keys` instead of it. The core library,
  `remote_add_to_target()`, does not include `picocalc_keys`.
- **Nothing blocks.** With no PC connected, no events arrive and `remote_log()` /
  `remote_send()` do nothing. Your program runs the same either way.
- **Mouse movement is relative**, and there is no key repeat from the PC. See the
  use cases above.
- **The USB ID.** The PicoCalc announces the vendor/product ID `F00F:F00D`,
  unregistered hobbyist values, defined once in `remote_protocol.h`. The relay uses
  it to find the serial port. If you publish your own project, pick your own pair
  there and both ends follow.
- **Both chips.** The library is written for RP2040 and RP2350 (it only needs the native
  USB controller). Tested on both: the cursor demo runs on an RP2040 and on an RP2350 PicoCalc.
- **Getting back to BOOTSEL.** While your program owns the USB port, the PC does
  not see a BOOTSEL drive. The optional `picocalc_keys` module gives you the
  shortcut used by the examples: press `~` (Shift + backtick) on the PicoCalc and
  it reboots to BOOTSEL (`picocalc_keys_bootsel_requested()` then
  `reset_usb_boot(0, 0)`, see `examples/remote_test.c`). Add it to your own
  program early, before anything that could hang.

---

## Memory use

What the library costs your PicoCalc program. Measured on an RP2040 build of
`examples/remote_test.c` (V0.01A, 2026-09-30) from the linker map file. The RP2350
figures should be very close but have not been measured.

**RAM** (statically allocated; the RP2040 has 264 KB):

| Part | RAM | What it is |
|------|-----|------------|
| `remote_input.c` | 259 B | The event queue (32 events, 192 B) and the packet parser (65 B) |
| `usb_descriptors.c` | 81 B | The USB string buffer and the serial number |
| TinyUSB USB serial class | 1,481 B | Mostly the 1,024 B send buffer and the 256 B receive buffer set in `tusb_config.h` |
| TinyUSB RP2040 driver | 1,029 B | Endpoint bookkeeping |
| TinyUSB core | 411 B | USB event queue and control-transfer state |
| **Total for the library** | **about 3.3 KB** | about 1.2% of the RP2040's RAM |
| `picocalc_keys.c` (optional) | 330 B | The PicoCalc keyboard reader |

**Flash:** about 2.4 KB for the library's own code (`remote_input.c`, `usb_descriptors.c`,
`hid_keymap.c`) and about 11.5 KB for the TinyUSB code it uses, roughly 14 KB in all.
The optional `picocalc_keys.c` adds about 1.2 KB plus the starter's south-bridge driver.

**Not counted above:**

- *Stack.* Calls use a little temporarily: `remote_log()` about 200 B while it runs,
  `remote_input_task()` about 130 B.
- *USB hardware buffer.* The chip has a separate 4 KB block of USB memory that is not
  part of the main RAM.
- *Anything already in your program.* The library adds no C library, display or SDK
  buffers of its own.

**Making it smaller:**

- The send buffer is the biggest single piece. `CFG_TUD_CDC_TX_BUFSIZE` in
  `remote/tusb_config.h` is 1024. If your program only sends an occasional
  `remote_log()` line and no other data, 256 saves about 750 B. (The cursor demo sends
  its screen contents and needs the larger size.)
- `EVENT_QUEUE_SIZE` in `remote/remote_config.h` is 32 events at 6 B each. A program that
  reads its events every loop pass can use fewer.

**Measuring it yourself.** After a build, run `arm-none-eabi-size` on the `.elf` for the
program's totals, or open the `.elf.map` file next to it and look at the `.bss` and
`.text` entries per object file (for example `remote_input.c.obj`, `cdc_device.c.obj`).
Comparing the totals of a build with and without the library also works.

## The relay (PC side)

`desktop/` builds the relay. It works with **any** program built on the library:
it does not know what your program does. Give people the prebuilt relay together
with your firmware, or build it from source (see `README.md`).

```
remote-kbd-relay-Windows.exe            (finds the PicoCalc by USB ID)
remote-kbd-relay-Windows.exe COM5       (or name the port)
./remote-kbd-relay-Linux /dev/ttyACM0
```

| Key | What it does |
|-----|--------------|
| click the window | Start capturing: your keys and mouse go to the PicoCalc |
| **Ctrl+Alt+G** | Stop or start capturing (capturing also stops when the window loses focus) |
| **Ctrl+Alt+L** | Turn the console log of keys and mouse movement on or off (off at start) |
| **Ctrl+Alt+K** | *Windows.* Type the PicoCalc's own keys into the PC window that has the focus (needs the optional `picocalc_keys` module in your firmware). Turns capture off |

The window border shows the state: blue = idle, green = capturing, orange = typing
into the PC, red = serial link lost. If the cable is pulled out or the PicoCalc is reset or re-flashed, the relay keeps running, looks for the PicoCalc again every second and reconnects by itself (click the window to capture again). Text from `remote_log()` appears on the relay's
console.

**About the picture in the window.** The relay's window also draws a live copy of the
cursor demo's screen, because the demo sends its screen contents. That is demo
support (`relay_mirror.c`). It is on by default, and you can switch it off with
`cmake -DRELAY_WITH_MIRROR=OFF` so the relay ships without any demo code; it then
shows just the status colour. The mirror is a good model if you want your own
program to show something in the relay window.

**Operating systems.** The relay's code is portable C, with the two
OS-specific jobs kept in their own small files: `relay_serial.c` (the serial port)
and `relay_inject.c` (typing into the PC). Windows is tested. The Linux build is
written but has not been built or tested yet, and on Linux typing into the PC is not
supported yet (the keyboard and mouse going *to* the PicoCalc work the same on both).

---

## How it works, briefly

- The PicoCalc appears on the PC as a **USB serial port** (CDC-ACM). It is a
  channel of its own, separate from the picocalc-text-starter UART0 console.
- Both directions carry small framed packets: two sync bytes, a type, a length, the
  payload and a checksum. A receiver that loses its place just scans for the next
  sync bytes, so unplugging and replugging is fine. The format is documented at the
  top of `remote/remote_protocol.h`.
- The relay takes key codes straight from SDL2, whose scancodes already match the USB
  HID key codes, so nothing is translated on the PC side.
- The library is small: `remote_input.c` is one screen of code around TinyUSB's CDC
  class driver.

---

## Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| Relay says "No PicoCalc found" | The PicoCalc is not running a program built on the library, or the USB cable is charge-only. Try naming the port yourself |
| No events arrive | Did you click the relay window? (The border should be green.) Is `remote_input_task()` being called every loop? |
| Events arrive late or in bursts | Something in your main loop takes too long between calls to `remote_input_task()` |
| `remote_log()` prints nothing | The relay is not connected (window border red). Check the cable; the relay reconnects by itself when the PicoCalc reappears |
| The PicoCalc does not show up as a serial port, or drops off the PC | Your program also uses USB stdio or another USB class. See *Things to watch out for* |
| Keys stick down on the PicoCalc | You only handled key-down events and missed the key-up; act on both, or track state |
| Build error about `tusb_config.h` | Your project has its own; merge them (see above) |

---

## Credits

Written by Thomas Dzubin. The demo and the optional `picocalc_keys` module use
drivers from picocalc-text-starter by **Blair Leduc** (LCD,
fonts, south-bridge keyboard), vendored unchanged in `picocalc-text-starter-main/`.
The library itself in `remote/` does not use them.

## License

MIT License, Copyright (c) 2026 Thomas Dzubin (see `LICENSE`): you may use, copy
and modify the library in your own projects, including commercial ones, as long as
the copyright notice and license text stay with it. The drivers from
picocalc-text-starter by Blair Leduc are under his own MIT license, included in
`picocalc-text-starter-main/LICENSE`.
