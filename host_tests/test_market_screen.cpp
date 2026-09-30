#include <gtest/gtest.h>

#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

extern "C" {
#include "info_screens_core.h"
#include "market_quotes.h"
#include "screen_canvas.h"
#include "screen_markets.h"
}

#include "guarded_canvas.h"

namespace
{

const int kBoardSizes[][2] = {{800, 480}, {480, 800}, {960, 540}, {1200, 1600}, {1872, 1404}};

std::string fixture(const std::string &name)
{
    std::ifstream in(std::string(MARKET_TEST_DATA_DIR) + "/" + name, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

market_series_t series_of(const char *file, const char *symbol)
{
    market_series_t series;
    market_parse_status_t status = market_parse_yahoo(fixture(file).c_str(), &series);
    EXPECT_EQ(status, MARKET_PARSE_OK) << file;
    snprintf(series.symbol, sizeof(series.symbol), "%s", symbol);
    return series;
}

// The four real Yahoo answers as page rows.
markets_screen_data_t real_page(int count)
{
    markets_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = MARKETS_SCREEN_OK;
    data.count = count;
    data.series[0] = series_of("yahoo-aapl.json", "AAPL");
    data.series[1] = series_of("yahoo-eunl-de.json", "EUNL.DE");
    data.series[2] = series_of("yahoo-gdaxi.json", "^GDAXI");
    data.series[3] = series_of("yahoo-btc-eur.json", "BTC-EUR");
    return data;
}

std::string picture(const markets_screen_data_t &data, bool german = false, int w = 800,
                    int h = 480)
{
    GuardedCanvas cv(w, h);
    info_now_t now;
    info_now_from_date(2026, 9, 30, german, &now);
    markets_screen_render(&cv.canvas, &now, &data);
    EXPECT_TRUE(cv.guards_intact());
    return std::string((const char *) cv.canvas.rgb, (size_t) w * h * 3);
}

markets_screen_data_t message_data(markets_screen_status_t status)
{
    markets_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = status;
    return data;
}

}  // namespace

TEST(MarketsScreen, EveryNumberOfRowsFitsEveryPanel)
{
    for (const auto &size : kBoardSizes) {
        for (int count = 1; count <= MARKET_MAX_SYMBOLS; count++) {
            for (bool german : {false, true}) {
                GuardedCanvas cv(size[0], size[1]);
                info_now_t now;
                info_now_from_date(2026, 9, 30, german, &now);
                markets_screen_data_t data = real_page(count);
                markets_screen_render(&cv.canvas, &now, &data);
                ASSERT_TRUE(cv.guards_intact()) << count << " " << size[0] << "x" << size[1];
                EXPECT_GT(cv.painted(), (size_t) 2000);
            }
        }
    }
}

TEST(MarketsScreen, TheTrendIsGreenOrRedAndAllColoursArePalette)
{
    markets_screen_data_t data = real_page(4);
    market_series_t &up = data.series[0];
    up.count = 2;
    up.values[0] = 100.0f;
    up.values[1] = 110.0f;
    market_series_t &down = data.series[1];
    down.count = 2;
    down.values[0] = 100.0f;
    down.values[1] = 90.0f;
    for (const auto &size : kBoardSizes) {
        GuardedCanvas cv(size[0], size[1]);
        info_now_t now;
        info_now_from_date(2026, 9, 30, false, &now);
        markets_screen_render(&cv.canvas, &now, &data);
        std::set<uint32_t> colours = cv.colours();
        EXPECT_TRUE(colours.count(0x00FF00)) << size[0] << "x" << size[1];
        EXPECT_TRUE(colours.count(0xFF0000)) << size[0] << "x" << size[1];
        EXPECT_TRUE(colours.count(0x0000FF));  // the header band
        for (uint32_t rgb : colours) {
            for (int shift : {16, 8, 0}) {
                uint32_t v = (rgb >> shift) & 0xFF;
                EXPECT_TRUE(v == 0 || v == 255) << std::hex << rgb;
            }
        }
    }
}

TEST(MarketsScreen, PricesFromTheCacheAreDrawnDifferently)
{
    markets_screen_data_t fresh = real_page(2);
    markets_screen_data_t old = fresh;
    old.series[1].stale = true;
    EXPECT_TRUE(picture(fresh) != picture(old));
    old.series[1].stale = false;
    EXPECT_TRUE(picture(fresh) == picture(old));
}

TEST(MarketsScreen, TheFooterNamesTheSources)
{
    markets_screen_data_t a = real_page(2);
    markets_screen_data_t b = a;
    b.series[1].provider = MARKET_ALPHAVANTAGE;
    EXPECT_TRUE(picture(a) != picture(b));
    EXPECT_TRUE(picture(a, false) != picture(a, true));
}

TEST(MarketsScreen, ASymbolWithoutDataGetsARowThatSaysSo)
{
    markets_screen_data_t data = real_page(3);
    markets_screen_data_t gap = data;
    gap.series[1].count = 0;
    EXPECT_TRUE(picture(data) != picture(gap));
    for (const auto &size : kBoardSizes) {
        picture(gap, false, size[0], size[1]);
        picture(gap, true, size[0], size[1]);
    }
}

TEST(MarketsScreen, NothingToShowIsTheMessage)
{
    markets_screen_data_t failed = message_data(MARKETS_SCREEN_FETCH_FAILED);

    markets_screen_data_t empty_ok = failed;  // says OK but no row has data
    empty_ok.status = MARKETS_SCREEN_OK;
    empty_ok.count = 2;
    strcpy(empty_ok.series[0].symbol, "AAPL");
    strcpy(empty_ok.series[1].symbol, "MSFT");
    EXPECT_TRUE(picture(failed) == picture(empty_ok));

    markets_screen_data_t no_rows = failed;  // OK with no rows at all
    no_rows.status = MARKETS_SCREEN_OK;
    EXPECT_TRUE(picture(failed) == picture(no_rows));
}

TEST(MarketsScreen, EveryReasonHasItsOwnMessageInBothLanguages)
{
    std::set<std::string> seen;
    for (bool german : {false, true}) {
        for (markets_screen_status_t status :
             {MARKETS_SCREEN_NO_NETWORK, MARKETS_SCREEN_NO_SOURCE, MARKETS_SCREEN_FETCH_FAILED}) {
            EXPECT_TRUE(seen.insert(picture(message_data(status), german)).second)
                << german << " " << status;
        }
    }
    // an unknown status is still a message, not a crash
    GuardedCanvas cv(800, 480);
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    markets_screen_data_t data = message_data((markets_screen_status_t) 99);
    markets_screen_render(&cv.canvas, &now, &data);
    EXPECT_TRUE(cv.guards_intact());
    EXPECT_GT(cv.painted(), (size_t) 300);
}

TEST(MarketsScreen, MessagesFitEveryPanel)
{
    for (const auto &size : kBoardSizes) {
        for (bool german : {false, true}) {
            for (markets_screen_status_t status :
                 {MARKETS_SCREEN_NO_NETWORK, MARKETS_SCREEN_NO_SOURCE,
                  MARKETS_SCREEN_FETCH_FAILED}) {
                GuardedCanvas cv(size[0], size[1]);
                info_now_t now;
                info_now_from_date(2026, 9, 30, german, &now);
                markets_screen_data_t data = message_data(status);
                markets_screen_render(&cv.canvas, &now, &data);
                ASSERT_TRUE(cv.guards_intact());
                EXPECT_GT(cv.painted(), (size_t) 200);
                int u = canvas_unit(&cv.canvas);
                for (int y = 0; y < size[1]; y++) {
                    for (int x = 0; x < u; x++) {
                        ASSERT_TRUE(cv.blank(x, y) && cv.blank(size[0] - 1 - x, y))
                            << size[0] << "x" << size[1] << " " << status;
                    }
                }
            }
        }
    }
}

TEST(MarketsScreen, DegenerateSeriesAreDrawnWithoutTrouble)
{
    markets_screen_data_t data = real_page(4);
    data.series[0].count = 1;  // one point: no change, no line
    data.series[1].count = 2;  // two equal points: a flat line, no arrow
    data.series[1].values[0] = data.series[1].values[1] = 50.0f;
    for (int i = 0; i < 30; i++) {  // thirty equal points
        data.series[2].values[i] = 7.0f;
    }
    data.series[2].count = 30;
    data.series[3].values[data.series[3].count - 1] = 0.000001f;  // a tiny price
    for (const auto &size : kBoardSizes) {
        for (bool german : {false, true}) {
            picture(data, german, size[0], size[1]);
        }
    }
}

TEST(MarketsScreen, MoreRowsThanSymbolsAreClamped)
{
    markets_screen_data_t four = real_page(4);
    markets_screen_data_t nine = four;
    nine.count = 9;  // damaged count: only what the struct holds is drawn
    EXPECT_TRUE(picture(four) == picture(nine));
}

TEST(MarketsScreen, LongTextsStayInsideTheMargins)
{
    markets_screen_data_t data = real_page(4);
    for (int i = 0; i < 4; i++) {
        snprintf(data.series[i].symbol, sizeof(data.series[i].symbol), "ABCDEFGHIJKLMN%d", i);
        memset(data.series[i].name, 'W', sizeof(data.series[i].name) - 1);
        data.series[i].name[sizeof(data.series[i].name) - 1] = 0;
        snprintf(data.series[i].currency, sizeof(data.series[i].currency), "WWWW");
        data.series[i].values[data.series[i].count - 1] = 98765432.0f;  // a huge price
        data.series[i].stale = i == 2;
        data.series[i].provider = (market_provider_t) (i % MARKET_PROVIDER_COUNT);
    }
    for (const auto &size : kBoardSizes) {
        for (bool german : {false, true}) {
            GuardedCanvas cv(size[0], size[1]);
            info_now_t now;
            info_now_from_date(2026, 9, 30, german, &now);
            markets_screen_render(&cv.canvas, &now, &data);
            ASSERT_TRUE(cv.guards_intact());
            int u = canvas_unit(&cv.canvas);
            int band = canvas_text_height(canvas_text_scale(&cv.canvas, 1)) + 2 * u;
            for (int y = band; y < size[1]; y++) {
                for (int x = size[0] - 2 * u; x < size[0]; x++) {
                    ASSERT_TRUE(cv.blank(x, y))
                        << size[0] << "x" << size[1] << " at " << x << "," << y;
                }
                for (int x = 0; x < 2 * u; x++) {
                    ASSERT_TRUE(cv.blank(x, y))
                        << size[0] << "x" << size[1] << " at " << x << "," << y;
                }
            }
        }
    }
}

TEST(MarketsScreen, TheHeaderShowsTheNewestDateOfAllRows)
{
    markets_screen_data_t a = real_page(2);
    markets_screen_data_t b = a;
    // the second row is newer than anything in the first: the header follows it
    strcpy(b.series[1].dates[b.series[1].count - 1], "2026-10-05");
    EXPECT_TRUE(picture(a).substr(0, 40 * 800 * 3) != picture(b).substr(0, 40 * 800 * 3));
    // an older second row leaves the band alone
    markets_screen_data_t c = a;
    strcpy(c.series[0].dates[c.series[0].count - 1], "2026-09-01");
    EXPECT_TRUE(picture(a).substr(0, 40 * 800 * 3) == picture(c).substr(0, 40 * 800 * 3));
}
