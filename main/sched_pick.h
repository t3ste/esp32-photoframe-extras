#ifndef SCHED_PICK_H
#define SCHED_PICK_H

/**
 * @file sched_pick.h
 * @brief Which schedule draws, when several schedules overlap (build option `schedule-pages`). Pure
 * C on top of cron.h, so the host tests link it and check it against a brute-force reference.
 *
 * The schedules are the entries of a list in priority order: entry 0 is the highest (Schedule 1 in
 * the Web UI), the last one the lowest (the photo rotation, when it takes part). Each entry has a
 * set of cron rules (it fires when any of them matches) and a hold time: how long, in minutes, its
 * display should stay before another one may replace it.
 *
 * A fire of entry k at minute t is **drawn** unless a higher entry j < k draws
 *  - within the hold of j before it (t - t_j < hold_j; the same minute always counts), or
 *  - within the hold of k after it (0 < t_j - t < hold_k): k's display would be cut short.
 * "Draws" is recursive: only fires that are themselves drawn count, so a suppressed fire of a
 * middle entry does not suppress a lower one. Entry 0 is never suppressed; at most one entry draws
 * in a minute; an entry never suppresses itself (its own rules may fire as close as they like).
 */

#include <stdint.h>
#include <time.h>

#include "cron.h"

#define SCHED_MAX_ENTITIES 8    // 7 agenda schedules and the photo rotation
#define SCHED_HOLD_MAX_MIN 240  // longest hold that is honoured

typedef struct {
    const cron_rule_t *rules;  // the entry's cron rules; it fires when any of them matches
    int n_rules;
    int hold_min;  // minutes its display should stay (0 = only the same minute is resolved)
} sched_entity_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Whether entry `k` has a rule that matches the minute of `when` (the schedule fires then,
 * whether or not it is drawn).
 */
int sched_matches(const sched_entity_t *entities, int count, time_t when, int k);

/**
 * @brief The entry that draws in the minute of `when`, or -1 when none does (nothing matches, or
 * everything that matches is suppressed). Out of memory for the scratch space: the highest entry
 * that matches (the plain priority, no hold).
 */
int sched_drawn_at(const sched_entity_t *entities, int count, time_t when);

/**
 * @brief Seconds from `now` until the next minute in which some entry draws - the next whole
 * minute at the earliest, like cron_seconds_until_next(). `*entity` (may be NULL) receives that
 * entry. CRON_FALLBACK_SEC when there is none within CRON_MAX_HORIZON_SEC.
 */
int sched_seconds_until_next(const sched_entity_t *entities, int count, time_t now, int *entity);

/** @brief The same, for the next minute in which entry `k` itself draws. */
int sched_seconds_until_next_of(const sched_entity_t *entities, int count, time_t now, int k);

#ifdef __cplusplus
}
#endif

#endif
