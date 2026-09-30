/*
 * usb_descriptors.c - picocalc-remote-kbd
 *
 * The USB descriptors that make the PicoCalc show up on the desktop as a
 * serial port (USB CDC-ACM) named "PicoCalc Remote KBD". Part of the
 * library: you normally do not need to edit this file.
 *
 * The USB vendor/product ID comes from remote_protocol.h
 * (REMOTE_USB_VID / REMOTE_USB_PID), because the desktop relay uses the same
 * pair to find the PicoCalc's serial port automatically. They are unregistered
 * hobbyist values; if you publish something built on this library, change
 * them there and both ends follow. The product string below is only a label.
 *
 * Structure follows TinyUSB's own reference "cdc" device example closely.
 *
 * Author: Thomas Dzubin
 */

#include <string.h>

#include "pico/unique_id.h"

#include "tusb.h"

#include "remote_protocol.h"
#include "usb_descriptors_config.h"

#define REMOTE_KBD_VID REMOTE_USB_VID
#define REMOTE_KBD_PID REMOTE_USB_PID

/* --------------------------------------------------------------------
 * Device Descriptor
 * -------------------------------------------------------------------- */

static tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,

    /* IAD required class/subclass/protocol for a composite (CDC) device */
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor = REMOTE_KBD_VID,
    .idProduct = REMOTE_KBD_PID,
    .bcdDevice = 0x0100,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01,
};

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

/* --------------------------------------------------------------------
 * Configuration Descriptor
 * -------------------------------------------------------------------- */

static uint8_t const desc_fs_configuration[] = {
    /* Config number, interface count, string index, total length, attribute, power in mA */
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    /* Interface number, string index, EP notification address and size, EP data address (out, in) and size */
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_fs_configuration;
}

/* --------------------------------------------------------------------
 * String Descriptors
 * -------------------------------------------------------------------- */

static char const *const string_desc_arr[] = {
    NULL,                          /* 0: language ID, handled specially below */
    "Thomas Dzubin",               /* 1: manufacturer */
    "PicoCalc Remote KBD",         /* 2: product (shown by the desktop OS) */
    NULL,                          /* 3: serial number, filled in below from the chip's unique ID */
    "PicoCalc Remote KBD Data",    /* 4: CDC interface */
};

/* PICO_UNIQUE_BOARD_ID_SIZE_BYTES is 8; as a hex string that's 16 chars + NUL. */
static char serial_number[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];

static uint16_t desc_str_buf[32];

void usb_descriptors_init(void)
{
    pico_get_unique_board_id_string(serial_number, sizeof(serial_number));
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;

    uint8_t char_count;

    if (index == 0)
    {
        desc_str_buf[1] = 0x0409; /* English (US) */
        char_count = 1;
    }
    else
    {
        const uint8_t string_desc_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]);
        char const *str = (index == STRING_SERIAL_INDEX) ? serial_number
                           : (index < string_desc_count)  ? string_desc_arr[index]
                                                           : NULL;
        if (str == NULL)
            return NULL;

        char_count = (uint8_t)strlen(str);
        if (char_count > 31)
            char_count = 31;

        for (uint8_t i = 0; i < char_count; i++)
            desc_str_buf[1 + i] = (uint16_t)str[i];
    }

    desc_str_buf[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * char_count + 2));

    return desc_str_buf;
}
