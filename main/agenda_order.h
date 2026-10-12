#ifndef AGENDA_ORDER_H
#define AGENDA_ORDER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/**
 * @file agenda_order.h
 * @brief Pure helpers of the Calendar view (build option `agenda`): the priority of the calendars
 * A-E, the order of the entries of one day, which entries stay when the screen has no room for all
 * of them, and the ISO week number. No ESP-IDF, no NVS - the host tests link this file.
 *
 * The priority list names the calendars from the most to the least important ("ABCDE": A first, E
 * last). It does two things. It decides the order of the entries inside a day (the entries of a
 * calendar of higher priority above those of a lower one; the setting "order within a day" can put
 * the time first instead), and it decides which entries are given up when the column or cell runs
 * out of rows: the ones of the lowest priority go first and are only counted in "+N more", so an
 * important entry is not pushed out by a flood of lesser ones.
 */

#define AGENDA_CAL_COUNT 5  // calendars A..E, calendar index 0..4

// "ABCDE": A has the highest priority, E the lowest.
#define AGENDA_PRIORITY_DEFAULT "ABCDE"
#define AGENDA_PRIORITY_LEN (AGENDA_CAL_COUNT + 1)  // five letters and the terminator

/**
 * Parses a priority list: each of the letters A-E exactly once, upper or lower case, in the order
 * from the most to the least important. Anything that is not a letter A-E (commas, spaces, ">",
 * ";") is skipped, so "A,B,C,D,E", "e > d > c > b > a" and "ABCDE" all work.
 * @param order out: the calendar indexes (0 = A ... 4 = E), the highest priority first. Untouched
 * on failure.
 * @return false for a list that does not name each of the five calendars exactly once (also for
 * NULL).
 */
bool agenda_priority_parse(const char *text, uint8_t order[AGENDA_CAL_COUNT]);

// The canonical text of an order: five upper-case letters ("ACBDE"), terminated.
void agenda_priority_format(const uint8_t order[AGENDA_CAL_COUNT], char out[AGENDA_PRIORITY_LEN]);

// rank_of[calendar index] = position in `order` (0 = highest priority).
void agenda_priority_ranks(const uint8_t order[AGENDA_CAL_COUNT],
                           uint8_t rank_of[AGENDA_CAL_COUNT]);

// What an entry is sorted by.
typedef struct {
    time_t start;
    time_t end;
    bool all_day;
    uint8_t rank;  // the calendar's priority rank, 0 = highest
    uint8_t cal;   // the calendar index 0..4
    uint16_t seq;  // the position in the input - the last tie-break, so the order never depends on
                   // the sort
} agenda_sort_key_t;

/**
 * Total order of two entries (like strcmp). priority_first: the rank is the primary key, then the
 * start time; otherwise the start time is, then the rank. Either way an all-day entry comes before
 * a timed one that starts at the same moment, a shorter one before a longer one, and `seq` decides
 * the rest - never "arbitrary".
 */
int agenda_event_compare(const agenda_sort_key_t *a, const agenda_sort_key_t *b,
                         bool priority_first);

/**
 * Which of `n` entries to keep when there is room for `capacity`: the ones with the best (lowest)
 * rank; among equal ranks the earlier entry of the array wins (the array is in display order: day
 * by day, in time). Marks the kept ones in shown[0..n-1].
 * @return the number kept: min(n, capacity), 0 for a capacity <= 0
 */
int agenda_select_by_rank(const uint8_t *ranks, int n, int capacity, bool *shown);

// The ISO 8601 week number (1..53) of a date; the arguments are the fields of struct tm.
int agenda_iso_week(int year_1900, int mon0, int mday);

// "KW42" (German) or "CW42": the calendar week as the day labels show it.
void agenda_week_text(char *out, size_t out_len, int week, bool german);

/**
 * The label of a day divider: "Mo 12." - and, when `with_week`, "Mo 12. KW42". `weekday` is the
 * abbreviation the caller already chose for the language.
 */
void agenda_day_label(char *out, size_t out_len, const char *weekday, int mday, bool with_week,
                      int week, bool german);

#endif
