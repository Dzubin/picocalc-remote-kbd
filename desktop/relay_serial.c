/*
 * relay_serial.c - picocalc-remote-kbd desktop relay, serial port layer
 *
 * Author: Thomas Dzubin
 */

#include "relay_serial.h"
#include "relay_config.h"

#ifdef _WIN32

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <initguid.h>
#include <devguid.h>
#include <setupapi.h>

static HANDLE port = INVALID_HANDLE_VALUE;

/* Walks the present COM-port devices and picks the one whose hardware ID
 * carries our VID/PID, then reads its "COMn" name from the device's registry
 * key. Only devices that are plugged in right now are considered. */
int relay_serial_find(uint16_t vid, uint16_t pid, char *out, int outsize)
{
    char want[32];
    HDEVINFO set;
    SP_DEVINFO_DATA dev;
    DWORD i;
    int found = -1;

    snprintf(want, sizeof(want), "VID_%04X&PID_%04X", (unsigned)vid, (unsigned)pid);
    set = SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE)
        return -1;

    dev.cbSize = sizeof(dev);
    for (i = 0; found != 0 && SetupDiEnumDeviceInfo(set, i, &dev); i++)
    {
        char id[256];
        HKEY key;

        if (!SetupDiGetDeviceInstanceIdA(set, &dev, id, sizeof(id), NULL))
            continue;
        if (!strstr(id, want))
            continue;

        key = SetupDiOpenDevRegKey(set, &dev, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (key != INVALID_HANDLE_VALUE)
        {
            DWORD size = (DWORD)outsize;
            DWORD type = 0;
            if (RegQueryValueExA(key, "PortName", NULL, &type, (LPBYTE)out, &size) == ERROR_SUCCESS && type == REG_SZ)
                found = 0;
            RegCloseKey(key);
        }
    }

    SetupDiDestroyDeviceInfoList(set);
    return found;
}

int relay_serial_open(const char *name, int baud)
{
    char path[64];
    DCB dcb;
    COMMTIMEOUTS to;

    /* The device-namespace prefix (backslash backslash dot backslash) is
     * required for COM10 and above; built from single characters so no
     * escape sequence can go wrong. */
    path[0] = '\\';
    path[1] = '\\';
    path[2] = '.';
    path[3] = '\\';
    snprintf(path + 4, sizeof(path) - 4, "%s", name);
    port = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (port == INVALID_HANDLE_VALUE)
    {
        printf("CreateFile(%s) failed, Windows error %lu\n", path, (unsigned long)GetLastError());
        return -1;
    }

    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(port, &dcb))
    {
        printf("GetCommState failed, Windows error %lu\n", (unsigned long)GetLastError());
        goto fail;
    }
    dcb.BaudRate = (DWORD)baud;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    if (!SetCommState(port, &dcb))
    {
        printf("SetCommState failed, Windows error %lu\n", (unsigned long)GetLastError());
        goto fail;
    }

    /* MAXDWORD interval with zero totals makes ReadFile return at once. */
    to.ReadIntervalTimeout = MAXDWORD;
    to.ReadTotalTimeoutMultiplier = 0;
    to.ReadTotalTimeoutConstant = 0;
    to.WriteTotalTimeoutMultiplier = 0;
    to.WriteTotalTimeoutConstant = RELAY_WRITE_TIMEOUT_MS;
    if (!SetCommTimeouts(port, &to))
    {
        printf("SetCommTimeouts failed, Windows error %lu\n", (unsigned long)GetLastError());
        goto fail;
    }

    EscapeCommFunction(port, SETDTR);
    return 0;

fail:
    CloseHandle(port);
    port = INVALID_HANDLE_VALUE;
    return -1;
}

int relay_serial_read(uint8_t *buf, int max)
{
    DWORD got = 0;
    if (port == INVALID_HANDLE_VALUE)
        return -1;
    if (!ReadFile(port, buf, (DWORD)max, &got, NULL))
        return -1;
    return (int)got;
}

int relay_serial_write(const uint8_t *buf, int len)
{
    DWORD put = 0;
    if (port == INVALID_HANDLE_VALUE)
        return -1;
    if (!WriteFile(port, buf, (DWORD)len, &put, NULL))
        return -1;
    return ((int)put == len) ? 0 : 1; /* short write: the write timeout hit, the PicoCalc is not draining */
}

void relay_serial_close(void)
{
    if (port != INVALID_HANDLE_VALUE)
        CloseHandle(port);
    port = INVALID_HANDLE_VALUE;
}

#else /* POSIX */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static int port = -1;

/* Reads a hex ID file (idVendor / idProduct) from sysfs; -1 if unreadable. */
static long read_hex_file(const char *path)
{
    char text[16];
    FILE *f = fopen(path, "r");
    long value = -1;

    if (!f)
        return -1;
    if (fgets(text, sizeof(text), f))
        value = strtol(text, NULL, 16);
    fclose(f);
    return value;
}

/* Scans /sys/class/tty/ttyACM* for one whose USB parent device has our
 * VID/PID. The tty's "device" link is the USB interface; its parent
 * directory is the USB device holding idVendor / idProduct. */
int relay_serial_find(uint16_t vid, uint16_t pid, char *out, int outsize)
{
    DIR *d = opendir("/sys/class/tty");
    struct dirent *e;
    int found = -1;

    if (!d)
        return -1;
    while (found != 0 && (e = readdir(d)) != NULL)
    {
        char path[512];
        if (strncmp(e->d_name, "ttyACM", 6) != 0)
            continue;
        snprintf(path, sizeof(path), "/sys/class/tty/%s/device/../idVendor", e->d_name);
        if (read_hex_file(path) != (long)vid)
            continue;
        snprintf(path, sizeof(path), "/sys/class/tty/%s/device/../idProduct", e->d_name);
        if (read_hex_file(path) != (long)pid)
            continue;
        snprintf(out, (size_t)outsize, "/dev/%s", e->d_name);
        found = 0;
    }
    closedir(d);
    return found;
}

static speed_t baud_constant(int baud)
{
    switch (baud)
    {
    case 9600:   return B9600;
    case 19200:  return B19200;
    case 38400:  return B38400;
    case 57600:  return B57600;
    default:     return B115200;
    }
}

int relay_serial_open(const char *name, int baud)
{
    struct termios tio;
    int bits = TIOCM_DTR | TIOCM_RTS;

    port = open(name, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (port < 0)
        return -1;

    if (tcgetattr(port, &tio) != 0)
        goto fail;
    cfmakeraw(&tio);
    cfsetispeed(&tio, baud_constant(baud));
    cfsetospeed(&tio, baud_constant(baud));
    tio.c_cflag |= CLOCAL | CREAD;
    if (tcsetattr(port, TCSANOW, &tio) != 0)
        goto fail;

    ioctl(port, TIOCMBIS, &bits);
    return 0;

fail:
    close(port);
    port = -1;
    return -1;
}

int relay_serial_read(uint8_t *buf, int max)
{
    ssize_t n;
    if (port < 0)
        return -1;
    n = read(port, buf, (size_t)max);
    if (n < 0)
        return (errno == EAGAIN || errno == EWOULDBLOCK) ? 0 : -1;
    return (int)n;
}

int relay_serial_write(const uint8_t *buf, int len)
{
    int done = 0;
    if (port < 0)
        return -1;
    while (done < len)
    {
        ssize_t n = write(port, buf + done, (size_t)(len - done));
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /* The tty buffer is full. Wait a short while for room; if
                 * none comes, give up instead of freezing the relay. */
                struct pollfd pfd = {port, POLLOUT, 0};
                if (poll(&pfd, 1, RELAY_WRITE_TIMEOUT_MS) <= 0)
                    return 1;
                continue;
            }
            return -1;
        }
        done += (int)n;
    }
    return 0;
}

void relay_serial_close(void)
{
    if (port >= 0)
        close(port);
    port = -1;
}

#endif
