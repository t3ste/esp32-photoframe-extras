// wake_minute.h: a timer wake a few seconds before its minute is matched as that minute (the Agenda
// and Alarm schedules; build options `agenda`, `alarmclock`).

#include <gtest/gtest.h>

#include <cstdlib>
#include <ctime>

extern "C" {
#include "cron.h"
#include "wake_minute.h"
}

namespace
{

constexpr int kTolerance = 5;  // EARLY_WAKE_TOLERANCE_SEC

// 2026-10-13 10:49:<sec> UTC (every test runs with TZ=UTC0)
time_t at(int minute, int sec)
{
    struct tm t = {};
    t.tm_year = 2026 - 1900;
    t.tm_mon = 9;
    t.tm_mday = 13;
    t.tm_hour = 10;
    t.tm_min = minute;
    t.tm_sec = sec;
    return timegm(&t);
}

bool matches(const char *rule, time_t wake)
{
    cron_rule_t r;
    EXPECT_TRUE(cron_parse(rule, &r));
    time_t clock = wake_minute_clock(wake, kTolerance);
    struct tm tm_local;
    localtime_r(&clock, &tm_local);
    return cron_match(&r, &tm_local);
}

}  // namespace

TEST(WakeMinute, OnTimeAndLateWakesKeepTheirClock)
{
    for (int sec = 0; sec < 60 - kTolerance; sec++) {
        EXPECT_EQ(wake_minute_clock(at(50, sec), kTolerance), at(50, sec)) << sec;
    }
}

TEST(WakeMinute, AWakeWithinToleranceBeforeTheMinuteBelongsToThatMinute)
{
    for (int sec = 60 - kTolerance; sec < 60; sec++) {
        EXPECT_EQ(wake_minute_clock(at(49, sec), kTolerance), at(50, 0)) << sec;
    }
}

TEST(WakeMinute, TheLastSecondOutsideTheToleranceStaysInItsMinute)
{
    EXPECT_EQ(wake_minute_clock(at(49, 54), kTolerance), at(49, 54));
    EXPECT_EQ(wake_minute_clock(at(49, 55), kTolerance), at(50, 0));
}

TEST(WakeMinute, NoToleranceMeansTheClockAsItIs)
{
    EXPECT_EQ(wake_minute_clock(at(49, 59), 0), at(49, 59));
}

TEST(WakeMinute, CrossesTheHourAndTheDay)
{
    struct tm t = {};
    t.tm_year = 2026 - 1900;
    t.tm_mon = 9;
    t.tm_mday = 13;
    t.tm_hour = 23;
    t.tm_min = 59;
    t.tm_sec = 58;
    time_t late = timegm(&t);
    EXPECT_EQ(wake_minute_clock(late, kTolerance), late + 2);
    struct tm next;
    time_t c = wake_minute_clock(late, kTolerance);
    gmtime_r(&c, &next);
    EXPECT_EQ(next.tm_mday, 14);
    EXPECT_EQ(next.tm_hour, 0);
    EXPECT_EQ(next.tm_min, 0);
}

// The case that lost a scheduled Agenda page: a wake at 10:49:57 for a 10:50 schedule
TEST(WakeMinute, ASlightlyEarlyWakeMatchesItsSchedule)
{
    EXPECT_TRUE(matches("50 10 *", at(49, 57)));
    EXPECT_TRUE(matches("50 10 *", at(49, 55)));
    EXPECT_TRUE(matches("50 10 *", at(50, 0)));
    EXPECT_TRUE(matches("50 10 *", at(50, 3)));
}

TEST(WakeMinute, ButNotAnEarlierWakeOrAnotherMinute)
{
    EXPECT_FALSE(matches("50 10 *", at(49, 54)));  // main.c sends such a wake back to sleep
    EXPECT_FALSE(matches("50 10 *", at(51, 3)));
    EXPECT_FALSE(matches("49 10 *", at(49, 57)));  // 10:49 already passed in its own wake
}
