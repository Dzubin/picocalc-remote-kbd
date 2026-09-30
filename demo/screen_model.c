/*
 * screen_model.c - picocalc-remote-kbd, the cursor demo's picture
 *
 * See screen_model.h. Uses the starter's 8x10 font (font-8x10.c by Blair
 * Leduc, glyphs stored one byte per row, leftmost pixel in bit 7).
 *
 * Author: Thomas Dzubin
 */

#include "screen_model.h"
#include "font.h"

/* Pointer shape: 'X' = outline, 'O' = fill, '.' = transparent. */
static const char *const shape[CURSOR_H] = {
    "X...........",
    "XX..........",
    "XOX.........",
    "XOOX........",
    "XOOOX.......",
    "XOOOOX......",
    "XOOOOOX.....",
    "XOOOOOOX....",
    "XOOOOOOOX...",
    "XOOOOOOOOX..",
    "XOOOOOXXXXXX",
    "XOOXOOX.....",
    "XOX.XOOX....",
    "XX..XOOX....",
    "X....XOOX...",
    ".....XOOX...",
    "......XOOX..",
    "......XOOX..",
    ".......XX...",
};

static const uint16_t fill_colours[FILL_COUNT] = {
    SCR_RGB(255, 255, 255), /* FILL_IDLE */
    SCR_RGB(255, 255, 0),   /* FILL_LEFT */
    SCR_RGB(0, 255, 0),     /* FILL_RIGHT */
    SCR_RGB(255, 0, 255),   /* FILL_MIDDLE */
};

uint16_t screen_pixel(const screen_state_t *s, int x, int y)
{
    int sx = x - s->arrow_x, sy = y - s->arrow_y;

    if (sx >= 0 && sx < CURSOR_W && sy >= 0 && sy < CURSOR_H && shape[sy][sx] != '.')
        return shape[sy][sx] == 'X' ? COLOR_OUTLINE : fill_colours[s->fill % FILL_COUNT];

    int col = x / CELL_W, row = y / CELL_H;
    uint8_t c = (uint8_t)s->text[row][col];
    if (c >= FONT_GLYPH_COUNT)
        c = FONT_FALLBACK_CHAR;
    bool on = false;

    if (c)
        on = (font_8x10.glyphs[c * CELL_H + (y % CELL_H)] >> (7 - (x % CELL_W))) & 1;
    if (col == s->text_col && row == s->text_row)
        on = !on; /* solid block text cursor: colours swap in this cell */
    return on ? COLOR_TEXT : COLOR_BACKGROUND;
}
