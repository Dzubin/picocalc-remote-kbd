/*
 * relay_mirror.c - picocalc-remote-kbd desktop relay, optional screen mirror
 *
 * See relay_mirror.h. Keeps a copy of the demo's screen state (screen_model.h,
 * shared with the PicoCalc firmware) and updates it from the packets the demo
 * sends, then draws it with the very same screen_pixel() routine the firmware
 * uses for the LCD, so the two pictures cannot differ.
 *
 * Author: Thomas Dzubin
 */

#include <string.h>

#include "relay_mirror.h"
#include "../demo/screen_model.h"
#include "../demo/screen_mirror_protocol.h"
#include "relay_config.h"

/* The window is laid out for a picture of this size (relay_config.h). */
_Static_assert(SCREEN_W == RELAY_PICTURE_W && SCREEN_H == RELAY_PICTURE_H, "relay_config.h picture size must match screen_model.h");

static screen_state_t mirror;     /* the PicoCalc's screen, rebuilt from its packets */
static bool mirror_dirty = true;  /* changed since the texture was last built */
static SDL_Texture *texture;

bool relay_mirror_init(SDL_Renderer *renderer)
{
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
                                SCREEN_W, SCREEN_H);
    return texture != NULL;
}

void relay_mirror_shutdown(void)
{
    if (texture)
        SDL_DestroyTexture(texture);
    texture = NULL;
}

bool relay_mirror_handle_packet(const remote_proto_packet_t *pkt)
{
    switch (pkt->type)
    {
    case SCREEN_PROTO_ROW:
        if (pkt->length >= 1 && pkt->payload[0] < TEXT_ROWS)
        {
            int n = pkt->length - 1;
            if (n > TEXT_COLS)
                n = TEXT_COLS;
            memcpy(mirror.text[pkt->payload[0]], pkt->payload + 1, (size_t)n);
            mirror_dirty = true;
        }
        return true;

    case SCREEN_PROTO_TEXT_CURSOR:
        if (pkt->length == 2 && pkt->payload[0] < TEXT_COLS && pkt->payload[1] < TEXT_ROWS)
        {
            mirror.text_col = pkt->payload[0];
            mirror.text_row = pkt->payload[1];
            mirror_dirty = true;
        }
        return true;

    case SCREEN_PROTO_ARROW:
        if (pkt->length == 5)
        {
            uint16_t x, y;
            uint8_t fill;
            screen_proto_decode_arrow(pkt, &x, &y, &fill);
            mirror.arrow_x = x;
            mirror.arrow_y = y;
            mirror.fill = fill;
            mirror_dirty = true;
        }
        return true;

    default:
        return false;
    }
}

/* Rebuilds the texture from the mirrored screen (only when it changed). */
static void rebuild_texture(void)
{
    void *pixels;
    int pitch, x, y;

    if (!mirror_dirty || SDL_LockTexture(texture, NULL, &pixels, &pitch) != 0)
        return;
    for (y = 0; y < SCREEN_H; y++)
    {
        uint16_t *row = (uint16_t *)((uint8_t *)pixels + (size_t)y * (size_t)pitch);
        for (x = 0; x < SCREEN_W; x++)
            row[x] = screen_pixel(&mirror, x, y);
    }
    SDL_UnlockTexture(texture);
    mirror_dirty = false;
}

void relay_mirror_draw(SDL_Renderer *renderer, const SDL_Rect *dest)
{
    rebuild_texture();
    SDL_RenderCopy(renderer, texture, NULL, dest);
}
