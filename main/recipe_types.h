#ifndef RECIPE_TYPES_H
#define RECIPE_TYPES_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @file recipe_types.h
 * @brief The recipe the page shows (build option `recipes`), in the form every part works on: the
 * readers fill it, the page draws it, the last one is kept in a file and read back.
 *
 * All text is in the Windows-1252 code, one byte per character (recipe_text.h makes it from
 * UTF-8), ready for the fonts of recipe_font.h. Pure C, no ESP-IDF dependency.
 */

#define RECIPE_TITLE_MAX 120
#define RECIPE_CATEGORY_MAX 56
#define RECIPE_TIME_MAX 24
#define RECIPE_SOURCE_MAX 56
#define RECIPE_URL_MAX 200
#define RECIPE_ID_MAX 24
#define RECIPE_INGREDIENTS_MAX 30
#define RECIPE_INGREDIENT_LEN 96
#define RECIPE_TEXT_MAX 4096

typedef struct {
    char title[RECIPE_TITLE_MAX];
    char category[RECIPE_CATEGORY_MAX];  // "Gemuese", "Beef"; empty if the source has none
    char time[RECIPE_TIME_MAX];          // "35 Min.", "1 Std. 20 Min."; empty if unknown
    char source[RECIPE_SOURCE_MAX];      // "Chefkoch - Rezept des Tages", "TheMealDB"
    char id[RECIPE_ID_MAX];              // the source's id, for the list of recipes seen lately
    char url[RECIPE_URL_MAX];            // where the recipe is (the QR code points there)
    char image_url[RECIPE_URL_MAX];      // the picture to download, empty if there is none
    bool german;                         // the language of the labels on the page
    int ingredient_count;
    char ingredients[RECIPE_INGREDIENTS_MAX][RECIPE_INGREDIENT_LEN];  // "Entenbrust (400 g)"
    char text[RECIPE_TEXT_MAX];  // the preparation, one paragraph per line ('\n' between)
    bool text_cut;  // the source had more text than RECIPE_TEXT_MAX or more ingredients than fit
} recipe_t;

#endif
