#pragma once

/*
 * relay_serial.h - picocalc-remote-kbd desktop relay, serial port layer
 *
 * The only place the relay touches OS-specific serial I/O. relay.c stays
 * portable ISO C and sees just these four functions. Implemented in
 * relay_serial.c for Windows (Win32 COM ports) and POSIX (Linux tty).
 *
 * Author: Thomas Dzubin
 */

#include <stdint.h>

/* Looks for a connected USB serial device with this vendor/product ID (the
 * PicoCalc firmware's, see relay_config.h) and writes its port name ("COM5",
 * "/dev/ttyACM0") into `out`. Returns 0 if found, -1 if not. */
int relay_serial_find(uint16_t vid, uint16_t pid, char *out, int outsize);

/* Opens the named port ("COM5" on Windows, "/dev/ttyACM0" on Linux) and
 * asserts DTR, which the PicoCalc firmware needs before it will talk.
 * Returns 0 on success, -1 on failure. */
int relay_serial_open(const char *name, int baud);

/* Non-blocking read. Returns the number of bytes read (0 if nothing is
 * waiting) or -1 if the link has failed. */
int relay_serial_read(uint8_t *buf, int max);

/* Writes all `len` bytes. Returns 0 on success, 1 if the PicoCalc was not
 * taking data and the write timed out (RELAY_WRITE_TIMEOUT_MS; the data may be
 * partly or wholly lost, the link is still considered up), -1 if the link has
 * failed. */
int relay_serial_write(const uint8_t *buf, int len);

void relay_serial_close(void);
