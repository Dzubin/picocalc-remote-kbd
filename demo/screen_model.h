#pragma once

/*
 * screen_model.h - picocalc-remote-kbd
 *
 * The picture the cursor demo shows: a 40x32 grid of text cells, a block text
 * cursor and an arrow pointer. This file and screen_model.c are plain portable
 * C with no hardware in them, and they are shared by the PicoCalc firmware
 * (which draws the result on the LCD) and the desktop relay (which draws the
 * same picture from the packets the firmware sends), so the two can never
 * look different.
 *
 * Author: Thomas Dzubin
 */

#include <stdint.h>
#include <stdbool.h>

#define SCREEN_W            320
#define SCREEN_H            320

/* Text grid: 8x10 pixel cells (the starter's 8x10 font, by Blair Leduc). */
#define CELL_W              8
#define CELL_H              10

/* The 8x10 font has glyphs for character codes 0 to 127 only; any other code is
 * drawn as FONT_FALLBACK_CHAR (the desktop relay shows whatever bytes arrive). */
#define FONT_GLYPH_COUNT    128
#define FONT_FALLBACK_CHAR  '?'
#define TEXT_COLS           (SCREEN_W / CELL_W)
#define TEXT_ROWS           (SCREEN_H / CELL_H)

/* 16-bit colour, 5-6-5 bits of red, green, blue. */
#define SCR_RGB(r, g, b)    ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

/* Bright, saturated colours only (dim colours are hard to read on the LCD). */
#define COLOR_BACKGROUND    SCR_RGB(0, 0, 200)
#define COLOR_TEXT          SCR_RGB(255, 255, 255)
#define COLOR_OUTLINE       SCR_RGB(0, 0, 0)

/* Arrow fill colour index, which is also what goes over the wire. */
#define FILL_IDLE           0
#define FILL_LEFT           1   /* left button held: yellow */
#define FILL_RIGHT          2   /* right button held: green */
#define FILL_MIDDLE         3   /* middle button held: magenta */
#define FILL_COUNT          4

#define CURSOR_W            12
#define CURSOR_H            19

typedef struct
{
    char text[TEXT_ROWS][TEXT_COLS]; /* 0 = empty cell */
    int text_col;
    int text_row;
    int arrow_x;
    int arrow_y;
    uint8_t fill; /* FILL_* */
} screen_state_t;

/* Colour of the pixel at x, y (0..SCREEN_W-1, 0..SCREEN_H-1): the arrow if it
 * covers that pixel, otherwise the text / block cursor / background. */
uint16_t screen_pixel(const screen_state_t *s, int x, int y);
