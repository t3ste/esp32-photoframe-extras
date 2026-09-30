#ifndef FACT_PACK_H
#define FACT_PACK_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @file fact_pack.h
 * @brief Facts for the fact-of-the-day page (build option `fact-of-the-day`): the format of a
 * pack of facts, one per line, the pick of the day, and a small built-in pack in English and
 * German. Pure C with no ESP-IDF dependency, so the host tests link it.
 *
 * A pack is a plain text file, one fact per line:
 *
 *     Fact text
 *     Title|Fact text
 *     Title|Fact text|A question to think about
 *
 * Empty lines and lines starting with `#` are ignored; a line whose fact text is empty is skipped.
 * Text is UTF-8 (umlauts and the other glyphs of the `glyphs` option work); a field that is too
 * long is cut at a character boundary.
 */

#define FACT_MAX 64
#define FACT_TITLE_MAX 48
#define FACT_TEXT_MAX 256
#define FACT_QUESTION_MAX 128
#define FACT_PACK_MAX_BYTES 16384  // the largest pack the frame takes

typedef struct {
    char title[FACT_TITLE_MAX];        // may be empty
    char text[FACT_TEXT_MAX];          // never empty
    char question[FACT_QUESTION_MAX];  // may be empty
} fact_t;

/**
 * @brief Parses a pack. Writes at most `max` facts to `out` and returns how many; `*skipped` (may
 * be NULL) receives the number of lines that were not usable (no fact text) or did not fit.
 */
int fact_pack_parse(const char *text, fact_t *out, int max, int *skipped);

/** @brief Days since 1970-01-01 of a calendar date (proleptic Gregorian). */
long fact_day_number(int year, int month, int day);

/**
 * @brief The index of the fact of a day among `count` facts: the same all day, the next one the
 * next day, wrapping. -1 for `count` < 1.
 */
int fact_pick_index(long day_number, int count);

/** @brief Number of built-in facts (the same in both languages). */
int fact_builtin_count(void);

/** @brief One built-in fact, in German or English; NULL for an index out of range. */
const fact_t *fact_builtin(int index, bool german);

#endif
