#pragma once

/*
 * pico/stdlib.h - stand-in for the Pico SDK header, desktop build only.
 *
 * The vendored starter's font.h (by Blair Leduc, read-only) includes
 * <pico/stdlib.h> just to get the fixed-width integer types, so the relay's
 * build provides this tiny header instead of the SDK.
 */

#include <stdint.h>
#include <stdbool.h>
