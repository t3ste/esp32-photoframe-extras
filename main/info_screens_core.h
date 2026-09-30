#ifndef INFO_SCREENS_CORE_H
#define INFO_SCREENS_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/**
 * @file info_screens_core.h
 * @brief The pure parts of the info screens (build option `info-screens`): the moment a screen is
 * drawn for, ISO weeks and names of days and months in English and German, which screen is next in
 * the rotation, and the parsing of the lists users type in. No ESP-IDF dependency, so the host
 * tests and the render harness link it.
 */

// The moment a screen is drawn for, computed once by the caller.
typedef struct {
    bool german;           // the language of the text drawn on the screen (English otherwise)
    int year, month, day;  // month 1-12
    int wday;              // 0 = Sunday .. 6 = Saturday
    int hour, minute;
    int iso_year, iso_week;  // ISO 8601 week of the date (the week of a Thursday decides the year)
} info_now_t;

/** @brief Fills `out` from a broken-down local time. */
void info_now_from_tm(const struct tm *local, bool german, info_now_t *out);

/** @brief Fills `out` from a calendar date (noon), for tests and fixed previews. */
void info_now_from_date(int year, int month, int day, bool german, info_now_t *out);

/** @brief The ISO 8601 week of a date (1-53); `iso_year` receives the year that week belongs to. */
int info_iso_week(int year, int month, int day, int *iso_year);

/** @brief 0 = Sunday .. 6 = Saturday. */
int info_weekday(int year, int month, int day);

/**
 * @brief Names of days and months as UTF-8 (the German ones with umlauts: "März"; pass them
 * through canvas_text_from_utf8() to draw them). `wday` 0 = Sunday, `month` 1-12; "?" outside.
 */
const char *info_weekday_name(int wday, bool german);
const char *info_weekday_short(int wday, bool german);
const char *info_month_name(int month, bool german);

/**
 * @brief The `counter`-th (counting from 0, wrapping) set bit of `mask`, looking at the lowest
 * `bit_count` bits: which screen is next when the screens of the rotation are shown one after the
 * other. -1 if no bit is set.
 */
int info_rotation_pick(uint32_t mask, uint32_t counter, int bit_count);

/** @brief How many of the lowest `bit_count` bits of `mask` are set. */
int info_rotation_size(uint32_t mask, int bit_count);

/**
 * @brief Splits a list typed by a user ("Anna, Ben; Clara" or one per line) into names: separators
 * are `,` `;` and line breaks, names are trimmed, empty ones dropped, long ones cut to `name_len -
 * 1` bytes without splitting a UTF-8 character. Returns how many names were written (at most
 * `max`).
 */
int info_parse_list(const char *text, char *names, size_t name_len, int max);

#endif
