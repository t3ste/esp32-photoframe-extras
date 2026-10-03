#include "recipe_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// What a try came to
typedef enum {
    TRY_FOUND,
    TRY_EMPTY,    // the source answered, but nothing usable was in it
    TRY_NETWORK,  // the source did not answer (or sent something unreadable)
} try_result_t;

typedef struct {
    const recipe_options_t *options;
    const recipe_canvas_t *canvas;
    const recipe_env_t *env;
    char *history;
    size_t history_cap;
    recipe_outcome_t *out;
    recipe_t *candidate;
    unsigned long start_ms;
} engine_t;

// Whether another request may be made: the time of the run is not up and the number of requests
// is not used up. Both are checked before every request, not only between the tries - a source
// that answers slowly (up to two attempts of 15 s a request) must not keep the frame busy for
// minutes inside one try.
static bool may_ask(const engine_t *e)
{
    return e->out->requests < RECIPE_MAX_REQUESTS &&
           e->env->now_ms(e->env->ctx) - e->start_ms <= RECIPE_BUDGET_MS;
}

static char *get(engine_t *e, const char *url, size_t max_bytes, size_t *len)
{
    if (!may_ask(e)) {
        return NULL;  // as if the source had not answered
    }
    int status = 0;
    e->out->requests++;
    return e->env->get(e->env->ctx, url, max_bytes, len, &status);
}

// The language and the place of the QR code: where the recipe's address leads.
static bool make_qr(const recipe_t *recipe, bool wanted, recipe_qr_t *qr)
{
    if (!wanted || !recipe->url[0]) {
        return false;
    }
    return recipe_qr_encode(recipe->url, qr);
}

// Whether the recipe fits the page in its smallest size (with the picture's box and the QR code
// if they are on), and how wide the picture's box is.
static bool fits_page(engine_t *e, const recipe_t *recipe, int *box_w, int *box_h)
{
    recipe_qr_t qr;
    bool have_qr = make_qr(recipe, e->options->qr, &qr);
    recipe_layout_input_t in = {0};
    in.show_image = e->options->image;
    in.have_photo = true;
    in.qr = have_qr ? &qr : NULL;
    recipe_layout_t *layout = malloc(sizeof(*layout));
    if (!layout) {
        return false;
    }
    bool fits = recipe_layout_build(recipe, &in, e->canvas->width, e->canvas->height,
                                    e->canvas->landscape, layout);
    int inset = layout->mat > 2 ? layout->mat / 3 + layout->mat : 1 + layout->mat;
    *box_w = layout->image.w - 2 * inset;
    *box_h = layout->image.h - 2 * inset;
    free(layout);
    return fits;
}

// Takes the picture of a recipe: the bytes and the decoded pixels. False if there is none.
static bool fetch_photo(engine_t *e, const recipe_t *recipe, int box_w, int box_h, uint8_t **jpeg,
                        size_t *jpeg_len, uint8_t **rgb, int *w, int *h)
{
    char url[RECIPE_URL_MAX + 40];
    bool ok;
    if (!e->env->decode) {
        return false;  // no decoder, no picture
    }
    if (recipe->german) {
        ok = recipe_chefkoch_image_url(recipe->image_url, box_w, url, sizeof(url));
    } else {
        ok = recipe_mealdb_image_url(recipe->image_url, box_w, url, sizeof(url));
    }
    if (!ok) {
        return false;
    }
    size_t len = 0;
    char *body = get(e, url, 400 * 1024, &len);
    if (!body || len < 100) {
        free(body);
        return false;
    }
    int pw = 0, ph = 0;
    uint8_t *pixels =
        e->env->decode(e->env->ctx, (const uint8_t *) body, len, box_w, box_h, &pw, &ph);
    if (!pixels || pw < 1 || ph < 1) {
        free(pixels);
        free(body);
        return false;
    }
    *jpeg = (uint8_t *) body;
    *jpeg_len = len;
    *rgb = pixels;
    *w = pw;
    *h = ph;
    return true;
}

// Whether the candidate can be shown; if so it becomes the outcome (with its picture).
static bool accept(engine_t *e, bool use_history)
{
    recipe_t *r = e->candidate;
    if (use_history && recipe_history_has(e->history, r->id)) {
        return false;
    }
    if (e->options->image && !r->image_url[0]) {
        return false;  // no picture: skipped
    }
    int box_w = 0, box_h = 0;
    if (!fits_page(e, r, &box_w, &box_h)) {
        return false;  // too much text: skipped
    }
    uint8_t *jpeg = NULL, *rgb = NULL;
    size_t jpeg_len = 0;
    int pw = 0, ph = 0;
    if (e->options->image && !fetch_photo(e, r, box_w, box_h, &jpeg, &jpeg_len, &rgb, &pw, &ph)) {
        return false;  // the picture cannot be had: skipped
    }
    *e->out->recipe = *r;
    e->out->photo = rgb;
    e->out->photo_w = pw;
    e->out->photo_h = ph;
    if (e->env->save_last) {
        e->env->save_last(e->env->ctx, r, jpeg, jpeg_len);
    }
    free(jpeg);
    if (use_history && r->id[0]) {
        recipe_history_add(e->history, e->history_cap, r->id, RECIPE_HISTORY_KEEP);
    }
    return true;
}

// ---- the sources

static try_result_t try_day(engine_t *e)
{
    size_t len = 0;
    char *page = get(e, RECIPE_DAY_URL, 160 * 1024, &len);
    if (!page) {
        return TRY_NETWORK;
    }
    char ids[12][RECIPE_ID_MAX];
    int count = recipe_parse_chefkoch_day(page, ids, 12);
    free(page);
    if (count == 0) {
        return TRY_NETWORK;  // the page is not what it was: as good as no answer
    }
    // today's recipe of the variant, then the same variant of the days before
    int first = recipe_day_position(e->options->variant);
    int tried = 0;
    for (int position = first; position < count && tried < RECIPE_MAX_CANDIDATES; position += 3) {
        char url[RECIPE_URL_MAX];
        if (!recipe_chefkoch_detail_url(ids[position], url, sizeof(url))) {
            continue;
        }
        tried++;
        char *body = get(e, url, 96 * 1024, &len);
        if (!body) {
            continue;
        }
        bool ok = recipe_parse_chefkoch_recipe(body, "Chefkoch \xE2\x80\x93 Rezept des Tages",
                                               e->candidate);
        free(body);
        if (ok && accept(e, false)) {
            return TRY_FOUND;
        }
    }
    return tried > 0 ? TRY_EMPTY : TRY_NETWORK;
}

static try_result_t try_search(engine_t *e, int stage)
{
    char url[512];
    recipe_candidate_t *found = malloc(sizeof(*found) * RECIPE_SEARCH_PAGE);
    if (!found) {
        return TRY_NETWORK;
    }
    int total = -1;
    int count = 0;
    bool answered = false;
    for (int round = 0; round < 2; round++) {
        int offset = recipe_pick_offset(total, RECIPE_SEARCH_PAGE, e->env->random(e->env->ctx));
        if (!recipe_search_url(e->options, stage, offset, RECIPE_SEARCH_PAGE, url, sizeof(url))) {
            free(found);
            return TRY_NETWORK;
        }
        size_t len = 0;
        char *body = get(e, url, 96 * 1024, &len);
        if (!body) {
            break;
        }
        answered = true;
        count = recipe_parse_chefkoch_search(body, found, RECIPE_SEARCH_PAGE, &total);
        free(body);
        if (count > 0 || total <= 0 || offset == 0) {
            break;  // else: the page was beyond the end of the results - ask again inside them
        }
    }
    if (count == 0) {
        free(found);
        return answered ? TRY_EMPTY : TRY_NETWORK;
    }
    // the usable ones, in a random rotation of the order of the answer
    int usable[RECIPE_SEARCH_PAGE];
    int n = 0;
    for (int i = 0; i < count; i++) {
        const recipe_candidate_t *c = &found[i];
        if (!c->usable || (e->options->image && !c->has_image) ||
            recipe_history_has(e->history, c->id)) {
            continue;
        }
        if (stage < 1 && e->options->property == 1 && c->difficulty > 1) {
            continue;  // "Einfach": the simplest recipes only
        }
        usable[n++] = i;
    }
    try_result_t result = TRY_EMPTY;
    if (n > 0) {
        int start = (int) (e->env->random(e->env->ctx) % (unsigned) n);
        for (int k = 0; k < n && k < RECIPE_MAX_CANDIDATES; k++) {
            const recipe_candidate_t *c = &found[usable[(start + k) % n]];
            if (!recipe_chefkoch_detail_url(c->id, url, sizeof(url))) {
                continue;
            }
            size_t len = 0;
            char *body = get(e, url, 96 * 1024, &len);
            if (!body) {
                continue;
            }
            bool ok = recipe_parse_chefkoch_recipe(body, "Chefkoch", e->candidate);
            free(body);
            if (ok && accept(e, true)) {
                result = TRY_FOUND;
                break;
            }
        }
    }
    free(found);
    return result;
}

static try_result_t try_mealdb(engine_t *e, int stage)
{
    char url[320];
    const char *key = e->env->mealdb_key;
    int category = stage < 1 ? e->options->mealdb_category : 0;
    int tried = 0;
    bool answered = false;
    if (category > 0 && category < RECIPE_MEALDB_CATEGORY_COUNT) {
        recipe_candidate_t *found = malloc(sizeof(*found) * 60);
        if (!found) {
            return TRY_NETWORK;
        }
        recipe_mealdb_list_url(key, RECIPE_MEALDB_CATEGORIES[category], url, sizeof(url));
        size_t len = 0;
        char *body = get(e, url, 64 * 1024, &len);
        int count = 0;
        if (body) {
            answered = true;
            count = recipe_parse_mealdb_list(body, found, 60);
            free(body);
        }
        int usable[60];
        int n = 0;
        for (int i = 0; i < count; i++) {
            if ((!e->options->image || found[i].has_image) &&
                !recipe_history_has(e->history, found[i].id)) {
                usable[n++] = i;
            }
        }
        try_result_t result = answered ? TRY_EMPTY : TRY_NETWORK;
        if (n > 0) {
            int start = (int) (e->env->random(e->env->ctx) % (unsigned) n);
            for (int k = 0; k < n && tried < RECIPE_MAX_CANDIDATES; k++) {
                const recipe_candidate_t *c = &found[usable[(start + k) % n]];
                if (!recipe_mealdb_lookup_url(key, c->id, url, sizeof(url))) {
                    continue;
                }
                tried++;
                body = get(e, url, 32 * 1024, &len);
                if (!body) {
                    continue;
                }
                bool ok = recipe_parse_mealdb_recipe(body, e->candidate);
                free(body);
                if (ok && accept(e, true)) {
                    result = TRY_FOUND;
                    break;
                }
            }
        }
        free(found);
        return result;
    }
    // no category: a random recipe, a few times
    try_result_t result = TRY_NETWORK;
    for (int k = 0; k < RECIPE_MAX_CANDIDATES; k++) {
        recipe_mealdb_random_url(key, url, sizeof(url));
        size_t len = 0;
        char *body = get(e, url, 32 * 1024, &len);
        if (!body) {
            continue;
        }
        result = TRY_EMPTY;
        bool ok = recipe_parse_mealdb_recipe(body, e->candidate);
        free(body);
        if (ok && accept(e, true)) {
            return TRY_FOUND;
        }
    }
    return result;
}

static try_result_t try_once(engine_t *e, int stage)
{
    switch (e->options->source) {
    case RECIPE_SOURCE_SEARCH:
        return try_search(e, stage);
    case RECIPE_SOURCE_MEALDB:
        return try_mealdb(e, stage);
    default:
        return try_day(e);
    }
}

// ---- the run

static void clear_outcome(recipe_outcome_t *out)
{
    out->photo = NULL;
    out->photo_w = out->photo_h = 0;
    out->warnings = 0;
    out->tries = 0;
    out->stage = 0;
    out->requests = 0;
}

recipe_result_t recipe_engine_run(const recipe_options_t *options, const recipe_canvas_t *canvas,
                                  const recipe_env_t *env, bool network_up, char *history,
                                  size_t history_cap, recipe_outcome_t *out)
{
    if (!options || !canvas || !env || !env->get || !env->random || !env->now_ms || !out ||
        !out->recipe) {
        return RECIPE_RESULT_NONE;
    }
    clear_outcome(out);
    memset(out->recipe, 0, sizeof(*out->recipe));
    recipe_t *candidate = malloc(sizeof(*candidate));
    if (!candidate) {
        return RECIPE_RESULT_NONE;
    }
    engine_t e = {options,     canvas, env,       history,
                  history_cap, out,    candidate, env->now_ms(env->ctx)};

    bool server_answered = false;  // some try reached the source and found nothing usable
    if (network_up) {
        // tries with the filters as they are, then one try each with fewer filters (only if the
        // source answered: with the network down relaxing the filters would not help)
        int last_stage = RECIPE_FILTER_STAGES - 1;
        int attempts = RECIPE_TRIES + last_stage;
        for (int a = 0; a < attempts; a++) {
            int stage = a < RECIPE_TRIES ? 0 : a - RECIPE_TRIES + 1;
            if (stage > 0 && !server_answered) {
                break;
            }
            if (options->source == RECIPE_SOURCE_DAY && stage > 0) {
                break;  // the recipe of the day has no filters to relax
            }
            if (env->now_ms(env->ctx) - e.start_ms > RECIPE_BUDGET_MS) {
                break;
            }
            out->tries++;
            try_result_t result = try_once(&e, stage);
            if (result == TRY_FOUND) {
                out->stage = stage;
                if (stage > 0) {
                    out->warnings |= RECIPE_WARN_RELAXED;
                }
                free(candidate);
                return RECIPE_RESULT_NEW;
            }
            if (result == TRY_EMPTY) {
                server_answered = true;
            }
        }
    }
    free(candidate);

    // nothing new: the last recipe, if there is one
    memset(out->recipe, 0, sizeof(*out->recipe));
    if (env->load_last) {
        uint8_t *jpeg = NULL;
        size_t jpeg_len = 0;
        if (env->load_last(env->ctx, out->recipe, &jpeg, &jpeg_len)) {
            out->warnings |= RECIPE_WARN_LAST_RECIPE;
            if (!network_up) {
                out->warnings |= RECIPE_WARN_NO_NETWORK;
            }
            if (options->image && env->decode && jpeg && jpeg_len > 0) {
                // a picture of its own size: the same box the page would have
                recipe_t *r = out->recipe;
                int box_w = 0, box_h = 0;
                engine_t shown = e;
                shown.candidate = r;
                fits_page(&shown, r, &box_w, &box_h);
                out->photo = env->decode(env->ctx, jpeg, jpeg_len, box_w, box_h, &out->photo_w,
                                         &out->photo_h);
            }
            free(jpeg);
            if (options->image && !out->photo) {
                out->warnings |= RECIPE_WARN_NO_IMAGE;
            }
            return RECIPE_RESULT_LAST;
        }
    }
    return RECIPE_RESULT_NONE;
}
