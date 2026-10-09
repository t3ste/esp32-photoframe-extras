// The rule adapter of the build option `agenda-rrule` (main/calendar_rrule.c) on top of libical
// (components/libical, vendored unmodified): the instances of a rule on wall-clock fields, the
// rules it refuses, and its bounds. The expected instances of the first group come from
// python-dateutil, an independent implementation of RFC 5545 (the dateutil rule text is the same as
// the one tested here).

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "calendar_rrule.h"
}

namespace
{

rrule_wall_t wall(int y, int mo, int d, int h = 0, int mi = 0, int s = 0)
{
    rrule_wall_t w;
    w.year = (int16_t) y;
    w.month = (int8_t) mo;
    w.day = (int8_t) d;
    w.hour = (int8_t) h;
    w.minute = (int8_t) mi;
    w.second = (int8_t) s;
    return w;
}

std::string text(const rrule_wall_t &w, bool all_day)
{
    char b[32];
    if (all_day) {
        snprintf(b, sizeof(b), "%04d-%02d-%02d", w.year, w.month, w.day);
    } else {
        snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d", w.year, w.month, w.day, w.hour,
                 w.minute);
    }
    return b;
}

// -1 is "the rule is not taken"
int expand(const std::string &rule, const rrule_wall_t &dtstart, bool all_day,
           const rrule_wall_t &from, const rrule_wall_t &to, std::vector<std::string> *got,
           int max_out = 400)
{
    std::vector<rrule_wall_t> out((size_t) max_out);
    int n = calendar_rrule_expand(rule.c_str(), rule.size(), &dtstart, all_day, &from, &to,
                                  out.data(), max_out);
    got->clear();
    for (int i = 0; i < n; i++) {
        got->push_back(text(out[(size_t) i], all_day));
    }
    return n;
}

struct Case {
    const char *name;
    const char *rule;
    int dtstart[6];
    bool all_day;
    int from[6];
    int to[6];
    std::vector<std::string> expected;
};

const Case kCases[] = {
    {"SecondMonday",
     "FREQ=MONTHLY;BYDAY=2MO",
     {2026, 1, 12, 10, 0, 0},
     false,
     {2026, 9, 1, 0, 0, 0},
     {2027, 1, 1, 0, 0, 0},
     {"2026-09-14 10:00", "2026-10-12 10:00", "2026-11-09 10:00", "2026-12-14 10:00"}},
    {"LastFriday",
     "FREQ=MONTHLY;BYDAY=-1FR",
     {2026, 1, 30, 10, 0, 0},
     false,
     {2026, 9, 1, 0, 0, 0},
     {2027, 1, 1, 0, 0, 0},
     {"2026-09-25 10:00", "2026-10-30 10:00", "2026-11-27 10:00", "2026-12-25 10:00"}},
    {"LastWeekdayBySetpos",
     "FREQ=MONTHLY;BYDAY=MO,TU,WE,TH,FR;BYSETPOS=-1",
     {2026, 1, 30, 17, 0, 0},
     false,
     {2026, 9, 1, 0, 0, 0},
     {2027, 1, 1, 0, 0, 0},
     {"2026-09-30 17:00", "2026-10-30 17:00", "2026-11-30 17:00", "2026-12-31 17:00"}},
    {"MonthlyThe31st",
     "FREQ=MONTHLY;BYMONTHDAY=31",
     {2026, 1, 31, 8, 0, 0},
     false,
     {2026, 1, 1, 0, 0, 0},
     {2027, 1, 1, 0, 0, 0},
     {"2026-01-31 08:00", "2026-03-31 08:00", "2026-05-31 08:00", "2026-07-31 08:00",
      "2026-08-31 08:00", "2026-10-31 08:00", "2026-12-31 08:00"}},
    {"MonthlyLastAndSecondToLastDay",
     "FREQ=MONTHLY;BYMONTHDAY=-2,-1",
     {2026, 1, 30, 9, 0, 0},
     false,
     {2026, 1, 1, 0, 0, 0},
     {2026, 5, 1, 0, 0, 0},
     {"2026-01-30 09:00", "2026-01-31 09:00", "2026-02-27 09:00", "2026-02-28 09:00",
      "2026-03-30 09:00", "2026-03-31 09:00", "2026-04-29 09:00", "2026-04-30 09:00"}},
    {"LeapDayYearly",
     "FREQ=YEARLY",
     {2024, 2, 29, 12, 0, 0},
     false,
     {2026, 1, 1, 0, 0, 0},
     {2033, 1, 1, 0, 0, 0},
     {"2028-02-29 12:00", "2032-02-29 12:00"}},
    {"YearlyYearDays",
     "FREQ=YEARLY;BYYEARDAY=1,100,-1",
     {2026, 1, 1, 9, 0, 0},
     false,
     {2026, 1, 1, 0, 0, 0},
     {2027, 1, 2, 0, 0, 0},
     {"2026-01-01 09:00", "2026-04-10 09:00", "2026-12-31 09:00", "2027-01-01 09:00"}},
    {"DailyAtTwoHours",
     "FREQ=DAILY;BYHOUR=9,17;BYMINUTE=15",
     {2026, 10, 6, 9, 15, 0},
     false,
     {2026, 10, 6, 0, 0, 0},
     {2026, 10, 9, 0, 0, 0},
     {"2026-10-06 09:15", "2026-10-06 17:15", "2026-10-07 09:15", "2026-10-07 17:15",
      "2026-10-08 09:15", "2026-10-08 17:15"}},
    {"WeeklyWkstSunday",
     "FREQ=WEEKLY;INTERVAL=2;BYDAY=SU,MO;WKST=SU",
     {2026, 10, 4, 9, 0, 0},
     false,
     {2026, 10, 1, 0, 0, 0},
     {2026, 11, 15, 0, 0, 0},
     {"2026-10-04 09:00", "2026-10-05 09:00", "2026-10-18 09:00", "2026-10-19 09:00",
      "2026-11-01 09:00", "2026-11-02 09:00"}},
    {"WeeklyWkstMonday",
     "FREQ=WEEKLY;INTERVAL=2;BYDAY=SU,MO;WKST=MO",
     {2026, 10, 4, 9, 0, 0},
     false,
     {2026, 10, 1, 0, 0, 0},
     {2026, 11, 15, 0, 0, 0},
     {"2026-10-04 09:00", "2026-10-12 09:00", "2026-10-18 09:00", "2026-10-26 09:00",
      "2026-11-01 09:00", "2026-11-09 09:00"}},
    {"WeeklyThreeDays",
     "FREQ=WEEKLY;BYDAY=MO,WE,FR",
     {2026, 9, 2, 9, 0, 0},
     false,
     {2026, 9, 28, 0, 0, 0},
     {2026, 10, 12, 0, 0, 0},
     {"2026-09-28 09:00", "2026-09-30 09:00", "2026-10-02 09:00", "2026-10-05 09:00",
      "2026-10-07 09:00", "2026-10-09 09:00"}},
    {"AllDayFirstMondayOfMonth",
     "FREQ=MONTHLY;BYDAY=1MO",
     {2026, 1, 5, 0, 0, 0},
     true,
     {2026, 6, 1, 0, 0, 0},
     {2026, 12, 1, 0, 0, 0},
     {"2026-06-01", "2026-07-06", "2026-08-03", "2026-09-07", "2026-10-05", "2026-11-02"}},
    {"MonthlyCount",
     "FREQ=MONTHLY;BYDAY=1FR;COUNT=4",
     {2026, 1, 2, 10, 0, 0},
     false,
     {2025, 1, 1, 0, 0, 0},
     {2027, 1, 1, 0, 0, 0},
     {"2026-01-02 10:00", "2026-02-06 10:00", "2026-03-06 10:00", "2026-04-03 10:00"}},
    {"MonthlyCountAfterTheEnd",
     "FREQ=MONTHLY;BYDAY=1FR;COUNT=4",
     {2026, 1, 2, 10, 0, 0},
     false,
     {2026, 6, 1, 0, 0, 0},
     {2027, 1, 1, 0, 0, 0},
     {}},
    {"FarPastWeekly",
     "FREQ=WEEKLY;BYDAY=MO,WE",
     {1995, 1, 2, 9, 0, 0},
     false,
     {2026, 10, 1, 0, 0, 0},
     {2026, 10, 15, 0, 0, 0},
     {"2026-10-05 09:00", "2026-10-07 09:00", "2026-10-12 09:00", "2026-10-14 09:00"}},
    {"EveryOtherMonthLastDay",
     "FREQ=MONTHLY;INTERVAL=2;BYMONTHDAY=-1",
     {2026, 1, 31, 12, 0, 0},
     false,
     {2026, 1, 1, 0, 0, 0},
     {2027, 1, 1, 0, 0, 0},
     {"2026-01-31 12:00", "2026-03-31 12:00", "2026-05-31 12:00", "2026-07-31 12:00",
      "2026-09-30 12:00", "2026-11-30 12:00"}},
    {"YearlyBirthday",
     "FREQ=YEARLY;BYMONTH=10;BYMONTHDAY=9",
     {1990, 10, 9, 8, 0, 0},
     false,
     {2024, 1, 1, 0, 0, 0},
     {2028, 1, 1, 0, 0, 0},
     {"2024-10-09 08:00", "2025-10-09 08:00", "2026-10-09 08:00", "2027-10-09 08:00"}},
    {"YearlyFriday13thInChosenMonths",
     "FREQ=YEARLY;BYMONTH=2,3,11;BYMONTHDAY=13;BYDAY=FR",
     {2026, 2, 13, 9, 0, 0},
     false,
     {2026, 1, 1, 0, 0, 0},
     {2028, 1, 1, 0, 0, 0},
     {"2026-02-13 09:00", "2026-03-13 09:00", "2026-11-13 09:00"}},
    {"FridayThe13thMonthly",
     "FREQ=MONTHLY;BYMONTHDAY=13;BYDAY=FR",
     {2026, 2, 13, 9, 0, 0},
     false,
     {2026, 1, 1, 0, 0, 0},
     {2028, 1, 1, 0, 0, 0},
     {"2026-02-13 09:00", "2026-03-13 09:00", "2026-11-13 09:00", "2027-08-13 09:00"}},
    {"IntervalFiveDaily",
     "FREQ=DAILY;INTERVAL=5",
     {2026, 9, 29, 9, 0, 0},
     false,
     {2026, 10, 1, 0, 0, 0},
     {2026, 10, 31, 0, 0, 0},
     {"2026-10-04 09:00", "2026-10-09 09:00", "2026-10-14 09:00", "2026-10-19 09:00",
      "2026-10-24 09:00", "2026-10-29 09:00"}},
};

TEST(CalendarRrule, InstancesMatchPythonDateutil)
{
    for (const Case &c : kCases) {
        std::vector<std::string> got;
        int n = expand(c.rule,
                       wall(c.dtstart[0], c.dtstart[1], c.dtstart[2], c.dtstart[3], c.dtstart[4]),
                       c.all_day, wall(c.from[0], c.from[1], c.from[2], c.from[3], c.from[4]),
                       wall(c.to[0], c.to[1], c.to[2], c.to[3], c.to[4]), &got);
        ASSERT_GE(n, 0) << c.name;
        EXPECT_EQ(got, c.expected) << c.name << ": " << c.rule;
    }
}

TEST(CalendarRrule, UntilIsLeftToTheCaller)
{
    // UNTIL is accepted and ignored here: calendar_ics.c bounds the instances by it
    std::vector<std::string> got;
    int n = expand("FREQ=WEEKLY;UNTIL=20261015T070000Z", wall(2026, 10, 1, 9), false,
                   wall(2026, 10, 1), wall(2026, 11, 1), &got);
    EXPECT_EQ(n, 5);
}

TEST(CalendarRrule, RuleTextMayBeLowerCaseAndHaveEmptyParts)
{
    std::vector<std::string> got;
    int n = expand("freq=monthly;;byday=2mo;", wall(2026, 1, 12, 10), false, wall(2026, 9, 1),
                   wall(2027, 1, 1), &got);
    EXPECT_EQ(n, 4);
    EXPECT_EQ(got.front(), "2026-09-14 10:00");
}

TEST(CalendarRrule, FromBeforeDtstartStartsAtDtstart)
{
    std::vector<std::string> got;
    int n = expand("FREQ=MONTHLY;BYDAY=2MO", wall(2026, 1, 12, 10), false, wall(2020, 1, 1),
                   wall(2026, 3, 1), &got);
    ASSERT_EQ(n, 2);
    EXPECT_EQ(got[0], "2026-01-12 10:00");
    EXPECT_EQ(got[1], "2026-02-09 10:00");
}

TEST(CalendarRrule, MaxOutKeepsTheEarliest)
{
    std::vector<std::string> got;
    int n = expand("FREQ=DAILY", wall(2026, 1, 1, 9), false, wall(2026, 1, 1), wall(2027, 1, 1),
                   &got, 3);
    ASSERT_EQ(n, 3);
    EXPECT_EQ(got[2], "2026-01-03 09:00");
}

TEST(CalendarRrule, AnEmptyRangeGivesNothing)
{
    std::vector<std::string> got;
    EXPECT_EQ(
        expand("FREQ=DAILY", wall(2026, 1, 1, 9), false, wall(2026, 5, 1), wall(2026, 5, 1), &got),
        0);
    EXPECT_EQ(
        expand("FREQ=DAILY", wall(2026, 1, 1, 9), false, wall(2026, 6, 1), wall(2026, 5, 1), &got),
        0);
}

// ---- rules that are not taken -------------------------------------------------------------------

TEST(CalendarRrule, RefusedRules)
{
    const char *refused[] = {
        "",                                    // nothing
        "FREQ",                                // no value
        "FREQ=",                               //
        "=DAILY",                              // no key
        "INTERVAL=2",                          // no FREQ
        "FREQ=FOO",                            // unknown
        "FREQ=HOURLY",                         // below DAILY
        "FREQ=MINUTELY",                       //
        "FREQ=SECONDLY;COUNT=100000000",       //
        "FREQ=DAILY;FREQ=WEEKLY",              // twice
        "FREQ=DAILY;INTERVAL=0",               // INTERVAL below 1
        "FREQ=DAILY;INTERVAL=-1",              //
        "FREQ=DAILY;INTERVAL=1001",            // above what is taken
        "FREQ=DAILY;INTERVAL=x",               //
        "FREQ=DAILY;COUNT=0",                  //
        "FREQ=DAILY;COUNT=-5",                 //
        "FREQ=DAILY;COUNT=100001",             //
        "FREQ=DAILY;X-FOO=bar",                // unknown part
        "RSCALE=HEBREW;FREQ=YEARLY",           // another calendar
        "FREQ=YEARLY;SKIP=FORWARD",            //
        "hello world",                         // not a rule
        "FREQ=WEEKLY;BYDAY=XX",                // no such day
        "FREQ=WEEKLY;BYDAY=",                  //
        "FREQ=WEEKLY;BYDAY=MO,,TU",            //
        "FREQ=WEEKLY;BYDAY=1MO",               // ordinal needs MONTHLY/YEARLY
        "FREQ=MONTHLY;BYDAY=0MO",              // ordinal 0
        "FREQ=MONTHLY;BYDAY=54MO",             // ordinal too large
        "FREQ=MONTHLY;BYDAY=+-1MO",            //
        "FREQ=MONTHLY;BYMONTHDAY=0",           //
        "FREQ=MONTHLY;BYMONTHDAY=32",          //
        "FREQ=MONTHLY;BYMONTHDAY=-32",         //
        "FREQ=WEEKLY;BYMONTHDAY=1",            // not with WEEKLY
        "FREQ=MONTHLY;BYMONTH=13",             //
        "FREQ=MONTHLY;BYMONTH=0",              //
        "FREQ=MONTHLY;BYMONTH=-1",             //
        "FREQ=MONTHLY;BYWEEKNO=5",             // BYWEEKNO only YEARLY
        "FREQ=MONTHLY;BYYEARDAY=5",            // BYYEARDAY only YEARLY
        "FREQ=YEARLY;BYWEEKNO=54",             //
        "FREQ=YEARLY;BYWEEKNO=5;BYDAY=1MO",    // ordinal with BYWEEKNO
        "FREQ=YEARLY;BYYEARDAY=367",           //
        "FREQ=YEARLY;BYMONTHDAY=13",           // yearly, a day but no month: read differently
        "FREQ=YEARLY;BYMONTHDAY=13;BYDAY=FR",  //
        "FREQ=YEARLY;BYWEEKNO=20;BYDAY=MO",    // week numbers: read differently at the turn of the
                                               // year
        "FREQ=YEARLY;BYWEEKNO=20",             //
        "FREQ=WEEKLY;INTERVAL=2;BYDAY=MO;WKST=TH",   // libical's weeks from WKST=TU..SA differ
        "FREQ=MONTHLY;BYSETPOS=1",                   // BYSETPOS needs another BY part
        "FREQ=MONTHLY;BYDAY=MO;BYSETPOS=0",          //
        "FREQ=DAILY;BYHOUR=24",                      //
        "FREQ=DAILY;BYMINUTE=60",                    //
        "FREQ=DAILY;BYSECOND=-1",                    //
        "FREQ=WEEKLY;WKST=XX",                       //
        "FREQ=WEEKLY;WKST=MON",                      //
        "FREQ=DAILY;UNTIL=1",                        // UNTIL too short to be a date
        "FREQ=DAILY;UNTIL=20261015;UNTIL=20261016",  // twice
        "FREQ=DAILY;COUNT=5;COUNT=6",                //
        "FREQ=WEEKLY;BYDAY=MO\xc3\xa4",              // not ASCII
    };
    for (const char *rule : refused) {
        std::vector<std::string> got;
        EXPECT_EQ(
            expand(rule, wall(2026, 10, 1, 9), false, wall(2026, 10, 1), wall(2026, 11, 1), &got),
            -1)
            << "[" << rule << "]";
    }
}

TEST(CalendarRrule, AllDayRulesWithATimeOfDayAreRefused)
{
    std::vector<std::string> got;
    EXPECT_EQ(expand("FREQ=DAILY;BYHOUR=9", wall(2026, 10, 1), true, wall(2026, 10, 1),
                     wall(2026, 11, 1), &got),
              -1);
    EXPECT_GE(
        expand("FREQ=DAILY", wall(2026, 10, 1), true, wall(2026, 10, 1), wall(2026, 11, 1), &got),
        0);
}

TEST(CalendarRrule, ALongListIsRefusedAndTheLongestAllowedOneIsNot)
{
    std::string many = "FREQ=MONTHLY;BYMONTHDAY=";
    for (int i = 0; i < 63; i++) {
        many += (i ? "," : "") + std::to_string(1 + i % 28);
    }
    std::vector<std::string> got;
    EXPECT_EQ(expand(many, wall(2026, 1, 1, 9), false, wall(2026, 1, 1), wall(2026, 2, 1), &got),
              -1);
    std::string ok = "FREQ=MONTHLY;BYMONTHDAY=";
    for (int i = 0; i < 31; i++) {
        ok += (i ? "," : "") + std::to_string(1 + i);
    }
    EXPECT_EQ(expand(ok, wall(2026, 1, 1, 9), false, wall(2026, 1, 1), wall(2026, 2, 1), &got), 31);
}

TEST(CalendarRrule, ARuleTooLongForTheBufferIsRefused)
{
    std::string r = "FREQ=WEEKLY;BYDAY=MO";
    while (r.size() < 400) {
        r += ";BYDAY=MO";
    }
    std::vector<std::string> got;
    EXPECT_EQ(expand(r, wall(2026, 10, 1, 9), false, wall(2026, 10, 1), wall(2026, 11, 1), &got),
              -1);
}

// The rule text is not NUL-terminated and the parts are cut off anywhere: nothing may be read past
// `len` (an exact-size heap copy lets AddressSanitizer see it; a plain run only checks the result).
TEST(CalendarRrule, TheRuleTextIsNeverReadPastItsLength)
{
    const char *cuts[] = {"WKST=M",
                          "WKST=",
                          "FREQ=WEEKLY;WKST=S",
                          "FREQ=WEEKLY;BYDAY=M",
                          "FREQ=WEEKLY;BYDAY=1",
                          "FREQ=WEEKLY;BYDAY=",
                          "FREQ=DAILY;INTERVAL=",
                          "FREQ=DAILY;COUNT=",
                          "FREQ=DAILY;UNTIL=2026",
                          "FREQ=MONTHLY;BYMONTHDAY=-",
                          "FREQ=MONTHLY;BYSETPOS=",
                          "FREQ=",
                          "FREQ",
                          ";",
                          "="};
    rrule_wall_t out[8];
    rrule_wall_t a = wall(2026, 10, 1, 9), f = wall(2026, 10, 1), t = wall(2026, 11, 1);
    for (const char *c : cuts) {
        size_t n = strlen(c);
        char *exact = (char *) malloc(n);
        memcpy(exact, c, n);
        EXPECT_EQ(calendar_rrule_expand(exact, n, &a, false, &f, &t, out, 8), -1) << c;
        free(exact);
    }
}

TEST(CalendarRrule, NullArgumentsAndABadDtstartAreRefused)
{
    rrule_wall_t out[4];
    rrule_wall_t a = wall(2026, 10, 1, 9), f = wall(2026, 10, 1), t = wall(2026, 11, 1);
    EXPECT_EQ(calendar_rrule_expand(nullptr, 0, &a, false, &f, &t, out, 4), -1);
    EXPECT_EQ(calendar_rrule_expand("FREQ=DAILY", 10, nullptr, false, &f, &t, out, 4), -1);
    EXPECT_EQ(calendar_rrule_expand("FREQ=DAILY", 10, &a, false, &f, &t, out, 0), -1);
    std::vector<std::string> got;
    EXPECT_EQ(
        expand("FREQ=DAILY", wall(2026, 2, 30, 9), false, wall(2026, 3, 1), wall(2026, 4, 1), &got),
        -1);
    EXPECT_EQ(
        expand("FREQ=DAILY", wall(2026, 13, 1, 9), false, wall(2026, 3, 1), wall(2026, 4, 1), &got),
        -1);
    EXPECT_EQ(
        expand("FREQ=DAILY", wall(2026, 1, 1, 25), false, wall(2026, 3, 1), wall(2026, 4, 1), &got),
        -1);
}

// ---- bounds -------------------------------------------------------------------------------------

TEST(CalendarRrule, ARuleWithCountThatEndsLongBeforeTheWindowIsNoCostAndNoInstance)
{
    std::vector<std::string> got;
    EXPECT_EQ(expand("FREQ=DAILY;COUNT=10", wall(2000, 1, 1, 9), false, wall(2026, 10, 1),
                     wall(2026, 11, 1), &got),
              0);
}

TEST(CalendarRrule, ARuleWithCountThatStartsTooLongBeforeTheWindowIsRefused)
{
    // 100000 daily instances, the window 20 years in: more than RRULE_MAX_STEPS instances to walk
    std::vector<std::string> got;
    EXPECT_EQ(expand("FREQ=DAILY;COUNT=100000", wall(2000, 1, 1, 9), false, wall(2026, 10, 1),
                     wall(2026, 11, 1), &got),
              -1);
    // ... but the same rule a few years in is fine
    EXPECT_EQ(expand("FREQ=DAILY;COUNT=100000", wall(2024, 1, 1, 9), false, wall(2026, 10, 1),
                     wall(2026, 10, 4), &got),
              3);
}

TEST(CalendarRrule, ARuleThatNeverMatchesGivesNothingQuickly)
{
    std::vector<std::string> got;
    // refused or empty - the point is that it returns, and soon
    EXPECT_LE(expand("FREQ=YEARLY;BYMONTH=2;BYMONTHDAY=30", wall(2026, 2, 28, 9), false,
                     wall(2026, 1, 1), wall(2060, 1, 1), &got),
              0);
    EXPECT_LE(expand("FREQ=MONTHLY;BYMONTH=2;BYMONTHDAY=31", wall(2026, 2, 28, 9), false,
                     wall(2026, 1, 1), wall(2060, 1, 1), &got),
              0);
}

TEST(CalendarRrule, ALongRangeIsLimitedByMaxOutNotByTheRule)
{
    std::vector<std::string> got;
    EXPECT_EQ(expand("FREQ=DAILY", wall(2026, 1, 1, 9), false, wall(2026, 1, 1), wall(2500, 1, 1),
                     &got, 112),
              112);
}

TEST(CalendarRrule, ManyCallsDoNotLeakOrCrash)
{
    // leaks show under the sanitizers; the loop makes any growth visible
    std::vector<std::string> got;
    for (int i = 0; i < 2000; i++) {
        int n = expand(i % 3 == 0 ? "FREQ=MONTHLY;BYDAY=-1FR"
                                  : (i % 3 == 1 ? "FREQ=WEEKLY;BYDAY=MO,WE" : "FREQ=FOO"),
                       wall(2026, 1, 2, 9), false, wall(2026, 9, 1), wall(2026, 12, 1), &got);
        if (i % 3 == 2) {
            EXPECT_EQ(n, -1);
        } else {
            EXPECT_GT(n, 0);
        }
    }
}

}  // namespace
