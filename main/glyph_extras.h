#ifndef GLYPH_EXTRAS_H
#define GLYPH_EXTRAS_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @file glyph_extras.h
 * @brief Real glyphs for the German umlauts, the sharp s, the degree and the euro sign in the
 * fixed 17x24 bitmap font (build option `glyphs`).
 *
 * The font has ASCII only, so text that carries these characters was transliterated (ae, oe, ue,
 * ss) and everything else dropped. With this option the sanitizer keeps them as single bytes in the
 * range GLYPH_EXTRA_FIRST..GLYPH_EXTRA_LAST - one byte per glyph like all other text the display
 * code handles - and the drawing code asks glyph_extras_compose() for their bitmaps.
 *
 * The umlauts are composed: the base letter of the font itself with two dots above it, so they
 * always match the font. The sharp s, the degree sign and the euro sign are drawn by hand in the
 * same 17x24 cell and stroke width (the euro is a C with two bars).
 *
 * Pure bit twiddling with no ESP-IDF dependency, so the host tests link it.
 */

#define GLYPH_EXTRA_FIRST 0x80
#define GLYPH_EXTRA_LAST 0x88

#define GLYPH_ROWS 24
#define GLYPH_BYTES_PER_ROW 3  // a 17-pixel row, MSB first
#define GLYPH_BYTES (GLYPH_ROWS * GLYPH_BYTES_PER_ROW)

// The codes, in the order of the table in glyph_extras.c.
enum {
    GLYPH_A_UMLAUT = 0x80,          // ä
    GLYPH_O_UMLAUT = 0x81,          // ö
    GLYPH_U_UMLAUT = 0x82,          // ü
    GLYPH_CAPITAL_A_UMLAUT = 0x83,  // Ä
    GLYPH_CAPITAL_O_UMLAUT = 0x84,  // Ö
    GLYPH_CAPITAL_U_UMLAUT = 0x85,  // Ü
    GLYPH_SHARP_S = 0x86,           // ß
    GLYPH_DEGREE = 0x87,            // °
    GLYPH_EURO = 0x88,              // €
};

/** @brief True if the byte is one of the extra glyph codes. */
bool glyph_extras_is_code(uint8_t byte);

/** @brief The code for a Unicode code point (ä ö ü Ä Ö Ü ß ° €), 0 for any other. */
uint8_t glyph_extras_code_for_codepoint(uint32_t codepoint);

/**
 * @brief Writes the bitmap of an extra glyph (GLYPH_ROWS rows of GLYPH_BYTES_PER_ROW bytes, the
 * same layout as a glyph of the font).
 *
 * @param code One of the codes above.
 * @param font_table The font's table: the glyph of a character `c` starts at
 * `(c - ' ') * GLYPH_BYTES` (the letters of the umlauts are taken from it).
 * @param out Receives GLYPH_BYTES bytes.
 * @return False (and `out` cleared) for a byte that is no extra code.
 */
bool glyph_extras_compose(uint8_t code, const uint8_t *font_table, uint8_t out[GLYPH_BYTES]);

#endif
