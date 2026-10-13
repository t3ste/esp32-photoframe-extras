#ifndef WAKE_MINUTE_H
#define WAKE_MINUTE_H

#include <time.h>

/**
 * @file wake_minute.h
 * @brief The clock reading a timer wake is matched against its schedule with.
 *
 * A wake from deep sleep lands a few seconds off its scheduled minute, before it as often as after
 * (the RTC's slow clock drifts). main.c lets a wake that is at most EARLY_WAKE_TOLERANCE_SEC early
 * go on as if it were on time. The match of the Agenda and Alarm schedules has to do the same: read
 * at the plain clock, a wake at 10:49:57 belongs to the minute 10:49, matches nothing, and is taken
 * for a photo rotation wake - the 10:50 Agenda page is never drawn (and an alarm never rings).
 */

/**
 * @brief `now`, or the start of the next minute when `now` is within `tolerance_sec` seconds
 * before it. Whole-minute time zone offsets keep the minute boundaries of local time and UTC equal.
 */
static inline time_t wake_minute_clock(time_t now, int tolerance_sec)
{
    int sec = (int) (now % 60);
    if (tolerance_sec > 0 && sec >= 60 - tolerance_sec) {
        return now + (60 - sec);
    }
    return now;
}

#endif
