#ifndef SCREEN_RECIPE_H
#define SCREEN_RECIPE_H

#include <stdbool.h>
#include <stdint.h>

#include "info_screens_core.h"
#include "recipe_source.h"
#include "recipe_types.h"
#include "screen_canvas.h"

/**
 * @file screen_recipe.h
 * @brief The recipe page as an information page (build option `recipes`): the recipe laid out by
 * recipe_layout.h, or - when there is no recipe at all - a short message in the firmware's own
 * font like the other pages have. Pure drawing with no ESP-IDF dependency, so the host tests and
 * the render harness link it.
 */

typedef enum {
    RECIPE_SCREEN_OK = 0,
    RECIPE_SCREEN_NO_NETWORK,  // no recipe, and the frame is offline
    RECIPE_SCREEN_NO_RECIPE,   // no recipe could be had, and there never was a last one
} recipe_screen_status_t;

typedef struct {
    recipe_screen_status_t status;
    const recipe_t *recipe;  // for OK
    recipe_options_t options;
    unsigned warnings;     // RECIPE_WARN_*
    const uint8_t *photo;  // the decoded picture (RGB888) or NULL
    int photo_w, photo_h;
    bool grayscale;  // a panel of sixteen grays
} recipe_screen_data_t;

/**
 * @brief Draws the page on the canvas; `landscape` says which layout (a canvas that is as tall as
 * wide uses the landscape one). `now` gives the language of the message page.
 */
void recipe_screen_render(canvas_t *canvas, bool landscape, const info_now_t *now,
                          const recipe_screen_data_t *data);

#endif
