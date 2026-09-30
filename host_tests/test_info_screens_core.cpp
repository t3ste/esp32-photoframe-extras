#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "info_screens_core.h"
}

TEST(InfoCore, WeekdaysOfKnownDates)
{
    EXPECT_EQ(info_weekday(1970, 1, 1), 4);    // Thursday
    EXPECT_EQ(info_weekday(2000, 1, 1), 6);    // Saturday
    EXPECT_EQ(info_weekday(2026, 9, 30), 3);   // Wednesday
    EXPECT_EQ(info_weekday(2024, 2, 29), 4);   // a leap day, Thursday
    EXPECT_EQ(info_weekday(2100, 3, 1), 1);    // 2100 is no leap year: Monday
    EXPECT_EQ(info_weekday(1969, 12, 31), 3);  // before the epoch: Wednesday
}

TEST(InfoCore, IsoWeeksOfKnownDates)
{
    const struct {
        int y, m, d, week, iso_year;
    } cases[] = {
        {2026, 9, 30, 40, 2026},  // an ordinary Wednesday
        {2026, 1, 1, 1, 2026},    // a Thursday: week 1
        {2025, 12, 29, 1, 2026},  // Monday of the week with 1 January 2026 (a Thursday)
        {2024, 12, 30, 1, 2025},  // Monday: already week 1 of 2025
        {2021, 1, 3, 53, 2020},   // Sunday: still week 53 of 2020
        {2021, 1, 4, 1, 2021},    {2020, 12, 31, 53, 2020},  // 2020 has 53 weeks
        {2027, 1, 1, 53, 2026},  // Friday: week 53 of 2026 (2026 has 53 weeks)
        {2018, 12, 31, 1, 2019},  {2016, 1, 3, 53, 2015},
        {2026, 12, 31, 53, 2026}, {2024, 2, 29, 9, 2024},  // a leap day
        {2019, 3, 10, 10, 2019},  {2026, 6, 15, 25, 2026},
    };
    for (const auto &c : cases) {
        int iso_year = 0;
        EXPECT_EQ(info_iso_week(c.y, c.m, c.d, &iso_year), c.week)
            << c.y << "-" << c.m << "-" << c.d;
        EXPECT_EQ(iso_year, c.iso_year) << c.y << "-" << c.m << "-" << c.d;
    }
    EXPECT_EQ(info_iso_week(2026, 9, 30, nullptr), 40);
}

TEST(InfoCore, EveryWeekOfTheYearFollowsTheLast)
{
    // walking through a year, the week number rises by one each Monday and never jumps
    int previous = 0;
    int previous_year = 0;
    for (int year : {2019, 2020, 2026}) {
        for (int month = 1; month <= 12; month++) {
            static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            int dim = days[month - 1] + ((month == 2 && (year % 4 == 0)) ? 1 : 0);
            for (int day = 1; day <= dim; day++) {
                int iso_year;
                int week = info_iso_week(year, month, day, &iso_year);
                EXPECT_GE(week, 1);
                EXPECT_LE(week, 53);
                if (previous_year == year && previous != 0) {
                    bool monday = info_weekday(year, month, day) == 1;
                    if (monday) {
                        EXPECT_TRUE(week == previous + 1 || week == 1)
                            << year << "-" << month << "-" << day;
                    } else {
                        EXPECT_EQ(week, previous) << year << "-" << month << "-" << day;
                    }
                }
                previous = week;
                previous_year = year;
            }
        }
        previous = 0;
    }
}

TEST(InfoCore, NowFromTmAndDate)
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 126;  // 2026
    tm.tm_mon = 8;     // September
    tm.tm_mday = 30;
    tm.tm_wday = 3;
    tm.tm_hour = 8;
    tm.tm_min = 35;
    info_now_t now;
    info_now_from_tm(&tm, true, &now);
    EXPECT_TRUE(now.german);
    EXPECT_EQ(now.year, 2026);
    EXPECT_EQ(now.month, 9);
    EXPECT_EQ(now.day, 30);
    EXPECT_EQ(now.wday, 3);
    EXPECT_EQ(now.hour, 8);
    EXPECT_EQ(now.minute, 35);
    EXPECT_EQ(now.iso_week, 40);
    EXPECT_EQ(now.iso_year, 2026);

    info_now_t fixed;
    info_now_from_date(2026, 10, 3, false, &fixed);
    EXPECT_FALSE(fixed.german);
    EXPECT_EQ(fixed.wday, 6);  // Saturday
    EXPECT_EQ(fixed.iso_week, 40);
}

TEST(InfoCore, Names)
{
    EXPECT_STREQ(info_weekday_name(3, false), "Wednesday");
    EXPECT_STREQ(info_weekday_name(3, true), "Mittwoch");
    EXPECT_STREQ(info_weekday_name(0, true), "Sonntag");
    EXPECT_STREQ(info_weekday_short(1, false), "Mon");
    EXPECT_STREQ(info_weekday_short(1, true), "Mo");
    EXPECT_STREQ(info_month_name(3, false), "March");
    EXPECT_STREQ(info_month_name(3, true),
                 "M\xC3\xA4rz");  // UTF-8, drawn through canvas_text_from_utf8
    EXPECT_STREQ(info_month_name(12, true), "Dezember");
    EXPECT_STREQ(info_weekday_name(7, false), "?");
    EXPECT_STREQ(info_weekday_name(-1, true), "?");
    EXPECT_STREQ(info_month_name(0, false), "?");
    EXPECT_STREQ(info_month_name(13, true), "?");
}

TEST(InfoCore, RotationWalksThroughTheSetBits)
{
    const uint32_t mask = 0b101101;  // bits 0, 2, 3, 5
    EXPECT_EQ(info_rotation_size(mask, 6), 4);
    EXPECT_EQ(info_rotation_pick(mask, 0, 6), 0);
    EXPECT_EQ(info_rotation_pick(mask, 1, 6), 2);
    EXPECT_EQ(info_rotation_pick(mask, 2, 6), 3);
    EXPECT_EQ(info_rotation_pick(mask, 3, 6), 5);
    EXPECT_EQ(info_rotation_pick(mask, 4, 6), 0);  // wraps
    EXPECT_EQ(info_rotation_pick(mask, 9, 6), 2);
    EXPECT_EQ(info_rotation_pick(mask, 0xFFFFFFFFu, 6), 5);  // a huge counter still lands on a bit
}

TEST(InfoCore, RotationIgnoresBitsThatAreNoScreens)
{
    EXPECT_EQ(info_rotation_size(0b1111'0000, 4), 0);  // only bits beyond the screens
    EXPECT_EQ(info_rotation_pick(0b1111'0000, 0, 4), -1);
    EXPECT_EQ(info_rotation_pick(0, 0, 6), -1);
    EXPECT_EQ(info_rotation_pick(0b1'0001, 1, 4), 0);  // bit 4 is outside the four screens
    EXPECT_EQ(info_rotation_size(0xFFFFFFFFu, 32), 32);
    EXPECT_EQ(info_rotation_pick(0x80000000u, 5, 32), 31);
    EXPECT_EQ(info_rotation_size(0xFFFFFFFFu, 40), 32);  // more bits than a mask has
}

TEST(InfoCore, RotationShowsEveryScreenOnceInARow)
{
    const uint32_t mask = 0b100110;
    std::vector<int> seen;
    for (uint32_t i = 0; i < 6; i++) {
        seen.push_back(info_rotation_pick(mask, i, 6));
    }
    EXPECT_EQ(seen, (std::vector<int>{1, 2, 5, 1, 2, 5}));
}

namespace
{

std::vector<std::string> parse(const char *text, size_t name_len = 24, int max = 8)
{
    std::vector<char> buf((size_t) max * name_len);
    int n = info_parse_list(text, buf.data(), name_len, max);
    std::vector<std::string> names;
    for (int i = 0; i < n; i++) {
        names.push_back(&buf[(size_t) i * name_len]);
    }
    return names;
}

}  // namespace

TEST(InfoCore, ListParsing)
{
    EXPECT_EQ(parse("Anna, Ben; Clara"), (std::vector<std::string>{"Anna", "Ben", "Clara"}));
    EXPECT_EQ(parse("Anna\nBen\r\nClara"), (std::vector<std::string>{"Anna", "Ben", "Clara"}));
    EXPECT_EQ(parse("  Anna  ,\tBen\t,   "), (std::vector<std::string>{"Anna", "Ben"}));
    EXPECT_EQ(parse(",,;;\n,"), (std::vector<std::string>{}));
    EXPECT_EQ(parse(""), (std::vector<std::string>{}));
    EXPECT_EQ(parse("One"), (std::vector<std::string>{"One"}));
    EXPECT_EQ(parse("Take out the bin, Water plants"),
              (std::vector<std::string>{"Take out the bin", "Water plants"}));
}

TEST(InfoCore, ListParsingLimits)
{
    EXPECT_EQ(parse("a,b,c,d,e", 24, 3), (std::vector<std::string>{"a", "b", "c"}));
    EXPECT_EQ(parse("abcdefghij", 6), (std::vector<std::string>{"abcde"}));  // cut to name_len - 1
    // a cut never splits a UTF-8 character: "\xC3\xA4" is one letter
    EXPECT_EQ(parse("abcd\xC3\xA4"
                    "fg",
                    6),
              (std::vector<std::string>{"abcd"}));
    EXPECT_EQ(parse("abc\xC3\xA4"
                    "fg",
                    6),
              (std::vector<std::string>{"abc\xC3\xA4"}));
    EXPECT_EQ(parse("x", 1), (std::vector<std::string>{}));  // no room for a name at all
    char buf[16];
    EXPECT_EQ(info_parse_list(nullptr, buf, 8, 2), 0);
    EXPECT_EQ(info_parse_list("a", buf, 8, 0), 0);
}

TEST(InfoCore, DaysBetweenDates)
{
    EXPECT_EQ(info_days_between("2026-09-01", "2026-09-30"), 29);
    EXPECT_EQ(info_days_between("2026-09-30", "2026-09-30"), 0);
    EXPECT_EQ(info_days_between("2026-08-18", "2026-09-29"), 42);
    EXPECT_EQ(info_days_between("2025-12-31", "2026-01-01"), 1);  // over a year end
    EXPECT_EQ(info_days_between("2024-02-28", "2024-03-01"), 2);  // a leap year
    EXPECT_EQ(info_days_between("2025-02-28", "2025-03-01"), 1);
    EXPECT_EQ(info_days_between("2026-09-30", "2026-09-01"), -1);  // backwards
    EXPECT_EQ(info_days_between("2026-09-01T10:00:00", "2026-09-05 12:00"),
              4);  // only the date counts
    EXPECT_EQ(info_days_between("", "2026-09-01"), -1);
    EXPECT_EQ(info_days_between("junk", "2026-09-01"), -1);
    EXPECT_EQ(info_days_between("2026-13-01", "2026-09-01"), -1);
    EXPECT_EQ(info_days_between("2026-09-00", "2026-09-01"), -1);
    EXPECT_EQ(info_days_between(nullptr, "2026-09-01"), -1);
    EXPECT_EQ(info_days_between("2026-09-01", nullptr), -1);
}

TEST(InfoCore, StampInBothLanguages)
{
    char text[40];
    ASSERT_TRUE(info_format_stamp(false, 2026, 9, 30, 14, 35, text, sizeof(text)));
    EXPECT_STREQ(text, "Updated 30 Sep 14:35");
    ASSERT_TRUE(info_format_stamp(true, 2026, 9, 30, 14, 35, text, sizeof(text)));
    EXPECT_STREQ(text, "Stand 30.09. 14:35");
    ASSERT_TRUE(info_format_stamp(false, 2026, 3, 4, 7, 5, text, sizeof(text)));
    EXPECT_STREQ(text,
                 "Updated 4 Mar 07:05");  // no leading zero on the day, two digits on the time
    ASSERT_TRUE(info_format_stamp(true, 2026, 3, 4, 7, 5, text, sizeof(text)));
    EXPECT_STREQ(text, "Stand 04.03. 07:05");
    ASSERT_TRUE(info_format_stamp(true, 2027, 12, 31, 23, 59, text, sizeof(text)));
    EXPECT_STREQ(text, "Stand 31.12. 23:59");
    ASSERT_TRUE(info_format_stamp(false, 2026, 1, 1, 0, 0, text, sizeof(text)));
    EXPECT_STREQ(text, "Updated 1 Jan 00:00");
}

TEST(InfoCore, NoStampWithoutAClockOrWithNonsense)
{
    char text[40] = "x";
    EXPECT_FALSE(
        info_format_stamp(false, 1970, 1, 1, 0, 0, text, sizeof(text)));  // clock never set
    EXPECT_STREQ(text, "");
    EXPECT_FALSE(info_format_stamp(false, 2023, 12, 31, 12, 0, text, sizeof(text)));
    EXPECT_FALSE(info_format_stamp(false, 2026, 0, 1, 12, 0, text, sizeof(text)));
    EXPECT_FALSE(info_format_stamp(false, 2026, 13, 1, 12, 0, text, sizeof(text)));
    EXPECT_FALSE(info_format_stamp(false, 2026, 9, 32, 12, 0, text, sizeof(text)));
    EXPECT_FALSE(info_format_stamp(false, 2026, 9, 30, 24, 0, text, sizeof(text)));
    EXPECT_FALSE(info_format_stamp(false, 2026, 9, 30, 12, 60, text, sizeof(text)));
    EXPECT_FALSE(info_format_stamp(false, 2026, 9, 30, -1, 0, text, sizeof(text)));
    EXPECT_FALSE(info_format_stamp(false, 2026, 9, 30, 12, 0, text, 0));  // no room, no crash
}

TEST(InfoCore, StampIsCutNotOverflowed)
{
    char text[8];
    info_format_stamp(false, 2026, 9, 30, 14, 35, text, sizeof(text));
    EXPECT_EQ(strlen(text), 7u);
    char guard[4 + 24];
    memset(guard, 'G', sizeof(guard));
    info_format_stamp(true, 2026, 9, 30, 14, 35, guard, 4);
    EXPECT_EQ(guard[4], 'G');  // nothing written behind the size
}

TEST(InfoCore, StampOfNow)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    now.hour = 14;
    now.minute = 35;
    char text[40];
    ASSERT_TRUE(info_format_stamp_now(&now, text, sizeof(text)));
    EXPECT_STREQ(text, "Updated 30 Sep 14:35");
    now.german = true;
    ASSERT_TRUE(info_format_stamp_now(&now, text, sizeof(text)));
    EXPECT_STREQ(text, "Stand 30.09. 14:35");
}

TEST(InfoCore, SpanNote)
{
    char text[16];
    info_format_span(42, false, text, sizeof(text));
    EXPECT_STREQ(text, "42 d");
    info_format_span(42, true, text, sizeof(text));
    EXPECT_STREQ(text, "42 T");
    info_format_span(1, false, text, sizeof(text));
    EXPECT_STREQ(text, "1 d");
    info_format_span(0, false, text, sizeof(text));
    EXPECT_STREQ(text, "");
    info_format_span(-1, true, text, sizeof(text));
    EXPECT_STREQ(text, "");
    info_format_span(1000000, false, text, sizeof(text));
    EXPECT_STREQ(text, "9999 d");
    info_format_span(5, false, text, 0);  // no room, no crash
}
