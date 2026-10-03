#include "recipe_source.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "json_scan.h"
#include "recipe_text.h"

// ---------------------------------------------------------------------------------------------
// The lists
// ---------------------------------------------------------------------------------------------

const char *const RECIPE_SOURCE_NAMES[RECIPE_SOURCE_COUNT] = {"day", "search", "mealdb"};
const char *const RECIPE_VARIANTS[RECIPE_VARIANT_COUNT] = {"classic", "vegetarian", "vegan"};
const char *const RECIPE_PROPERTIES[RECIPE_PROPERTY_COUNT] = {"", "Einfach", "Schnell",
                                                              "Basisrezepte", "Preiswert"};
const char *const RECIPE_HEALTH[RECIPE_HEALTH_COUNT] = {
    "",        "Vegetarisch", "Vegan",   "Kalorienarm", "Low Carb",
    "Ketogen", "Paleo",       "Fettarm", "Vollwert"};
const char *const RECIPE_CATEGORIES[RECIPE_CATEGORY_COUNT] = {
    "",      "Auflauf",           "Pizza",     "Salat",  "Tarte", "Fingerfood", "Dips", "Saucen",
    "Suppe", "Brot und Brötchen", "Süßspeise", "Kuchen", "Torte", "Getränke"};
const char *const RECIPE_COUNTRIES[RECIPE_COUNTRY_COUNT] = {
    "",       "Deutschland", "Italien", "Spanien", "Frankreich", "Griechenland",
    "Türkei", "Asien",       "Indien",  "Japan",   "Mexiko"};
const char *const RECIPE_MEALS[RECIPE_MEAL_COUNT] = {
    "", "Hauptspeise", "Vorspeise", "Beilage", "Dessert", "Snack", "Frühstück"};
const char *const RECIPE_SORTS[RECIPE_SORT_COUNT] = {"recommended", "rating", "newest"};
const char *const RECIPE_MEALDB_CATEGORIES[RECIPE_MEALDB_CATEGORY_COUNT] = {
    "",      "Beef", "Breakfast", "Chicken", "Dessert", "Goat",  "Lamb",      "Miscellaneous",
    "Pasta", "Pork", "Seafood",   "Side",    "Starter", "Vegan", "Vegetarian"};
const int RECIPE_MAX_MINUTES[RECIPE_TIME_COUNT] = {0, 15, 30, 60, 120};
const int RECIPE_MIN_RATINGS[RECIPE_RATING_COUNT] = {0, 20, 30, 40, 45};

int recipe_list_index(const char *const *list, int count, const char *name)
{
    if (!name) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (strcasecmp(list[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

void recipe_options_defaults(recipe_options_t *options)
{
    memset(options, 0, sizeof(*options));
    options->source = RECIPE_SOURCE_DAY;
    options->image = true;
}

// ---------------------------------------------------------------------------------------------
// Requests
// ---------------------------------------------------------------------------------------------

typedef struct {
    char *out;
    size_t cap;
    size_t len;
    bool overflow;
} out_t;

static void out_raw(out_t *o, const char *text)
{
    size_t n = strlen(text);
    if (o->len + n + 1 > o->cap) {
        o->overflow = true;
        return;
    }
    memcpy(o->out + o->len, text, n + 1);
    o->len += n;
}

// Percent-encodes UTF-8 text for the query of a URL (a space as %20).
static void out_encoded(out_t *o, const char *text)
{
    static const char HEX[] = "0123456789ABCDEF";
    for (const unsigned char *p = (const unsigned char *) text; *p; p++) {
        char piece[4];
        if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') {
            piece[0] = (char) *p;
            piece[1] = '\0';
        } else {
            piece[0] = '%';
            piece[1] = HEX[*p >> 4];
            piece[2] = HEX[*p & 15];
            piece[3] = '\0';
        }
        out_raw(o, piece);
    }
}

static void out_int(out_t *o, int value)
{
    char text[16];
    snprintf(text, sizeof(text), "%d", value);
    out_raw(o, text);
}

static void add_word(char *words, size_t cap, const char *word)
{
    if (!word || !word[0]) {
        return;
    }
    size_t len = strlen(words);
    if (len + strlen(word) + 2 > cap) {
        return;
    }
    if (len > 0) {
        words[len++] = ' ';
    }
    strcpy(words + len, word);
}

bool recipe_search_url(const recipe_options_t *options, int stage, int offset, int limit, char *out,
                       size_t out_len)
{
    if (!options || !out || out_len == 0) {
        return false;
    }
    int sort = options->sort >= 0 && options->sort < RECIPE_SORT_COUNT ? options->sort : 0;
    char words[160] = {0};
    if (stage < 3) {
        add_word(words, sizeof(words), options->query);
        if (stage < 2) {
            if (options->category > 0 && options->category < RECIPE_CATEGORY_COUNT) {
                add_word(words, sizeof(words), RECIPE_CATEGORIES[options->category]);
            }
            if (options->country > 0 && options->country < RECIPE_COUNTRY_COUNT) {
                add_word(words, sizeof(words), RECIPE_COUNTRIES[options->country]);
            }
        }
        if (stage < 1) {
            if (options->meal > 0 && options->meal < RECIPE_MEAL_COUNT) {
                add_word(words, sizeof(words), RECIPE_MEALS[options->meal]);
            }
            if (options->health > 0 && options->health < RECIPE_HEALTH_COUNT) {
                add_word(words, sizeof(words), RECIPE_HEALTH[options->health]);
            }
            // "Einfach" and "Schnell" are no words but a difficulty and a time (the readers
            // and the maximum time below take care of them)
            if (options->property >= 3 && options->property < RECIPE_PROPERTY_COUNT) {
                add_word(words, sizeof(words), RECIPE_PROPERTIES[options->property]);
            }
        }
    }
    out_t o = {.out = out, .cap = out_len, .len = 0, .overflow = false};
    out[0] = '\0';
    out_raw(&o, "https://api.chefkoch.de/v2/recipes?query=");
    out_encoded(&o, words);
    out_raw(&o, "&limit=");
    out_int(&o, limit);
    out_raw(&o, "&offset=");
    out_int(&o, offset);
    out_raw(&o, "&orderBy=");
    out_int(&o, sort == 1 ? 3 : (sort == 2 ? 1 : 2));
    if (stage < 3) {
        int minutes = 0;
        if (options->max_time > 0 && options->max_time < RECIPE_TIME_COUNT) {
            minutes = RECIPE_MAX_MINUTES[options->max_time];
        }
        if (stage < 1 && options->property == 2 && (minutes == 0 || minutes > 30)) {
            minutes = 30;  // "Schnell"
        }
        if (minutes > 0) {
            out_raw(&o, "&maximumTime=");
            out_int(&o, minutes);
        }
        if (options->min_rating > 0 && options->min_rating < RECIPE_RATING_COUNT) {
            int tenths = RECIPE_MIN_RATINGS[options->min_rating];
            out_raw(&o, "&minimumRating=");
            out_int(&o, tenths / 10);
            if (tenths % 10) {
                out_raw(&o, ".");
                out_int(&o, tenths % 10);
            }
        }
    }
    return !o.overflow;
}

static bool id_valid(const char *id)
{
    if (!id || !id[0]) {
        return false;
    }
    for (const char *p = id; *p; p++) {
        if (!isalnum((unsigned char) *p)) {
            return false;
        }
    }
    return strlen(id) < RECIPE_ID_MAX;
}

static bool put_url(char *out, size_t out_len, const char *a, const char *b, const char *c)
{
    out_t o = {.out = out, .cap = out_len, .len = 0, .overflow = false};
    if (out_len == 0) {
        return false;
    }
    out[0] = '\0';
    out_raw(&o, a);
    out_raw(&o, b);
    out_raw(&o, c);
    return !o.overflow;
}

bool recipe_chefkoch_detail_url(const char *id, char *out, size_t out_len)
{
    return id_valid(id) && put_url(out, out_len, "https://api.chefkoch.de/v2/recipes/", id, "");
}

bool recipe_chefkoch_short_url(const char *id, char *out, size_t out_len)
{
    return id_valid(id) && put_url(out, out_len, "https://www.chefkoch.de/rezepte/", id, "/");
}

bool recipe_chefkoch_image_url(const char *image_template, int width, char *out, size_t out_len)
{
    if (!image_template || !out || out_len == 0) {
        return false;
    }
    const char *crop =
        width <= 240 ? "crop-240x160" : (width <= 360 ? "crop-360x240" : "crop-642x428");
    const char *marker = strstr(image_template, "<format>");
    if (!marker) {
        return put_url(out, out_len, image_template, "", "");
    }
    out_t o = {.out = out, .cap = out_len, .len = 0, .overflow = false};
    out[0] = '\0';
    char head[RECIPE_URL_MAX];
    size_t head_len = (size_t) (marker - image_template);
    if (head_len >= sizeof(head)) {
        return false;
    }
    memcpy(head, image_template, head_len);
    head[head_len] = '\0';
    out_raw(&o, head);
    out_raw(&o, crop);
    out_raw(&o, marker + strlen("<format>"));
    return !o.overflow;
}

bool recipe_mealdb_key_valid(const char *key)
{
    if (!key || !key[0] || strlen(key) > 24) {
        return false;
    }
    for (const char *p = key; *p; p++) {
        if (!isalnum((unsigned char) *p)) {
            return false;
        }
    }
    return true;
}

static bool mealdb_url(const char *key, const char *tail, const char *value, char *out,
                       size_t out_len)
{
    if (!out || out_len == 0) {
        return false;
    }
    out_t o = {.out = out, .cap = out_len, .len = 0, .overflow = false};
    out[0] = '\0';
    out_raw(&o, "https://www.themealdb.com/api/json/v1/");
    out_raw(&o, recipe_mealdb_key_valid(key) ? key : RECIPE_MEALDB_DEFAULT_KEY);
    out_raw(&o, "/");
    out_raw(&o, tail);
    if (value) {
        out_encoded(&o, value);
    }
    return !o.overflow;
}

bool recipe_mealdb_list_url(const char *key, const char *category, char *out, size_t out_len)
{
    return category && category[0] && mealdb_url(key, "filter.php?c=", category, out, out_len);
}

bool recipe_mealdb_lookup_url(const char *key, const char *id, char *out, size_t out_len)
{
    return id_valid(id) && mealdb_url(key, "lookup.php?i=", id, out, out_len);
}

bool recipe_mealdb_random_url(const char *key, char *out, size_t out_len)
{
    return mealdb_url(key, "random.php", NULL, out, out_len);
}

bool recipe_mealdb_image_url(const char *thumb, int width, char *out, size_t out_len)
{
    if (!thumb || !thumb[0]) {
        return false;
    }
    return put_url(out, out_len, thumb, width <= 200 ? "/small" : "/medium", "");
}

bool recipe_mealdb_short_url(const char *id, char *out, size_t out_len)
{
    return id_valid(id) && put_url(out, out_len, "https://www.themealdb.com/meal/", id, "");
}

// ---------------------------------------------------------------------------------------------
// Reading the answers
// ---------------------------------------------------------------------------------------------

static const char *str_of(const cJSON *object, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : "";
}

static double num_of(const cJSON *object, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsNumber(item) ? item->valuedouble : 0;
}

static bool bool_of(const cJSON *object, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsTrue(item);
}

static void copy_clean(char *out, size_t cap, const char *utf8)
{
    recipe_text_clean(utf8, out, cap, NULL);
}

// An id from the JSON: Chefkoch writes it as a string, TheMealDB too, but a number is tolerated.
static void id_of(const cJSON *object, const char *key, char *out, size_t cap)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    out[0] = '\0';
    if (cJSON_IsString(item) && item->valuestring) {
        snprintf(out, cap, "%s", item->valuestring);
    } else if (cJSON_IsNumber(item)) {
        snprintf(out, cap, "%.0f", item->valuedouble);
    }
    if (!id_valid(out)) {
        out[0] = '\0';
    }
}

int recipe_parse_chefkoch_search(const char *json, recipe_candidate_t *out, int max, int *total)
{
    if (total) {
        *total = -1;
    }
    if (!json || !out || max <= 0) {
        return 0;
    }
    const char *results = strstr(json, "\"results\"");
    const char *count_key = strstr(json, "\"count\"");
    if (total && count_key && (!results || count_key < results)) {
        const char *colon = strchr(count_key, ':');
        if (colon) {
            *total = atoi(colon + 1);
        }
    }
    if (!results) {
        return 0;
    }
    const char *p = strchr(results, '[');
    if (!p) {
        return 0;
    }
    p++;
    int count = 0;
    while (count < max) {
        p = json_skip_blanks(p);
        if (*p == ',') {
            p = json_skip_blanks(p + 1);
        }
        if (*p != '{') {
            break;
        }
        const char *end = json_object_end(p);
        if (!end) {
            break;  // cut off in the middle of a result: what came before is enough
        }
        size_t len = (size_t) (end - p) + 1;
        char *slice = malloc(len + 1);
        if (!slice) {
            break;
        }
        memcpy(slice, p, len);
        slice[len] = '\0';
        p = end + 1;
        cJSON *item = cJSON_Parse(slice);
        free(slice);
        if (!item) {
            continue;
        }
        const cJSON *recipe = cJSON_GetObjectItemCaseSensitive(item, "recipe");
        if (!cJSON_IsObject(recipe)) {
            recipe = item;
        }
        recipe_candidate_t *candidate = &out[count];
        memset(candidate, 0, sizeof(*candidate));
        id_of(recipe, "id", candidate->id, sizeof(candidate->id));
        if (candidate->id[0]) {
            copy_clean(candidate->title, sizeof(candidate->title), str_of(recipe, "title"));
            candidate->minutes = (int) num_of(recipe, "preparationTime");
            const cJSON *rating = cJSON_GetObjectItemCaseSensitive(recipe, "rating");
            candidate->rating_tenths = (int) (num_of(rating, "rating") * 10 + 0.5);
            candidate->difficulty = (int) num_of(recipe, "difficulty");
            candidate->has_image = bool_of(recipe, "hasImage");
            candidate->usable = !bool_of(recipe, "isPremium") && !bool_of(recipe, "isPlus") &&
                                !bool_of(recipe, "isRejected");
            count++;
        }
        cJSON_Delete(item);
    }
    return count;
}

// The digits after "/rezept/" or "/rezepte/" in an address.
static bool id_from_recipe_url(const char *url, char *out, size_t cap)
{
    const char *at = strstr(url, "/rezepte/");
    size_t skip = 9;
    if (!at) {
        at = strstr(url, "/rezept/");
        skip = 8;
    }
    if (!at) {
        return false;
    }
    at += skip;
    size_t n = 0;
    while (isdigit((unsigned char) at[n]) && n + 1 < cap) {
        out[n] = at[n];
        n++;
    }
    out[n] = '\0';
    return n > 0;
}

int recipe_parse_chefkoch_day(const char *html, char ids[][RECIPE_ID_MAX], int max)
{
    if (!html || !ids || max <= 0) {
        return 0;
    }
    const char *marker = strstr(html, "application/ld+json");
    if (!marker) {
        return 0;
    }
    const char *start = strchr(marker, '>');
    if (!start) {
        return 0;
    }
    start++;
    const char *end = strstr(start, "</script>");
    if (!end) {
        return 0;  // the page was cut off before the end of the list
    }
    size_t len = (size_t) (end - start);
    char *block = malloc(len + 1);
    if (!block) {
        return 0;
    }
    memcpy(block, start, len);
    block[len] = '\0';
    cJSON *root = cJSON_Parse(block);
    free(block);
    if (!root) {
        return 0;
    }
    int count = 0;
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(root, "itemListElement");
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, list)
    {
        if (count >= max) {
            break;
        }
        const char *url = str_of(item, "url");
        if (id_from_recipe_url(url, ids[count], RECIPE_ID_MAX)) {
            count++;
        }
    }
    cJSON_Delete(root);
    return count;
}

int recipe_day_position(int variant)
{
    return variant >= 0 && variant < RECIPE_VARIANT_COUNT ? variant : 0;
}

// Writes "name (amount unit note)" - the parts that exist - as one ingredient.
static void ingredient_label(char *out, size_t cap, const char *name, const char *amount,
                             const char *unit, const char *note, bool paren)
{
    char clean_name[RECIPE_INGREDIENT_LEN];
    char clean_unit[24];
    char clean_note[64];
    copy_clean(clean_name, sizeof(clean_name), name);
    copy_clean(clean_unit, sizeof(clean_unit), unit);
    copy_clean(clean_note, sizeof(clean_note), note);
    char detail[RECIPE_INGREDIENT_LEN] = {0};
    add_word(detail, sizeof(detail), amount);
    add_word(detail, sizeof(detail), clean_unit);
    add_word(detail, sizeof(detail), clean_note);
    if (detail[0] && paren) {
        snprintf(out, cap, "%s (%s)", clean_name, detail);
    } else {
        snprintf(out, cap, "%s", clean_name);
    }
}

bool recipe_parse_chefkoch_recipe(const char *json, const char *source_label, recipe_t *out)
{
    if (!out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (!json) {
        return false;
    }
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return false;
    }
    const cJSON *recipe = cJSON_GetObjectItemCaseSensitive(root, "recipe");
    if (!cJSON_IsObject(recipe)) {
        recipe = root;
    }
    if (bool_of(recipe, "isPremium") || bool_of(recipe, "isPlus")) {
        cJSON_Delete(root);
        return false;
    }
    out->german = true;
    copy_clean(out->title, sizeof(out->title), str_of(recipe, "title"));
    id_of(recipe, "id", out->id, sizeof(out->id));
    copy_clean(out->source, sizeof(out->source), source_label ? source_label : "Chefkoch");
    snprintf(out->image_url, sizeof(out->image_url), "%s",
             str_of(recipe, "previewImageUrlTemplate"));
    if (!bool_of(recipe, "hasImage")) {
        out->image_url[0] = '\0';
    }
    const char *site = str_of(recipe, "siteUrl");
    if (site[0]) {
        snprintf(out->url, sizeof(out->url), "%s", site);
    } else if (out->id[0]) {
        recipe_chefkoch_short_url(out->id, out->url, sizeof(out->url));
    }

    // the category: the last entry of the breadcrumb that has a name
    const cJSON *crumbs = cJSON_GetObjectItemCaseSensitive(recipe, "categoryBreadcrumb");
    const cJSON *crumb = NULL;
    cJSON_ArrayForEach(crumb, crumbs)
    {
        char name[RECIPE_CATEGORY_MAX];
        copy_clean(name, sizeof(name), cJSON_IsObject(crumb) ? str_of(crumb, "title") : "");
        if (name[0]) {
            snprintf(out->category, sizeof(out->category), "%s", name);
        }
    }
    int minutes = (int) num_of(recipe, "totalTime");
    if (minutes <= 0) {
        minutes = (int) (num_of(recipe, "preparationTime") + num_of(recipe, "cookingTime") +
                         num_of(recipe, "restingTime"));
    }
    recipe_text_format_minutes(minutes, true, out->time, sizeof(out->time));

    const cJSON *groups = cJSON_GetObjectItemCaseSensitive(recipe, "ingredientGroups");
    const cJSON *group = NULL;
    cJSON_ArrayForEach(group, groups)
    {
        const cJSON *items = cJSON_GetObjectItemCaseSensitive(group, "ingredients");
        const cJSON *ingredient = NULL;
        cJSON_ArrayForEach(ingredient, items)
        {
            if (out->ingredient_count >= RECIPE_INGREDIENTS_MAX) {
                break;
            }
            const char *name = str_of(ingredient, "name");
            if (!name[0]) {
                continue;
            }
            char amount[16];
            recipe_text_format_amount(num_of(ingredient, "amount"), true, amount, sizeof(amount));
            char *label = out->ingredients[out->ingredient_count];
            ingredient_label(label, RECIPE_INGREDIENT_LEN, name, amount, str_of(ingredient, "unit"),
                             str_of(ingredient, "usageInfo"), true);
            if (label[0]) {
                out->ingredient_count++;
            }
        }
    }

    const char *instructions = str_of(recipe, "instructions");
    size_t work_cap = strlen(instructions) + 16;
    char *work = malloc(work_cap);
    if (work) {
        bool cut = false;
        recipe_text_clean(instructions, work, work_cap, &cut);
        char *paragraphs = malloc(work_cap + work_cap / 8);
        if (paragraphs) {
            recipe_text_paragraphs(work, paragraphs, work_cap + work_cap / 8);
            // too long for the page: the text is cut (and the page skips such a recipe)
            size_t len = strlen(paragraphs);
            if (len >= sizeof(out->text)) {
                out->text_cut = true;
            }
            snprintf(out->text, sizeof(out->text), "%s", paragraphs);
            free(paragraphs);
        }
        free(work);
    }
    cJSON_Delete(root);
    return out->title[0] && out->ingredient_count > 0 && out->text[0];
}

int recipe_parse_mealdb_list(const char *json, recipe_candidate_t *out, int max)
{
    if (!json || !out || max <= 0) {
        return 0;
    }
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return 0;
    }
    int count = 0;
    const cJSON *meals = cJSON_GetObjectItemCaseSensitive(root, "meals");
    const cJSON *meal = NULL;
    cJSON_ArrayForEach(meal, meals)
    {
        if (count >= max) {
            break;
        }
        recipe_candidate_t *candidate = &out[count];
        memset(candidate, 0, sizeof(*candidate));
        id_of(meal, "idMeal", candidate->id, sizeof(candidate->id));
        if (!candidate->id[0]) {
            continue;
        }
        copy_clean(candidate->title, sizeof(candidate->title), str_of(meal, "strMeal"));
        candidate->has_image = str_of(meal, "strMealThumb")[0] != '\0';
        candidate->usable = true;
        count++;
    }
    cJSON_Delete(root);
    return count;
}

bool recipe_parse_mealdb_recipe(const char *json, recipe_t *out)
{
    if (!out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (!json) {
        return false;
    }
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return false;
    }
    const cJSON *meals = cJSON_GetObjectItemCaseSensitive(root, "meals");
    const cJSON *meal = cJSON_IsArray(meals) ? cJSON_GetArrayItem(meals, 0) : NULL;
    if (!cJSON_IsObject(meal)) {
        cJSON_Delete(root);
        return false;
    }
    out->german = false;
    id_of(meal, "idMeal", out->id, sizeof(out->id));
    copy_clean(out->title, sizeof(out->title), str_of(meal, "strMeal"));
    copy_clean(out->category, sizeof(out->category), str_of(meal, "strCategory"));
    copy_clean(out->source, sizeof(out->source), "TheMealDB");
    snprintf(out->image_url, sizeof(out->image_url), "%s", str_of(meal, "strMealThumb"));
    recipe_mealdb_short_url(out->id, out->url, sizeof(out->url));
    for (int i = 1; i <= 20 && out->ingredient_count < RECIPE_INGREDIENTS_MAX; i++) {
        char key[24];
        snprintf(key, sizeof(key), "strIngredient%d", i);
        const char *name = str_of(meal, key);
        snprintf(key, sizeof(key), "strMeasure%d", i);
        const char *measure = str_of(meal, key);
        // "Beef (500g)": the measure is all the detail there is; a name without text is none
        char label[RECIPE_INGREDIENT_LEN];
        ingredient_label(label, sizeof(label), name, "", "", measure, true);
        if (!label[0]) {
            continue;
        }
        snprintf(out->ingredients[out->ingredient_count++], RECIPE_INGREDIENT_LEN, "%s", label);
    }
    const char *instructions = str_of(meal, "strInstructions");
    size_t work_cap = strlen(instructions) + 16;
    char *cleaned = malloc(work_cap);
    char *merged = malloc(work_cap + work_cap / 4);
    char *paragraphs = malloc(work_cap + work_cap / 2);
    if (cleaned && merged && paragraphs) {
        recipe_text_clean(instructions, cleaned, work_cap, NULL);
        recipe_text_merge_steps(cleaned, merged, work_cap + work_cap / 4);
        recipe_text_paragraphs(merged, paragraphs, work_cap + work_cap / 2);
        if (strlen(paragraphs) >= sizeof(out->text)) {
            out->text_cut = true;
        }
        snprintf(out->text, sizeof(out->text), "%s", paragraphs);
    }
    free(cleaned);
    free(merged);
    free(paragraphs);
    cJSON_Delete(root);
    return out->title[0] && out->ingredient_count > 0 && out->text[0];
}

// ---------------------------------------------------------------------------------------------
// Choosing
// ---------------------------------------------------------------------------------------------

int recipe_pick_offset(int total, int limit, unsigned random)
{
    int reach = total > 0 && total < RECIPE_SEARCH_DEPTH ? total : RECIPE_SEARCH_DEPTH;
    int last = reach - limit;
    if (last <= 0) {
        return 0;
    }
    return (int) (random % (unsigned) (last + 1));
}

bool recipe_history_has(const char *history, const char *id)
{
    if (!history || !id || !id[0]) {
        return false;
    }
    size_t id_len = strlen(id);
    const char *line = history;
    while (*line) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t) (end - line) : strlen(line);
        if (len == id_len && strncmp(line, id, len) == 0) {
            return true;
        }
        if (!end) {
            break;
        }
        line = end + 1;
    }
    return false;
}

void recipe_history_add(char *history, size_t cap, const char *id, int keep)
{
    if (!history || cap == 0 || !id || !id[0] || keep < 1) {
        return;
    }
    // collect the lines that stay (everything but this id), then this id at the end
    char *work = malloc(cap + strlen(id) + 2);
    if (!work) {
        return;
    }
    work[0] = '\0';
    size_t w = 0;
    size_t id_len = strlen(id);
    const char *line = history;
    while (*line) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t) (end - line) : strlen(line);
        if (len > 0 && !(len == id_len && strncmp(line, id, len) == 0)) {
            memcpy(work + w, line, len);
            w += len;
            work[w++] = '\n';
        }
        if (!end) {
            break;
        }
        line = end + 1;
    }
    memcpy(work + w, id, id_len);
    w += id_len;
    work[w++] = '\n';
    work[w] = '\0';
    // keep the newest `keep` lines, and as many of them as fit
    int lines = 0;
    for (size_t i = 0; i < w; i++) {
        if (work[i] == '\n') {
            lines++;
        }
    }
    const char *from = work;
    while (lines > keep || strlen(from) + 1 > cap) {
        const char *newline = strchr(from, '\n');
        if (!newline || !newline[1]) {
            break;
        }
        from = newline + 1;
        lines--;
    }
    // no trailing newline: the lines are separated by one
    size_t n = strlen(from);
    if (n > 0 && from[n - 1] == '\n') {
        n--;
    }
    if (n + 1 > cap) {
        n = cap - 1;
    }
    memcpy(history, from, n);
    history[n] = '\0';
    free(work);
}
