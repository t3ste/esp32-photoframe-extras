// agenda_order.c: the priority of the calendars A-E, the order of the entries of a day, which
// entries stay when there is no room for all of them, the ISO week number and the day labels (build
// option `agenda`).

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "agenda_order.h"
}

namespace
{

std::string parsed(const char *text)
{
    uint8_t order[AGENDA_CAL_COUNT];
    if (!agenda_priority_parse(text, order)) {
        return "FAIL";
    }
    char out[AGENDA_PRIORITY_LEN];
    agenda_priority_format(order, out);
    return out;
}

agenda_sort_key_t key(time_t start, time_t end, bool all_day, uint8_t cal, const uint8_t *rank_of,
                      uint16_t seq)
{
    agenda_sort_key_t k;
    k.start = start;
    k.end = end;
    k.all_day = all_day;
    k.rank = rank_of[cal];
    k.cal = cal;
    k.seq = seq;
    return k;
}

}  // namespace

TEST(AgendaPriority, TheDefaultListIsAbcde)
{
    EXPECT_EQ(parsed(AGENDA_PRIORITY_DEFAULT), "ABCDE");
}

TEST(AgendaPriority, AnyOrderOfTheFiveLettersIsAccepted)
{
    EXPECT_EQ(parsed("EDCBA"), "EDCBA");
    EXPECT_EQ(parsed("CABED"), "CABED");
}

TEST(AgendaPriority, SeparatorsAndCaseDoNotMatter)
{
    EXPECT_EQ(parsed("A,B,C,D,E"), "ABCDE");
    EXPECT_EQ(parsed("e > d > c > b > a"), "EDCBA");
    EXPECT_EQ(parsed(" b; a ,c d e "), "BACDE");
}

TEST(AgendaPriority, AListThatIsNotAPermutationIsRefused)
{
    EXPECT_EQ(parsed(""), "FAIL");
    EXPECT_EQ(parsed("ABCD"), "FAIL");    // E missing
    EXPECT_EQ(parsed("ABCDEE"), "FAIL");  // E twice
    EXPECT_EQ(parsed("AABCD"), "FAIL");   // A twice, E missing
    EXPECT_EQ(parsed("ABCDEA"), "FAIL");  // a sixth letter
    EXPECT_EQ(parsed("12345"), "FAIL");   // no calendar letters at all
    EXPECT_EQ(parsed(nullptr), "FAIL");
}

TEST(AgendaPriority, AFailedParseLeavesTheOrderAlone)
{
    uint8_t order[AGENDA_CAL_COUNT] = {4, 3, 2, 1, 0};
    EXPECT_FALSE(agenda_priority_parse("AAAAA", order));
    EXPECT_EQ(order[0], 4);
    EXPECT_EQ(order[4], 0);
}

TEST(AgendaPriority, RanksAreThePositionsInTheList)
{
    uint8_t order[AGENDA_CAL_COUNT];
    ASSERT_TRUE(agenda_priority_parse("CAEBD", order));
    uint8_t rank_of[AGENDA_CAL_COUNT];
    agenda_priority_ranks(order, rank_of);
    EXPECT_EQ(rank_of[2], 0);  // C first
    EXPECT_EQ(rank_of[0], 1);  // A second
    EXPECT_EQ(rank_of[4], 2);
    EXPECT_EQ(rank_of[1], 3);
    EXPECT_EQ(rank_of[3], 4);  // D last
}

TEST(AgendaOrder, PriorityFirstPutsAHigherCalendarAboveAnEarlierEntryOfALowerOne)
{
    uint8_t order[AGENDA_CAL_COUNT];
    ASSERT_TRUE(agenda_priority_parse("ABCDE", order));
    uint8_t rank_of[AGENDA_CAL_COUNT];
    agenda_priority_ranks(order, rank_of);
    agenda_sort_key_t b_early = key(1000, 2000, false, 1, rank_of, 0);
    agenda_sort_key_t a_late = key(5000, 6000, false, 0, rank_of, 1);
    EXPECT_GT(agenda_event_compare(&b_early, &a_late, true), 0);   // A (late) comes first
    EXPECT_LT(agenda_event_compare(&b_early, &a_late, false), 0);  // by time the early one does
}

TEST(AgendaOrder, WithinACalendarTheTimeDecides)
{
    uint8_t rank_of[AGENDA_CAL_COUNT] = {0, 1, 2, 3, 4};
    agenda_sort_key_t first = key(1000, 2000, false, 0, rank_of, 5);
    agenda_sort_key_t second = key(3000, 4000, false, 0, rank_of, 0);
    EXPECT_LT(agenda_event_compare(&first, &second, true), 0);
    EXPECT_LT(agenda_event_compare(&first, &second, false), 0);
}

TEST(AgendaOrder, AnAllDayEntryComesBeforeATimedOneAtTheSameMoment)
{
    uint8_t rank_of[AGENDA_CAL_COUNT] = {0, 1, 2, 3, 4};
    agenda_sort_key_t timed = key(1000, 1500, false, 0, rank_of, 0);
    agenda_sort_key_t all_day = key(1000, 87400, true, 0, rank_of, 1);
    EXPECT_GT(agenda_event_compare(&timed, &all_day, true), 0);
    EXPECT_GT(agenda_event_compare(&timed, &all_day, false), 0);
}

TEST(AgendaOrder, EqualEntriesKeepTheirInputOrder)
{
    // the all-day entries of several calendars all start at midnight: the order must not be
    // arbitrary
    uint8_t rank_of[AGENDA_CAL_COUNT] = {0, 1, 2, 3, 4};
    std::vector<agenda_sort_key_t> items;
    for (uint16_t i = 0; i < 12; i++) {
        items.push_back(key(86400, 172800, true, (uint8_t) (i % 3), rank_of, i));
    }
    std::vector<agenda_sort_key_t> shuffled(items.rbegin(), items.rend());
    auto sorter = [](const agenda_sort_key_t &x, const agenda_sort_key_t &y) {
        return agenda_event_compare(&x, &y, false) < 0;
    };
    std::sort(items.begin(), items.end(), sorter);
    std::sort(shuffled.begin(), shuffled.end(), sorter);
    for (size_t i = 0; i < items.size(); i++) {
        EXPECT_EQ(items[i].seq, shuffled[i].seq) << i;
    }
    // by calendar first, then by the input position
    EXPECT_EQ(items[0].cal, 0);
    EXPECT_EQ(items[0].seq, 0);
    EXPECT_EQ(items[1].seq, 3);
}

TEST(AgendaOrder, TheCompareIsAConsistentTotalOrder)
{
    uint8_t rank_of[AGENDA_CAL_COUNT] = {2, 0, 1, 4, 3};
    std::vector<agenda_sort_key_t> items;
    uint16_t seq = 0;
    for (int cal = 0; cal < 5; cal++) {
        for (int t = 0; t < 4; t++) {
            items.push_back(key(1000 * (t % 3), 1000 * (t % 3) + 100 * (t + 1), t == 2,
                                (uint8_t) cal, rank_of, seq++));
        }
    }
    for (bool priority_first : {true, false}) {
        for (const auto &a : items) {
            EXPECT_EQ(agenda_event_compare(&a, &a, priority_first), 0);
            for (const auto &b : items) {
                int ab = agenda_event_compare(&a, &b, priority_first);
                int ba = agenda_event_compare(&b, &a, priority_first);
                EXPECT_EQ((ab > 0) - (ab < 0), -((ba > 0) - (ba < 0)));
                if (&a != &b) {
                    EXPECT_NE(ab, 0);  // distinct entries never tie
                }
            }
        }
    }
}

TEST(AgendaSelect, KeepsTheBestRanksAndTheEarlierOnesAmongEquals)
{
    // five entries in display order; ranks 2 0 1 0 2
    const uint8_t ranks[5] = {2, 0, 1, 0, 2};
    bool shown[5];
    EXPECT_EQ(agenda_select_by_rank(ranks, 5, 3, shown), 3);
    EXPECT_FALSE(shown[0]);
    EXPECT_TRUE(shown[1]);
    EXPECT_TRUE(shown[2]);
    EXPECT_TRUE(shown[3]);
    EXPECT_FALSE(shown[4]);
    EXPECT_EQ(agenda_select_by_rank(ranks, 5, 4, shown), 4);
    EXPECT_TRUE(shown[0]);  // the earlier of the two rank-2 entries wins
    EXPECT_FALSE(shown[4]);
}

TEST(AgendaSelect, CapacityBeyondTheCountKeepsEverything)
{
    const uint8_t ranks[3] = {1, 0, 2};
    bool shown[3];
    EXPECT_EQ(agenda_select_by_rank(ranks, 3, 10, shown), 3);
    EXPECT_TRUE(shown[0] && shown[1] && shown[2]);
}

TEST(AgendaSelect, NoRoomKeepsNothing)
{
    const uint8_t ranks[2] = {0, 1};
    bool shown[2] = {true, true};
    EXPECT_EQ(agenda_select_by_rank(ranks, 2, 0, shown), 0);
    EXPECT_FALSE(shown[0] || shown[1]);
    EXPECT_EQ(agenda_select_by_rank(ranks, 2, -3, shown), 0);
    EXPECT_EQ(agenda_select_by_rank(ranks, 0, 5, shown), 0);
}

TEST(AgendaSelect, ALowPriorityFloodDoesNotPushOutTheImportantEntry)
{
    // 12 entries of the lowest calendar in front, one of the highest at the very end
    uint8_t ranks[13];
    for (int i = 0; i < 12; i++) {
        ranks[i] = 4;
    }
    ranks[12] = 0;
    bool shown[13];
    EXPECT_EQ(agenda_select_by_rank(ranks, 13, 3, shown), 3);
    EXPECT_TRUE(shown[12]);
    EXPECT_TRUE(shown[0]);
    EXPECT_TRUE(shown[1]);
    EXPECT_FALSE(shown[2]);
}

TEST(AgendaWeek, KnownDates)
{
    struct {
        int y, m, d, week;
    } known[] = {
        {2026, 10, 12, 42}, {2026, 10, 11, 41}, {2026, 1, 1, 1},   {2025, 12, 29, 1},
        {2025, 12, 28, 52}, {2026, 12, 31, 53}, {2027, 1, 1, 53},  {2027, 1, 3, 53},
        {2027, 1, 4, 1},    {2020, 12, 31, 53}, {2021, 1, 3, 53},  {2021, 1, 4, 1},
        {2024, 12, 30, 1},  {2024, 12, 29, 52}, {2016, 1, 3, 53},  {2016, 1, 4, 1},
        {2015, 12, 31, 53}, {2000, 1, 1, 52},   {2000, 1, 3, 1},   {2024, 2, 29, 9},
        {2023, 1, 1, 52},   {2023, 1, 2, 1},    {2026, 6, 15, 25}, {2018, 12, 31, 1},
        {2019, 1, 6, 1},
    };
    for (const auto &k : known) {
        EXPECT_EQ(agenda_iso_week(k.y - 1900, k.m - 1, k.d), k.week)
            << k.y << "-" << k.m << "-" << k.d;
    }
}

TEST(AgendaWeek, TheNumberOnlyChangesOnAMondayAndStaysInRange)
{
    // walk 60 years day by day (a civil calendar of our own, independent of the code under test)
    int y = 1990, m = 0, d = 1;
    int wd = 1;  // 1990-01-01 was a Monday
    int last = agenda_iso_week(y - 1900, m, d);
    static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int n = 0; n < 60 * 366; n++) {
        bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
        int dim = days[m] + (m == 1 && leap ? 1 : 0);
        d++;
        if (d > dim) {
            d = 1;
            if (++m == 12) {
                m = 0;
                y++;
            }
        }
        wd = wd % 7 + 1;
        int week = agenda_iso_week(y - 1900, m, d);
        ASSERT_GE(week, 1);
        ASSERT_LE(week, 53);
        if (wd == 1) {
            // a new week: the next number, or 1 after the year's last week
            EXPECT_TRUE(week == last + 1 || week == 1)
                << y << "-" << m + 1 << "-" << d << " " << last << "->" << week;
        } else {
            EXPECT_EQ(week, last) << y << "-" << m + 1 << "-" << d;
        }
        last = week;
    }
}

TEST(AgendaWeek, TheTextFollowsTheLanguage)
{
    char out[8];
    agenda_week_text(out, sizeof(out), 42, true);
    EXPECT_STREQ(out, "KW42");
    agenda_week_text(out, sizeof(out), 5, false);
    EXPECT_STREQ(out, "CW5");
}

TEST(AgendaDayLabel, WithAndWithoutTheWeek)
{
    char out[16];
    agenda_day_label(out, sizeof(out), "Mo", 12, false, 42, true);
    EXPECT_STREQ(out, "Mo 12.");
    agenda_day_label(out, sizeof(out), "Mo", 12, true, 42, true);
    EXPECT_STREQ(out, "Mo 12. KW42");
    agenda_day_label(out, sizeof(out), "Mon", 31, true, 53, false);
    EXPECT_STREQ(out, "Mon 31. CW53");
}

TEST(AgendaDayLabel, ALongLabelFitsTheBufferTheRendererUses)
{
    char out[16];  // draw_calendar_column() / draw_day_cell()
    agenda_day_label(out, sizeof(out), "Mon", 31, true, 53, true);
    EXPECT_LT(strlen(out), sizeof(out));
    EXPECT_STREQ(out, "Mon 31. KW53");
}
