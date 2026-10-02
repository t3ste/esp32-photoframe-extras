// The two small notes of the pages with data from the internet: "Updated 30 Sep 14:35" at the foot
// (weather, exchange rates, fuel prices, markets) and the time a line chart covers ("41 d",
// exchange rates and markets).
#include <gtest/gtest.h>

#include <string>

#include "guarded_canvas.h"
#include "render_screens_extra.h"

namespace
{

const int kSizes[][2] = {{800, 480}, {480, 800}, {960, 540}, {1200, 1600}, {1872, 1404}};

enum Page { kWeather, kFinance, kFuel, kMarkets };
const Page kPages[] = {kWeather, kFinance, kFuel, kMarkets};

// Draws a page of the made-up sample data at a moment; `year` 1970 is a clock that was never set.
std::string draw(Page page, int w, int h, bool german, int year = 2026, int minute = 35,
                 bool guards = true, int month = 9, int day = 30)
{
    GuardedCanvas cv(w, h);
    info_now_t now;
    info_now_from_date(year, month, day, german, &now);
    now.hour = 14;
    now.minute = minute;
    switch (page) {
    case kWeather: {
        static const int highs[] = {24, 22, 19, 17, 15}, lows[] = {12, 11, 9, 6, 2};
        static const int codes[] = {2, 61, 3, 95, 71};
        weather_screen_data_t data = weather_sample("Berlin", 5, highs, lows, codes);
        weather_screen_render(&cv.canvas, &now, &data);
        break;
    }
    case kFinance: {
        finance_screen_data_t data = finance_sample(4);
        finance_screen_render(&cv.canvas, &now, &data);
        break;
    }
    case kFuel: {
        fuel_screen_data_t data = fuel_sample(5, FUEL_DIESEL, 5);
        fuel_screen_render(&cv.canvas, &now, &data);
        break;
    }
    case kMarkets: {
        markets_screen_data_t data = markets_sample(4);
        if (year >= 2024) {
            data.updated_year = year;
            data.updated_month = 9;
            data.updated_day = 30;
            data.updated_hour = 14;
            data.updated_minute = minute;
        }
        markets_screen_render(&cv.canvas, &now, &data);
        break;
    }
    }
    if (guards) {
        EXPECT_TRUE(cv.guards_intact()) << page << " " << w << "x" << h;
    }
    return std::string((const char *) cv.canvas.rgb, (size_t) w * h * 3);
}

// Pixels that are not white in the strip of the last text line above the bottom margin.
int ink_in_last_line(const std::string &picture, int w, int h, int line_offset = 0)
{
    int u = (w < h ? w : h) / 40;
    int s = (w < h ? w : h) >= 1000 ? 3 : 1;
    int line = 24 * s;
    int top = h - u - line - line_offset * line;
    int ink = 0;
    for (int y = top; y < top + line; y++) {
        for (int x = 0; x < w; x++) {
            const unsigned char *p =
                (const unsigned char *) picture.data() + ((size_t) y * w + x) * 3;
            ink += (p[0] != 255 || p[1] != 255 || p[2] != 255) ? 1 : 0;
        }
    }
    return ink;
}

}  // namespace

TEST(InfoNotes, EveryDataPageTellsWhenItsDataWereFetched)
{
    for (Page page : kPages) {
        for (const auto &size : kSizes) {
            for (bool german : {false, true}) {
                std::string with_clock = draw(page, size[0], size[1], german);
                std::string without_clock = draw(page, size[0], size[1], german, 1970);
                EXPECT_NE(with_clock, without_clock) << page << " " << size[0] << "x" << size[1];
                // the note is the last line of the page
                EXPECT_GT(ink_in_last_line(with_clock, size[0], size[1]), 100)
                    << page << " " << size[0] << "x" << size[1];
            }
        }
    }
}

TEST(InfoNotes, TheTimeOfTheNoteFollowsTheClock)
{
    for (Page page : kPages) {
        EXPECT_NE(draw(page, 800, 480, false, 2026, 35), draw(page, 800, 480, false, 2026, 36))
            << page;
    }
}

TEST(InfoNotes, WithoutAClockThereIsNoNoteAtAll)
{
    // no clock, no time on the page: the minute makes no difference then (with a clock it does, see
    // above), and the footer is still there
    for (Page page : {kFinance, kFuel, kMarkets}) {
        for (const auto &size : kSizes) {
            for (bool german : {false, true}) {
                EXPECT_EQ(draw(page, size[0], size[1], german, 1970, 35),
                          draw(page, size[0], size[1], german, 1970, 36))
                    << page << " " << size[0] << "x" << size[1];
                EXPECT_GT(
                    ink_in_last_line(draw(page, size[0], size[1], german, 1970), size[0], size[1]),
                    50)
                    << page << " " << size[0] << "x" << size[1];  // the source stays
            }
        }
    }
    // and the weather page keeps its bottom margin blank either way
    for (const auto &size : kSizes) {
        std::string picture = draw(kWeather, size[0], size[1], false, 1970);
        int u = (size[0] < size[1] ? size[0] : size[1]) / 40;
        for (int y = size[1] - u; y < size[1]; y++) {
            for (int x = 0; x < size[0]; x++) {
                const unsigned char *p =
                    (const unsigned char *) picture.data() + ((size_t) y * size[0] + x) * 3;
                ASSERT_TRUE(p[0] == 255 && p[1] == 255 && p[2] == 255);
            }
        }
    }
}

TEST(InfoNotes, SourceAndTimeShareOneLineWhereTheyFit)
{
    // on an 800 x 480 panel the footer is one line with the time and one line without it: the pages
    // are the same above the last line, so the rows have not moved
    for (Page page : {kFinance, kFuel, kMarkets}) {
        std::string with_clock = draw(page, 800, 480, false);
        std::string without_clock = draw(page, 800, 480, false, 1970);
        size_t above_the_footer = (size_t) (480 - 12 - 24) * 800 * 3;
        EXPECT_EQ(with_clock.substr(0, above_the_footer), without_clock.substr(0, above_the_footer))
            << page;
        EXPECT_NE(with_clock, without_clock) << page;  // but the last line has the time
    }
}

TEST(InfoNotes, TheNoteIsInsideTheMargins)
{
    for (Page page : kPages) {
        for (const auto &size : kSizes) {
            std::string picture = draw(page, size[0], size[1], true);
            int u = (size[0] < size[1] ? size[0] : size[1]) / 40;
            int scale = (size[0] < size[1] ? size[0] : size[1]) >= 1000 ? 3 : 1;
            int band = 24 * scale + 2 * u;  // the header bands run to the edge: look below them
            for (int y = 0; y < size[1]; y++) {
                for (int x = size[0] - u; x < size[0]; x++) {
                    const unsigned char *p =
                        (const unsigned char *) picture.data() + ((size_t) y * size[0] + x) * 3;
                    if (y >= band) {
                        ASSERT_TRUE(p[0] == 255 && p[1] == 255 && p[2] == 255)
                            << page << " " << size[0] << "x" << size[1] << " at " << x << "," << y;
                    }
                }
            }
        }
    }
}

TEST(InfoNotes, TheSpanOfAChartFollowsItsDates)
{
    // a chart that covers 41 days has a note, a chart of one day has none
    for (bool markets : {false, true}) {
        for (const auto &size : kSizes) {
            GuardedCanvas long_span(size[0], size[1]), no_span(size[0], size[1]);
            info_now_t now;
            info_now_from_date(1970, 9, 30, false, &now);  // no clock: only the charts differ
            if (markets) {
                markets_screen_data_t a = markets_sample(3), b = markets_sample(3);
                for (int r = 0; r < 3; r++) {
                    for (int i = 0; i < b.series[r].count; i++) {
                        strcpy(b.series[r].dates[i], "2026-09-30");
                    }
                }
                markets_screen_render(&long_span.canvas, &now, &a);
                markets_screen_render(&no_span.canvas, &now, &b);
            } else {
                finance_screen_data_t a = finance_sample(3), b = finance_sample(3);
                for (int r = 0; r < 3; r++) {
                    for (int i = 0; i < b.series[r].count; i++) {
                        strcpy(b.series[r].dates[i], "2026-09-30");
                    }
                }
                finance_screen_render(&long_span.canvas, &now, &a);
                finance_screen_render(&no_span.canvas, &now, &b);
            }
            EXPECT_NE(
                memcmp(long_span.canvas.rgb, no_span.canvas.rgb, (size_t) size[0] * size[1] * 3), 0)
                << markets << " " << size[0] << "x" << size[1];
            EXPECT_TRUE(long_span.guards_intact());
            EXPECT_TRUE(no_span.guards_intact());
        }
    }
}

TEST(InfoNotes, MarketsFourFooterLinesStillFit)
{
    // long sources, a row from the cache and the note: the longest footer the page can have
    for (const auto &size : kSizes) {
        for (bool german : {false, true}) {
            GuardedCanvas cv(size[0], size[1]);
            info_now_t now;
            info_now_from_date(2026, 9, 30, german, &now);
            markets_screen_data_t data = markets_sample(4);
            data.series[0].provider = MARKET_YAHOO;
            data.series[1].provider = MARKET_TWELVEDATA;
            data.series[2].provider = MARKET_ALPHAVANTAGE;
            data.series[2].stale = true;
            data.updated_year = 2026;
            data.updated_month = 9;
            data.updated_day = 30;
            data.updated_hour = 14;
            data.updated_minute = 35;
            markets_screen_render(&cv.canvas, &now, &data);
            EXPECT_TRUE(cv.guards_intact()) << size[0] << "x" << size[1];
            EXPECT_GT(ink_in_last_line(
                          std::string((const char *) cv.canvas.rgb, (size_t) size[0] * size[1] * 3),
                          size[0], size[1]),
                      100);
        }
    }
}

namespace
{
// Pixels of exactly the yellow of the late marker inside the header band of a page
int yellow_in_header(const std::string &picture, int w, int h)
{
    int u = (w < h ? w : h) / 40;
    int scale = (w < h ? w : h) >= 1000 ? 3 : 1;
    int band = 24 * scale + 2 * u;
    int count = 0;
    for (int y = 0; y < band; y++) {
        for (int x = 0; x < w; x++) {
            const unsigned char *p =
                (const unsigned char *) picture.data() + ((size_t) y * w + x) * 3;
            count += (p[0] == 255 && p[1] == 255 && p[2] == 0) ? 1 : 0;
        }
    }
    return count;
}
}  // namespace

TEST(InfoNotes, TheHeaderMarksDataOlderThanATradingDay)
{
    // the sample charts end on Wednesday 2026-09-30
    struct Moment {
        int month, day;
        bool late;
        const char *what;
    };
    const Moment moments[] = {
        {9, 30, false, "the same day"},
        {10, 1, false, "the next day, still no later close"},
        {10, 2, true, "Friday: Thursday's close is missing"},
        {10, 3, true, "Saturday"},
        {10, 5, true, "Monday"},
    };
    for (Page page : {kFinance, kMarkets}) {
        for (const auto &size : kSizes) {
            for (const Moment &moment : moments) {
                std::string picture =
                    draw(page, size[0], size[1], false, 2026, 35, true, moment.month, moment.day);
                int yellow = yellow_in_header(picture, size[0], size[1]);
                if (moment.late) {
                    EXPECT_GT(yellow, 10)
                        << page << " " << size[0] << "x" << size[1] << " " << moment.what;
                } else {
                    EXPECT_EQ(yellow, 0)
                        << page << " " << size[0] << "x" << size[1] << " " << moment.what;
                }
            }
        }
    }
}

TEST(InfoNotes, WithoutAClockNothingIsLate)
{
    // a clock that was never set says 1970: every price is "in the future" then, none is late
    for (Page page : {kFinance, kMarkets}) {
        for (bool german : {false, true}) {
            std::string picture = draw(page, 800, 480, german, 1970, 35, true, 10, 2);
            EXPECT_EQ(yellow_in_header(picture, 800, 480), 0) << page;
        }
    }
}

TEST(InfoNotes, TheHeaderNamesTheDayOfTheData)
{
    // the day label changes with the language and with the date of the newest point
    std::string en = draw(kMarkets, 800, 480, false);
    std::string de = draw(kMarkets, 800, 480, true);
    EXPECT_NE(en, de);
    // another "now" with the same data differs in the late marker only, so equal otherwise when the
    // marker is not drawn: the 30th and the 1st give the same page
    EXPECT_EQ(draw(kMarkets, 800, 480, false, 2026, 35, true, 9, 30),
              draw(kMarkets, 800, 480, false, 2026, 35, true, 10, 1));
}
