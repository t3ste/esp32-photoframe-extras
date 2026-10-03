#ifndef RECIPE_SERVICE_H
#define RECIPE_SERVICE_H

#include <stdbool.h>

#include "recipe_engine.h"
#include "recipe_source.h"
#include "recipe_types.h"

/**
 * @file recipe_service.h
 * @brief The recipe page's connection to the device (build option `recipes`): the network, the
 * JPEG decoder, the clock and the dice, and three small files - the last recipe that was shown
 * (with its picture as it was downloaded) and the ids of the recipes shown lately. What it
 * does with them is in recipe_engine.h. The requests are never logged (they hold the search words
 * and the user's key of TheMealDB).
 */

/**
 * @brief Gets the recipe for the page. `out->photo` (if any) is allocated: free() it.
 *
 * @param recipe Receives the recipe (the caller supplies the structure).
 * @param out Receives the outcome; its `recipe` is set to `recipe`.
 * @param width Canvas the page is drawn on (a picture and the size of its box follow).
 * @return RECIPE_RESULT_NEW, RECIPE_RESULT_LAST (the last recipe, with a warning in `out`) or
 * RECIPE_RESULT_NONE.
 */
recipe_result_t recipe_service_load(recipe_t *recipe, recipe_outcome_t *out,
                                    const recipe_options_t *options, bool wifi_connected, int width,
                                    int height, bool landscape);

#endif
