#ifndef CALENDAR_RRULE_H
#define CALENDAR_RRULE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "feature_config.h"

/**
 * @file calendar_rrule.h
 * @brief Recurrence rules of the Agenda's calendars through libical (build option `agenda-rrule`).
 *
 * calendar_ics.c's own expander (compiled only without this option) knows DAILY and WEEKLY with a
 * single BYDAY; this module hands every rule to libical's recurrence iterator (components/libical,
 * unmodified), which knows all of RFC 5545: MONTHLY and YEARLY, BYDAY lists and ordinals ("second
 * Monday", "last Friday"), BYMONTHDAY (negative too), BYMONTH, BYYEARDAY, BYWEEKNO, BYSETPOS,
 * BYHOUR/BYMINUTE/BYSECOND, WKST, invalid dates skipped.
 *
 * What this module adds around libical:
 *  - the rule is checked here first (known parts only, ranges, the combinations the standard
 * allows) and libical only ever sees a rule that passed - an internal assertion of libical is a
 * firmware abort;
 *  - it works on wall-clock fields (year ... second), no time zones: calendar_ics.c converts them
 * to instants (device time zone, or UTC for a DTSTART with a "Z"), so daylight saving time is
 * handled in one place, and a date-time that does not exist or occurs twice is calendar_ics.c's
 * business;
 *  - UNTIL is not passed on (calendar_ics.c bounds the result by it, as it does for its own
 * expander);
 *  - the work is bounded: a rule with COUNT is walked from DTSTART for at most RRULE_MAX_STEPS
 * instances to reach the window (more: the event is left out), a rule without COUNT jumps to the
 * window; the search for the next instance is limited too (a rule that never matches, like "30
 * February");
 *  - FREQ below DAILY, RSCALE, SKIP, unknown parts and everything libical reports an error for: the
 * rule is refused and the caller leaves the event out (fail-closed, as for its own expander).
 */

#if FEATURE_AGENDA_RRULE

// A wall-clock date and time, as written in the feed.
typedef struct {
    int16_t year;
    int8_t month;   // 1..12
    int8_t day;     // 1..31
    int8_t hour;    // 0..23
    int8_t minute;  // 0..59
    int8_t second;  // 0..59
} rrule_wall_t;

// Instances of a rule with COUNT that are walked (counted from DTSTART) to reach the window.
#define RRULE_MAX_STEPS 5000

/**
 * @brief The instances of `rule` (the value of an RRULE) that fall into [from, to), in order.
 *
 * @param rule     RRULE text, `len` bytes (not NUL-terminated), UNTIL included (it is ignored
 * here).
 * @param dtstart  DTSTART as written; `all_day` for a bare date (the time fields are then ignored).
 * @param from     First wall-clock time wanted (may be before DTSTART).
 * @param to       End of the range, exclusive.
 * @param out      Receives at most `max_out` instances.
 * @return Number of instances written, or -1 if the rule is not taken (unsupported, invalid, too
 * costly to reach the window): the caller leaves the event out.
 */
int calendar_rrule_expand(const char *rule, size_t len, const rrule_wall_t *dtstart, bool all_day,
                          const rrule_wall_t *from, const rrule_wall_t *to, rrule_wall_t *out,
                          int max_out);

#endif  // FEATURE_AGENDA_RRULE

#endif
