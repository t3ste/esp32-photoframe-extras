#ifndef TEXT_OUTLINE_H
#define TEXT_OUTLINE_H

/**
 * @file text_outline.h
 * @brief The look of the overlay and caption text (build option `text-outline`): the firmware's
 * plain bar across the picture, or the text alone - white with a black border or black with a
 * white border - with a border of one or two pixels. Pure C (no ESP-IDF), so the host tests link
 * it; the drawing itself is image_processor.c's. The types are always declared, the functions
 * exist only in a build with the option.
 */

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TEXT_STYLE_BAR = 0,    // the bar across the picture (black or white, see the invert setting)
    TEXT_STYLE_WHITE = 1,  // white text with a black border, no bar
    TEXT_STYLE_BLACK = 2,  // black text with a white border, no bar
} text_style_t;

#define TEXT_STYLE_COUNT 3
#define TEXT_BORDER_MIN 1
#define TEXT_BORDER_MAX 2
#define TEXT_BORDER_DEFAULT 2
/** Room for the offsets of the widest border (the 5 x 5 square without its middle and corners). */
#define TEXT_BORDER_MAX_OFFSETS 24

/** What to draw: the style and the width of its border (ignored by the bar). */
typedef struct {
    text_style_t style;
    int border_px;
} text_look_t;

/** @brief "bar", "white" or "black" (the API's names); anything out of range is "bar". */
const char *text_style_name(text_style_t style);

/** @brief The style of an API name, or -1 when the name is not one of the three. */
int text_style_from_name(const char *name);

/** @brief The style of a stored number; anything out of range is the bar. */
text_style_t text_style_clamp(int value);

/** @brief The border width brought into [TEXT_BORDER_MIN, TEXT_BORDER_MAX]. */
int text_border_clamp(int px);

/** @brief True when the text itself is white (and so the border black). */
bool text_style_ink_white(text_style_t style);

/**
 * @brief The offsets (dx, dy) at which the text is drawn in the border colour before it is drawn
 * in its own: every point within the border width that is at most dx*dx + dy*dy <= r*r + 1
 * away, without (0, 0) - one pixel gives the 8 neighbours, two pixels the 5 x 5 square without
 * its four corners (20 points), which keeps the corners of the border round. Writes at most `max`
 * of them and returns how many.
 */
int text_border_offsets(int border_px, int8_t offsets[][2], int max);

#endif
