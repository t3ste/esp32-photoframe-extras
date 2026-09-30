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
                 bool guards = true)
{
    GuardedCanvas cv(w, h);
    info_now_t now;
    info_now_from_date(year, 9, 30, german, &now);
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
    // the footer text of a page is the same with and without the note, only one line higher with
    // it: so the last line without a clock looks like the line above the note with a clock
    for (Page page : {kFinance, kFuel, kMarkets}) {
        for (const auto &size : kSizes) {
            std::string with_clock = draw(page, size[0], size[1], false);
            std::string without_clock = draw(page, size[0], size[1], false, 1970);
            EXPECT_EQ(ink_in_last_line(without_clock, size[0], size[1]),
                      ink_in_last_line(with_clock, size[0], size[1], 1))
                << page << " " << size[0] << "x" << size[1];
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
