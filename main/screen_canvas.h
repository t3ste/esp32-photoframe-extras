#ifndef SCREEN_CANVAS_H
#define SCREEN_CANVAS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @file screen_canvas.h
 * @brief Drawing primitives for the info screens (build option `info-screens`): an RGB888 canvas
 * the size of the panel, filled rectangles and frames, discs, ring sectors (donut wheels), pills,
 * and text in the frame's 17x24 bitmap font at an integer scale.
 *
 * Everything clips to the canvas, colours are the panel's palette primaries (an RGB canvas is
 * quantized to the palette of the panel when it is written), and layout is done in *units* so one
 * layout function serves all panels and both orientations: unit = min(width, height) / 40, i.e.
 * 12 px on an 800x480 panel and 35 px on a 1872x1404 one.
 *
 * Text is one byte per glyph like all display text (see glyph_extras.h for the bytes 0x80-0x88 when
 * the `glyphs` option is built in). Pure C with no ESP-IDF dependency besides the font table, so
 * the host tests and the host render harness link it.
 */

typedef struct {
    uint8_t r, g, b;
} canvas_color_t;

typedef struct {
    uint8_t *rgb;  // width * height * 3 bytes, row by row
    int width;
    int height;
} canvas_t;

#define CANVAS_BLACK ((canvas_color_t){0, 0, 0})
#define CANVAS_WHITE ((canvas_color_t){255, 255, 255})
#define CANVAS_RED ((canvas_color_t){255, 0, 0})
#define CANVAS_YELLOW ((canvas_color_t){255, 255, 0})
#define CANVAS_GREEN ((canvas_color_t){0, 255, 0})
#define CANVAS_BLUE ((canvas_color_t){0, 0, 255})

/** @brief The layout unit of a canvas: min(width, height) / 40 (at least 1). */
int canvas_unit(const canvas_t *canvas);

/**
 * @brief Text scale that suits the canvas for text `steps` times the size of the body text: body
 * text (steps = 1) is 1x on an 800x480 panel and 3x on the largest ones.
 */
int canvas_text_scale(const canvas_t *canvas, int steps);

void canvas_fill(canvas_t *canvas, canvas_color_t color);

/** @brief A filled rectangle. */
void canvas_rect(canvas_t *canvas, int x, int y, int w, int h, canvas_color_t color);

/** @brief The outline of a rectangle, `thickness` pixels wide, inside the given box. */
void canvas_frame(canvas_t *canvas, int x, int y, int w, int h, int thickness,
                  canvas_color_t color);

/** @brief A filled circle. */
void canvas_disc(canvas_t *canvas, int cx, int cy, int radius, canvas_color_t color);

/**
 * @brief A sector of a ring (a slice of a donut): between the radii, from `start_deg` to `end_deg`,
 * measured clockwise from 12 o'clock (0 = up, 90 = right). An end angle beyond 360 wraps.
 */
void canvas_ring_sector(canvas_t *canvas, int cx, int cy, int outer_radius, int inner_radius,
                        float start_deg, float end_deg, canvas_color_t color);

/** @brief A rectangle with fully rounded short sides. */
void canvas_pill(canvas_t *canvas, int x, int y, int w, int h, canvas_color_t color);

/** @brief A triangle pointing down, its tip at (tip_x, tip_y) and `size` wide and high. */
void canvas_triangle_down(canvas_t *canvas, int tip_x, int tip_y, int size, canvas_color_t color);

/** @brief A straight stroke with round ends, `radius` pixels either side of the line (at least 1).
 */
void canvas_line(canvas_t *canvas, float x0, float y0, float x1, float y1, int radius,
                 canvas_color_t color);

/** @brief A filled triangle, `size` wide and high, centred on (cx, cy), pointing up or down. */
void canvas_arrow(canvas_t *canvas, int cx, int cy, int size, bool up, canvas_color_t color);

/**
 * @brief A line chart of `count` values, oldest first, in the box (x, y, w, h): a black stroke from
 * the lowest to the highest value (the middle of the box if they are all equal) and a bigger disc
 * in `end_color` at the newest point. Draws nothing for fewer than two values or a box under 4
 * pixels.
 */
void canvas_sparkline(canvas_t *canvas, int x, int y, int w, int h, const float *values, int count,
                      canvas_color_t end_color);

/** @brief Width of a text at a scale: one 17-pixel cell per byte. */
int canvas_text_width(const char *text, int scale);

/** @brief Height of a line of text at a scale (24 pixels per step). */
int canvas_text_height(int scale);

/**
 * @brief Draws text with its top left corner at (x, y): the font's bitmap, every pixel `scale`
 * pixels wide and high. Returns the x after the last glyph.
 */
int canvas_text(canvas_t *canvas, int x, int y, const char *text, int scale, canvas_color_t color);

/** @brief Text centred on `center_x`. */
void canvas_text_centered(canvas_t *canvas, int center_x, int y, const char *text, int scale,
                          canvas_color_t color);

/** @brief Text ending at `right_x`. */
void canvas_text_right(canvas_t *canvas, int right_x, int y, const char *text, int scale,
                       canvas_color_t color);

/**
 * @brief Copies `text` into `out`, cut with a trailing `~` (as the agenda does) if it does not fit
 * `max_width` pixels at that scale.
 */
void canvas_text_fit(const char *text, int max_width, int scale, char *out, size_t out_len);

/**
 * @brief The largest scale of 1..`max_scale` at which `text` fits `max_width` (1 if even that does
 * not).
 */
int canvas_text_fit_scale(const char *text, int max_width, int max_scale);

/**
 * @brief Turns UTF-8 text (what the Web UI stores) into display text, one byte per glyph: ASCII as
 * it is; with the `glyphs` option the umlauts, sharp s, degree and euro sign become their glyph
 * bytes, without it the umlauts become ae/oe/ue (Ae/Oe/Ue, ss); everything else is dropped. The
 * same as the frame's text sanitizer (image_processor_sanitize_ascii()) - a host test keeps the two
 * equal - but usable where the image code is not linked.
 */
void canvas_text_from_utf8(const char *utf8, char *out, size_t out_len);

#define CANVAS_WRAP_LINE_MAX 96

/**
 * @brief Breaks text into lines of at most `max_width` pixels at a scale, at spaces (a word longer
 * than a line is cut). Returns the number of lines written (at most `max_lines`; text that does not
 * fit ends the last line with `~`).
 */
int canvas_text_wrap(const char *text, int max_width, int scale, char lines[][CANVAS_WRAP_LINE_MAX],
                     int max_lines);

/**
 * @brief Draws the first of `texts` that is at most `max_width` wide at a scale, with its top left
 * corner at (x, y); the texts run from the most to the least informative. Returns the index drawn,
 * or -1 if none fits (nothing is drawn).
 */
int canvas_text_first_fit(canvas_t *canvas, int x, int y, int max_width, int scale,
                          canvas_color_t color, const char *const *texts, int count);

/** @brief The width of a header label with its "late" marker (see canvas_header_label()). */
int canvas_header_label_width(const char *label, bool late, int scale);

/**
 * @brief The label at the right end of a header band, ending at `right_x`: in `color`, and when
 * `late` a yellow "!" behind it (the data are older than they should be).
 */
void canvas_header_label(canvas_t *canvas, int right_x, int y, int scale, const char *label,
                         bool late, canvas_color_t color);

#define CANVAS_NOTE_LINES_MAX 3

/**
 * @brief The note at the foot of a page with data from the internet: where the data come from and
 * when they were fetched, on ONE line if they fit (`source - stamp`), else the source on its own
 * line (wrapped to two if need be) and the stamp under it. `sources` are the wordings of the
 * source, the fullest first ("Source: Yahoo Finance", "Yahoo Finance"); `stamp_long` and
 * `stamp_short` the stamp ("Updated 30 Sep 14:35", "30 Sep 14:35"). Of the one-line forms the first
 * that fits wins, trying each source with the long stamp, then the short one. Either part may be
 * missing (NULL or empty): the note is then the other alone. `lines` takes CANVAS_NOTE_LINES_MAX
 * lines; returns how many were written (0 if there is nothing to say).
 */
int canvas_note_lines(const char *const *sources, int source_count, const char *stamp_long,
                      const char *stamp_short, int max_width, int scale,
                      char lines[][CANVAS_WRAP_LINE_MAX]);

#endif
