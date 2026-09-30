#pragma once

/*
 * relay_mirror.h - picocalc-remote-kbd desktop relay, optional screen mirror
 *
 * Draws a live copy of the cursor demo's screen in the relay window, from the
 * screen packets the demo firmware sends (demo/screen_mirror_protocol.h).
 *
 * This is demo support, not part of the generic relay. The relay works with any
 * PicoCalc program that uses the remote library; it only shows a picture if
 * that program sends these particular packets. To build the relay without it,
 * leave relay_mirror.c out of the build and do not define RELAY_WITH_MIRROR
 * (see desktop/CMakeLists.txt). You can also model your own mirror on this
 * file if your program wants to show something in the relay window.
 *
 * Author: Thomas Dzubin
 */

#include <stdbool.h>

#include <SDL.h>

#include "../remote/remote_protocol.h"

/* Creates the picture's texture. Returns false on failure. */
bool relay_mirror_init(SDL_Renderer *renderer);

void relay_mirror_shutdown(void);

/* If `pkt` is one of the screen mirror packets, applies it and returns true;
 * otherwise returns false and does nothing. */
bool relay_mirror_handle_packet(const remote_proto_packet_t *pkt);

/* Draws the mirrored screen into `dest` (scaled to fit). Rebuilds the picture
 * first if a packet changed it since the last call. */
void relay_mirror_draw(SDL_Renderer *renderer, const SDL_Rect *dest);
