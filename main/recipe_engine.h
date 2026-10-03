#ifndef RECIPE_ENGINE_H
#define RECIPE_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "recipe_layout.h"
#include "recipe_source.h"
#include "recipe_types.h"

/**
 * @file recipe_engine.h
 * @brief How the recipe page gets its recipe (build option `recipes`): the tries, the choosing and
 * the rules, with the network, the picture decoder, the clock and the random numbers handed in, so
 * the host tests run it with made-up answers. recipe_service.c gives it the real ones.
 *
 * What a user gets, in this order:
 *  - up to three tries with exactly the filters that were set; a try asks the source, takes
 *    candidates in a random order (the recipe of the day: today's first, then the same variant of
 *    earlier days), and the first that has a picture (if pictures are on), fits the page in the
 *    smallest size without being cut, and has not been shown lately is the recipe;
 *  - if the three found nothing the source could give (the filters left no usable recipe), the
 *    filters are relaxed in steps and asked once more each, and the page says so;
 *  - if nothing was found at all (also: no network), the last recipe that was fetched, with a
 *    warning; and none if there never was one.
 */

#define RECIPE_TRIES 3           // with the filters as they are set
#define RECIPE_MAX_CANDIDATES 6  // recipes read in a try before it gives up
#define RECIPE_BUDGET_MS 110000  // after this long no request and no further try is made
#define RECIPE_MAX_REQUESTS 30   // requests in one run, whatever the sources answer
#define RECIPE_HISTORY_KEEP 20   // recipes remembered as shown lately
#define RECIPE_HISTORY_CAP 640   // bytes of that list

typedef struct {
    // A GET: the body (malloc'd, NUL-terminated, the caller frees it), NULL if the server did not
    // answer or answered with an error; `*status` is the HTTP status, 0 if there was none.
    char *(*get)(void *ctx, const char *url, size_t max_bytes, size_t *len, int *status);
    // Decodes a JPEG into RGB888 (malloc'd), at most about `box_w` x `box_h` big; NULL on failure.
    uint8_t *(*decode)(void *ctx, const uint8_t *jpeg, size_t len, int box_w, int box_h, int *w,
                       int *h);
    unsigned (*random)(void *ctx);
    unsigned long (*now_ms)(void *ctx);
    // The last recipe that was shown (and its picture as the bytes that were downloaded; the
    // caller frees *jpeg), and keeping a new one. May be NULL: no last recipe is kept.
    bool (*load_last)(void *ctx, recipe_t *recipe, uint8_t **jpeg, size_t *jpeg_len);
    void (*save_last)(void *ctx, const recipe_t *recipe, const uint8_t *jpeg, size_t jpeg_len);
    void *ctx;
    const char *mealdb_key;  // TheMealDB's key, NULL or "" for the development key
} recipe_env_t;

typedef struct {
    int width, height;  // the canvas the page is drawn on
    bool landscape;
} recipe_canvas_t;

typedef enum {
    RECIPE_RESULT_NONE = 0,  // no recipe at all
    RECIPE_RESULT_NEW,       // a recipe from the source
    RECIPE_RESULT_LAST,      // the last recipe fetched, with a warning
} recipe_result_t;

typedef struct {
    recipe_t *recipe;  // supplied by the caller, filled
    uint8_t *photo;    // the decoded picture (malloc'd, the caller frees), NULL if none
    int photo_w, photo_h;
    unsigned warnings;  // RECIPE_WARN_*
    int tries;          // how many requests series were made
    int stage;          // 0 = with every filter, 1.. = relaxed
    int requests;       // HTTP requests made
} recipe_outcome_t;

/**
 * @brief Gets a recipe for the options. `history` is the list of ids shown lately (updated).
 * @param network_up Without a network no try is made and the last recipe is the answer.
 */
recipe_result_t recipe_engine_run(const recipe_options_t *options, const recipe_canvas_t *canvas,
                                  const recipe_env_t *env, bool network_up, char *history,
                                  size_t history_cap, recipe_outcome_t *out);

#endif
