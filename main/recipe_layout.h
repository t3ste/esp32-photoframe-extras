#ifndef RECIPE_LAYOUT_H
#define RECIPE_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "recipe_font.h"
#include "recipe_qr.h"
#include "recipe_types.h"
#include "screen_canvas.h"

/**
 * @file recipe_layout.h
 * @brief The recipe page (build option `recipes`): where everything goes, and drawing it.
 *
 * A narrow header (title, category and time, the source, small warnings), the ingredients with
 * bullets on the left, the preparation in paragraphs on the right, the picture top right at the
 * height of the headings, and optionally the QR code of the recipe's address bottom right. In
 * landscape the preparation flows in the column beside the picture and below it; in portrait the
 * ingredients and the picture share the top and the preparation runs the full width below them.
 * The text gives way to the picture and the QR code line by line and never overlaps them.
 *
 * The layout takes the biggest of a ladder of font sizes at which everything fits; if even the
 * smallest does not hold it all, it shows as much as fits and ends the text with an ellipsis.
 * Pure C with no ESP-IDF dependency (the host tests and the render harness link it), laid out in
 * design units of an 800x480 panel (480x800 in portrait) that scale with the canvas.
 */

#define RECIPE_LAYOUT_ING_LINES 160
#define RECIPE_LAYOUT_BODY_LINES 140

// Warnings the page can show (a mask); "text cut" is added by the layout itself.
#define RECIPE_WARN_LAST_RECIPE 0x01  // the last recipe fetched, because a new one could not be had
#define RECIPE_WARN_RELAXED 0x02      // found only with fewer filters than were set
#define RECIPE_WARN_NO_IMAGE 0x04     // the picture is missing
#define RECIPE_WARN_NO_NETWORK 0x08   // offline

typedef struct {
    int x, y, w, h;
} recipe_rect_t;

// A line of text: `len` bytes from `start` in the text it belongs to (an ingredient of the
// recipe, or the preparation), at (x, y) = the left end and the top of the line.
typedef struct {
    int16_t x, y;
    int16_t width;  // the room the line had: an ellipsis has to fit in it too
    uint16_t start, len;
    uint8_t ingredient;  // which ingredient (ingredient lines only)
    uint8_t flags;
} recipe_line_t;

#define RECIPE_LINE_ELLIPSIS 0x01  // draw an ellipsis after the text: the text goes on
#define RECIPE_LINE_BULLET 0x02    // the first line of an ingredient: a bullet in front

typedef struct {
    bool show_image;        // reserve the picture's box (false: the text takes its room)
    bool have_photo;        // a picture is there to draw (else the box holds a note)
    const recipe_qr_t *qr;  // the QR code to show, NULL for none
    unsigned warnings;      // RECIPE_WARN_*
} recipe_layout_input_t;

typedef struct {
    int width, height;
    bool landscape;
    int scale;            // pixels of the fonts are repeated this often
    int ing_px, body_px;  // the sizes the ladder gave
    int ing_line_h, body_line_h;
    bool cut;  // not everything fits: the text ends with an ellipsis
    bool ing_cut, body_cut;

    const recipe_font_t *title_font, *meta_font, *head_font, *ing_font, *body_font, *small_font,
        *warn_font;

    // the header
    recipe_line_t title[2];
    int title_count;
    int meta_x, meta_y;
    char meta[RECIPE_CATEGORY_MAX + RECIPE_TIME_MAX + 8];
    int source_right, source_y;
    char source[RECIPE_SOURCE_MAX + 16];
    int warn_count;  // at most two lines in the corner
    char warn[2][40];
    int warn_right, warn_y[2];
    int rule_y, rule_h;

    // the headings of the two columns
    int ing_head_x, ing_head_y, body_head_x, body_head_y;
    const char *ing_label, *body_label;

    // the ingredients and the preparation
    int ing_count;
    recipe_line_t ing[RECIPE_LAYOUT_ING_LINES];
    int body_count;
    recipe_line_t body[RECIPE_LAYOUT_BODY_LINES];
    int bullet_size;

    // the picture and the QR code (w == 0: none)
    recipe_rect_t image, qr;
    bool image_placeholder;
    int qr_module_px;
    int mat;  // the white margin inside the picture's frame
} recipe_layout_t;

/**
 * @brief Lays the recipe out on a canvas of this size and orientation (`landscape` = the layout
 * with the preparation beside the ingredients; false = the layout of a tall canvas).
 *
 * @return true if everything fits in the largest size that does; false if even the smallest size
 * had to cut something (the layout then shows as much as fits).
 */
bool recipe_layout_build(const recipe_t *recipe, const recipe_layout_input_t *input, int width,
                         int height, bool landscape, recipe_layout_t *out);

/**
 * @brief Draws a laid out recipe on the canvas (the canvas is filled white first). `photo` is the
 * decoded picture (RGB888, photo_w x photo_h) or NULL; it is cropped to fill its box and dithered
 * to the palette (`grayscale` panels: sixteen grays).
 */
void recipe_layout_draw(canvas_t *canvas, const recipe_t *recipe, const recipe_layout_t *layout,
                        const recipe_qr_t *qr, const uint8_t *photo, int photo_w, int photo_h,
                        bool grayscale);

/**
 * @brief Draws a picture into a box: cropped to fill it, scaled by averaging, and dithered
 * (Floyd-Steinberg) to the six colours of the palette, or to sixteen grays.
 */
void recipe_draw_photo(canvas_t *canvas, int x, int y, int w, int h, const uint8_t *rgb, int src_w,
                       int src_h, bool grayscale);

#endif
