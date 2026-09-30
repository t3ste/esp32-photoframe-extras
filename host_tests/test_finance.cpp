#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "fx_rates.h"
#include "info_screens_core.h"
#include "screen_canvas.h"
#include "screen_finance.h"
}

#include "guarded_canvas.h"

namespace
{

// Real answers of the ECB data portal (public reference data, see data/ecb/README.md).
std::string fixture(const std::string &name)
{
    std::ifstream in(std::string(FINANCE_TEST_DATA_DIR) + "/" + name, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::vector<std::string> codes_of(const std::string &text, int max = FX_MAX_CURRENCIES)
{
    std::vector<std::string> out;
    char codes[16][FX_CODE_LEN];
    int n = fx_parse_codes(text.c_str(), codes, max);
    for (int i = 0; i < n; i++) {
        out.push_back(codes[i]);
    }
    return out;
}

std::vector<fx_series_t> parse(const std::string &csv, int max = FX_MAX_CURRENCIES)
{
    std::vector<fx_series_t> series(max);
    int n = fx_parse_csv(csv.c_str(), series.data(), max);
    series.resize(n);
    return series;
}

const int kBoardSizes[][2] = {{800, 480}, {480, 800}, {960, 540}, {1200, 1600}, {1872, 1404}};

finance_screen_data_t sample(int count, float first_rate, float step)
{
    finance_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = FINANCE_SCREEN_OK;
    data.count = count;
    static const char *const names[] = {"USD", "GBP", "CHF", "JPY"};
    for (int i = 0; i < count; i++) {
        fx_series_t &series = data.series[i];
        strcpy(series.code, names[i]);
        series.count = FX_MAX_POINTS;
        for (int p = 0; p < FX_MAX_POINTS; p++) {
            snprintf(series.dates[p], FX_DATE_LEN, "2026-09-%02d", 1 + p % 28);
            series.values[p] = (first_rate + (float) i * 40.0f) * (1.0f + step * (float) p);
        }
    }
    return data;
}

}  // namespace

// ---- currency codes and the request -------------------------------------------------------------

TEST(FxCodes, AListAsUsersTypeIt)
{
    EXPECT_EQ(codes_of("usd, gbp; CHF"), (std::vector<std::string>{"USD", "GBP", "CHF"}));
    EXPECT_EQ(codes_of("USD\nGBP\r\n  jpy  "), (std::vector<std::string>{"USD", "GBP", "JPY"}));
    EXPECT_EQ(codes_of("Usd"), (std::vector<std::string>{"USD"}));
}

TEST(FxCodes, EuroDoublesAndNonsenseAreLeftOut)
{
    EXPECT_EQ(codes_of("EUR, usd, USD, eur"), (std::vector<std::string>{"USD"}));
    EXPECT_EQ(codes_of("US, USDX, 12A, U$D, usd"), (std::vector<std::string>{"USD"}));
    EXPECT_TRUE(codes_of("").empty());
    EXPECT_TRUE(codes_of(",;  ,").empty());
    EXPECT_TRUE(codes_of("EUR").empty());
    char codes[4][FX_CODE_LEN];
    EXPECT_EQ(fx_parse_codes(nullptr, codes, 4), 0);
}

TEST(FxCodes, AtMostAsManyAsAskedFor)
{
    EXPECT_EQ(codes_of("USD GBP CHF JPY SEK NOK").size(), 4u);
    EXPECT_EQ(codes_of("USD GBP CHF JPY SEK NOK", 2), (std::vector<std::string>{"USD", "GBP"}));
}

TEST(FxUrl, OneRequestForAllCurrencies)
{
    char codes[4][FX_CODE_LEN] = {"USD", "GBP", "CHF", "JPY"};
    char url[256];
    ASSERT_TRUE(fx_build_url(codes, 4, 30, url, sizeof(url)));
    EXPECT_STREQ(url,
                 "https://data-api.ecb.europa.eu/service/data/EXR/D.USD+GBP+CHF+JPY.EUR.SP00.A"
                 "?lastNObservations=30&format=csvdata&detail=dataonly");
    ASSERT_TRUE(fx_build_url(codes, 1, 5, url, sizeof(url)));
    EXPECT_NE(std::string(url).find("/D.USD.EUR."), std::string::npos);
    EXPECT_NE(std::string(url).find("lastNObservations=5&"), std::string::npos);
}

TEST(FxUrl, PointsAreClampedAndBadInputIsRefused)
{
    char codes[4][FX_CODE_LEN] = {"USD", "GBP", "CHF", "JPY"};
    char url[256];
    ASSERT_TRUE(fx_build_url(codes, 2, 0, url, sizeof(url)));
    EXPECT_NE(std::string(url).find("lastNObservations=1&"), std::string::npos);
    ASSERT_TRUE(fx_build_url(codes, 2, 5000, url, sizeof(url)));
    EXPECT_NE(std::string(url).find("lastNObservations=30&"), std::string::npos);
    EXPECT_FALSE(fx_build_url(codes, 0, 30, url, sizeof(url)));
    EXPECT_FALSE(fx_build_url(codes, 5, 30, url, sizeof(url)));
    char small[40];
    EXPECT_FALSE(fx_build_url(codes, 2, 30, small, sizeof(small)));
}

// ---- the answer
// -----------------------------------------------------------------------------------

TEST(FxParse, TheRealAnswerForFourCurrencies)
{
    auto series = parse(fixture("exr-four-30.csv"));
    ASSERT_EQ(series.size(), 4u);
    std::set<std::string> seen;
    for (const auto &s : series) {
        seen.insert(s.code);
        EXPECT_EQ(s.count, 30) << s.code;
        for (int i = 1; i < s.count; i++) {
            EXPECT_LT(strcmp(s.dates[i - 1], s.dates[i]), 0) << s.code << " dates ascend";
        }
        for (int i = 0; i < s.count; i++) {
            EXPECT_GT(s.values[i], 0.0f);
        }
    }
    EXPECT_EQ(seen, (std::set<std::string>{"USD", "GBP", "CHF", "JPY"}));
}

TEST(FxParse, ValuesAreThoseOfTheFile)
{
    auto series = parse(fixture("exr-four-30.csv"));
    // the fixture is the answer of one day; look the newest USD line up in the text itself
    std::string text = fixture("exr-four-30.csv");
    std::string last_usd;
    std::istringstream lines(text);
    for (std::string line; std::getline(lines, line);) {
        if (line.rfind("EXR.D.USD.EUR", 0) == 0) {
            last_usd = line;
        }
    }
    ASSERT_FALSE(last_usd.empty());
    float expected = strtof(last_usd.substr(last_usd.rfind(',') + 1).c_str(), nullptr);
    for (const auto &s : series) {
        if (std::string(s.code) == "USD") {
            EXPECT_FLOAT_EQ(s.values[s.count - 1], expected);
            EXPECT_EQ(std::string(s.dates[s.count - 1]),
                      last_usd.substr(last_usd.rfind(',') - 10, 10));
        }
    }
}

TEST(FxParse, TheFullFormatWithQuotedTitlesWorksToo)
{
    auto series = parse(fixture("exr-usd-full-3.csv"));
    ASSERT_EQ(series.size(), 1u);
    EXPECT_STREQ(series[0].code, "USD");
    EXPECT_EQ(series[0].count, 3);
    EXPECT_GT(series[0].values[2], 0.5f);
}

TEST(FxParse, SeriesAreOrderedLikeTheUsersList)
{
    auto series = parse(fixture("exr-four-30.csv"));  // the ECB answers CHF, GBP, JPY, USD
    ASSERT_EQ(series.size(), 4u);
    EXPECT_STREQ(series[0].code, "CHF");
    char codes[4][FX_CODE_LEN] = {"USD", "JPY", "GBP", "CHF"};
    EXPECT_EQ(fx_order_series(series.data(), (int) series.size(), codes, 4), 4);
    EXPECT_STREQ(series[0].code, "USD");
    EXPECT_STREQ(series[1].code, "JPY");
    EXPECT_STREQ(series[2].code, "GBP");
    EXPECT_STREQ(series[3].code, "CHF");
    // a shorter list drops the others; a currency the answer does not have is skipped
    char some[3][FX_CODE_LEN] = {"SEK", "GBP", "USD"};
    EXPECT_EQ(fx_order_series(series.data(), 4, some, 3), 2);
    EXPECT_STREQ(series[0].code, "GBP");
    EXPECT_STREQ(series[1].code, "USD");
}

TEST(FxParse, RowsWithoutAValueAreSkippedAndUnsortedInputIsSorted)
{
    auto series = parse(
        "KEY,FREQ,CURRENCY,CURRENCY_DENOM,EXR_TYPE,EXR_SUFFIX,TIME_PERIOD,OBS_VALUE\n"
        "k,D,USD,EUR,SP00,A,2026-09-03,1.30\n"
        "k,D,USD,EUR,SP00,A,2026-09-01,1.10\n"
        "k,D,USD,EUR,SP00,A,2026-09-02,\n"
        "k,D,USD,EUR,SP00,A,2026-09-04,NaN\n"
        "k,D,USD,EUR,SP00,A,2026-09-05,abc\n"
        "k,D,USD,EUR,SP00,A,2026-09-06,-2\n"
        "k,D,USD,EUR,SP00,A,2026-09-07,1.20\n");
    ASSERT_EQ(series.size(), 1u);
    ASSERT_EQ(series[0].count, 3);
    EXPECT_STREQ(series[0].dates[0], "2026-09-01");
    EXPECT_STREQ(series[0].dates[1], "2026-09-03");
    EXPECT_STREQ(series[0].dates[2], "2026-09-07");
    EXPECT_FLOAT_EQ(series[0].values[1], 1.30f);
}

TEST(FxParse, TheSameDayTwiceKeepsTheLaterLine)
{
    auto series = parse(
        "CURRENCY,TIME_PERIOD,OBS_VALUE\n"
        "USD,2026-09-01,1.10\n"
        "USD,2026-09-01,1.11\n");
    ASSERT_EQ(series.size(), 1u);
    EXPECT_EQ(series[0].count, 1);
    EXPECT_FLOAT_EQ(series[0].values[0], 1.11f);
}

TEST(FxParse, OnlyTheNewestPointsAreKept)
{
    std::string csv = "CURRENCY,TIME_PERIOD,OBS_VALUE\n";
    for (int day = 1; day <= 40; day++) {
        char line[64];
        snprintf(line, sizeof(line), "USD,2026-08-%02d,%d.5\n", day <= 31 ? day : 31, day);
        if (day > 31) {
            snprintf(line, sizeof(line), "USD,2026-09-%02d,%d.5\n", day - 31, day);
        }
        csv += line;
    }
    auto series = parse(csv);
    ASSERT_EQ(series.size(), 1u);
    ASSERT_EQ(series[0].count, FX_MAX_POINTS);
    EXPECT_FLOAT_EQ(series[0].values[FX_MAX_POINTS - 1], 40.5f);
    EXPECT_FLOAT_EQ(series[0].values[0], 11.5f);  // days 1-10 have been dropped
}

TEST(FxParse, ColumnOrderDoesNotMatter)
{
    auto series = parse(
        "OBS_VALUE,TIME_PERIOD,CURRENCY\n"
        "1.5,2026-09-01,GBP\n"
        "1.6,2026-09-02,GBP\n");
    ASSERT_EQ(series.size(), 1u);
    EXPECT_STREQ(series[0].code, "GBP");
    EXPECT_FLOAT_EQ(series[0].values[1], 1.6f);
}

TEST(FxParse, AnswersThatAreNotRatesGiveNothing)
{
    EXPECT_TRUE(parse("").empty());
    EXPECT_TRUE(parse("<html><body>503 Service Unavailable</body></html>").empty());
    EXPECT_TRUE(parse("KEY,FREQ\nx,D\n").empty());
    EXPECT_TRUE(parse("CURRENCY,TIME_PERIOD,OBS_VALUE\n").empty());  // header only
    EXPECT_TRUE(parse("CURRENCY,TIME_PERIOD,OBS_VALUE\nUSD,2026-09-01,\n").empty());
    char nothing[1];
    EXPECT_EQ(fx_parse_csv(nullptr, (fx_series_t *) nothing, 1), 0);
}

TEST(FxParse, TooManyCurrenciesAreCapped)
{
    std::string csv = "CURRENCY,TIME_PERIOD,OBS_VALUE\n";
    for (const char *code : {"USD", "GBP", "CHF", "JPY", "SEK", "NOK"}) {
        csv += std::string(code) + ",2026-09-01,1.0\n";
    }
    EXPECT_EQ(parse(csv, 4).size(), 4u);
    EXPECT_EQ(parse(csv, 2).size(), 2u);
}

TEST(FxParse, CrLfLinesAndBlankLinesAreFine)
{
    auto series = parse("CURRENCY,TIME_PERIOD,OBS_VALUE\r\n\r\nUSD,2026-09-01,1.25\r\n\r\n");
    ASSERT_EQ(series.size(), 1u);
    EXPECT_FLOAT_EQ(series[0].values[0], 1.25f);
}

TEST(FxStale, CurrenciesTheEcbStoppedPublishingAreDropped)
{
    auto series = parse(
        "CURRENCY,TIME_PERIOD,OBS_VALUE\n"
        "USD,2026-09-29,1.13\n"
        "BGN,2025-12-31,1.95\n"
        "GBP,2026-09-25,0.86\n"
        "RUB,2022-03-01,100\n"
        "JPY,2026-09-10,162\n",
        8);
    ASSERT_EQ(series.size(), 5u);
    int kept = fx_drop_stale(series.data(), (int) series.size(), 2026, 9, 30, 14);
    ASSERT_EQ(kept, 2);  // JPY is 20 days old, BGN and RUB years
    EXPECT_STREQ(series[0].code, "USD");
    EXPECT_STREQ(series[1].code, "GBP");
}

TEST(FxStale, TheAgeIsCountedInRealDaysAcrossMonthsAndYears)
{
    auto series = parse("CURRENCY,TIME_PERIOD,OBS_VALUE\nUSD,2026-12-30,1.1\n");
    ASSERT_EQ(series.size(), 1u);
    EXPECT_EQ(fx_drop_stale(series.data(), 1, 2027, 1, 12, 14), 1);  // 13 days
    EXPECT_EQ(fx_drop_stale(series.data(), 1, 2027, 1, 13, 14), 1);  // 14 days
    EXPECT_EQ(fx_drop_stale(series.data(), 1, 2027, 1, 14, 14), 0);  // 15 days
}

TEST(FxStale, AWrongClockDoesNotDropCurrentRates)
{
    auto series = parse("CURRENCY,TIME_PERIOD,OBS_VALUE\nUSD,2026-09-29,1.1\n");
    EXPECT_EQ(fx_drop_stale(series.data(), 1, 1970, 1, 1, 14), 1);  // "today" is before the rate
    EXPECT_EQ(fx_drop_stale(series.data(), 0, 2026, 9, 30, 14), 0);
}

TEST(FxNumbers, ChangeAgainstTheDayBefore)
{
    fx_series_t s;
    memset(&s, 0, sizeof(s));
    s.count = 2;
    s.values[0] = 1.00f;
    s.values[1] = 1.02f;
    EXPECT_NEAR(fx_change_percent(&s), 2.0f, 1e-3);
    s.values[1] = 0.95f;
    EXPECT_NEAR(fx_change_percent(&s), -5.0f, 1e-3);
    s.count = 1;
    EXPECT_EQ(fx_change_percent(&s), 0.0f);
    s.count = 0;
    EXPECT_EQ(fx_change_percent(&s), 0.0f);
    EXPECT_EQ(fx_change_percent(nullptr), 0.0f);
}

TEST(FxNumbers, RatesGetASensibleNumberOfDecimals)
{
    char text[24];
    fx_format_rate(1.1355f, text, sizeof(text));
    EXPECT_STREQ(text, "1.1355");
    fx_format_rate(42.4531f, text, sizeof(text));
    EXPECT_STREQ(text, "42.453");
    fx_format_rate(162.34f, text, sizeof(text));
    EXPECT_STREQ(text, "162.34");
    fx_format_rate(0.8674f, text, sizeof(text));
    EXPECT_STREQ(text, "0.8674");
    fx_format_rate(1234.5f, text, sizeof(text));
    EXPECT_STREQ(text, "1234.50");
}

// ---- the page
// --------------------------------------------------------------------------------------

TEST(FinanceScreen, EveryNumberOfCurrenciesFitsEveryPanel)
{
    for (const auto &size : kBoardSizes) {
        for (int count = 1; count <= FX_MAX_CURRENCIES; count++) {
            for (bool german : {false, true}) {
                GuardedCanvas cv(size[0], size[1]);
                info_now_t now;
                info_now_from_date(2026, 9, 30, german, &now);
                finance_screen_data_t data = sample(count, 1.1f, 0.003f);
                finance_screen_render(&cv.canvas, &now, &data);
                ASSERT_TRUE(cv.guards_intact()) << count << " " << size[0] << "x" << size[1];
                EXPECT_GT(cv.painted(), (size_t) 2000);
            }
        }
    }
}

TEST(FinanceScreen, TrendColoursFollowTheDirection)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    GuardedCanvas rising(800, 480), falling(800, 480), flat(800, 480);
    finance_screen_data_t up = sample(2, 1.1f, 0.004f);
    finance_screen_data_t down = sample(2, 1.1f, -0.004f);
    finance_screen_data_t none = sample(2, 1.1f, 0.0f);
    finance_screen_render(&rising.canvas, &now, &up);
    finance_screen_render(&falling.canvas, &now, &down);
    finance_screen_render(&flat.canvas, &now, &none);
    EXPECT_TRUE(rising.colours().count(0x00FF00));
    EXPECT_FALSE(rising.colours().count(0xFF0000));
    EXPECT_TRUE(falling.colours().count(0xFF0000));
    EXPECT_FALSE(falling.colours().count(0x00FF00));
    EXPECT_FALSE(flat.colours().count(0x00FF00));
    EXPECT_FALSE(flat.colours().count(0xFF0000));
    EXPECT_TRUE(rising.colours().count(0x0000FF));  // the blue header
}

TEST(FinanceScreen, AllColoursAreExactPaletteColours)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, true, &now);
    for (const auto &size : {std::pair<int, int>{800, 480}, std::pair<int, int>{480, 800}}) {
        GuardedCanvas cv(size.first, size.second);
        finance_screen_data_t data = sample(4, 1.1f, 0.003f);
        finance_screen_render(&cv.canvas, &now, &data);
        for (uint32_t rgb : cv.colours()) {
            for (int shift : {16, 8, 0}) {
                uint32_t v = (rgb >> shift) & 0xFF;
                EXPECT_TRUE(v == 0 || v == 255) << std::hex << rgb;
            }
        }
    }
}

TEST(FinanceScreen, MessagesForNoNetworkAndNoAnswerInBothLanguages)
{
    std::set<std::string> seen;
    for (bool german : {false, true}) {
        for (finance_screen_status_t status :
             {FINANCE_SCREEN_NO_NETWORK, FINANCE_SCREEN_FETCH_FAILED}) {
            GuardedCanvas cv(800, 480);
            info_now_t now;
            info_now_from_date(2026, 9, 30, german, &now);
            finance_screen_data_t data;
            memset(&data, 0, sizeof(data));
            data.status = status;
            finance_screen_render(&cv.canvas, &now, &data);
            EXPECT_TRUE(cv.guards_intact());
            EXPECT_GT(cv.painted(), (size_t) 300);
            EXPECT_TRUE(
                seen.insert(std::string((const char *) cv.canvas.rgb, 800 * 480 * 3)).second);
        }
    }
    // rates that are "ok" but empty count as a failed fetch
    GuardedCanvas a(800, 480), b(800, 480);
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    finance_screen_data_t empty;
    memset(&empty, 0, sizeof(empty));
    empty.status = FINANCE_SCREEN_OK;
    finance_screen_render(&a.canvas, &now, &empty);
    empty.status = FINANCE_SCREEN_FETCH_FAILED;
    finance_screen_render(&b.canvas, &now, &empty);
    EXPECT_EQ(memcmp(a.canvas.rgb, b.canvas.rgb, (size_t) 800 * 480 * 3), 0);
}

TEST(FinanceScreen, TheRealAnswerDrawsOnEveryPanel)
{
    auto parsed = parse(fixture("exr-four-30.csv"));
    char codes[4][FX_CODE_LEN] = {"USD", "GBP", "CHF", "JPY"};
    finance_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = FINANCE_SCREEN_OK;
    data.count = fx_order_series(parsed.data(), (int) parsed.size(), codes, 4);
    ASSERT_EQ(data.count, 4);
    for (int i = 0; i < data.count; i++) {
        data.series[i] = parsed[i];
    }
    for (const auto &size : kBoardSizes) {
        GuardedCanvas cv(size[0], size[1]);
        info_now_t now;
        info_now_from_date(2026, 9, 30, false, &now);
        finance_screen_render(&cv.canvas, &now, &data);
        EXPECT_TRUE(cv.guards_intact());
        EXPECT_GT(cv.painted(), (size_t) 5000);
    }
}

TEST(FinanceScreen, ExtremeRatesAndFlatSeriesStayInsideTheCanvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    for (const auto &size : kBoardSizes) {
        GuardedCanvas cv(size[0], size[1]);
        finance_screen_data_t data = sample(4, 12345.678f, 0.0f);  // flat and huge
        data.series[1].count = 1;                                  // a single point: no sparkline
        data.series[2].count = 2;
        data.series[3].values[5] = 1e-6f;  // a spike far below the rest
        finance_screen_render(&cv.canvas, &now, &data);
        EXPECT_TRUE(cv.guards_intact()) << size[0] << "x" << size[1];
    }
}
