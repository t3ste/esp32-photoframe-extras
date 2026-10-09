#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

extern "C" {
#include "calendar_ics.h"
#include "fake_http_fetch.h"
}

namespace
{

// All tests run with TZ forced to UTC0, matching test_cron.cpp's own
// precedent - makes a bare/TZID-qualified ("local time") DTSTART directly
// comparable to a "Z"-suffixed (UTC) one via plain mktime(), since with
// TZ=UTC0 local time *is* UTC.
class CalendarIcs : public ::testing::Test
{
   protected:
    void SetUp() override
    {
#if defined(_WIN32)
        _putenv_s("TZ", "UTC0");
        _tzset();
#else
        setenv("TZ", "UTC0", 1);
        tzset();
#endif
    }
};

time_t make_utc(int year, int mon, int day, int hour, int minute, int sec)
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

ics_event_list_t parse(const char *ics_text, time_t window_start, time_t window_end)
{
    std::vector<char> buf(ics_text, ics_text + strlen(ics_text) + 1);
    ics_event_list_t out;
    calendar_ics_parse(buf.data(), strlen(ics_text), window_start, window_end, &out);
    return out;
}

}  // namespace

TEST_F(CalendarIcs, SingleUtcEventWithinWindow)
{
    const char *ics =
        "BEGIN:VCALENDAR\n"
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T090000Z\n"
        "DTEND:20240115T100000Z\n"
        "SUMMARY:Team Meeting\n"
        "END:VEVENT\n"
        "END:VCALENDAR\n";

    time_t window_start = make_utc(2024, 1, 15, 0, 0, 0);
    time_t window_end = make_utc(2024, 1, 16, 0, 0, 0);
    ics_event_list_t out = parse(ics, window_start, window_end);

    ASSERT_EQ(out.count, 1);
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 15, 9, 0, 0));
    EXPECT_EQ(out.events[0].end, make_utc(2024, 1, 15, 10, 0, 0));
    EXPECT_FALSE(out.events[0].all_day);
    EXPECT_STREQ(out.events[0].summary, "Team Meeting");
}

TEST_F(CalendarIcs, LineFoldingReassemblesSummary)
{
    // The fold point has TWO leading spaces on the continuation line: one
    // natural word-separator (kept) and one fold-indicator (removed by
    // unfolding, per RFC 5545) - so the reassembled text reads with a
    // single space, not glued or double-spaced.
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T090000Z\n"
        "SUMMARY:This is a long summary that\n"
        "  continues on the next line\n"
        "END:VEVENT\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_STREQ(out.events[0].summary, "This is a long summary that continues on the next line");
}

TEST_F(CalendarIcs, MissingDtendAllDayDefaultsToOneDay)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART;VALUE=DATE:20240115\n"
        "SUMMARY:Holiday\n"
        "END:VEVENT\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_TRUE(out.events[0].all_day);
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 15, 0, 0, 0));
    EXPECT_EQ(out.events[0].end, make_utc(2024, 1, 16, 0, 0, 0));
}

TEST_F(CalendarIcs, MissingDtendTimedDefaultsToZeroDuration)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T090000Z\n"
        "SUMMARY:Quick call\n"
        "END:VEVENT\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_EQ(out.events[0].start, out.events[0].end);
}

TEST_F(CalendarIcs, TzidQualifiedTimestampParsed)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART;TZID=Europe/Berlin:20240115T090000\n"
        "SUMMARY:Local meeting\n"
        "END:VEVENT\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    // With TZ forced to UTC0 for this test process, mktime()'s local-time
    // interpretation coincides numerically with UTC.
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 15, 9, 0, 0));
}

TEST_F(CalendarIcs, NonVeventBlocksIgnored)
{
    const char *ics =
        "BEGIN:VCALENDAR\n"
        "BEGIN:VTIMEZONE\n"
        "TZID:Europe/Berlin\n"
        "BEGIN:STANDARD\n"
        "DTSTART:19701025T030000\n"
        "END:STANDARD\n"
        "END:VTIMEZONE\n"
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T090000Z\n"
        "SUMMARY:Real event\n"
        "BEGIN:VALARM\n"
        "TRIGGER:-PT15M\n"
        "END:VALARM\n"
        "END:VEVENT\n"
        "END:VCALENDAR\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_STREQ(out.events[0].summary, "Real event");
}

// A VALARM (RFC 5545 §3.6.6) is a sub-block nested inside VEVENT that can
// carry its own SUMMARY (e.g. an EMAIL-action alarm's message subject,
// distinct from the event's own title). Without tracking "inside VALARM"
// separately from "inside VEVENT", the alarm's SUMMARY line would overwrite
// the event's real one, since both share the same property name.
TEST_F(CalendarIcs, ValarmSummaryDoesNotOverwriteEventSummary)
{
    const char *ics =
        "BEGIN:VCALENDAR\n"
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T090000Z\n"
        "SUMMARY:Real event title\n"
        "BEGIN:VALARM\n"
        "ACTION:EMAIL\n"
        "TRIGGER:-P1D\n"
        "SUMMARY:Reminder email subject\n"
        "DESCRIPTION:Don't forget tomorrow\n"
        "END:VALARM\n"
        "END:VEVENT\n"
        "END:VCALENDAR\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_STREQ(out.events[0].summary, "Real event title");
}

TEST_F(CalendarIcs, EventFullyOutsideWindowExcluded)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240110T090000Z\n"
        "DTEND:20240110T100000Z\n"
        "SUMMARY:Too early\n"
        "END:VEVENT\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    EXPECT_EQ(out.count, 0);
}

TEST_F(CalendarIcs, EventSpanningWindowStartIncluded)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240114T220000Z\n"
        "DTEND:20240115T020000Z\n"
        "SUMMARY:Overnight\n"
        "END:VEVENT\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_STREQ(out.events[0].summary, "Overnight");
}

TEST_F(CalendarIcs, MissingSummaryHandledGracefully)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T090000Z\n"
        "END:VEVENT\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_STREQ(out.events[0].summary, "(untitled)");
}

TEST_F(CalendarIcs, DailyRruleExpandsWithinWindow)
{
    // DTSTART is well before the window; a 3-day window should surface
    // exactly the occurrences that land inside it.
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240101T090000Z\n"
        "DTEND:20240101T093000Z\n"
        "SUMMARY:Daily standup\n"
        "RRULE:FREQ=DAILY\n"
        "END:VEVENT\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 18, 0, 0, 0));
    ASSERT_EQ(out.count, 3);
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 15, 9, 0, 0));
    EXPECT_EQ(out.events[0].end, make_utc(2024, 1, 15, 9, 30, 0));
    EXPECT_EQ(out.events[1].start, make_utc(2024, 1, 16, 9, 0, 0));
    EXPECT_EQ(out.events[2].start, make_utc(2024, 1, 17, 9, 0, 0));
    for (int i = 0; i < out.count; i++) {
        EXPECT_STREQ(out.events[i].summary, "Daily standup");
    }
}

// Regression test for a real bug: expand_rrule()'s occurrence-count safety
// cap used to be a fixed "8", correct only as long as every caller stuck to
// the original 1-3 day rotation/agenda lookahead window. A wider window
// (e.g. the 30-day expansion agenda_manager.c's extra ICS sources use to
// build their flat cache, so large files don't need re-parsing on every
// wake) would have silently truncated a DAILY recurrence to its first ~8
// occurrences and dropped the rest.
TEST_F(CalendarIcs, DailyRruleExpandsAcrossWideThirtyDayWindow)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240101T090000Z\n"
        "DTEND:20240101T093000Z\n"
        "SUMMARY:Daily reminder\n"
        "RRULE:FREQ=DAILY\n"
        "END:VEVENT\n";

    time_t window_start = make_utc(2024, 1, 15, 0, 0, 0);
    time_t window_end = window_start + 30 * 86400;  // 30-day expansion window
    ics_event_list_t out = parse(ics, window_start, window_end);
    // ICS_MAX_EVENTS (48) is above the 30 daily occurrences of the window, so all
    // of them must come through - the point of this test is that the expansion
    // does not silently stop at ~8.
    EXPECT_EQ(out.count, 30);
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 15, 9, 0, 0));
    EXPECT_EQ(out.events[29].start, make_utc(2024, 2, 13, 9, 0, 0));
}

TEST_F(CalendarIcs, DailyRruleClosedFormJumpFromFarPastDtstart)
{
    // DTSTART is years before the window - this specifically exercises the
    // closed-form jump to the first in-window occurrence rather than a
    // slow (or wrong) day-by-day walk from DTSTART.
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20200101T080000Z\n"
        "SUMMARY:Ancient daily reminder\n"
        "RRULE:FREQ=DAILY\n"
        "END:VEVENT\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 15, 8, 0, 0));
}

TEST_F(CalendarIcs, WeeklyRruleExpandsOnCorrectDays)
{
    // DTSTART is a Monday (2024-01-01); weekly occurrences should land on
    // Mondays only, one per 7-day period.
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240101T100000Z\n"
        "SUMMARY:Weekly sync\n"
        "RRULE:FREQ=WEEKLY\n"
        "END:VEVENT\n";

    // Window covers 2024-01-08 (Mon) through 2024-01-21 (Sun) - two weekly
    // occurrences (Jan 8 and Jan 15) should fall inside it.
    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 8, 0, 0, 0), make_utc(2024, 1, 22, 0, 0, 0));
    ASSERT_EQ(out.count, 2);
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 8, 10, 0, 0));
    EXPECT_EQ(out.events[1].start, make_utc(2024, 1, 15, 10, 0, 0));
}

TEST_F(CalendarIcs, MultiDayWeeklyOccurrenceInProgressAtWindowStartIncluded)
{
    // DTSTART is a Monday, each occurrence spans 2 days (09:00 Mon to 09:00
    // Wed). The window starts mid-occurrence (Tuesday), after the first
    // occurrence's own start but before its end - the closed-form k0 jump
    // (which only finds the first occurrence whose START is >= window
    // start) must back up one period to still find this in-progress
    // occurrence, not silently skip it.
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240101T090000Z\n"
        "DTEND:20240103T090000Z\n"
        "SUMMARY:Multi-day trip\n"
        "RRULE:FREQ=WEEKLY\n"
        "END:VEVENT\n";

    ics_event_list_t out = parse(ics, make_utc(2024, 1, 2, 0, 0, 0), make_utc(2024, 1, 4, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 1, 9, 0, 0));
    EXPECT_EQ(out.events[0].end, make_utc(2024, 1, 3, 9, 0, 0));
}

TEST_F(CalendarIcs, IntervalTwoLandsOnCorrectBoundary)
{
    // DTSTART 2024-01-01, INTERVAL=2 (every other day): occurrences on
    // Jan 1, 3, 5, 7, 9, 11, 13, 15, 17... A window covering just Jan 16
    // should NOT include the Jan-15 or Jan-17 occurrences (off-by-one check
    // on the ceiling-division math that picks the first in-window k).
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240101T120000Z\n"
        "SUMMARY:Every other day\n"
        "RRULE:FREQ=DAILY;INTERVAL=2\n"
        "END:VEVENT\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 16, 0, 0, 0), make_utc(2024, 1, 17, 0, 0, 0));
    EXPECT_EQ(out.count, 0);

    ics_event_list_t out2 =
        parse(ics, make_utc(2024, 1, 17, 0, 0, 0), make_utc(2024, 1, 18, 0, 0, 0));
    ASSERT_EQ(out2.count, 1);
    EXPECT_EQ(out2.events[0].start, make_utc(2024, 1, 17, 12, 0, 0));
}

TEST_F(CalendarIcs, CountExcludesOccurrenceBeyondLimit)
{
    // DTSTART 2024-01-01, DAILY, COUNT=3 -> only Jan 1/2/3 ever occur. A
    // window covering Jan 10 should see nothing, even though the naive
    // (COUNT-less) pattern would otherwise land there.
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240101T090000Z\n"
        "SUMMARY:Three-day trial\n"
        "RRULE:FREQ=DAILY;COUNT=3\n"
        "END:VEVENT\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 10, 0, 0, 0), make_utc(2024, 1, 13, 0, 0, 0));
    EXPECT_EQ(out.count, 0);

    // But the window covering the original 3 days should still see them.
    ics_event_list_t out2 =
        parse(ics, make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 4, 0, 0, 0));
    EXPECT_EQ(out2.count, 3);
}

TEST_F(CalendarIcs, RruleUntilBoundsActiveSeriesMidWindow)
{
    // DTSTART 2024-01-01 (Mon), WEEKLY, UNTIL 2024-01-15 (inclusive per RFC
    // 5545) - occurrences on Jan 1/8/15 are valid, Jan 22 is not. A window
    // spanning Jan 1 through Jan 29 should see exactly the first three.
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240101T100000Z\n"
        "SUMMARY:Limited-run class\n"
        "RRULE:FREQ=WEEKLY;UNTIL=20240115T100000Z\n"
        "END:VEVENT\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 29, 0, 0, 0));
    ASSERT_EQ(out.count, 3);
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 1, 10, 0, 0));
    EXPECT_EQ(out.events[1].start, make_utc(2024, 1, 8, 10, 0, 0));
    EXPECT_EQ(out.events[2].start, make_utc(2024, 1, 15, 10, 0, 0));
}

TEST_F(CalendarIcs, RruleUntilInPastYieldsNoOccurrencesButStaysSupported)
{
    // UNTIL entirely before the query window - a series that has simply
    // ended, not an unsupported rule. Distinguishes this from the old
    // behavior (before UNTIL was supported) where the whole rule, and any
    // still-relevant part of it, would have been rejected outright by
    // fall-through to the generic unsupported-component branch.
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20200101T100000Z\n"
        "SUMMARY:Long-ended series\n"
        "RRULE:FREQ=WEEKLY;UNTIL=20200201T100000Z\n"
        "END:VEVENT\n";

    ics_event_list_t out = parse(ics, make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 8, 0, 0, 0));
    EXPECT_EQ(out.count, 0);
}

TEST_F(CalendarIcs, RruleWithBydayUnsupportedSkippedEntirely)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240101T090000Z\n"
        "SUMMARY:Weekdays only\n"
        "RRULE:FREQ=WEEKLY;BYDAY=MO,TU,WE,TH,FR\n"
        "END:VEVENT\n";

    ics_event_list_t out = parse(ics, make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 8, 0, 0, 0));
    EXPECT_EQ(out.count, 0);
}

TEST_F(CalendarIcs, RruleWithSingleBydayMatchingDtstartWeekdayAccepted)
{
    // DTSTART is a Monday (2024-01-01); a single BYDAY=MO matches it
    // exactly - the common case real calendar apps (Google/Outlook/Apple)
    // emit for a plain "repeat weekly" event even with no other special
    // pattern, so this is now accepted instead of failing closed like a
    // multi-value BYDAY does.
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240101T100000Z\n"
        "SUMMARY:Weekly sync\n"
        "RRULE:FREQ=WEEKLY;BYDAY=MO\n"
        "END:VEVENT\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 8, 0, 0, 0), make_utc(2024, 1, 22, 0, 0, 0));
    ASSERT_EQ(out.count, 2);
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 8, 10, 0, 0));
    EXPECT_EQ(out.events[1].start, make_utc(2024, 1, 15, 10, 0, 0));
}

TEST_F(CalendarIcs, RruleWithWkstAndSingleBydayAccepted)
{
    // Real-world regression: an all-day weekly event exported from a real
    // calendar app (aCalendar, via Android) - DTSTART is a Wednesday
    // (2026-09-16), and the RRULE includes WKST alongside a single BYDAY.
    // WKST alone used to reject the whole rule even after BYDAY itself was
    // accepted, since it fell through to the generic unsupported-component
    // branch - confirmed live against the reporting user's actual .ics
    // export.
    const char *ics =
        "BEGIN:VEVENT\n"
        "SUMMARY:16:30 Lia Sport\n"
        "DTSTART;VALUE=DATE:20260916\n"
        "DTEND;VALUE=DATE:20260917\n"
        "RRULE:FREQ=WEEKLY;WKST=MO;BYDAY=WE\n"
        "END:VEVENT\n";

    // Window covers 2026-09-16 (Wed) through 2026-09-30 inclusive - three
    // Wednesdays (16th, 23rd, 30th).
    ics_event_list_t out =
        parse(ics, make_utc(2026, 9, 16, 0, 0, 0), make_utc(2026, 10, 1, 0, 0, 0));
    ASSERT_EQ(out.count, 3);
    EXPECT_EQ(out.events[0].start, make_utc(2026, 9, 16, 0, 0, 0));
    EXPECT_EQ(out.events[1].start, make_utc(2026, 9, 23, 0, 0, 0));
    EXPECT_EQ(out.events[2].start, make_utc(2026, 9, 30, 0, 0, 0));
}

TEST_F(CalendarIcs, RruleWithSingleBydayMismatchingDtstartWeekdaySkipped)
{
    // DTSTART is a Monday, but BYDAY names Wednesday instead - a genuinely
    // different pattern (occurrences that don't follow DTSTART's own
    // weekday) this project's simple weekly-with-interval model can't
    // represent, so the whole event is still skipped entirely (fail
    // closed), same as any other unsupported RRULE.
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240101T100000Z\n"
        "SUMMARY:Mismatched weekday\n"
        "RRULE:FREQ=WEEKLY;BYDAY=WE\n"
        "END:VEVENT\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 8, 0, 0, 0), make_utc(2024, 1, 22, 0, 0, 0));
    EXPECT_EQ(out.count, 0);
}

TEST_F(CalendarIcs, RruleWithUnsupportedFreqSkippedEntirely)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240101T090000Z\n"
        "SUMMARY:Monthly report\n"
        "RRULE:FREQ=MONTHLY\n"
        "END:VEVENT\n";

    ics_event_list_t out = parse(ics, make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 2, 1, 0, 0, 0));
    EXPECT_EQ(out.count, 0);
}

// ---------------------------------------------------------------------------------------------
// Series in a zone with daylight saving time (the device's own: a bare or TZID-qualified time is
// read as local time, see calendar_ics.h). Every other test of this file runs with TZ=UTC0, where
// "n * 86400 s" and "the same wall-clock time on the n-th day" are the same thing - so none of
// them could show a series drifting an hour when the clocks change. These run in CET/CEST (the
// rules of Europe/Berlin as a POSIX string, so no tzdata is needed): spring forward on
// 2026-03-29 02:00, fall back on 2026-10-25 03:00.
// ---------------------------------------------------------------------------------------------
class CalendarIcsLocalTime : public CalendarIcs
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

// A real UTC instant (make_utc() above goes through mktime(), which is local time).
static time_t at_utc(int year, int mon, int day, int hour, int minute, int sec)
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

// Local wall-clock time of an instant as hour * 100 + minute (9:30 -> 930).
static int local_hhmm(time_t t)
{
    struct tm tm;
    localtime_r(&t, &tm);
    return tm.tm_hour * 100 + tm.tm_min;
}

static int local_mday(time_t t)
{
    struct tm tm;
    localtime_r(&t, &tm);
    return tm.tm_mday;
}

TEST_F(CalendarIcsLocalTime, DailySeriesStaysAtItsWallClockTimeAcrossSpringForward)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART;TZID=Europe/Berlin:20260325T090000\n"
        "DTEND;TZID=Europe/Berlin:20260325T100000\n"
        "SUMMARY:Standup\n"
        "RRULE:FREQ=DAILY\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2026, 3, 26, 0, 0, 0), make_utc(2026, 4, 2, 0, 0, 0));
    ASSERT_EQ(out.count, 7);
    for (int i = 0; i < out.count; i++) {
        EXPECT_EQ(local_hhmm(out.events[i].start), 900) << "day " << 26 + i;
        EXPECT_EQ(out.events[i].end - out.events[i].start, 3600);
    }
    // 29 March is the 23-hour day: the instant is one hour closer to the day before
    EXPECT_EQ(out.events[3].start - out.events[2].start, 23 * 3600);
}

TEST_F(CalendarIcsLocalTime, DailySeriesStaysAtItsWallClockTimeAcrossFallBack)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20261020T090000\n"
        "SUMMARY:Standup\n"
        "RRULE:FREQ=DAILY\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2026, 10, 22, 0, 0, 0), make_utc(2026, 10, 29, 0, 0, 0));
    ASSERT_EQ(out.count, 7);
    for (int i = 0; i < out.count; i++) {
        EXPECT_EQ(local_hhmm(out.events[i].start), 900) << "day " << 22 + i;
    }
}

TEST_F(CalendarIcsLocalTime, SeriesBegunYearsEarlierInTheOtherSeasonKeepsItsTime)
{
    // Begun in winter 2000, asked for in summer 2026 and in winter 2027: 09:00 both times (a drift
    // of one hour per switch would show up as 10:00 in summer).
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20000103T090000\n"
        "SUMMARY:Old habit\n"
        "RRULE:FREQ=DAILY\n"
        "END:VEVENT\n";
    ics_event_list_t summer =
        parse(ics, make_utc(2026, 7, 6, 0, 0, 0), make_utc(2026, 7, 9, 0, 0, 0));
    ASSERT_EQ(summer.count, 3);
    for (int i = 0; i < summer.count; i++) {
        EXPECT_EQ(local_hhmm(summer.events[i].start), 900);
    }
    ics_event_list_t winter =
        parse(ics, make_utc(2027, 1, 11, 0, 0, 0), make_utc(2027, 1, 14, 0, 0, 0));
    ASSERT_EQ(winter.count, 3);
    for (int i = 0; i < winter.count; i++) {
        EXPECT_EQ(local_hhmm(winter.events[i].start), 900);
    }
}

TEST_F(CalendarIcsLocalTime, WeeklySeriesKeepsItsTimeAcrossBothSwitches)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20260105T080000\n"
        "DTEND:20260105T093000\n"
        "SUMMARY:Monday meeting\n"
        "RRULE:FREQ=WEEKLY\n"
        "END:VEVENT\n";
    // 23 and 30 March (the second is after the switch), 5 April
    ics_event_list_t spring =
        parse(ics, make_utc(2026, 3, 23, 0, 0, 0), make_utc(2026, 4, 6, 12, 0, 0));
    ASSERT_EQ(spring.count, 3);
    for (int i = 0; i < spring.count; i++) {
        EXPECT_EQ(local_hhmm(spring.events[i].start), 800);
        EXPECT_EQ(spring.events[i].end - spring.events[i].start, 90 * 60);
    }
    ics_event_list_t autumn =
        parse(ics, make_utc(2026, 10, 19, 0, 0, 0), make_utc(2026, 11, 3, 0, 0, 0));
    ASSERT_EQ(autumn.count, 3);  // 19 and 26 October, 2 November
    for (int i = 0; i < autumn.count; i++) {
        EXPECT_EQ(local_hhmm(autumn.events[i].start), 800);
    }
}

TEST_F(CalendarIcsLocalTime, SeriesInUtcKeepsItsInstantsAndTheLocalTimeMoves)
{
    // "07:00Z every day" is the same instant all year (that is what the Z means); on the wall
    // clock it is 08:00 in winter and 09:00 in summer.
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20260325T070000Z\n"
        "SUMMARY:Satellite pass\n"
        "RRULE:FREQ=DAILY\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2026, 3, 26, 0, 0, 0), make_utc(2026, 4, 2, 0, 0, 0));
    ASSERT_EQ(out.count, 7);
    for (int i = 0; i < out.count; i++) {
        EXPECT_EQ(out.events[i].start, at_utc(2026, 3, 26 + i, 7, 0, 0)) << "day " << 26 + i;
    }
    EXPECT_EQ(local_hhmm(out.events[0].start), 800);  // 26 March, CET
    EXPECT_EQ(local_hhmm(out.events[6].start), 900);  // 1 April, CEST
}

TEST_F(CalendarIcsLocalTime, IntervalCountAndUntilWorkOnTheWallClock)
{
    const char *count_ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20260326T090000\n"
        "SUMMARY:Every other day\n"
        "RRULE:FREQ=DAILY;INTERVAL=2;COUNT=5\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(count_ics, make_utc(2026, 3, 20, 0, 0, 0), make_utc(2026, 4, 30, 0, 0, 0));
    ASSERT_EQ(out.count, 5);  // 26, 28, 30 March, 1 and 3 April
    const int days[] = {26, 28, 30, 1, 3};
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(local_mday(out.events[i].start), days[i]);
        EXPECT_EQ(local_hhmm(out.events[i].start), 900);
    }

    // UNTIL is inclusive: 09:00 CEST on 30 March is 07:00Z
    const char *until_ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20260326T090000\n"
        "SUMMARY:Until the 30th\n"
        "RRULE:FREQ=DAILY;UNTIL=20260330T070000Z\n"
        "END:VEVENT\n";
    out = parse(until_ics, make_utc(2026, 3, 20, 0, 0, 0), make_utc(2026, 4, 30, 0, 0, 0));
    EXPECT_EQ(out.count, 5);  // 26 .. 30 March
}

TEST_F(CalendarIcsLocalTime, AllDaySeriesStartsAtLocalMidnightAndEndsAtTheNextOne)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART;VALUE=DATE:20260327\n"
        "DTEND;VALUE=DATE:20260328\n"
        "SUMMARY:Holiday\n"
        "RRULE:FREQ=DAILY\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2026, 3, 27, 0, 0, 0), make_utc(2026, 4, 1, 0, 0, 0));
    ASSERT_EQ(out.count, 5);
    for (int i = 0; i < out.count; i++) {
        EXPECT_TRUE(out.events[i].all_day);
        EXPECT_EQ(out.events[i].start, make_utc(2026, 3, 27 + i, 0, 0, 0)) << "day " << 27 + i;
        EXPECT_EQ(out.events[i].end, make_utc(2026, 3, 28 + i, 0, 0, 0)) << "day " << 27 + i;
    }
    EXPECT_EQ(out.events[2].end - out.events[2].start, 23 * 3600);  // 29 March
}

TEST_F(CalendarIcsLocalTime, AllDayEventWithoutDtendEndsAtTheNextLocalMidnight)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART;VALUE=DATE:20260329\n"
        "SUMMARY:The short day\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2026, 3, 29, 0, 0, 0), make_utc(2026, 3, 30, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_EQ(out.events[0].end, make_utc(2026, 3, 30, 0, 0, 0));
}

TEST_F(CalendarIcsLocalTime, MultiDayAllDaySeriesKeepsItsLengthInDaysAcrossTheSwitch)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART;VALUE=DATE:20260327\n"
        "DTEND;VALUE=DATE:20260330\n"
        "SUMMARY:Long weekend\n"
        "RRULE:FREQ=WEEKLY\n"
        "END:VEVENT\n";
    // 27-29 March spans the switch; 3-5 April does not
    ics_event_list_t out =
        parse(ics, make_utc(2026, 3, 27, 0, 0, 0), make_utc(2026, 4, 10, 0, 0, 0));
    ASSERT_EQ(out.count, 2);
    EXPECT_EQ(out.events[0].start, make_utc(2026, 3, 27, 0, 0, 0));
    EXPECT_EQ(out.events[0].end, make_utc(2026, 3, 30, 0, 0, 0));
    EXPECT_EQ(out.events[1].start, make_utc(2026, 4, 3, 0, 0, 0));
    EXPECT_EQ(out.events[1].end, make_utc(2026, 4, 6, 0, 0, 0));
}

TEST_F(CalendarIcsLocalTime, MultiDayAllDayOccurrenceInProgressAfterTheSwitchIsFound)
{
    // started on the 28th (before the switch), still going on the 30th, when the window opens
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART;VALUE=DATE:20260328\n"
        "DTEND;VALUE=DATE:20260402\n"
        "SUMMARY:Conference\n"
        "RRULE:FREQ=WEEKLY;COUNT=2\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2026, 3, 31, 0, 0, 0), make_utc(2026, 4, 1, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_EQ(out.events[0].start, make_utc(2026, 3, 28, 0, 0, 0));
}

TEST_F(CalendarIcsLocalTime, AnHourAndAHalfStaysAnHourAndAHalfOnTheSwitchDay)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20260327T200000\n"
        "DTEND:20260327T213000\n"
        "SUMMARY:Evening class\n"
        "RRULE:FREQ=DAILY\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2026, 3, 28, 0, 0, 0), make_utc(2026, 3, 31, 0, 0, 0));
    ASSERT_EQ(out.count, 3);
    for (int i = 0; i < out.count; i++) {
        EXPECT_EQ(local_hhmm(out.events[i].start), 2000);
        EXPECT_EQ(out.events[i].end - out.events[i].start, 90 * 60);
    }
}

TEST_F(CalendarIcsLocalTime, ATimeThatDoesNotExistOnTheSwitchDayStillGivesOneInstance)
{
    // 02:30 does not exist on 29 March (02:00 -> 03:00): one instance that day, on that day.
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20260327T023000\n"
        "SUMMARY:Night shift handover\n"
        "RRULE:FREQ=DAILY\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2026, 3, 28, 0, 0, 0), make_utc(2026, 3, 31, 0, 0, 0));
    ASSERT_EQ(out.count, 3);
    EXPECT_EQ(local_hhmm(out.events[0].start), 230);
    EXPECT_EQ(local_mday(out.events[1].start), 29);
    EXPECT_EQ(local_hhmm(out.events[2].start), 230);
}

TEST_F(CalendarIcsLocalTime, ATimeThatOccursTwiceOnTheSwitchDayGivesOneInstance)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20261023T023000\n"
        "SUMMARY:Night shift handover\n"
        "RRULE:FREQ=DAILY\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2026, 10, 24, 0, 0, 0), make_utc(2026, 10, 27, 0, 0, 0));
    ASSERT_EQ(out.count, 3);
    EXPECT_EQ(local_mday(out.events[1].start), 25);
}

TEST_F(CalendarIcsLocalTime, BydayOfADtstartInUtcIsTheWeekdayInUtc)
{
    // 23:30Z on Wednesday 1 April is already Thursday 02 April locally (CEST): the BYDAY=WE of the
    // rule is about the Wednesday of DTSTART's own zone, so this is a plain weekly series.
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20260401T233000Z\n"
        "SUMMARY:Late call\n"
        "RRULE:FREQ=WEEKLY;BYDAY=WE\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2026, 4, 6, 0, 0, 0), make_utc(2026, 4, 16, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_EQ(out.events[0].start, at_utc(2026, 4, 8, 23, 30, 0));
}

// ---------------------------------------------------------------------------------------------
// Exceptions of a series: EXDATE, RDATE, RECURRENCE-ID (a moved or called-off instance), STATUS.
// Before these were read, an excluded instance was still shown, a moved one was shown twice and a
// cancelled one stayed - all of it wrong, none of it left out.
// ---------------------------------------------------------------------------------------------

// A weekly Monday series of 2024-01-01 10:00Z (an hour long) with extra lines between the rule and
// END:VEVENT, for the tests of EXDATE/RDATE.
static std::string weekly_with(const std::string &extra_lines, const char *rrule = "FREQ=WEEKLY")
{
    return std::string(
               "BEGIN:VEVENT\n"
               "UID:series-1\n"
               "DTSTART:20240101T100000Z\n"
               "DTEND:20240101T110000Z\n"
               "SUMMARY:Weekly sync\n"
               "RRULE:") +
           rrule + "\n" + extra_lines + "END:VEVENT\n";
}

TEST_F(CalendarIcs, ExdateRemovesTheNamedInstanceOnly)
{
    std::string ics = weekly_with("EXDATE:20240108T100000Z\n");
    ics_event_list_t out =
        parse(ics.c_str(), make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 29, 0, 0, 0));
    ASSERT_EQ(out.count, 3);
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 1, 10, 0, 0));
    EXPECT_EQ(out.events[1].start, make_utc(2024, 1, 15, 10, 0, 0));
    EXPECT_EQ(out.events[2].start, make_utc(2024, 1, 22, 10, 0, 0));
}

TEST_F(CalendarIcs, ExdateTakesSeveralLinesAndCommaLists)
{
    std::string ics = weekly_with(
        "EXDATE:20240108T100000Z,20240115T100000Z\n"
        "EXDATE:20240122T100000Z\n");
    ics_event_list_t out =
        parse(ics.c_str(), make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 2, 5, 0, 0, 0));
    ASSERT_EQ(out.count, 2);  // 1 and 29 January
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 1, 10, 0, 0));
    EXPECT_EQ(out.events[1].start, make_utc(2024, 1, 29, 10, 0, 0));
}

TEST_F(CalendarIcs, ExdateInAnotherNotationOfTheSameInstantMatches)
{
    // DTSTART in local time (TZ=UTC0 here, so local = UTC), EXDATE in UTC with the zone's own TZID
    // form
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240101T100000\n"
        "SUMMARY:Local series\n"
        "RRULE:FREQ=WEEKLY\n"
        "EXDATE;TZID=Europe/Berlin:20240108T100000\n"
        "EXDATE:20240115T100000Z\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 23, 0, 0, 0));
    ASSERT_EQ(out.count, 2);  // 1 and 22 January
}

TEST_F(CalendarIcs, ExcludedInstancesStillCountTowardsCount)
{
    // COUNT=3 makes the instances of 1, 8 and 15 January; excluding the 8th leaves two, it does not
    // make the series run on to the 22nd.
    std::string ics = weekly_with("EXDATE:20240108T100000Z\n", "FREQ=WEEKLY;COUNT=3");
    ics_event_list_t out =
        parse(ics.c_str(), make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 3, 1, 0, 0, 0));
    ASSERT_EQ(out.count, 2);
    EXPECT_EQ(out.events[1].start, make_utc(2024, 1, 15, 10, 0, 0));
}

TEST_F(CalendarIcs, ExdateOfAnAllDaySeriesIsADate)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART;VALUE=DATE:20240101\n"
        "SUMMARY:Every day\n"
        "RRULE:FREQ=DAILY\n"
        "EXDATE;VALUE=DATE:20240103\n"
        "END:VEVENT\n";
    ics_event_list_t out = parse(ics, make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 6, 0, 0, 0));
    ASSERT_EQ(out.count, 4);
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 1, 0, 0, 0));
    EXPECT_EQ(out.events[1].start, make_utc(2024, 1, 2, 0, 0, 0));
    EXPECT_EQ(out.events[2].start, make_utc(2024, 1, 4, 0, 0, 0));
    EXPECT_EQ(out.events[3].start, make_utc(2024, 1, 5, 0, 0, 0));
}

TEST_F(CalendarIcs, ExdateOfATimedSeriesGivenAsADateExcludesThatDay)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240101T100000Z\n"
        "SUMMARY:Every day\n"
        "RRULE:FREQ=DAILY\n"
        "EXDATE;VALUE=DATE:20240103\n"
        "END:VEVENT\n";
    ics_event_list_t out = parse(ics, make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 5, 0, 0, 0));
    ASSERT_EQ(out.count, 3);
    EXPECT_EQ(out.events[2].start, make_utc(2024, 1, 4, 10, 0, 0));
}

TEST_F(CalendarIcs, AnExdateLineLongerThanTheLineBufferIsReadInFull)
{
    // 40 values on one line: more than 600 characters, a series that is excluded for nearly every
    // day of the window - and nothing but the right days.
    std::string list;
    for (int day = 2; day <= 41; day++) {  // 2 January .. 10 February
        char item[32];
        snprintf(item, sizeof(item), "%s2024%02d%02dT100000Z", list.empty() ? "" : ",",
                 day <= 31 ? 1 : 2, day <= 31 ? day : day - 31);
        list += item;
    }
    ASSERT_GT(list.size(), 600u);
    const std::string ics =
        "BEGIN:VEVENT\nDTSTART:20240101T100000Z\nSUMMARY:Nearly never\nRRULE:FREQ=DAILY\nEXDATE:" +
        list + "\nEND:VEVENT\n";
    ics_event_list_t out =
        parse(ics.c_str(), make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 2, 15, 0, 0, 0));
    ASSERT_EQ(out.count, 5);  // 1 January and 11-14 February
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 1, 10, 0, 0));
    EXPECT_EQ(out.events[1].start, make_utc(2024, 2, 11, 10, 0, 0));
    EXPECT_EQ(out.events[4].start, make_utc(2024, 2, 14, 10, 0, 0));
}

TEST_F(CalendarIcs, AnExdateThatCannotBeReadDropsTheWholeEvent)
{
    std::string ics = weekly_with("EXDATE;VALUE=PERIOD:20240108T100000Z/20240108T110000Z\n");
    ics_event_list_t out =
        parse(ics.c_str(), make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 29, 0, 0, 0));
    EXPECT_EQ(out.count, 0);
    ics = weekly_with("EXDATE:not-a-date\n");
    out = parse(ics.c_str(), make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 29, 0, 0, 0));
    EXPECT_EQ(out.count, 0);
}

TEST_F(CalendarIcs, ExdateDoesNotMakeAnUnsupportedRuleSupported)
{
    std::string ics = weekly_with("EXDATE:20240108T100000Z\n", "FREQ=MONTHLY");
    ics_event_list_t out =
        parse(ics.c_str(), make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 3, 1, 0, 0, 0));
    EXPECT_EQ(out.count, 0);
}

TEST_F(CalendarIcs, RdateAddsAnInstanceWithTheLengthOfTheSeries)
{
    std::string ics = weekly_with("RDATE:20240110T150000Z\n");
    ics_event_list_t out =
        parse(ics.c_str(), make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 15, 0, 0, 0));
    ASSERT_EQ(out.count, 3);  // 1 January, the RDATE of the 10th, 8 January - sorted
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 1, 10, 0, 0));
    EXPECT_EQ(out.events[1].start, make_utc(2024, 1, 8, 10, 0, 0));
    EXPECT_EQ(out.events[2].start, make_utc(2024, 1, 10, 15, 0, 0));
    EXPECT_EQ(out.events[2].end, make_utc(2024, 1, 10, 16, 0, 0));
}

TEST_F(CalendarIcs, AnRdateThatTheRuleMakesAnywayIsOneInstance)
{
    std::string ics = weekly_with("RDATE:20240108T100000Z\n");
    ics_event_list_t out =
        parse(ics.c_str(), make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    EXPECT_EQ(out.count, 3);
}

TEST_F(CalendarIcs, RdateOnAnEventWithoutARule)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240101T100000Z\n"
        "DTEND:20240101T120000Z\n"
        "SUMMARY:Workshop\n"
        "RDATE:20240103T100000Z,20240105T100000Z\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 10, 0, 0, 0));
    ASSERT_EQ(out.count, 3);
    EXPECT_EQ(out.events[1].start, make_utc(2024, 1, 3, 10, 0, 0));
    EXPECT_EQ(out.events[1].end, make_utc(2024, 1, 3, 12, 0, 0));
}

TEST_F(CalendarIcs, ExdateAlsoRemovesAnRdateAndASingleEventsOwnDate)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240101T100000Z\n"
        "SUMMARY:Called off\n"
        "RDATE:20240103T100000Z\n"
        "EXDATE:20240103T100000Z\n"
        "EXDATE:20240101T100000Z\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 10, 0, 0, 0));
    EXPECT_EQ(out.count, 0);
}

TEST_F(CalendarIcs, AnRdatePeriodDropsTheWholeEvent)
{
    std::string ics = weekly_with("RDATE;VALUE=PERIOD:20240110T150000Z/PT2H\n");
    ics_event_list_t out =
        parse(ics.c_str(), make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 15, 0, 0, 0));
    EXPECT_EQ(out.count, 0);
}

TEST_F(CalendarIcsLocalTime, ExdateInUtcMatchesALocalSeriesAcrossTheSwitch)
{
    // Monday 30 March 09:00 CEST and 6 April: the EXDATE names the instant in UTC (07:00Z)
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART;TZID=Europe/Berlin:20260105T090000\n"
        "SUMMARY:Monday\n"
        "RRULE:FREQ=WEEKLY\n"
        "EXDATE:20260330T070000Z\n"
        "EXDATE;TZID=Europe/Berlin:20260406T090000\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2026, 3, 23, 0, 0, 0), make_utc(2026, 4, 14, 0, 0, 0));
    ASSERT_EQ(out.count, 2);  // 23 March and 13 April
    EXPECT_EQ(local_mday(out.events[0].start), 23);
    EXPECT_EQ(local_mday(out.events[1].start), 13);
}

// ---- RECURRENCE-ID ---------------------------------------------------------------------------

static const char *kSeriesWithMovedAndCancelled =
    "BEGIN:VEVENT\n"
    "UID:series-1\n"
    "DTSTART:20240101T100000Z\n"
    "DTEND:20240101T110000Z\n"
    "SUMMARY:Weekly sync\n"
    "RRULE:FREQ=WEEKLY\n"
    "END:VEVENT\n"
    "BEGIN:VEVENT\n"
    "UID:series-1\n"
    "RECURRENCE-ID:20240108T100000Z\n"
    "DTSTART:20240109T140000Z\n"
    "DTEND:20240109T150000Z\n"
    "SUMMARY:Weekly sync (moved)\n"
    "END:VEVENT\n"
    "BEGIN:VEVENT\n"
    "UID:series-1\n"
    "RECURRENCE-ID:20240115T100000Z\n"
    "DTSTART:20240115T100000Z\n"
    "SUMMARY:Weekly sync\n"
    "STATUS:CANCELLED\n"
    "END:VEVENT\n";

TEST_F(CalendarIcs, AMovedInstanceReplacesTheOriginalAndACancelledOneIsGone)
{
    ics_event_list_t out = parse(kSeriesWithMovedAndCancelled, make_utc(2024, 1, 1, 0, 0, 0),
                                 make_utc(2024, 1, 23, 0, 0, 0));
    ASSERT_EQ(out.count, 3);
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 1, 10, 0, 0));
    EXPECT_STREQ(out.events[0].summary, "Weekly sync");
    EXPECT_EQ(out.events[1].start, make_utc(2024, 1, 9, 14, 0, 0));
    EXPECT_STREQ(out.events[1].summary, "Weekly sync (moved)");
    EXPECT_EQ(out.events[2].start, make_utc(2024, 1, 22, 10, 0, 0));
}

TEST_F(CalendarIcs, TheExceptionMayComeBeforeTheSeriesInTheFeed)
{
    // the same three events, the series last
    std::string text = kSeriesWithMovedAndCancelled;
    size_t master_end = text.find("BEGIN:VEVENT", 5);
    std::string reordered = text.substr(master_end) + text.substr(0, master_end);
    ics_event_list_t out =
        parse(reordered.c_str(), make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 23, 0, 0, 0));
    ASSERT_EQ(out.count, 3);
    EXPECT_STREQ(out.events[1].summary, "Weekly sync (moved)");
}

TEST_F(CalendarIcs, AnExceptionOfAnotherUidLeavesTheSeriesAlone)
{
    std::string text = kSeriesWithMovedAndCancelled;
    // rename the UID of the two exceptions (not of the series, which is the first)
    size_t first = text.find("UID:series-1");
    size_t pos = text.find("UID:series-1", first + 1);
    while (pos != std::string::npos) {
        text.replace(pos, 12, "UID:other-99");
        pos = text.find("UID:series-1", pos + 1);
    }
    ics_event_list_t out =
        parse(text.c_str(), make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 23, 0, 0, 0));
    // series: 1, 8, 15, 22 January; the exceptions are events of their own now: 9 January (moved)
    // and the called-off one is left out
    ASSERT_EQ(out.count, 5);
}

TEST_F(CalendarIcs, ThisAndFutureHidesTheInstancesFromThereOnAndTheChangedEventIsLeftOut)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "UID:series-1\n"
        "DTSTART:20240101T100000Z\n"
        "SUMMARY:Weekly sync\n"
        "RRULE:FREQ=WEEKLY\n"
        "END:VEVENT\n"
        "BEGIN:VEVENT\n"
        "UID:series-1\n"
        "RECURRENCE-ID;RANGE=THISANDFUTURE:20240115T100000Z\n"
        "DTSTART:20240115T130000Z\n"
        "SUMMARY:Weekly sync (new time)\n"
        "END:VEVENT\n";
    ics_event_list_t out = parse(ics, make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 2, 5, 0, 0, 0));
    ASSERT_EQ(out.count, 2);  // 1 and 8 January only - the changed ones are not guessed
    EXPECT_EQ(out.events[1].start, make_utc(2024, 1, 8, 10, 0, 0));
}

TEST_F(CalendarIcs, ExceptionsOfAllDayInstancesAreMatchedByDate)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "UID:trip\n"
        "DTSTART;VALUE=DATE:20240101\n"
        "SUMMARY:Daily check\n"
        "RRULE:FREQ=DAILY\n"
        "END:VEVENT\n"
        "BEGIN:VEVENT\n"
        "UID:trip\n"
        "RECURRENCE-ID;VALUE=DATE:20240103\n"
        "DTSTART;VALUE=DATE:20240110\n"
        "SUMMARY:Daily check (later)\n"
        "END:VEVENT\n";
    ics_event_list_t out = parse(ics, make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 5, 0, 0, 0));
    ASSERT_EQ(out.count, 3);  // 1, 2, 4 January (the 3rd moved out of the window)
    EXPECT_EQ(out.events[2].start, make_utc(2024, 1, 4, 0, 0, 0));
}

TEST_F(CalendarIcs, ExpandedInstancesWithTheirOwnRecurrenceIdAreJustEvents)
{
    // what a CalDAV server sends for "expand": one VEVENT per instance, each named by its own start
    const char *ics =
        "BEGIN:VEVENT\n"
        "UID:s\nRECURRENCE-ID:20240101T100000Z\nDTSTART:20240101T100000Z\nSUMMARY:Sync\n"
        "END:VEVENT\n"
        "BEGIN:VEVENT\n"
        "UID:s\nRECURRENCE-ID:20240108T100000Z\nDTSTART:20240108T100000Z\nSUMMARY:Sync\n"
        "END:VEVENT\n"
        "BEGIN:VEVENT\n"
        "UID:s\nRECURRENCE-ID:20240115T100000Z\nDTSTART:20240115T100000Z\nSUMMARY:Sync\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 29, 0, 0, 0));
    EXPECT_EQ(out.count, 3);
}

TEST_F(CalendarIcs, MoreExceptionsThanTheTableHoldsAreBoundedAndHarmless)
{
    // 600 called-off instances of 600 other series (more than the table of exceptions holds), next
    // to one ordinary daily series: the feed is still read, the series is untouched, the list is in
    // order.
    std::string ics =
        "BEGIN:VEVENT\n"
        "UID:mine\n"
        "DTSTART:20240101T100000Z\n"
        "SUMMARY:Daily\n"
        "RRULE:FREQ=DAILY\n"
        "END:VEVENT\n";
    for (int i = 0; i < 600; i++) {
        char item[200];
        snprintf(item, sizeof(item),
                 "BEGIN:VEVENT\n"
                 "UID:other-%d\n"
                 "RECURRENCE-ID:20240110T100000Z\n"
                 "DTSTART:20240110T100000Z\n"
                 "SUMMARY:Gone\n"
                 "STATUS:CANCELLED\n"
                 "END:VEVENT\n",
                 i);
        ics += item;
    }
    ics_event_list_t out =
        parse(ics.c_str(), make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 1, 31, 0, 0, 0));
    ASSERT_EQ(out.count, 30);
    for (int i = 1; i < out.count; i++) {
        EXPECT_LT(out.events[i - 1].start, out.events[i].start);
    }
}

// ---- STATUS -----------------------------------------------------------------------------------

TEST_F(CalendarIcs, ACancelledEventIsNotShown)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T090000Z\n"
        "SUMMARY:Off\n"
        "STATUS:CANCELLED\n"
        "END:VEVENT\n"
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T100000Z\n"
        "SUMMARY:Maybe\n"
        "STATUS:TENTATIVE\n"
        "END:VEVENT\n"
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T110000Z\n"
        "SUMMARY:Off too\n"
        "STATUS:Cancelled\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_STREQ(out.events[0].summary, "Maybe");
}

TEST_F(CalendarIcs, ACancelledSeriesIsNotShown)
{
    std::string ics = weekly_with("STATUS:CANCELLED\n");
    ics_event_list_t out =
        parse(ics.c_str(), make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 2, 1, 0, 0, 0));
    EXPECT_EQ(out.count, 0);
}

TEST_F(CalendarIcs, AnExceptionIsOneInstanceEvenIfItCarriesTheSeriesRuleToo)
{
    // Some producers copy the RRULE (and EXDATE) of the series into the exception. It is still the
    // one instance it names - not a new series that starts there.
    const char *ics =
        "BEGIN:VEVENT\n"
        "UID:series-1\n"
        "DTSTART:20240101T100000Z\n"
        "SUMMARY:Every third week\n"
        "RRULE:FREQ=WEEKLY;INTERVAL=3\n"
        "END:VEVENT\n"
        "BEGIN:VEVENT\n"
        "UID:series-1\n"
        "RECURRENCE-ID:20240122T100000Z\n"
        "DTSTART:20240123T100000Z\n"
        "SUMMARY:Every third week (moved)\n"
        "RRULE:FREQ=WEEKLY;INTERVAL=3\n"
        "EXDATE:20240612T100000Z\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 3, 20, 0, 0, 0));
    // series: 1 January, 22 January (replaced), 12 February, 4 March; the exception: 23 January
    // only
    ASSERT_EQ(out.count, 4);
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 1, 10, 0, 0));
    EXPECT_EQ(out.events[1].start, make_utc(2024, 1, 23, 10, 0, 0));
    EXPECT_STREQ(out.events[1].summary, "Every third week (moved)");
    EXPECT_EQ(out.events[2].start, make_utc(2024, 2, 12, 10, 0, 0));
    EXPECT_EQ(out.events[3].start, make_utc(2024, 3, 4, 10, 0, 0));
}

// ---- DURATION -----------------------------------------------------------------------------------

TEST_F(CalendarIcs, DurationGivesTheLengthOfAnEventWithoutDtend)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T090000Z\n"
        "DURATION:PT1H30M\n"
        "SUMMARY:Class\n"
        "END:VEVENT\n"
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T120000Z\n"
        "DURATION:P1DT2H\n"
        "SUMMARY:Retreat\n"
        "END:VEVENT\n"
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T140000Z\n"
        "DURATION:P1W\n"
        "SUMMARY:Week-long\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    ASSERT_EQ(out.count, 3);
    EXPECT_EQ(out.events[0].end - out.events[0].start, 90 * 60);
    EXPECT_EQ(out.events[1].end - out.events[1].start, 26 * 3600);
    EXPECT_EQ(out.events[2].end - out.events[2].start, 7 * 86400);
}

TEST_F(CalendarIcs, DtendWinsOverDurationAndABadDurationIsIgnored)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T090000Z\n"
        "DTEND:20240115T100000Z\n"
        "DURATION:PT5H\n"
        "SUMMARY:Both\n"
        "END:VEVENT\n"
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T110000Z\n"
        "DURATION:-PT1H\n"
        "SUMMARY:Negative\n"
        "END:VEVENT\n"
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T120000Z\n"
        "DURATION:P3M\n"
        "SUMMARY:Months\n"
        "END:VEVENT\n"
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T130000Z\n"
        "DURATION:nonsense\n"
        "SUMMARY:Text\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    ASSERT_EQ(out.count, 4);
    EXPECT_EQ(out.events[0].end - out.events[0].start, 3600);
    for (int i = 1; i < 4; i++) {
        EXPECT_EQ(out.events[i].end, out.events[i].start) << i;
    }
}

TEST_F(CalendarIcs, ASeriesWithDurationReachesIntoTheWindowFromTheInstanceBefore)
{
    // started at 22:00 on the 14th and lasts 4 hours: still going on when the window opens at
    // midnight
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240101T220000Z\n"
        "DURATION:PT4H\n"
        "SUMMARY:Night shift\n"
        "RRULE:FREQ=DAILY\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 15, 1, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_EQ(out.events[0].start, make_utc(2024, 1, 14, 22, 0, 0));
    EXPECT_EQ(out.events[0].end, make_utc(2024, 1, 15, 2, 0, 0));
}

TEST_F(CalendarIcsLocalTime, AnAllDayDurationEndsAtALocalMidnight)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART;VALUE=DATE:20260328\n"
        "DURATION:P2D\n"
        "SUMMARY:Weekend\n"
        "END:VEVENT\n";
    ics_event_list_t out =
        parse(ics, make_utc(2026, 3, 28, 0, 0, 0), make_utc(2026, 3, 31, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_EQ(out.events[0].end, make_utc(2026, 3, 30, 0, 0, 0));
}

// ---- the list limit ---------------------------------------------------------------------------

TEST_F(CalendarIcs, AFullListKeepsTheEarliestEventsNotTheFirstInTheFile)
{
    // 60 events, the latest one first in the file: the 48 that survive are the 48 that start first
    std::string ics;
    for (int i = 60; i >= 1; i--) {
        char item[160];
        snprintf(item, sizeof(item),
                 "BEGIN:VEVENT\nDTSTART:202401%02dT%02d0000Z\nSUMMARY:E%d\nEND:VEVENT\n",
                 1 + i / 24, i % 24, i);
        ics += item;
    }
    ics_event_list_t out =
        parse(ics.c_str(), make_utc(2024, 1, 1, 0, 0, 0), make_utc(2024, 2, 1, 0, 0, 0));
    ASSERT_EQ(out.count, ICS_MAX_EVENTS);
    EXPECT_STREQ(out.events[0].summary, "E1");
    EXPECT_STREQ(out.events[ICS_MAX_EVENTS - 1].summary, "E48");
    for (int i = 1; i < out.count; i++) {
        EXPECT_LT(out.events[i - 1].start, out.events[i].start);
    }
}

TEST_F(CalendarIcs, CrlfLineEndingsTolerated)
{
    const char *ics =
        "BEGIN:VEVENT\r\n"
        "DTSTART:20240115T090000Z\r\n"
        "SUMMARY:CRLF event\r\n"
        "END:VEVENT\r\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_STREQ(out.events[0].summary, "CRLF event");
}

TEST_F(CalendarIcs, MultipleEventsSortedByStart)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T150000Z\n"
        "SUMMARY:Later\n"
        "END:VEVENT\n"
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T090000Z\n"
        "SUMMARY:Earlier\n"
        "END:VEVENT\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    ASSERT_EQ(out.count, 2);
    EXPECT_STREQ(out.events[0].summary, "Earlier");
    EXPECT_STREQ(out.events[1].summary, "Later");
}

TEST_F(CalendarIcs, TextEscapesDecoded)
{
    const char *ics =
        "BEGIN:VEVENT\n"
        "DTSTART:20240115T090000Z\n"
        "SUMMARY:Comma\\, semicolon\\; and backslash\\\\ here\n"
        "END:VEVENT\n";

    ics_event_list_t out =
        parse(ics, make_utc(2024, 1, 15, 0, 0, 0), make_utc(2024, 1, 16, 0, 0, 0));
    ASSERT_EQ(out.count, 1);
    EXPECT_STREQ(out.events[0].summary, "Comma, semicolon; and backslash\\ here");
}

TEST_F(CalendarIcs, NoMatchingEventsIsNotAnError)
{
    const char *ics = "BEGIN:VCALENDAR\nEND:VCALENDAR\n";
    ics_event_list_t out;
    std::vector<char> buf(ics, ics + strlen(ics) + 1);
    esp_err_t err = calendar_ics_parse(buf.data(), strlen(ics), make_utc(2024, 1, 15, 0, 0, 0),
                                       make_utc(2024, 1, 16, 0, 0, 0), &out);
    EXPECT_EQ(err, ESP_OK);
    EXPECT_EQ(out.count, 0);
}

TEST_F(CalendarIcs, NullBodyIsInvalidArg)
{
    ics_event_list_t out;
    esp_err_t err = calendar_ics_parse(nullptr, 0, 0, 0, &out);
    EXPECT_EQ(err, ESP_ERR_INVALID_ARG);
    EXPECT_EQ(out.count, 0);
}

TEST_F(CalendarIcs, NullOutIsInvalidArg)
{
    char body[] = "x";
    esp_err_t err = calendar_ics_parse(body, 1, 0, 0, nullptr);
    EXPECT_EQ(err, ESP_ERR_INVALID_ARG);
}

// calendar_ics_has_upcoming_event() - used by agenda_manager.c's extra ICS
// sources (holidays/school-holidays/etc.) to decide whether to inject a
// "this source is stale, please update it" reminder, since those three
// sources never refresh themselves.
TEST_F(CalendarIcs, HasUpcomingEventEmptyListIsFalse)
{
    ics_event_list_t list;
    memset(&list, 0, sizeof(list));
    EXPECT_FALSE(calendar_ics_has_upcoming_event(&list, make_utc(2024, 1, 15, 0, 0, 0)));
}

TEST_F(CalendarIcs, HasUpcomingEventAllInPastIsFalse)
{
    ics_event_list_t list;
    memset(&list, 0, sizeof(list));
    list.count = 2;
    list.events[0].start = make_utc(2020, 1, 1, 0, 0, 0);
    list.events[0].end = make_utc(2020, 1, 2, 0, 0, 0);
    list.events[1].start = make_utc(2021, 6, 1, 0, 0, 0);
    list.events[1].end = make_utc(2021, 6, 2, 0, 0, 0);
    EXPECT_FALSE(calendar_ics_has_upcoming_event(&list, make_utc(2024, 1, 15, 0, 0, 0)));
}

TEST_F(CalendarIcs, HasUpcomingEventOneFutureIsTrue)
{
    ics_event_list_t list;
    memset(&list, 0, sizeof(list));
    list.count = 2;
    list.events[0].start = make_utc(2020, 1, 1, 0, 0, 0);
    list.events[0].end = make_utc(2020, 1, 2, 0, 0, 0);
    list.events[1].start = make_utc(2030, 1, 1, 0, 0, 0);
    list.events[1].end = make_utc(2030, 1, 2, 0, 0, 0);
    EXPECT_TRUE(calendar_ics_has_upcoming_event(&list, make_utc(2024, 1, 15, 0, 0, 0)));
}

TEST_F(CalendarIcs, HasUpcomingEventEndExactlyAtNowIsFalse)
{
    // end > now is the exact rule (event.end == now is treated as fully
    // passed, matching event_touches_day()'s own end-is-exclusive
    // convention in agenda_renderer.c).
    ics_event_list_t list;
    memset(&list, 0, sizeof(list));
    list.count = 1;
    time_t now = make_utc(2024, 1, 15, 12, 0, 0);
    list.events[0].start = make_utc(2024, 1, 15, 11, 0, 0);
    list.events[0].end = now;
    EXPECT_FALSE(calendar_ics_has_upcoming_event(&list, now));
}

TEST_F(CalendarIcs, HasUpcomingEventNullListIsFalse)
{
    EXPECT_FALSE(calendar_ics_has_upcoming_event(nullptr, make_utc(2024, 1, 15, 0, 0, 0)));
}

// calendar_ics_write_expanded_cache()/calendar_ics_read_expanded_cache() -
// the flat, already-expanded cache agenda_manager.c's extra ICS sources use
// to avoid re-parsing a large raw .ics file on every agenda wake.
class CalendarIcsExpandedCache : public CalendarIcs
{
   protected:
    const char *path = "test_expanded_cache_tmp.txt";

    void TearDown() override
    {
        remove(path);
    }
};

TEST_F(CalendarIcsExpandedCache, RoundTripPreservesFields)
{
    ics_event_list_t list;
    memset(&list, 0, sizeof(list));
    list.count = 2;
    list.events[0].start = make_utc(2024, 3, 1, 9, 0, 0);
    list.events[0].end = make_utc(2024, 3, 1, 10, 0, 0);
    list.events[0].all_day = false;
    strncpy(list.events[0].summary, "Team Meeting", sizeof(list.events[0].summary) - 1);
    list.events[1].start = make_utc(2024, 3, 2, 0, 0, 0);
    list.events[1].end = make_utc(2024, 3, 3, 0, 0, 0);
    list.events[1].all_day = true;
    strncpy(list.events[1].summary, "Public Holiday", sizeof(list.events[1].summary) - 1);

    ASSERT_EQ(calendar_ics_write_expanded_cache(path, &list), ESP_OK);

    ics_event_list_t out;
    ASSERT_EQ(calendar_ics_read_expanded_cache(path, &out), ESP_OK);
    ASSERT_EQ(out.count, 2);
    EXPECT_EQ(out.events[0].start, list.events[0].start);
    EXPECT_EQ(out.events[0].end, list.events[0].end);
    EXPECT_FALSE(out.events[0].all_day);
    EXPECT_STREQ(out.events[0].summary, "Team Meeting");
    EXPECT_EQ(out.events[1].start, list.events[1].start);
    EXPECT_EQ(out.events[1].end, list.events[1].end);
    EXPECT_TRUE(out.events[1].all_day);
    EXPECT_STREQ(out.events[1].summary, "Public Holiday");
}

TEST_F(CalendarIcsExpandedCache, EmbeddedTabAndNewlineSanitizedNotCorrupting)
{
    ics_event_list_t list;
    memset(&list, 0, sizeof(list));
    list.count = 1;
    list.events[0].start = make_utc(2024, 3, 1, 0, 0, 0);
    list.events[0].end = make_utc(2024, 3, 2, 0, 0, 0);
    list.events[0].all_day = true;
    strncpy(list.events[0].summary, "Weird\tTitle\nWith Newline",
            sizeof(list.events[0].summary) - 1);

    ASSERT_EQ(calendar_ics_write_expanded_cache(path, &list), ESP_OK);

    ics_event_list_t out;
    ASSERT_EQ(calendar_ics_read_expanded_cache(path, &out), ESP_OK);
    // A raw embedded tab/newline would otherwise be mistaken for a field
    // separator or a second line - write-side sanitization replaces them
    // with spaces, so this must round-trip as exactly one event.
    ASSERT_EQ(out.count, 1);
    EXPECT_STREQ(out.events[0].summary, "Weird Title With Newline");
}

TEST_F(CalendarIcsExpandedCache, MissingFileIsNotFound)
{
    ics_event_list_t out;
    EXPECT_EQ(calendar_ics_read_expanded_cache("does_not_exist.txt", &out), ESP_ERR_NOT_FOUND);
    EXPECT_EQ(out.count, 0);
}

TEST_F(CalendarIcsExpandedCache, MalformedLineSkippedFailSoft)
{
    FILE *fp = fopen(path, "wb");
    ASSERT_NE(fp, nullptr);
    fprintf(fp, "not a valid line\n");
    fprintf(fp, "%lld\t%lld\t%d\t%s\n", (long long) make_utc(2024, 3, 1, 0, 0, 0),
            (long long) make_utc(2024, 3, 2, 0, 0, 0), 1, "Valid Entry");
    fclose(fp);

    ics_event_list_t out;
    ASSERT_EQ(calendar_ics_read_expanded_cache(path, &out), ESP_OK);
    ASSERT_EQ(out.count, 1);
    EXPECT_STREQ(out.events[0].summary, "Valid Entry");
}

// webcal:// subscription links (FEATURE_WEBCAL) - what calendar apps hand out for
// "subscribe" - are fetched over https://; everything else passes through untouched.
TEST(CalendarIcsWebcal, WebcalBecomesHttps)
{
    char buf[ICS_URL_MAX_LEN];
    EXPECT_STREQ(calendar_ics_resolve_url("webcal://cal.example.org/a/b.ics?x=1", buf, sizeof(buf)),
                 "https://cal.example.org/a/b.ics?x=1");
    EXPECT_STREQ(calendar_ics_resolve_url("webcals://cal.example.org/a.ics", buf, sizeof(buf)),
                 "https://cal.example.org/a.ics");
}

TEST(CalendarIcsWebcal, SchemeIsCaseInsensitive)
{
    char buf[ICS_URL_MAX_LEN];
    EXPECT_STREQ(calendar_ics_resolve_url("WEBCAL://Cal.Example.org/A.ics", buf, sizeof(buf)),
                 "https://Cal.Example.org/A.ics");  // only the scheme is rewritten
    EXPECT_STREQ(calendar_ics_resolve_url("WebCals://x.example/y", buf, sizeof(buf)),
                 "https://x.example/y");
}

TEST(CalendarIcsWebcal, OtherUrlsAreReturnedUnchanged)
{
    char buf[ICS_URL_MAX_LEN] = "untouched";
    const char *https_url = "https://cal.example.org/a.ics";
    const char *http_url = "http://cal.example.org/a.ics";
    const char *odd = "webcal.example.org/a.ics";  // "webcal" in the host, no scheme
    EXPECT_EQ(calendar_ics_resolve_url(https_url, buf, sizeof(buf)), https_url);
    EXPECT_EQ(calendar_ics_resolve_url(http_url, buf, sizeof(buf)), http_url);
    EXPECT_EQ(calendar_ics_resolve_url(odd, buf, sizeof(buf)), odd);
    EXPECT_STREQ(buf, "untouched");  // nothing was written
}

TEST(CalendarIcsWebcal, TooSmallBufferIsReported)
{
    char tiny[12];
    EXPECT_EQ(calendar_ics_resolve_url("webcal://cal.example.org/a.ics", tiny, sizeof(tiny)),
              nullptr);
    char exact[sizeof("https://a.example/b")];
    EXPECT_STREQ(calendar_ics_resolve_url("webcal://a.example/b", exact, sizeof(exact)),
                 "https://a.example/b");
}

#if FEATURE_CALDAV
// CalDAV (FEATURE_CALDAV): a caldav(s):// address is answered by a REPORT - the events of the
// window come back as a multistatus with the iCalendar text inside, repeats expanded by the
// server. http_fetch_report() is the scriptable stub of fake_http_fetch.c.
namespace
{

// What a Sabre-style server sends: two objects, CR written as &#13;, one summary with an entity.
const char *kMultistatus =
    "<?xml version=\"1.0\"?><d:multistatus xmlns:d=\"DAV:\" "
    "xmlns:cal=\"urn:ietf:params:xml:ns:caldav\">"
    "<d:response><d:href>/dav/a.ics</d:href><d:propstat><d:prop>"
    "<cal:calendar-data>BEGIN:VCALENDAR&#13;\nVERSION:2.0&#13;\nBEGIN:VEVENT&#13;\n"
    "DTSTART:20261001T090000Z&#13;\nDTEND:20261001T100000Z&#13;\nSUMMARY:Dentist &amp; co&#13;\n"
    "END:VEVENT&#13;\nEND:VCALENDAR&#13;\n</cal:calendar-data></d:prop></d:propstat></d:response>"
    "<d:response><d:href>/dav/b.ics</d:href><d:propstat><d:prop>"
    "<cal:calendar-data>BEGIN:VCALENDAR&#13;\nBEGIN:VEVENT&#13;\n"
    "DTSTART:20261002T120000Z&#13;\nDTEND:20261002T130000Z&#13;\nSUMMARY:Plumber&#13;\n"
    "END:VEVENT&#13;\nEND:VCALENDAR&#13;\n</cal:calendar-data></d:prop></d:propstat></d:response>"
    "</d:multistatus>";

class CalendarIcsCaldav : public CalendarIcs
{
   protected:
    void SetUp() override
    {
        CalendarIcs::SetUp();
        fake_report_reset();
        fake_report.response = kMultistatus;
    }
};

}  // namespace

TEST_F(CalendarIcsCaldav, QueriesTheServerAndParsesTheAnswer)
{
    ics_event_list_t out;
    time_t start = make_utc(2026, 10, 1, 0, 0, 0), end = make_utc(2026, 10, 8, 0, 0, 0);
    ASSERT_EQ(calendar_ics_fetch("caldavs://u:p@cal.example.org/dav/me/cal/", 0, start, end,
                                 nullptr, nullptr, nullptr, 0, &out),
              ESP_OK);
    ASSERT_EQ(fake_report.calls, 1);
    EXPECT_STREQ(fake_report.url[0], "https://u:p@cal.example.org/dav/me/cal/");
    std::string body = fake_report.body[0];
    EXPECT_NE(body.find("<c:time-range start=\"20261001T000000Z\" end=\"20261008T000000Z\"/>"),
              std::string::npos);
    EXPECT_NE(body.find("<c:expand "), std::string::npos);
    ASSERT_EQ(out.count, 2);
    EXPECT_STREQ(out.events[0].summary, "Dentist & co");
    EXPECT_EQ(out.events[0].start, make_utc(2026, 10, 1, 9, 0, 0));
    EXPECT_STREQ(out.events[1].summary, "Plumber");
}

TEST_F(CalendarIcsCaldav, PlainHttpSchemeIsKeptAsHttp)
{
    ics_event_list_t out;
    ASSERT_EQ(
        calendar_ics_fetch("caldav://192.168.1.5:5232/me/cal/", 0, make_utc(2026, 10, 1, 0, 0, 0),
                           make_utc(2026, 10, 8, 0, 0, 0), nullptr, nullptr, nullptr, 0, &out),
        ESP_OK);
    EXPECT_STREQ(fake_report.url[0], "http://192.168.1.5:5232/me/cal/");
}

TEST_F(CalendarIcsCaldav, ServerWithoutExpandIsAskedAgainWithout)
{
    fake_report.status[0] = 400;  // first try (expand) refused, the second gets the default 207
    ics_event_list_t out;
    ASSERT_EQ(
        calendar_ics_fetch("caldavs://cal.example.org/dav/", 0, make_utc(2026, 10, 1, 0, 0, 0),
                           make_utc(2026, 10, 8, 0, 0, 0), nullptr, nullptr, nullptr, 0, &out),
        ESP_OK);
    ASSERT_EQ(fake_report.calls, 2);
    EXPECT_NE(std::string(fake_report.body[0]).find("<c:expand "), std::string::npos);
    EXPECT_EQ(std::string(fake_report.body[1]).find("expand"), std::string::npos);
    EXPECT_NE(std::string(fake_report.body[1]).find("<c:time-range "), std::string::npos);
    EXPECT_EQ(out.count, 2);
}

TEST_F(CalendarIcsCaldav, RefusedLoginIsNotTriedAgain)
{
    fake_report.status[0] = 401;
    ics_event_list_t out;
    EXPECT_NE(calendar_ics_fetch("caldavs://u:wrong@cal.example.org/dav/", 0,
                                 make_utc(2026, 10, 1, 0, 0, 0), make_utc(2026, 10, 8, 0, 0, 0),
                                 nullptr, nullptr, nullptr, 0, &out),
              ESP_OK);
    EXPECT_EQ(fake_report.calls, 1);
    EXPECT_EQ(out.count, 0);
}

TEST_F(CalendarIcsCaldav, OtherClientErrorsAreNotRetriedEither)
{
    for (int status : {403, 404, 405}) {
        fake_report_reset();
        fake_report.response = kMultistatus;
        fake_report.status[0] = status;
        ics_event_list_t out;
        EXPECT_NE(
            calendar_ics_fetch("caldavs://cal.example.org/dav/", 0, make_utc(2026, 10, 1, 0, 0, 0),
                               make_utc(2026, 10, 8, 0, 0, 0), nullptr, nullptr, nullptr, 0, &out),
            ESP_OK)
            << status;
        EXPECT_EQ(fake_report.calls, 1) << status;
    }
}

TEST_F(CalendarIcsCaldav, EmptyMultistatusIsAnEmptyCalendar)
{
    fake_report.response = "<d:multistatus xmlns:d=\"DAV:\"></d:multistatus>";
    ics_event_list_t out;
    ASSERT_EQ(
        calendar_ics_fetch("caldavs://cal.example.org/dav/", 0, make_utc(2026, 10, 1, 0, 0, 0),
                           make_utc(2026, 10, 8, 0, 0, 0), nullptr, nullptr, nullptr, 0, &out),
        ESP_OK);
    EXPECT_EQ(out.count, 0);
}

TEST_F(CalendarIcsCaldav, OneShotSourceIsCachedAsPlainIcs)
{
    const char *path = "caldav_once_test.ics";
    std::remove(path);
    ASSERT_EQ(calendar_ics_fetch_once("caldavs://cal.example.org/dav/holidays/", 0, path), ESP_OK);
    ASSERT_EQ(fake_report.calls, 1);
    // a window from last week to a year ahead: 366+7 days
    std::string body = fake_report.body[0];
    size_t at = body.find("<c:time-range start=\"");
    ASSERT_NE(at, std::string::npos);
    FILE *fp = std::fopen(path, "rb");
    ASSERT_NE(fp, nullptr);
    char data[4096] = {0};
    size_t n = std::fread(data, 1, sizeof(data) - 1, fp);
    std::fclose(fp);
    std::remove(path);
    std::string text(data, n);
    EXPECT_NE(text.find("SUMMARY:Dentist & co"), std::string::npos);
    EXPECT_NE(text.find("SUMMARY:Plumber"), std::string::npos);
    EXPECT_EQ(text.find('<'), std::string::npos);
}

TEST_F(CalendarIcsCaldav, OneShotFailureWritesNothing)
{
    const char *path = "caldav_once_fail_test.ics";
    std::remove(path);
    fake_report.status[0] = 404;
    EXPECT_NE(calendar_ics_fetch_once("caldavs://cal.example.org/dav/x/", 0, path), ESP_OK);
    FILE *fp = std::fopen(path, "rb");
    EXPECT_EQ(fp, nullptr);
    if (fp) {
        std::fclose(fp);
        std::remove(path);
    }
}
// The real thing: what a Radicale server answered to the frame's query (host_tests/data/caldav).
namespace
{

std::string read_fixture(const char *name)
{
    std::string path = std::string(CALDAV_TEST_DATA_DIR) + "/" + name;
    FILE *fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        return "";
    }
    std::string data;
    char chunk[4096];
    size_t n;
    while ((n = std::fread(chunk, 1, sizeof(chunk), fp)) > 0) {
        data.append(chunk, n);
    }
    std::fclose(fp);
    return data;
}

std::vector<std::string> summaries(const ics_event_list_t &list)
{
    std::vector<std::string> names;
    for (int i = 0; i < list.count; i++) {
        names.push_back(list.events[i].summary);
    }
    std::sort(names.begin(), names.end());
    return names;
}

}  // namespace

TEST_F(CalendarIcsCaldav, RadicaleAnswerWithExpandShowsEveryOccurrence)
{
    std::string xml = read_fixture("radicale-report-expand.xml");
    ASSERT_FALSE(xml.empty());
    fake_report.response = xml.c_str();
    ics_event_list_t out;
    ASSERT_EQ(calendar_ics_fetch("caldavs://u:p@cal.example.org/tester/frame/", 0,
                                 make_utc(2026, 10, 1, 0, 0, 0), make_utc(2026, 10, 4, 0, 0, 0),
                                 nullptr, nullptr, nullptr, 0, &out),
              ESP_OK);
    std::vector<std::string> names = summaries(out);
    // the daily rule with an EXDATE (2 Oct left out) gives 1 and 3 Oct; the monthly and the
    // BYDAY=MO,TH rules - which the on-device reader cannot expand - arrive as single events
    ASSERT_EQ(names.size(), 7u);
    EXPECT_EQ(std::count(names.begin(), names.end(), "Daily except tomorrow"), 2);
    EXPECT_EQ(std::count(names.begin(), names.end(), "Monthly bins"), 1);
    EXPECT_EQ(std::count(names.begin(), names.end(), "Two days a week"), 1);
    EXPECT_EQ(std::count(names.begin(), names.end(), "Dentist & co"), 1);
    EXPECT_EQ(std::count(names.begin(), names.end(), "Weekly team"), 1);
    bool umlaut = false;
    for (const std::string &n : names) {
        umlaut = umlaut || n.find("Umlaute") == 0;
    }
    EXPECT_TRUE(umlaut);
}

TEST_F(CalendarIcsCaldav, RadicaleAnswerWithoutExpandKeepsWhatTheReaderKnows)
{
    // a server that refuses `expand`: the second answer holds the master events with their rules,
    // of which the frame's reader expands the simple ones and skips the rest (fail closed)
    std::string xml = read_fixture("radicale-report-plain.xml");
    ASSERT_FALSE(xml.empty());
    fake_report.response = xml.c_str();
    fake_report.status[0] = 422;
    ics_event_list_t out;
    ASSERT_EQ(calendar_ics_fetch("caldavs://u:p@cal.example.org/tester/frame/", 0,
                                 make_utc(2026, 10, 1, 0, 0, 0), make_utc(2026, 10, 4, 0, 0, 0),
                                 nullptr, nullptr, nullptr, 0, &out),
              ESP_OK);
    EXPECT_EQ(fake_report.calls, 2);
    std::vector<std::string> names = summaries(out);
    EXPECT_EQ(std::count(names.begin(), names.end(), "Dentist & co"), 1);
    EXPECT_EQ(std::count(names.begin(), names.end(), "Weekly team"), 1);
    EXPECT_EQ(std::count(names.begin(), names.end(), "Monthly bins"), 0);
    EXPECT_EQ(std::count(names.begin(), names.end(), "Two days a week"), 0);
    EXPECT_LT(names.size(), 7u);
}
#endif
