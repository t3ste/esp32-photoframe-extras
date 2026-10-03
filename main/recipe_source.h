#ifndef RECIPE_SOURCE_H
#define RECIPE_SOURCE_H

#include <stdbool.h>
#include <stddef.h>

#include "recipe_types.h"

/**
 * @file recipe_source.h
 * @brief The sources of the recipe page (build option `recipes`): what the user can choose, the
 * requests, and the readers of the answers of Chefkoch (the recipe of the day, the search) and
 * TheMealDB. Pure C on cJSON, no ESP-IDF dependency, so the host tests link it; the fetching and
 * the choosing among candidates are in recipe_service.c.
 *
 * Chefkoch has no official API: the day page is read for the JSON-LD list its recipes are in, the
 * search and the recipes themselves come from api.chefkoch.de/v2, which is what the website uses.
 * The search reaches only the first RECIPE_SEARCH_DEPTH results of an order (the server answers
 * an offset beyond that with nothing or an error), so "all recipes" is a random one of the first
 * thousand.
 */

// ---------------------------------------------------------------------------------------------
// What can be chosen. Index 0 of every list is "no filter". The names are what the settings and
// the web UI call them (and, for Chefkoch, the words the search is given).
// ---------------------------------------------------------------------------------------------

typedef enum {
    RECIPE_SOURCE_DAY = 0,     // Chefkoch, the recipe of the day
    RECIPE_SOURCE_SEARCH = 1,  // Chefkoch, search with filters
    RECIPE_SOURCE_MEALDB = 2,  // TheMealDB, by category
    RECIPE_SOURCE_COUNT
} recipe_source_t;

#define RECIPE_VARIANT_COUNT 3
#define RECIPE_PROPERTY_COUNT 5
#define RECIPE_HEALTH_COUNT 9
#define RECIPE_CATEGORY_COUNT 14
#define RECIPE_COUNTRY_COUNT 11
#define RECIPE_MEAL_COUNT 7
#define RECIPE_TIME_COUNT 5
#define RECIPE_RATING_COUNT 5
#define RECIPE_SORT_COUNT 3
#define RECIPE_MEALDB_CATEGORY_COUNT 15

extern const char *const RECIPE_SOURCE_NAMES[RECIPE_SOURCE_COUNT];  // "day", "search", "mealdb"
extern const char *const RECIPE_VARIANTS[RECIPE_VARIANT_COUNT];     // classic, vegetarian, vegan
extern const char *const RECIPE_PROPERTIES[RECIPE_PROPERTY_COUNT];
extern const char *const RECIPE_HEALTH[RECIPE_HEALTH_COUNT];
extern const char *const RECIPE_CATEGORIES[RECIPE_CATEGORY_COUNT];
extern const char *const RECIPE_COUNTRIES[RECIPE_COUNTRY_COUNT];
extern const char *const RECIPE_MEALS[RECIPE_MEAL_COUNT];
extern const char *const RECIPE_SORTS[RECIPE_SORT_COUNT];  // "recommended", "rating", "newest"
extern const char *const RECIPE_MEALDB_CATEGORIES[RECIPE_MEALDB_CATEGORY_COUNT];
extern const int RECIPE_MAX_MINUTES[RECIPE_TIME_COUNT];    // 0 = any, 15, 30, 60, 120
extern const int RECIPE_MIN_RATINGS[RECIPE_RATING_COUNT];  // in tenths: 0 = any, 20, 30, 40, 45

/** @brief The index of a name in a list (compared without regard to case), -1 if it is not there.
 */
int recipe_list_index(const char *const *list, int count, const char *name);

#define RECIPE_QUERY_MAX 48

typedef struct {
    int source;   // recipe_source_t
    int variant;  // of the recipe of the day
    char query[RECIPE_QUERY_MAX];
    int property;  // the other lists: an index, 0 = no filter
    int health;
    int category;
    int country;
    int meal;
    int max_time;    // index into RECIPE_MAX_MINUTES
    int min_rating;  // index into RECIPE_MIN_RATINGS
    int sort;
    int mealdb_category;
    bool image;  // show the picture (recipes without one are skipped)
    bool qr;     // a QR code of the recipe's address
} recipe_options_t;

/** @brief The options as they are until the user chooses: the recipe of the day, no filter. */
void recipe_options_defaults(recipe_options_t *options);

/**
 * @brief Brings every field into its range (an unknown source or list entry becomes the default,
 * "no filter") and cleans the search text: control characters out, one space between words, at
 * most RECIPE_QUERY_MAX - 1 bytes, no white space at the ends.
 */
void recipe_options_sanitize(recipe_options_t *options);

/** @brief Length of the longest text recipe_options_pack() writes, with its terminator. */
#define RECIPE_OPTIONS_TEXT_MAX 160

/**
 * @brief The options as a short text for the settings memory: "src=1;var=0;pro=0;hea=0;cat=8;
 * cty=0;mea=1;tim=0;rat=0;srt=0;mdb=0;img=1;qr=0;q=Suppe". The search text comes last and
 * takes the rest of the line, so it needs no escaping.
 * @return the length written, 0 if `out` is too small.
 */
size_t recipe_options_pack(const recipe_options_t *options, char *out, size_t out_len);

/**
 * @brief Reads that text back into `options` (set to the defaults first): a field that is missing
 * or unknown is ignored, one out of range is the default.
 */
void recipe_options_unpack(const char *text, recipe_options_t *options);

// ---------------------------------------------------------------------------------------------
// Requests
// ---------------------------------------------------------------------------------------------

#define RECIPE_SEARCH_DEPTH 1000  // the server's reach: offset + limit
#define RECIPE_SEARCH_PAGE 20     // how many results one request asks for
#define RECIPE_FILTER_STAGES 4    // 0 = all filters ... 3 = none (see recipe_search_url())

#define RECIPE_DAY_URL "https://www.chefkoch.de/rezept-des-tages/"

/**
 * @brief The request of the Chefkoch search. Stage 0 uses every filter; each later stage drops
 * some, so that a request that found nothing can be asked again: 1 drops the type of meal, the diet
 * and the property words, 2 drops the category and the country too, 3 asks with no filter at all
 * (only the order). The words of a filter are added to the search text (the API has no documented
 * parameter for them); the longest time and the lowest rating are real parameters.
 *
 * @param offset The first result (0 .. RECIPE_SEARCH_DEPTH - limit).
 * @return false if the request did not fit `out`.
 */
bool recipe_search_url(const recipe_options_t *options, int stage, int offset, int limit, char *out,
                       size_t out_len);

/** @brief The request for one recipe of Chefkoch. */
bool recipe_chefkoch_detail_url(const char *id, char *out, size_t out_len);

/**
 * @brief The address of a picture from a template with "<format>" in it, with the crop that
 * suits a box `width` pixels wide (240x160, 360x240 or 642x428). An address without the
 * placeholder is copied as it is.
 */
bool recipe_chefkoch_image_url(const char *image_template, int width, char *out, size_t out_len);

/** @brief The address the QR code of a Chefkoch recipe points to: the short form without a name. */
bool recipe_chefkoch_short_url(const char *id, char *out, size_t out_len);

#define RECIPE_MEALDB_DEFAULT_KEY "1"

/** @brief TheMealDB's list of a category ("Seafood"), a recipe by id, or a random one. */
bool recipe_mealdb_list_url(const char *key, const char *category, char *out, size_t out_len);
bool recipe_mealdb_lookup_url(const char *key, const char *id, char *out, size_t out_len);
bool recipe_mealdb_random_url(const char *key, char *out, size_t out_len);

/** @brief TheMealDB's picture in the size that suits a box `width` pixels wide (200 or 350). */
bool recipe_mealdb_image_url(const char *thumb, int width, char *out, size_t out_len);

/** @brief The address of a recipe on TheMealDB's site, for the QR code. */
bool recipe_mealdb_short_url(const char *id, char *out, size_t out_len);

/** @brief Whether a key of TheMealDB is well formed: 1 to 24 letters and digits. */
bool recipe_mealdb_key_valid(const char *key);

// ---------------------------------------------------------------------------------------------
// Answers
// ---------------------------------------------------------------------------------------------

typedef struct {
    char id[RECIPE_ID_MAX];
    char title[96];
    int minutes;        // the preparation time (0 if unknown)
    int rating_tenths;  // 0 if the recipe has none yet
    int difficulty;     // 1 simple, 2 normal, 3 demanding; 0 unknown
    bool has_image;
    bool usable;  // not premium, not rejected
} recipe_candidate_t;

/**
 * @brief Whether an address is one the frame may fetch a picture from or show as a QR code: https
 * only, a host name with a dot (no IP address, no "localhost", none of the private-network
 * endings .local .lan .internal ...), no user name and password, and no port but 443. The
 * addresses come out of the answers of the sources; an answer that was tampered with must not
 * make the frame ask a device of the home network.
 */
bool recipe_https_url_ok(const char *url);

/** The largest side of a picture that is decoded (the real ones are 200-642 pixels). */
#define RECIPE_IMAGE_MAX_DIM 4096

/** @brief Whether the sides of a JPEG as its header gives them are within RECIPE_IMAGE_MAX_DIM. */
bool recipe_image_dims_ok(int width, int height);

/**
 * @brief Whether the output size esp_jpeg reported matches the sides of the picture: it multiplies
 * in 32 bit and a header of 40000 x 35792 pixels comes out as 72 704 bytes instead of 4.3 GB, so a
 * decoder given that buffer would write far past it. `shift` is the number of halvings of the
 * scale.
 */
bool recipe_image_output_ok(int width, int height, int shift, size_t output_len);

/** How deeply nested an answer of a source may be (the real ones go 6-8 levels deep). */
#define RECIPE_JSON_MAX_DEPTH 32

/**
 * @brief Whether the JSON text is nested no deeper than `max_depth` (brackets inside strings do
 * not count). cJSON parses recursively and needs about 64 bytes of stack for each level on the
 * device, while the build allows a thousand levels: an answer with thousands of '[' would overflow
 * the 16 KB stack of the task that parses it. Every parser of this file asks first, in one linear
 * pass without recursion.
 */
bool recipe_json_depth_ok(const char *json, int max_depth);

/**
 * @brief The results of a Chefkoch search, one object at a time (so a long answer needs little
 * memory, and one that was cut off still gives what came before the cut).
 *
 * @param total Receives the number of recipes the search found (all of them, not only these), -1
 * if the answer does not say.
 * @return How many candidates were written (at most `max`).
 */
int recipe_parse_chefkoch_search(const char *json, recipe_candidate_t *out, int max, int *total);

/**
 * @brief The recipes of the day from the day page (HTML): the ids of the list in its JSON-LD in
 * the order of the page. The page lists the recipes of several days, three a day, the newest day
 * first: classic, vegetarian, vegan. Returns how many ids were written, 0 if the list is not in
 * the text (also if it was cut off before its end).
 */
int recipe_parse_chefkoch_day(const char *html, char ids[][RECIPE_ID_MAX], int max);

/** @brief The id for a variant of the recipe of the day: position 0, 1 or 2 of the list. */
int recipe_day_position(int variant);

/**
 * @brief Reads a recipe of Chefkoch (the answer of api.chefkoch.de/v2/recipes/<id>) into `out`
 * (all of it is overwritten). `source_label` is what the page names as the source. The picture
 * address is the template with "<format>" (see recipe_chefkoch_image_url()).
 *
 * @return false if the answer is no recipe or has nothing to show (no title, no ingredients, no
 * preparation, a premium recipe).
 */
bool recipe_parse_chefkoch_recipe(const char *json, const char *source_label, recipe_t *out);

/** @brief The list of a category of TheMealDB (filter.php): ids and titles. Returns the count. */
int recipe_parse_mealdb_list(const char *json, recipe_candidate_t *out, int max);

/** @brief A recipe of TheMealDB (lookup.php or random.php), the first of "meals". */
bool recipe_parse_mealdb_recipe(const char *json, recipe_t *out);

// ---------------------------------------------------------------------------------------------
// Choosing
// ---------------------------------------------------------------------------------------------

/**
 * @brief A first result for a search page of `limit` results out of `total`, taken from `random`
 * (any number): the page lies inside the reach of the search and inside the results there are.
 */
int recipe_pick_offset(int total, int limit, unsigned random);

/**
 * @brief Whether an id is in the list of recipes shown lately (ids one per line, the newest last).
 */
bool recipe_history_has(const char *history, const char *id);

/**
 * @brief Adds an id to the list (at the end) and keeps the newest `keep` of them. `history` is
 * rewritten in place (capacity `cap`).
 */
void recipe_history_add(char *history, size_t cap, const char *id, int keep);

#endif
