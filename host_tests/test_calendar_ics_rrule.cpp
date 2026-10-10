// calendar_ics.c with the build option `agenda-rrule` on: the rules of the Agenda's calendars
// through libical (main/calendar_rrule.c, components/libical). The other calendar tests
// (test_calendar_ics.cpp) build the same file with the option off - that is the default firmware,
// whose reader takes daily and weekly rules only. Here: what the option adds (monthly, yearly,
// BYDAY lists and ordinals, BYSETPOS, ...) on the real parser, with the exceptions of a series
// (EXDATE, RDATE, RECURRENCE-ID), UNTIL and COUNT, all-day events, and daylight saving time (a zone
// with it: CET/CEST as a POSIX rule, so no tzdata is needed on the host).

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

extern "C" {
#include "calendar_ics.h"
}

namespace
{

class CalendarIcsRrule : public ::testing::Test
{
   protected:
    void SetUp() override
    {
#if defined(_WIN32)
        GTEST_SKIP() << "needs POSIX TZ rules";
#else
        setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
        tzset();
#endif
    }
};

time_t local(int year, int mon, int day, int hour = 0, int minute = 0, int sec = 0)
{
    struct tm tm {
    };
    tm.tm_year = year - 1900;
    tm.tm_mon = mon - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = sec;
    tm.tm_isdst = -1;
    return mktime(&tm);
}

time_t at_utc(int year, int mon, int day, int hour = 0, int minute = 0, int sec = 0)
{
    struct tm tm {
    };
    tm.tm_year = year - 1900;
    tm.tm_mon = mon - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = sec;
    return timegm(&tm);
}

ics_event_list_t parse(const std::string &text, time_t from, time_t to)
{
    std::vector<char> buf(text.begin(), text.end());
    buf.push_back('\0');
    ics_event_list_t out;
    calendar_ics_parse(buf.data(), text.size(), from, to, &out);
    return out;
}

// "YYYY-MM-DD HH:MM" in local time of the n-th event
std::string when(const ics_event_list_t &l, int n)
{
    time_t t = l.events[n].start;
    struct tm tm;
    localtime_r(&t, &tm);
    char b[32];
    if (l.events[n].all_day) {
        snprintf(b, sizeof(b), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    } else {
        snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1,
                 tm.tm_mday, tm.tm_hour, tm.tm_min);
    }
    return b;
}

std::vector<std::string> all(const ics_event_list_t &l)
{
    std::vector<std::string> v;
    for (int i = 0; i < l.count; i++) {
        v.push_back(when(l, i));
    }
    return v;
}

std::string vevent(const std::string &lines)
{
    return "BEGIN:VEVENT\nUID:e1\nSUMMARY:Rule\n" + lines + "END:VEVENT\n";
}

using V = std::vector<std::string>;

}  // namespace

TEST_F(CalendarIcsRrule, TheSecondMondayOfEveryMonth)
{
    auto l = parse(
        vevent("DTSTART:20260112T100000\nDTEND:20260112T110000\nRRULE:FREQ=MONTHLY;BYDAY=2MO\n"),
        local(2026, 9, 1), local(2027, 1, 1));
    EXPECT_EQ(all(l),
              (V{"2026-09-14 10:00", "2026-10-12 10:00", "2026-11-09 10:00", "2026-12-14 10:00"}));
    // a local time of day stays at 10:00 across the switch (25 October)
    EXPECT_EQ(l.events[1].end - l.events[1].start, 3600);
}

TEST_F(CalendarIcsRrule, TheLastFridayAndTheLastWeekday)
{
    auto fr = parse(vevent("DTSTART:20260130T100000\nRRULE:FREQ=MONTHLY;BYDAY=-1FR\n"),
                    local(2026, 9, 1), local(2027, 1, 1));
    EXPECT_EQ(all(fr),
              (V{"2026-09-25 10:00", "2026-10-30 10:00", "2026-11-27 10:00", "2026-12-25 10:00"}));
    auto wd = parse(
        vevent("DTSTART:20260130T170000\nRRULE:FREQ=MONTHLY;BYDAY=MO,TU,WE,TH,FR;BYSETPOS=-1\n"),
        local(2026, 9, 1), local(2027, 1, 1));
    EXPECT_EQ(all(wd),
              (V{"2026-09-30 17:00", "2026-10-30 17:00", "2026-11-30 17:00", "2026-12-31 17:00"}));
}

TEST_F(CalendarIcsRrule, MonthlyOnThe31stSkipsShortMonths)
{
    auto l = parse(vevent("DTSTART:20260131T080000\nRRULE:FREQ=MONTHLY;BYMONTHDAY=31\n"),
                   local(2026, 1, 1), local(2027, 1, 1));
    EXPECT_EQ(l.count, 7);
    EXPECT_EQ(when(l, 1), "2026-03-31 08:00");
}

TEST_F(CalendarIcsRrule, WeeklyOnSeveralDays)
{
    auto l = parse(vevent("DTSTART:20260902T090000\nRRULE:FREQ=WEEKLY;BYDAY=MO,WE,FR\n"),
                   local(2026, 9, 28), local(2026, 10, 12));
    EXPECT_EQ(all(l), (V{"2026-09-28 09:00", "2026-09-30 09:00", "2026-10-02 09:00",
                         "2026-10-05 09:00", "2026-10-07 09:00", "2026-10-09 09:00"}));
}

TEST_F(CalendarIcsRrule, YearlyBirthdayAndLeapDay)
{
    auto b =
        parse(vevent("DTSTART;VALUE=DATE:19901009\nRRULE:FREQ=YEARLY;BYMONTH=10;BYMONTHDAY=9\n"),
              local(2026, 10, 1), local(2026, 11, 1));
    ASSERT_EQ(b.count, 1);
    EXPECT_TRUE(b.events[0].all_day);
    EXPECT_EQ(when(b, 0), "2026-10-09");
    auto leap = parse(vevent("DTSTART:20240229T120000\nRRULE:FREQ=YEARLY\n"), local(2026, 1, 1),
                      local(2033, 1, 1));
    EXPECT_EQ(all(leap), (V{"2028-02-29 12:00", "2032-02-29 12:00"}));
}

TEST_F(CalendarIcsRrule, ASeriesBegunInWinterKeepsItsTimeInSummer)
{
    // the first of the month at 09:00, begun in January: 09:00 in July as well
    auto l = parse(vevent("DTSTART:20260101T090000\nRRULE:FREQ=MONTHLY;BYMONTHDAY=1\n"),
                   local(2026, 6, 1), local(2026, 9, 1));
    EXPECT_EQ(all(l), (V{"2026-06-01 09:00", "2026-07-01 09:00", "2026-08-01 09:00"}));
}

TEST_F(CalendarIcsRrule, ASeriesInUtcKeepsItsInstants)
{
    auto l = parse(vevent("DTSTART:20260101T080000Z\nRRULE:FREQ=MONTHLY;BYMONTHDAY=1\n"),
                   local(2026, 6, 1), local(2026, 9, 1));
    ASSERT_EQ(l.count, 3);
    EXPECT_EQ(l.events[0].start, at_utc(2026, 6, 1, 8));
    EXPECT_EQ(l.events[2].start, at_utc(2026, 8, 1, 8));
}

TEST_F(CalendarIcsRrule, AllDayMonthlyAndTheLengthInDays)
{
    auto l = parse(vevent("DTSTART;VALUE=DATE:20260105\nDTEND;VALUE=DATE:20260108\nRRULE:FREQ="
                          "MONTHLY;BYDAY=1MO\n"),
                   local(2026, 6, 1), local(2026, 9, 1));
    EXPECT_EQ(all(l), (V{"2026-06-01", "2026-07-06", "2026-08-03"}));
    for (int i = 0; i < l.count; i++) {
        EXPECT_TRUE(l.events[i].all_day);
        // three calendar days: an end at a local midnight, 72 h give or take the hour of a switch
        struct tm e;
        localtime_r(&l.events[i].end, &e);
        EXPECT_EQ(e.tm_hour * 60 + e.tm_min, 0) << i;
        EXPECT_NEAR((double) (l.events[i].end - l.events[i].start), 3 * 86400.0, 3600.0) << i;
    }
}

TEST_F(CalendarIcsRrule, AnInProgressMultiDayInstanceBeforeTheWindowIsFound)
{
    // 5 days from Monday 5 January each month; the window opens on the 7th of January
    auto l = parse(vevent("DTSTART;VALUE=DATE:20260105\nDTEND;VALUE=DATE:20260110\nRRULE:FREQ="
                          "MONTHLY;BYDAY=1MO\n"),
                   local(2026, 1, 7), local(2026, 1, 8));
    ASSERT_EQ(l.count, 1);
    EXPECT_EQ(when(l, 0), "2026-01-05");
}

TEST_F(CalendarIcsRrule, CountAndUntil)
{
    auto c = parse(vevent("DTSTART:20260102T100000\nRRULE:FREQ=MONTHLY;BYDAY=1FR;COUNT=4\n"),
                   local(2025, 1, 1), local(2027, 1, 1));
    EXPECT_EQ(all(c),
              (V{"2026-01-02 10:00", "2026-02-06 10:00", "2026-03-06 10:00", "2026-04-03 10:00"}));
    // UNTIL is inclusive, in UTC: 10:00 CET on 6 March is 09:00Z
    auto u = parse(
        vevent("DTSTART:20260102T100000\nRRULE:FREQ=MONTHLY;BYDAY=1FR;UNTIL=20260306T090000Z\n"),
        local(2025, 1, 1), local(2027, 1, 1));
    EXPECT_EQ(all(u), (V{"2026-01-02 10:00", "2026-02-06 10:00", "2026-03-06 10:00"}));
    auto d =
        parse(vevent("DTSTART;VALUE=DATE:20260105\nRRULE:FREQ=MONTHLY;BYDAY=1MO;UNTIL=20260302\n"),
              local(2026, 1, 1), local(2027, 1, 1));
    EXPECT_EQ(all(d), (V{"2026-01-05", "2026-02-02", "2026-03-02"}));
}

TEST_F(CalendarIcsRrule, ExdateRdateAndAMovedInstanceOnAMonthlySeries)
{
    auto l = parse(
        "BEGIN:VEVENT\nUID:m1\nSUMMARY:Board\nDTSTART:20260112T100000\nDTEND:20260112T110000\n"
        "RRULE:FREQ=MONTHLY;BYDAY=2MO\n"
        "EXDATE:20261012T100000\n"
        "RDATE:20261120T150000\n"
        "END:VEVENT\n"
        "BEGIN:VEVENT\nUID:m1\nSUMMARY:Board (moved)\nRECURRENCE-ID:20261109T100000\n"
        "DTSTART:20261110T140000\nDTEND:20261110T150000\nEND:VEVENT\n",
        local(2026, 9, 1), local(2027, 1, 1));
    // 14 Sep, (12 Oct excluded), (9 Nov moved to the 10th), 14 Dec, and the extra one on the 20th
    // of November
    EXPECT_EQ(all(l),
              (V{"2026-09-14 10:00", "2026-11-10 14:00", "2026-11-20 15:00", "2026-12-14 10:00"}));
}

TEST_F(CalendarIcsRrule, RulesThatAreNotTakenLeaveTheEventOutAndTheOthersAlone)
{
    std::string feed;
    const char *rules[] = {"FREQ=FOO",
                           "FREQ=DAILY;INTERVAL=0",
                           "FREQ=HOURLY",
                           "RSCALE=HEBREW;FREQ=YEARLY",
                           "FREQ=YEARLY;BYMONTHDAY=13",
                           "FREQ=WEEKLY;BYDAY=XX",
                           "garbage"};
    for (const char *r : rules) {
        feed += vevent(std::string("DTSTART:20261008T090000\nRRULE:") + r + "\n");
    }
    feed +=
        "BEGIN:VEVENT\nUID:ok\nSUMMARY:Fine\nDTSTART:20261008T090000\nRRULE:FREQ=WEEKLY\nEND:"
        "VEVENT\n";
    auto l = parse(feed, local(2026, 10, 5), local(2026, 10, 12));
    ASSERT_EQ(l.count, 1);
    EXPECT_STREQ(l.events[0].summary, "Fine");
}

TEST_F(CalendarIcsRrule, ASingleEventWithAnRdateIsASeriesOfTwo)
{
    // no RRULE: the event itself plus the extra date, no rule involved
    auto l =
        parse(vevent("DTSTART:20261008T090000\nDTEND:20261008T100000\nRDATE:20261015T090000\n"),
              local(2026, 10, 1), local(2026, 11, 1));
    EXPECT_EQ(all(l), (V{"2026-10-08 09:00", "2026-10-15 09:00"}));
    EXPECT_EQ(l.events[1].end - l.events[1].start, 3600);
}

TEST_F(CalendarIcsRrule, ASingleEventWithAnExdateIsGoneOnlyWhenItIsItsOwnDate)
{
    auto gone = parse(vevent("DTSTART:20261008T090000\nEXDATE:20261008T090000\n"),
                      local(2026, 10, 1), local(2026, 11, 1));
    EXPECT_EQ(gone.count, 0);
    auto stays = parse(vevent("DTSTART:20261008T090000\nEXDATE:20261015T090000\n"),
                       local(2026, 10, 1), local(2026, 11, 1));
    EXPECT_EQ(all(stays), (V{"2026-10-08 09:00"}));
    // an all-day event with an EXDATE of its day (a bare date)
    auto gone_day = parse(vevent("DTSTART;VALUE=DATE:20261008\nEXDATE;VALUE=DATE:20261008\n"),
                          local(2026, 10, 1), local(2026, 11, 1));
    EXPECT_EQ(gone_day.count, 0);
}

TEST_F(CalendarIcsRrule, ASingleEventOutsideTheWindowStaysOutEvenWithAnRdateInside)
{
    auto l = parse(vevent("DTSTART:20260108T090000\nRDATE:20261015T090000\n"), local(2026, 10, 1),
                   local(2026, 11, 1));
    EXPECT_EQ(all(l), (V{"2026-10-15 09:00"}));
}

TEST_F(CalendarIcsRrule, WhatTheOldReaderTookGivesTheSameInstances)
{
    // daily, weekly, interval, count, a single BYDAY - the rules the option-off reader understands
    struct {
        const char *rule;
        const char *dtstart;
    } same[] = {{"FREQ=DAILY", "20260101T090000"},
                {"FREQ=WEEKLY", "20260105T100000"},
                {"FREQ=DAILY;INTERVAL=3", "20260101T090000"},
                {"FREQ=WEEKLY;INTERVAL=2;BYDAY=MO", "20260105T100000"},
                {"FREQ=DAILY;COUNT=40", "20260920T090000"},
                {"FREQ=WEEKLY;WKST=MO;BYDAY=WE", "20260107T120000"}};
    for (auto &c : same) {
        auto l = parse(vevent(std::string("DTSTART:") + c.dtstart + "\nRRULE:" + c.rule + "\n"),
                       local(2026, 9, 28), local(2026, 11, 2));
        ASSERT_GT(l.count, 0) << c.rule;
        // wall-clock constant: the time of day is DTSTART's on every instance, across 25 October
        std::string hhmm =
            std::string(c.dtstart).substr(9, 2) + ":" + std::string(c.dtstart).substr(11, 2);
        for (int i = 0; i < l.count; i++) {
            EXPECT_EQ(when(l, i).substr(11), hhmm) << c.rule << " " << when(l, i);
        }
    }
}

TEST_F(CalendarIcsRrule, TheListLimitStillKeepsTheEarliest)
{
    // a daily series and a window of 100 days: more than ICS_MAX_EVENTS (48) instances, the first
    // 48 stay
    auto l = parse(vevent("DTSTART:20260101T090000\nRRULE:FREQ=DAILY\n"), local(2026, 3, 1),
                   local(2026, 6, 10));
    ASSERT_EQ(l.count, ICS_MAX_EVENTS);
    EXPECT_EQ(when(l, 0), "2026-03-01 09:00");
    EXPECT_EQ(when(l, ICS_MAX_EVENTS - 1), "2026-04-17 09:00");
}

TEST_F(CalendarIcsRrule, ADstartThatDoesNotMatchTheRuleIsStillTheFirstInstance)
{
    // DTSTART is a Wednesday, the rule says Mondays: the standard calls this undefined; libical
    // gives DTSTART and then the Mondays - which is what most calendar apps do as well. The
    // instances after DTSTART are the ones that matter for a window that is not at the start.
    auto l = parse(vevent("DTSTART:20261007T090000\nRRULE:FREQ=WEEKLY;BYDAY=MO\n"),
                   local(2026, 10, 12), local(2026, 10, 27));
    EXPECT_EQ(all(l), (V{"2026-10-12 09:00", "2026-10-19 09:00", "2026-10-26 09:00"}));
}

TEST_F(CalendarIcsRrule, ACancelledEventAndAnEventWithoutDtstartAreLeftOut)
{
    auto l =
        parse(vevent("DTSTART:20261007T090000\nSTATUS:CANCELLED\nRRULE:FREQ=MONTHLY;BYDAY=2MO\n") +
                  vevent("RRULE:FREQ=MONTHLY;BYDAY=2MO\n"),
              local(2026, 10, 1), local(2027, 1, 1));
    EXPECT_EQ(l.count, 0);
}

TEST_F(CalendarIcsRrule, AnExceptionCarryingTheRuleIsOneInstance)
{
    auto l = parse(
        "BEGIN:VEVENT\nUID:m1\nSUMMARY:Series\nDTSTART:20260105T100000\nRRULE:FREQ=MONTHLY;BYDAY="
        "1MO\nEND:VEVENT\n"
        "BEGIN:VEVENT\nUID:m1\nSUMMARY:Series "
        "(moved)\nRECURRENCE-ID:20260302T100000\nDTSTART:20260303T100000\n"
        "RRULE:FREQ=MONTHLY;BYDAY=1MO\nEND:VEVENT\n",
        local(2026, 1, 1), local(2026, 6, 1));
    EXPECT_EQ(all(l), (V{"2026-01-05 10:00", "2026-02-02 10:00", "2026-03-03 10:00",
                         "2026-04-06 10:00", "2026-05-04 10:00"}));
}
