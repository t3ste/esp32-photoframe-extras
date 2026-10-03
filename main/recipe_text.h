#ifndef RECIPE_TEXT_H
#define RECIPE_TEXT_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @file recipe_text.h
 * @brief What the recipe sources send, made fit to draw (build option `recipes`): UTF-8 into the
 * one-byte Windows-1252 code of recipe_font.h, HTML tags and entities removed, the odd characters
 * and the numbers of steps tidied, the preparation split into paragraphs, times and amounts
 * written the way the page shows them. Pure C, no ESP-IDF dependency.
 */

/**
 * @brief Cleans one text of a source into `out` (NUL-terminated): UTF-8 decoded into Windows-1252
 * (what the font has: German letters, accents, typographic quotes and dashes, the ellipsis, the
 * euro sign, the fractions; a few other signs become their plain ASCII form, the rest is dropped),
 * HTML entities decoded and tags removed (a <br> or the end of a paragraph is a line break), tabs
 * and other white space made one space, control characters dropped, blank lines and the spaces
 * around line breaks removed, both ends trimmed. Line breaks (single '\n') are kept.
 *
 * @param out_cut Set true if `out` was too small and the text was cut; may be NULL.
 * @return The length of the text written.
 */
size_t recipe_text_clean(const char *utf8, char *out, size_t out_cap, bool *out_cut);

/**
 * @brief Joins the numbers of steps that stand on a line of their own ("1", "2.", "STEP 3", "Step
 * 4:") with the text after them: "3. Add the cabbage ...". A line that is only a number or only
 * signs is such a marker (a bare bullet gets the next number). `text` is the cleaned text.
 *
 * @return The length written to `out`.
 */
size_t recipe_text_merge_steps(const char *text, char *out, size_t out_cap);

/**
 * @brief Splits the cleaned preparation into paragraphs, one per line of `out`: each line of the
 * text is a paragraph, but a long one is cut after every second sentence. A sentence ends at ". ",
 * "! " or "? " before a capital letter or a digit - not after an abbreviation (ca., z. B., Min.,
 * evtl., ...) and not after a number ("Die 2. Haelfte").
 *
 * @return The length written to `out`.
 */
size_t recipe_text_paragraphs(const char *text, char *out, size_t out_cap);

/**
 * @brief A duration for the page: "35 Min.", "1 Std. 20 Min.", "2 Std." - in English "35 min",
 * "1 h 20 min", "2 h". An empty string for no or a negative number of minutes.
 */
void recipe_text_format_minutes(int minutes, bool german, char *out, size_t out_cap);

/**
 * @brief A quantity as it is written in a recipe: "400", "0,5", "1,25" (German) or "0.5" (English);
 * at most two decimals, no trailing zeros. An empty string for zero or less.
 */
void recipe_text_format_amount(double value, bool german, char *out, size_t out_cap);

#endif
