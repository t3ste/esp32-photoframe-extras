#ifndef RECIPE_FONT_H
#define RECIPE_FONT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "screen_canvas.h"

/**
 * @file recipe_font.h
 * @brief Small proportional bitmap fonts for the recipe page (build option `recipes`).
 *
 * The firmware's own font is one monospace 17x24 bitmap (screen_canvas.h); a recipe has far more
 * text than that gives room for, so the page uses these: Noto Sans at 12-30 pixels, rendered once
 * (scripts/gen_recipe_font.py) into recipe_font_data.c. Text is one byte per character in the
 * Windows-1252 code (recipe_text.h turns UTF-8 into it): ASCII, the Latin-1 letters, the
 * typographic quotes and dashes, the ellipsis, the euro sign and the common fractions.
 *
 * Every size can be drawn at an integer scale (pixels repeated), which is how the page fills the
 * larger panels. Pure C with no ESP-IDF dependency, so the host tests and the render harness link
 * it.
 */

#define RECIPE_FONT_FIRST 0x20
#define RECIPE_FONT_LAST 0xFF

typedef struct {
    uint8_t advance;  // pixels to the next character
    int8_t left;      // from the origin to the first column of the bitmap
    int8_t top;       // from the baseline up to the first row of the bitmap
    uint8_t width;    // the bitmap, one bit per pixel, each row padded to whole bytes
    uint8_t height;
    uint16_t offset;  // of the bitmap in the font's table
} recipe_glyph_t;

typedef struct {
    uint8_t px;  // the size the font was made for
    bool bold;
    uint8_t ascent;                // above the baseline
    uint8_t descent;               // below it
    const recipe_glyph_t *glyphs;  // RECIPE_FONT_FIRST..RECIPE_FONT_LAST
    const uint8_t *bits;
} recipe_font_t;

extern const recipe_font_t RECIPE_FONTS[];
extern const int RECIPE_FONT_COUNT;

/** @brief The font made for exactly this size and weight, NULL if there is none. */
const recipe_font_t *recipe_font_find(int px, bool bold);

/** @brief Height of a line of text without extra leading (ascent + descent), unscaled. */
int recipe_font_line_height(const recipe_font_t *font);

/** @brief Advance of one character (0 for a code outside the table), unscaled. */
int recipe_font_char_width(const recipe_font_t *font, uint8_t code);

/** @brief Width of `len` bytes of text at an integer scale. */
int recipe_font_text_width(const recipe_font_t *font, const char *text, size_t len, int scale);

/**
 * @brief Draws `len` bytes of text with the top of its line at `y` and its start at `x`, every
 * pixel `scale` pixels wide and high. Returns the x after the last character.
 */
int recipe_font_draw(canvas_t *canvas, const recipe_font_t *font, int scale, int x, int y,
                     const char *text, size_t len, canvas_color_t color);

#endif
