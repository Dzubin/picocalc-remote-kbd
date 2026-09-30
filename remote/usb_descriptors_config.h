#pragma once

/*
 * usb_descriptors_config.h - picocalc-remote-kbd USB descriptor constants
 *
 * The interface numbers, endpoint addresses and string indexes that
 * usb_descriptors.c lays out. Part of the library; normally left alone.
 *
 * Author: Thomas Dzubin
 */

#include "tusb.h"

enum
{
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL,
};

#define EPNUM_CDC_NOTIF     0x81
#define EPNUM_CDC_OUT       0x02
#define EPNUM_CDC_IN        0x82

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

#define STRING_SERIAL_INDEX 3
