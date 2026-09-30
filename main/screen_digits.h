#ifndef SCREEN_DIGITS_H
#define SCREEN_DIGITS_H

#include "screen_canvas.h"

/**
 * @file screen_digits.h
 * @brief Big numbers for the info screens (build option `info-screens`): digits, signs and the
 * degree ring drawn as thick round strokes at any height, so a temperature can fill a quarter of
 * the panel without the stair steps of the 17x24 bitmap font scaled up.
 *
 * Characters: `0`-`9`, `-`, `+`, `.`, `:` and `*` (the degree ring); a space is a gap. Anything
 * else is skipped. Every digit has the same width, like a tabular font. Pure C on top of
 * screen_canvas.h, so the host tests and the render harness link it.
 */

/** @brief Width of a text at a glyph height. */
int canvas_big_width(const char *text, int height);

/**
 * @brief Draws `text` with its top left corner at (x, y), each glyph `height` pixels high; the
 * strokes are about a ninth of the height thick. Returns the x after the last glyph.
 */
int canvas_big_text(canvas_t *canvas, int x, int y, const char *text, int height,
                    canvas_color_t color);

/** @brief The same, centred on `center_x`. */
void canvas_big_text_centered(canvas_t *canvas, int center_x, int y, const char *text, int height,
                              canvas_color_t color);

/**
 * @brief The largest height of 1..`max_height` at which `text` is at most `max_width` wide (1 if
 * even that does not fit).
 */
int canvas_big_fit_height(const char *text, int max_width, int max_height);

#endif
