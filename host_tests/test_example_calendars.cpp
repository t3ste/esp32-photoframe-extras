// Exercises the demo package's example calendars/ToDo file
// (examples/waveshare_photopainter_73/) through the firmware's own parsers
// (calendar_ics_parse()/todo_parse()), not a hand-rolled re-implementation -
// so a change to either the example files or the parser itself that breaks
// the demo shows up here instead of only on a real device. See
// docs/DEMO_PLAN.md for what these files are for.

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

extern "C" {
#include "calendar_ics.h"
#include "todo.h"
}

#ifndef EXAMPLE_DIR
#error "EXAMPLE_DIR must point at examples/waveshare_photopainter_73"
#endif

namespace
{

// Every example event uses a floating (no "Z", no TZID) DTSTART, so it is
// interpreted in whatever TZ is active when calendar_ics_parse() runs (see
// docs/CALENDAR_RRULE_SUPPORT.md) - the same rule the firmware itself
// applies. Force the demo's own configured zone (Paris) for the whole
// suite, exactly the POSIX string examples/.../demo-config-url.json sets
// as "timezone", so this test proves what the imported config will
// actually show, not some other zone's arithmetic.
class ExampleCalendars : public ::testing::Test
{
   protected:
    void SetUp() override
    {
#if defined(_WIN32)
        _putenv_s("TZ", "CET-1CEST,M3.5.0,M10.5.0/3");
        _tzset();
#else
        setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
        tzset();
#endif
    }
};

std::string read_file(const std::string &name)
{
    std::string path = std::string(EXAMPLE_DIR) + "/" + name;
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) {
        ADD_FAILURE() << "could not open " << path;
        return "";
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string body(static_cast<size_t>(len), '\0');
    size_t got = fread(&body[0], 1, static_cast<size_t>(len), f);
    fclose(f);
    body.resize(got);
    return body;
}

// Local (Paris) midnight of a given day - mirrors make_utc() in
// test_calendar_ics.cpp but goes through mktime() (local time) rather than
// timegm(), since the example events are floating/local, not "Z"-suffixed.
time_t local_midnight(int year, int mon, int day)
{
    struct tm tm {
    };
    tm.tm_year = year - 1900;
    tm.tm_mon = mon - 1;
    tm.tm_mday = day;
    tm.tm_isdst = -1;
    return mktime(&tm);
}

ics_event_list_t parse_calendar(const std::string &name, time_t window_start, time_t window_end)
{
    std::string body = read_file("calendars/" + name);
    std::vector<char> buf(body.begin(), body.end());
    buf.push_back('\0');
    ics_event_list_t out;
    calendar_ics_parse(buf.data(), body.size(), window_start, window_end, &out);
    return out;
}

int count_on_day(const ics_event_list_t &list, time_t day_start, time_t day_end)
{
    int n = 0;
    for (int i = 0; i < list.count; i++) {
        if (list.events[i].start >= day_start && list.events[i].start < day_end) {
            n++;
        }
    }
    return n;
}

// 2026-01-05 is a Monday - the week every example calendar's DTSTART is
// anchored to (see the calendar files' own comments/UIDs).
constexpr int kBaseYear = 2026;
constexpr int kBaseMonth = 1;
constexpr int kBaseMonday = 5;

}  // namespace

// Calendar A ("Family"): 8 events on Monday, 1 on each other weekday - 14 a
// week - matching docs/DEMO_PLAN.md's table exactly, on the anchor week.
TEST_F(ExampleCalendars, CalendarAAnchorWeek)
{
    time_t week_start = local_midnight(kBaseYear, kBaseMonth, kBaseMonday);
    time_t week_end = week_start + 7 * 86400;
    ics_event_list_t out = parse_calendar("calendar-a.ics", week_start, week_end);
    EXPECT_EQ(out.count, 14);
    EXPECT_EQ(count_on_day(out, week_start, week_start + 86400), 8) << "Monday";
    for (int day = 1; day < 7; day++) {
        time_t day_start = week_start + day * 86400;
        EXPECT_EQ(count_on_day(out, day_start, day_start + 86400), 1) << "day offset " << day;
    }
}

// The weekly rules must keep firing indefinitely (no accidental COUNT/UNTIL
// in the source file) - spot-check several weeks spread across two years
// instead of walking all ~730 days.
TEST_F(ExampleCalendars, CalendarARecursForTwoYears)
{
    const int week_offsets[] = {0, 26, 52, 78, 104};  // ~0, 6, 12, 18, 24 months out
    for (int weeks : week_offsets) {
        time_t week_start =
            local_midnight(kBaseYear, kBaseMonth, kBaseMonday) + (time_t) weeks * 7 * 86400;
        time_t week_end = week_start + 7 * 86400;
        ics_event_list_t out = parse_calendar("calendar-a.ics", week_start, week_end);
        EXPECT_EQ(out.count, 14) << "week offset " << weeks;
        EXPECT_EQ(count_on_day(out, week_start, week_start + 86400), 8)
            << "Monday, week offset " << weeks;
    }
}

// Calendar B ("Work"): 5 weekday events plus one multi-day (Sat-Mon) event
// that overlaps the anchor week exactly once, not once per day it spans.
TEST_F(ExampleCalendars, CalendarBAnchorWeek)
{
    time_t week_start = local_midnight(kBaseYear, kBaseMonth, kBaseMonday);
    time_t week_end = week_start + 7 * 86400;
    ics_event_list_t out = parse_calendar("calendar-b.ics", week_start, week_end);
    EXPECT_EQ(out.count, 6);
    bool found_multiday = false;
    for (int i = 0; i < out.count; i++) {
        if (out.events[i].end - out.events[i].start == 3 * 86400) {
            found_multiday = true;
            EXPECT_TRUE(out.events[i].all_day);
        }
    }
    EXPECT_TRUE(found_multiday);
}

TEST_F(ExampleCalendars, CalendarCAnchorWeek)
{
    time_t week_start = local_midnight(kBaseYear, kBaseMonth, kBaseMonday);
    ics_event_list_t out = parse_calendar("calendar-c.ics", week_start, week_start + 7 * 86400);
    EXPECT_EQ(out.count, 3);
}

TEST_F(ExampleCalendars, CalendarDAnchorWeek)
{
    time_t week_start = local_midnight(kBaseYear, kBaseMonth, kBaseMonday);
    ics_event_list_t out = parse_calendar("calendar-d.ics", week_start, week_start + 7 * 86400);
    EXPECT_EQ(out.count, 5);
}

TEST_F(ExampleCalendars, CalendarEAnchorWeek)
{
    time_t week_start = local_midnight(kBaseYear, kBaseMonth, kBaseMonday);
    ics_event_list_t out = parse_calendar("calendar-e.ics", week_start, week_start + 7 * 86400);
    EXPECT_EQ(out.count, 3);
}

// Every extra ICS source (C/D/E) is re-expanded over a 30-day window
// (AGENDA_EXTRA_ICS_EXPAND_DAYS, main/config.h) and must stay under
// ICS_MAX_EVENTS (48) there - the actual cap load_extra_ics_source() relies
// on, not just the 1-week anchor check above.
TEST_F(ExampleCalendars, ExtraCalendarsStayUnderEventCapOver30Days)
{
    time_t window_start = local_midnight(kBaseYear, kBaseMonth, kBaseMonday);
    time_t window_end = window_start + 30 * 86400;
    for (const char *name : {"calendar-c.ics", "calendar-d.ics", "calendar-e.ics"}) {
        ics_event_list_t out = parse_calendar(name, window_start, window_end);
        EXPECT_LT(out.count, ICS_MAX_EVENTS) << name;
        EXPECT_GT(out.count, 0) << name;
    }
}

TEST(ExampleTodo, ParsesWithinLimits)
{
    std::string body = read_file("todo.txt");
    ASSERT_FALSE(body.empty());
    todo_list_t out;
    esp_err_t err = todo_parse(body.c_str(), body.size(), &out);
    EXPECT_EQ(err, ESP_OK);
    EXPECT_GT(out.count, 0);
    EXPECT_LE(out.count, TODO_MAX_ITEMS);
    for (int i = 0; i < out.count; i++) {
        // "x " (completed) lines are excluded by todo_parse() itself - this
        // just confirms none of the example's OWN lines start that way
        // (which would mean an intended task silently never shows up).
        EXPECT_NE(out.items[i].text[0], '\0') << "item " << i;
    }
}

// A static file's due: dates inevitably fall into the past as real time
// moves on - this doesn't break parsing (todo_parse() has no notion of
// "today"), but it silently turns the demo's intended mix (one overdue date,
// several upcoming ones, several undated tasks - docs/DEMO_PLAN.md section
// 5.2) into "everything is overdue", which looks broken to whoever tries the
// demo. Catches that directly, using the real clock, rather than only
// noticing on a real device: whoever sees this fail should push the due
// dates in examples/waveshare_photopainter_73/todo.txt forward.
TEST(ExampleTodo, DueDatesAreNotAllStale)
{
    std::string body = read_file("todo.txt");
    todo_list_t out;
    ASSERT_EQ(todo_parse(body.c_str(), body.size(), &out), ESP_OK);

    time_t now = time(nullptr);
    struct tm today_tm {
    };
#if defined(_WIN32)
    localtime_s(&today_tm, &now);
#else
    localtime_r(&now, &today_tm);
#endif
    char today[11];
    strftime(today, sizeof(today), "%Y-%m-%d", &today_tm);

    int future_count = 0;
    int undated_count = 0;
    for (int i = 0; i < out.count; i++) {
        if (out.items[i].due_date[0] == '\0') {
            undated_count++;
            continue;
        }
        // Plain lexicographic compare is valid for "YYYY-MM-DD" strings.
        if (strcmp(out.items[i].due_date, today) >= 0) {
            future_count++;
        }
    }
    EXPECT_GT(future_count, 0) << "every dated task in todo.txt is now overdue - refresh the dates";
    EXPECT_GT(undated_count, 0);
}
