// Which schedule draws when several overlap (main/sched_pick.c), checked case by case and against
// an independent brute-force reference over random rule sets, also across the days the clock
// changes.

#include <gtest/gtest.h>
#include <stdlib.h>
#include <time.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "cron.h"
#include "sched_pick.h"

namespace
{

// A set of entities that owns its compiled rules, so the pointers stay valid.
struct Set {
    std::vector<std::vector<cron_rule_t>> rules;
    std::vector<int> holds;
    std::vector<sched_entity_t> entities;

    Set &add(std::initializer_list<const char *> exprs, int hold)
    {
        std::vector<cron_rule_t> compiled;
        for (const char *expr : exprs) {
            cron_rule_t rule;
            EXPECT_TRUE(cron_parse(expr, &rule)) << expr;
            compiled.push_back(rule);
        }
        rules.push_back(compiled);
        holds.push_back(hold);
        entities.clear();
        for (size_t i = 0; i < rules.size(); i++) {
            entities.push_back({rules[i].data(), (int) rules[i].size(), holds[i]});
        }
        return *this;
    }
    int count() const
    {
        return (int) entities.size();
    }
};

// Local time of day -> time_t on a given day (2026-10-05 is a Monday).
time_t at(int year, int month, int day, int hour, int minute, int second = 0)
{
    struct tm local = {};
    local.tm_year = year - 1900;
    local.tm_mon = month - 1;
    local.tm_mday = day;
    local.tm_hour = hour;
    local.tm_min = minute;
    local.tm_sec = second;
    local.tm_isdst = -1;
    return mktime(&local);
}

time_t monday(int hour, int minute, int second = 0)
{
    return at(2026, 10, 5, hour, minute, second);
}

class SchedPick : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        setenv("TZ", "UTC", 1);
        tzset();
    }
};

}  // namespace

TEST_F(SchedPick, NothingToChooseFromDrawsNothing)
{
    Set none;
    EXPECT_EQ(sched_drawn_at(none.entities.data(), 0, monday(6, 0)), -1);
    EXPECT_EQ(sched_drawn_at(nullptr, 3, monday(6, 0)), -1);
    Set one;
    one.add({"0 6 *"}, 15);
    EXPECT_EQ(sched_drawn_at(one.entities.data(), 9, monday(6, 0)), -1);  // too many entities
    EXPECT_EQ(sched_seconds_until_next(nullptr, 1, monday(5, 0), nullptr), CRON_FALLBACK_SEC);
}

TEST_F(SchedPick, ASingleEntityDrawsEveryMatch)
{
    Set set;
    set.add({"0 6-8 *"}, 15);
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 1, monday(6, 0, 30)),
              0);  // seconds do not matter
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 1, monday(7, 0)), 0);
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 1, monday(7, 1)), -1);  // no match
}

TEST_F(SchedPick, TheSameMinuteGoesToTheLowestNumber)
{
    Set set;
    set.add({"30 6 *"}, 0).add({"30 6 *"}, 0).add({"30 6 *"}, 0);
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 3, monday(6, 30)), 0);
    Set second_only;
    second_only.add({"0 6 *"}, 0).add({"30 6 *"}, 0).add({"30 6 *"}, 0);
    EXPECT_EQ(sched_drawn_at(second_only.entities.data(), 3, monday(6, 30)), 1);
}

TEST_F(SchedPick, AHigherDisplayHoldsOffALowerOneAfterIt)
{
    Set set;
    set.add({"0 6 *"}, 15).add({"10 6 *"}, 15);
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 2, monday(6, 0)), 0);
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 2, monday(6, 10)), -1);  // 10 < 15 minutes
    Set later;
    later.add({"0 6 *"}, 15).add({"15 6 *"}, 15);
    EXPECT_EQ(sched_drawn_at(later.entities.data(), 2, monday(6, 15)),
              1);  // exactly the hold: free
    Set no_hold;
    no_hold.add({"0 6 *"}, 0).add({"1 6 *"}, 0);
    EXPECT_EQ(sched_drawn_at(no_hold.entities.data(), 2, monday(6, 1)),
              1);  // hold 0: only the same minute
}

TEST_F(SchedPick, ALowerDisplayThatWouldBeReplacedSoonIsSkipped)
{
    Set set;
    set.add({"15 6 *"}, 15).add({"10 6 *"}, 15);
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 2, monday(6, 10)),
              -1);  // 5 minutes before a higher one
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 2, monday(6, 15)), 0);

    // with a shorter own hold the lower display is worth it: it stays 5 minutes, which is its hold
    Set short_hold;
    short_hold.add({"15 6 *"}, 15).add({"10 6 *"}, 5);
    EXPECT_EQ(sched_drawn_at(short_hold.entities.data(), 2, monday(6, 10)), 1);
}

TEST_F(SchedPick, ASuppressedFireDoesNotSuppressAnotherOne)
{
    Set set;
    set.add({"0 6 *"}, 15).add({"10 6 *"}, 15).add({"20 6 *"}, 15);
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 3, monday(6, 0)), 0);
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 3, monday(6, 10)), -1);  // held off by the first
    // the third is 20 minutes after the first and the second never drew: it is shown
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 3, monday(6, 20)), 2);
}

TEST_F(SchedPick, ASchedulesOwnRulesDoNotHoldEachOtherOff)
{
    Set set;
    set.add({"0 6 *", "5 6 *", "10 6 *"}, 15);
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 1, monday(6, 0)), 0);
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 1, monday(6, 5)), 0);
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 1, monday(6, 10)), 0);
    Set every_ten;
    every_ten.add({"*/10 6 *"}, 60);
    EXPECT_EQ(sched_drawn_at(every_ten.entities.data(), 1, monday(6, 20)), 0);
}

TEST_F(SchedPick, AHoldPerScheduleKeepsAMorningPageUntilItHasBeenSeen)
{
    // 6:30 page, hold 60 minutes; the hourly agenda (own hold 0) would replace it at 7:00
    Set set;
    set.add({"30 6 *"}, 60).add({"0 6-18 *"}, 0);
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 2, monday(6, 0)),
              1);  // 30 minutes before: its own hold is 0
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 2, monday(6, 30)), 0);
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 2, monday(7, 0)), -1);  // held off, 30 < 60
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 2, monday(8, 0)), 1);   // over
}

TEST_F(SchedPick, TheNextDrawnFireSkipsTheOnesThatAreHeldOff)
{
    Set set;
    set.add({"0 6 *"}, 15).add({"*/5 6 *"}, 15);
    int entity = -1;
    // from 5:59:30 the first fire is 6:00 (schedule 1); then 6:05 .. 6:10 are held off, 6:15 draws
    EXPECT_EQ(sched_seconds_until_next(set.entities.data(), 2, monday(5, 59, 30), &entity), 30);
    EXPECT_EQ(entity, 0);
    EXPECT_EQ(sched_seconds_until_next(set.entities.data(), 2, monday(6, 0, 30), &entity),
              14 * 60 + 30);
    EXPECT_EQ(entity, 1);
    EXPECT_EQ(sched_seconds_until_next_of(set.entities.data(), 2, monday(5, 0), 1),
              60 * 60 + 15 * 60);
}

TEST_F(SchedPick, TheNextFireNeverCountsTheCurrentMinute)
{
    Set set;
    set.add({"0 6 *"}, 0);
    EXPECT_EQ(sched_seconds_until_next(set.entities.data(), 1, monday(6, 0, 0), nullptr),
              24 * 3600);  // the next day, as cron_seconds_until_next() does
    EXPECT_EQ(sched_seconds_until_next(set.entities.data(), 1, monday(5, 59, 59), nullptr), 1);
}

TEST_F(SchedPick, NothingInTheHorizonGivesTheFallback)
{
    Set set;
    set.add({"0 6 6"}, 0);  // Saturdays only: Monday 7:00 -> Saturday 6:00 is 4 days 23 hours
    EXPECT_EQ(sched_seconds_until_next(set.entities.data(), 1, monday(7, 0), nullptr), 428400);
    Set never;
    cron_rule_t impossible = {};  // no bit set: never matches
    never.rules.push_back({impossible});
    never.holds.push_back(0);
    never.entities.push_back({never.rules[0].data(), 1, 0});
    EXPECT_EQ(sched_seconds_until_next(never.entities.data(), 1, monday(7, 0), nullptr),
              CRON_FALLBACK_SEC);
    EXPECT_EQ(sched_seconds_until_next_of(never.entities.data(), 1, monday(7, 0), 5),
              CRON_FALLBACK_SEC);
}

TEST_F(SchedPick, MatchesReportsTheScheduleWhetherOrNotItIsDrawn)
{
    Set set;
    set.add({"0 6 *"}, 15).add({"5 6 *"}, 15);
    EXPECT_TRUE(sched_matches(set.entities.data(), 2, monday(6, 5), 1));
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 2, monday(6, 5)), -1);
    EXPECT_FALSE(sched_matches(set.entities.data(), 2, monday(6, 5), 0));
    EXPECT_FALSE(sched_matches(set.entities.data(), 2, monday(6, 5), 7));
}

// ---------------------------------------------------------------------------------------------
// Against a reference that follows the definition literally

namespace
{

struct Reference {
    const Set &set;
    std::map<std::pair<int, long long>, bool> memo;

    bool matches(int k, long long minute) const
    {
        time_t when = (time_t) (minute * 60);
        struct tm local;
        localtime_r(&when, &local);
        for (const cron_rule_t &rule : set.rules[k]) {
            if (cron_match(&rule, &local)) {
                return true;
            }
        }
        return false;
    }

    static int hold(int h)
    {
        return h < 0 ? 0 : (h > SCHED_HOLD_MAX_MIN ? SCHED_HOLD_MAX_MIN : h);
    }

    bool drawn(int k, long long minute)
    {
        auto key = std::make_pair(k, minute);
        auto found = memo.find(key);
        if (found != memo.end()) {
            return found->second;
        }
        bool result = matches(k, minute);
        for (int j = 0; result && j < k; j++) {
            long long behind = std::max(hold(set.holds[j]), 1);
            for (long long m = minute - behind + 1; result && m <= minute; m++) {
                if (drawn(j, m)) {
                    result = false;
                }
            }
            for (long long m = minute + 1; result && m < minute + hold(set.holds[k]); m++) {
                if (drawn(j, m)) {
                    result = false;
                }
            }
        }
        memo[key] = result;
        return result;
    }
};

// A small random set: up to `max_entities` entities of one to two rules each in a few hours
Set random_set(unsigned *seed, int max_entities)
{
    Set set;
    int count = 1 + (int) (rand_r(seed) % (unsigned) max_entities);
    static const char *const kExprs[] = {"0 6 *",       "30 6 *",     "*/10 6 *", "*/7 6-7 *",
                                         "15,45 6-8 *", "5 6,7 *",    "*/5 7 *",  "0 6-8 *",
                                         "20 6 *",      "*/15 6-7 *", "50 5-7 *", "10 7 1-5"};
    static const int kHolds[] = {0, 1, 5, 15, 15, 30, 45, 60};
    for (int k = 0; k < count; k++) {
        int rules = 1 + (int) (rand_r(seed) % 2);
        std::vector<const char *> exprs;
        for (int r = 0; r < rules; r++) {
            exprs.push_back(kExprs[rand_r(seed) % (sizeof(kExprs) / sizeof(kExprs[0]))]);
        }
        std::vector<cron_rule_t> compiled;
        for (const char *expr : exprs) {
            cron_rule_t rule;
            cron_parse(expr, &rule);
            compiled.push_back(rule);
        }
        set.rules.push_back(compiled);
        set.holds.push_back(kHolds[rand_r(seed) % (sizeof(kHolds) / sizeof(kHolds[0]))]);
    }
    for (size_t i = 0; i < set.rules.size(); i++) {
        set.entities.push_back({set.rules[i].data(), (int) set.rules[i].size(), set.holds[i]});
    }
    return set;
}

void check_against_reference(time_t from, int minutes, int cases, unsigned seed_start)
{
    for (int c = 0; c < cases; c++) {
        unsigned seed = seed_start + (unsigned) c;
        Set set = random_set(&seed, SCHED_MAX_ENTITIES);
        Reference reference{set, {}};
        long long first = (long long) (from / 60);
        for (int i = 0; i < minutes; i++) {
            long long minute = first + i;
            int expected = -1;
            for (int k = 0; k < set.count(); k++) {
                if (reference.drawn(k, minute)) {
                    ASSERT_EQ(expected, -1)
                        << "two schedules draw in one minute (case " << c << ")";
                    expected = k;
                }
            }
            int got = sched_drawn_at(set.entities.data(), set.count(), (time_t) (minute * 60));
            ASSERT_EQ(got, expected)
                << "case " << c << " minute " << i << " of " << set.count() << " schedules";
        }
    }
}

}  // namespace

TEST_F(SchedPick, AgreesWithTheLiteralDefinitionOverRandomSchedules)
{
    check_against_reference(monday(5, 0), 5 * 60, 60, 1);
}

TEST_F(SchedPick, AgreesAcrossTheDayTheClockGoesForward)
{
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    // 2026-03-29: 2:00 -> 3:00; a schedule at "5 2 *" does not exist that day, others go on
    check_against_reference(at(2026, 3, 29, 0, 0), 9 * 60, 25, 1000);
}

TEST_F(SchedPick, AgreesAcrossTheDayTheClockGoesBack)
{
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    check_against_reference(at(2026, 10, 25, 0, 0), 9 * 60, 25, 2000);
}

TEST_F(SchedPick, WithoutAHoldItIsThePlainPriority)
{
    for (unsigned seed_start = 0; seed_start < 40; seed_start++) {
        unsigned seed = 7000 + seed_start;
        Set set = random_set(&seed, SCHED_MAX_ENTITIES);
        for (size_t i = 0; i < set.holds.size(); i++) {
            set.holds[i] = 0;
            set.entities[i].hold_min = 0;
        }
        for (int i = 0; i < 4 * 60; i++) {
            time_t when = monday(5, 0) + i * 60;
            int lowest = -1;
            for (int k = 0; k < set.count() && lowest < 0; k++) {
                if (sched_matches(set.entities.data(), set.count(), when, k)) {
                    lowest = k;
                }
            }
            ASSERT_EQ(sched_drawn_at(set.entities.data(), set.count(), when), lowest)
                << seed_start << " " << i;
        }
    }
}

TEST_F(SchedPick, TheNextFireEqualsTheFirstDrawnMinuteOfASimulation)
{
    for (int c = 0; c < 20; c++) {
        unsigned seed = 9000 + (unsigned) c;
        Set set = random_set(&seed, SCHED_MAX_ENTITIES);
        time_t now = monday(4, 59, 20);
        int entity = -2;
        int seconds = sched_seconds_until_next(set.entities.data(), set.count(), now, &entity);
        int expected = -1;
        int expected_entity = -1;
        for (int i = 1; i <= 8 * 24 * 60 && expected < 0; i++) {
            time_t when = (now - now % 60) + i * 60;
            int k = sched_drawn_at(set.entities.data(), set.count(), when);
            if (k >= 0) {
                expected = (int) (when - now);
                expected_entity = k;
            }
        }
        if (expected < 0) {
            EXPECT_EQ(seconds, CRON_FALLBACK_SEC) << c;
        } else {
            EXPECT_EQ(seconds, expected) << c;
            EXPECT_EQ(entity, expected_entity) << c;
            EXPECT_EQ(
                sched_seconds_until_next_of(set.entities.data(), set.count(), now, expected_entity),
                expected)
                << c;
        }
    }
}

TEST_F(SchedPick, TheLongestHoldIsHonouredAndAHugeOneIsCapped)
{
    Set set;
    set.add({"0 6 *"}, 100000)
        .add({"0 8 *"}, 0);  // capped at 240 minutes: 8:00 is 120 minutes later
    EXPECT_EQ(sched_drawn_at(set.entities.data(), 2, monday(8, 0)), -1);
    Set later;
    later.add({"0 6 *"}, 100000).add({"1 10 *"}, 0);  // 4 hours and a minute later: free
    EXPECT_EQ(sched_drawn_at(later.entities.data(), 2, monday(10, 1)), 1);
    Set negative;
    negative.add({"0 6 *"}, -5).add({"1 6 *"}, 0);
    EXPECT_EQ(sched_drawn_at(negative.entities.data(), 2, monday(6, 1)),
              1);  // a negative hold counts as 0
}
