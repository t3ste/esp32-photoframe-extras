#include "sched_pick.h"

#include <stdlib.h>
#include <string.h>

static int hold_of(const sched_entity_t *entity)
{
    int hold = entity->hold_min;
    if (hold < 0) {
        hold = 0;
    }
    if (hold > SCHED_HOLD_MAX_MIN) {
        hold = SCHED_HOLD_MAX_MIN;
    }
    return hold;
}

// Bit k is set when entity k has a rule that matches the minute `minute`.
static uint8_t match_bits(const sched_entity_t *entities, int count, time_t minute)
{
    struct tm local;
    localtime_r(&minute, &local);
    uint8_t bits = 0;
    for (int k = 0; k < count; k++) {
        for (int r = 0; r < entities[k].n_rules; r++) {
            if (cron_match(&entities[k].rules[r], &local)) {
                bits |= (uint8_t) (1u << k);
                break;
            }
        }
    }
    return bits;
}

static time_t minute_of(time_t when)
{
    time_t rest = when % 60;
    if (rest < 0) {
        rest += 60;
    }
    return when - rest;
}

int sched_matches(const sched_entity_t *entities, int count, time_t when, int k)
{
    if (!entities || count <= 0 || count > SCHED_MAX_ENTITIES || k < 0 || k >= count) {
        return 0;
    }
    return (match_bits(entities, count, minute_of(when)) >> k) & 1u;
}

int sched_drawn_at(const sched_entity_t *entities, int count, time_t when)
{
    if (!entities || count <= 0 || count > SCHED_MAX_ENTITIES) {
        return -1;
    }
    time_t centre = minute_of(when);

    // The fire of entity k depends on fires of higher entities up to one hold away, which depend
    // on higher ones again: the last entity looks (count) holds in each direction at most.
    int longest = 1;
    for (int k = 0; k < count; k++) {
        int hold = hold_of(&entities[k]);
        if (hold > longest) {
            longest = hold;
        }
    }
    int radius = count * longest;
    int size = 2 * radius + 1;

    uint8_t *match = malloc((size_t) size);
    uint8_t *drawn = calloc((size_t) size, 1);
    uint8_t *blocked = malloc((size_t) size);
    if (!match || !drawn || !blocked) {
        free(match);
        free(drawn);
        free(blocked);
        uint8_t now_bits = match_bits(entities, count, centre);  // no memory: plain priority
        for (int k = 0; k < count; k++) {
            if ((now_bits >> k) & 1u) {
                return k;
            }
        }
        return -1;
    }

    for (int g = 0; g < size; g++) {
        match[g] = match_bits(entities, count, centre + (time_t) (g - radius) * 60);
    }

    for (int k = 0; k < count; k++) {
        int own_hold = hold_of(&entities[k]);
        memset(blocked, 0, (size_t) size);
        for (int j = 0; j < k; j++) {
            int their_hold = hold_of(&entities[j]);
            if (their_hold < 1) {
                their_hold = 1;  // the same minute is always resolved
            }
            // a higher fire at or before this minute, within its own hold
            int last = -1000000;
            for (int g = 0; g < size; g++) {
                if ((drawn[g] >> j) & 1u) {
                    last = g;
                }
                if (g - last < their_hold) {
                    blocked[g] = 1;
                }
            }
            // a higher fire after this minute, before the hold of this entity has passed
            int next = 1000000;
            for (int g = size - 1; g >= 0; g--) {
                if (next - g < own_hold && next > g) {
                    blocked[g] = 1;
                }
                if ((drawn[g] >> j) & 1u) {
                    next = g;
                }
            }
        }
        for (int g = 0; g < size; g++) {
            if (((match[g] >> k) & 1u) && !blocked[g]) {
                drawn[g] |= (uint8_t) (1u << k);
            }
        }
    }

    int winner = -1;
    for (int k = 0; k < count; k++) {
        if ((drawn[radius] >> k) & 1u) {
            winner = k;
            break;
        }
    }
    free(match);
    free(drawn);
    free(blocked);
    return winner;
}

// The first minute after `now` at which `wanted` (-1: any entity) draws.
static int seconds_until(const sched_entity_t *entities, int count, time_t now, int wanted,
                         int *entity)
{
    if (!entities || count <= 0 || count > SCHED_MAX_ENTITIES) {
        return CRON_FALLBACK_SEC;
    }
    time_t start = minute_of(now) + 60;
    for (time_t t = start; t <= now + CRON_MAX_HORIZON_SEC; t += 60) {
        uint8_t bits = match_bits(entities, count, t);
        if (bits == 0 || (wanted >= 0 && !((bits >> wanted) & 1u))) {
            continue;
        }
        int k = sched_drawn_at(entities, count, t);
        if (k >= 0 && (wanted < 0 || k == wanted)) {
            if (entity) {
                *entity = k;
            }
            return (int) (t - now);
        }
    }
    return CRON_FALLBACK_SEC;
}

int sched_seconds_until_next(const sched_entity_t *entities, int count, time_t now, int *entity)
{
    return seconds_until(entities, count, now, -1, entity);
}

int sched_seconds_until_next_of(const sched_entity_t *entities, int count, time_t now, int k)
{
    if (k < 0 || k >= count) {
        return CRON_FALLBACK_SEC;
    }
    return seconds_until(entities, count, now, k, NULL);
}
