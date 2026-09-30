#include <gtest/gtest.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

extern "C" {
#include "fuel_prices.h"
#include "info_screens_core.h"
#include "screen_canvas.h"
#include "screen_fuel.h"
}

#include "guarded_canvas.h"

namespace
{

std::string fixture(const std::string &name)
{
    std::ifstream in(std::string(FUEL_TEST_DATA_DIR) + "/" + name, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

fuel_result_t parse(const std::string &json, bool hide_closed = true, int max = FUEL_MAX_STATIONS)
{
    fuel_result_t result;
    fuel_parse(json.c_str(), hide_closed, max, &result);
    return result;
}

bool valid_utf8(const char *s)
{
    const unsigned char *p = (const unsigned char *) s;
    while (*p) {
        int extra = *p < 0x80         ? 0
                    : (*p >> 5) == 6  ? 1
                    : (*p >> 4) == 14 ? 2
                    : (*p >> 3) == 30 ? 3
                                      : -1;
        if (extra < 0) {
            return false;
        }
        for (int i = 1; i <= extra; i++) {
            if ((p[i] & 0xC0) != 0x80) {
                return false;
            }
        }
        p += 1 + extra;
    }
    return true;
}

}  // namespace

// ---- the request
// ----------------------------------------------------------------------------------

TEST(FuelUrl, TheDocumentedRequest)
{
    char url[256];
    ASSERT_TRUE(fuel_build_url(url, sizeof(url), "52.521", "13.438", 5, FUEL_DIESEL,
                               "00000000-0000-0000-0000-000000000002"));
    EXPECT_STREQ(url,
                 "https://creativecommons.tankerkoenig.de/json/list.php?lat=52.521&lng=13.438&rad=5"
                 "&sort=price&type=diesel&apikey=00000000-0000-0000-0000-000000000002");
    ASSERT_TRUE(fuel_build_url(url, sizeof(url), "-33.9", "+151.2", 3, FUEL_E10,
                               "00000000-0000-0000-0000-000000000002"));
    EXPECT_NE(std::string(url).find("lat=-33.9&lng=+151.2&rad=3&sort=price&type=e10"),
              std::string::npos);
}

TEST(FuelUrl, RadiusIsClamped)
{
    char url[256];
    const char *key = "00000000-0000-0000-0000-000000000002";
    ASSERT_TRUE(fuel_build_url(url, sizeof(url), "1", "2", 0, FUEL_E5, key));
    EXPECT_NE(std::string(url).find("&rad=1&"), std::string::npos);
    ASSERT_TRUE(fuel_build_url(url, sizeof(url), "1", "2", 500, FUEL_E5, key));
    EXPECT_NE(std::string(url).find("&rad=25&"), std::string::npos);
}

TEST(FuelUrl, AnythingThatDoesNotBelongInTheRequestIsRefused)
{
    char url[256];
    const char *key = "00000000-0000-0000-0000-000000000002";
    EXPECT_FALSE(fuel_build_url(url, sizeof(url), "", "13", 5, FUEL_E5, key));
    EXPECT_FALSE(fuel_build_url(url, sizeof(url), "52", "", 5, FUEL_E5, key));
    EXPECT_FALSE(fuel_build_url(url, sizeof(url), nullptr, "13", 5, FUEL_E5, key));
    EXPECT_FALSE(fuel_build_url(url, sizeof(url), "52.5&x=1", "13", 5, FUEL_E5, key));
    EXPECT_FALSE(fuel_build_url(url, sizeof(url), "52.5", "1 3", 5, FUEL_E5, key));
    EXPECT_FALSE(fuel_build_url(url, sizeof(url), "52.5", "13", 5, FUEL_E5, nullptr));
    EXPECT_FALSE(fuel_build_url(url, sizeof(url), "52.5", "13", 5, FUEL_E5, ""));
    EXPECT_FALSE(fuel_build_url(url, sizeof(url), "52.5", "13", 5, FUEL_E5, "short"));
    EXPECT_FALSE(fuel_build_url(url, sizeof(url), "52.5", "13", 5, FUEL_E5, "abcdefgh&rad=99"));
    EXPECT_FALSE(fuel_build_url(url, sizeof(url), "52.5", "13", 5, FUEL_E5, "abcdefgh/../x"));
    EXPECT_FALSE(fuel_build_url(url, sizeof(url), "12345678901234567", "13", 5, FUEL_E5, key));
    char small[60];
    EXPECT_FALSE(fuel_build_url(small, sizeof(small), "52.5", "13", 5, FUEL_E5, key));
}

TEST(FuelType, NamesRoundTrip)
{
    for (fuel_type_t type : {FUEL_E5, FUEL_E10, FUEL_DIESEL}) {
        EXPECT_EQ(fuel_type_from_name(fuel_type_name(type), FUEL_E5), type);
    }
    EXPECT_EQ(fuel_type_from_name("DIESEL", FUEL_E5), FUEL_DIESEL);
    EXPECT_EQ(fuel_type_from_name("E10", FUEL_E5), FUEL_E10);
    EXPECT_EQ(fuel_type_from_name("super", FUEL_E10), FUEL_E10);
    EXPECT_EQ(fuel_type_from_name(nullptr, FUEL_DIESEL), FUEL_DIESEL);
}

// ---- the answer
// -----------------------------------------------------------------------------------

TEST(FuelParse, TheServicesOwnDemoAnswer)
{
    fuel_result_t r = parse(fixture("list-e5-demo.json"));
    ASSERT_EQ(r.status, FUEL_PARSE_OK);
    ASSERT_EQ(r.count, FUEL_MAX_STATIONS);  // 7 stations, the 5 cheapest
    EXPECT_STREQ(r.stations[0].name, "Sprint");
    EXPECT_STREQ(r.stations[0].place, "Kniprodestr., Berlin");
    EXPECT_FLOAT_EQ(r.stations[0].dist_km, 1.5f);
    EXPECT_NEAR(r.stations[0].price, 1.009f, 1e-4);
    for (int i = 1; i < r.count; i++) {
        EXPECT_LE(r.stations[i - 1].price, r.stations[i].price);
    }
}

TEST(FuelParse, MixedPricesAreSortedAndMissingOnesNeverShownAsZero)
{
    fuel_result_t r = parse(fixture("list-diesel-mixed.json"));
    ASSERT_EQ(r.status, FUEL_PARSE_OK);
    ASSERT_EQ(r.count, 5);
    // closed (1.699), no price and price 0 are out; equal prices keep the order of the answer
    EXPECT_STREQ(r.stations[0].name, "NORD");
    EXPECT_STREQ(r.stations[1].name, "CHEAP");
    EXPECT_NEAR(r.stations[0].price, 1.719f, 1e-4);
    EXPECT_NEAR(r.stations[2].price, 1.759f, 1e-4);
    EXPECT_NEAR(r.stations[3].price, 1.789f, 1e-4);
    EXPECT_NEAR(r.stations[4].price, 1.849f, 1e-4);
    for (int i = 0; i < r.count; i++) {
        EXPECT_GT(r.stations[i].price, 0.0f);
        EXPECT_STRNE(r.stations[i].name, "NOPRICE");
        EXPECT_STRNE(r.stations[i].name, "ZERO");
        EXPECT_STRNE(r.stations[i].name, "CLOSED");
    }
}

TEST(FuelParse, ClosedStationsCanBeKept)
{
    fuel_result_t r = parse(fixture("list-diesel-mixed.json"), false);
    ASSERT_EQ(r.status, FUEL_PARSE_OK);
    EXPECT_STREQ(r.stations[0].name, "CLOSED");
    EXPECT_NEAR(r.stations[0].price, 1.699f, 1e-4);
}

TEST(FuelParse, UmlautsAndLongNamesComeOutAsValidUtf8)
{
    fuel_result_t r = parse(fixture("list-diesel-mixed.json"));
    ASSERT_EQ(r.status, FUEL_PARSE_OK);
    bool found_umlaut_street = false;
    for (int i = 0; i < r.count; i++) {
        EXPECT_TRUE(valid_utf8(r.stations[i].name)) << i;
        EXPECT_TRUE(valid_utf8(r.stations[i].place)) << i;
        EXPECT_LT(strlen(r.stations[i].name), (size_t) FUEL_NAME_MAX);
        EXPECT_LT(strlen(r.stations[i].place), (size_t) FUEL_PLACE_MAX);
        found_umlaut_street = found_umlaut_street || std::string(r.stations[i].place)
                                                             .find(
                                                                 "B\xC3\xA4"
                                                                 "ckerstra\xC3\x9F"
                                                                 "e") != std::string::npos;
    }
    EXPECT_TRUE(found_umlaut_street);  // "ä" and "ß" escapes are decoded
}

TEST(FuelParse, CutsNeverSplitACharacter)
{
    // a brand of 60 umlauts: the cut at FUEL_NAME_MAX - 1 bytes must fall between characters
    std::string umlauts;
    for (int i = 0; i < 60; i++) {
        umlauts += "\xC3\xA4";
    }
    std::string json = "{\"ok\":true,\"stations\":[{\"brand\":\"" + umlauts + "\",\"street\":\"" +
                       umlauts + "\",\"place\":\"" + umlauts +
                       "\",\"dist\":1,\"price\":1.5,\"isOpen\":true}]}";
    fuel_result_t r = parse(json);
    ASSERT_EQ(r.status, FUEL_PARSE_OK);
    EXPECT_TRUE(valid_utf8(r.stations[0].name));
    EXPECT_TRUE(valid_utf8(r.stations[0].place));
    EXPECT_EQ(strlen(r.stations[0].name) % 2, 0u);
}

TEST(FuelParse, AnAnswerThatWasCutOffStillGivesTheStationsBeforeTheCut)
{
    std::string whole = fixture("list-diesel-mixed.json");
    // cut in the middle of the seventh station
    size_t cut = 0;
    for (int i = 0; i < 7; i++) {
        cut = whole.find("{\"id\"", cut + 1);
    }
    ASSERT_NE(cut, std::string::npos);
    fuel_result_t r = parse(whole.substr(0, cut + 20));
    ASSERT_EQ(r.status, FUEL_PARSE_OK);
    ASSERT_GE(r.count, 3);
    EXPECT_STREQ(r.stations[0].name, "NORD");
    for (size_t n : {size_t(40), size_t(100), whole.size() / 2}) {
        r = parse(whole.substr(0, n));  // whatever the cut, no crash and a sane status
        EXPECT_TRUE(r.status == FUEL_PARSE_OK || r.status == FUEL_PARSE_EMPTY ||
                    r.status == FUEL_PARSE_BAD_JSON)
            << n;
    }
}

TEST(FuelParse, BracesAndQuotesInsideTextDoNotConfuseTheReader)
{
    fuel_result_t r = parse(
        "{\"ok\":true,\"stations\":[{\"brand\":\"A}B \\\"C\\\" {\",\"price\":1.5,\"isOpen\":true,"
        "\"street\":\"S\",\"place\":\"P\",\"dist\":1},{\"brand\":\"Next\",\"price\":1.4,"
        "\"isOpen\":true,\"street\":\"\",\"place\":\"\",\"dist\":2}]}");
    ASSERT_EQ(r.status, FUEL_PARSE_OK);
    ASSERT_EQ(r.count, 2);
    EXPECT_STREQ(r.stations[0].name, "Next");
    EXPECT_STREQ(r.stations[1].name, "A}B \"C\" {");
    EXPECT_STREQ(r.stations[1].place, "S, P");
    EXPECT_STREQ(r.stations[0].place, "");
}

TEST(FuelParse, TheLimitKeepsTheCheapest)
{
    fuel_result_t r = parse(fixture("list-diesel-mixed.json"), true, 2);
    ASSERT_EQ(r.count, 2);
    EXPECT_STREQ(r.stations[0].name, "NORD");
    EXPECT_STREQ(r.stations[1].name, "CHEAP");
    r = parse(fixture("list-diesel-mixed.json"), true, 100);  // never more than the cap
    EXPECT_EQ(r.count, 5);
    r = parse(fixture("list-diesel-mixed.json"), true, 0);
    EXPECT_EQ(r.count, 0);
    EXPECT_EQ(r.status, FUEL_PARSE_EMPTY);
}

TEST(FuelParse, ARefusalCarriesTheServicesMessage)
{
    fuel_result_t r = parse(fixture("error-bad-key.json"));
    EXPECT_EQ(r.status, FUEL_PARSE_API_ERROR);
    EXPECT_NE(std::string(r.message).find("apikey"), std::string::npos);
    EXPECT_EQ(r.count, 0);
}

TEST(FuelParse, AnswersThatAreNotStationListsAreRecognised)
{
    EXPECT_EQ(parse("").status, FUEL_PARSE_BAD_JSON);
    EXPECT_EQ(parse("<html>503</html>").status, FUEL_PARSE_BAD_JSON);
    EXPECT_EQ(parse("[]").status, FUEL_PARSE_BAD_JSON);
    EXPECT_EQ(parse("{}").status, FUEL_PARSE_BAD_JSON);
    EXPECT_EQ(parse("{\"ok\":true,\"stations\":{}}").status, FUEL_PARSE_BAD_JSON);
    EXPECT_EQ(parse("{\"ok\":true,\"stations\":[]}").status, FUEL_PARSE_EMPTY);
    EXPECT_EQ(parse("{\"ok\":true,\"stations\":[{\"price\":null},{\"price\":\"1.5\"},{}]}").status,
              FUEL_PARSE_EMPTY);
    fuel_result_t r;
    fuel_parse(nullptr, true, 5, &r);
    EXPECT_EQ(r.status, FUEL_PARSE_BAD_JSON);
}

TEST(FuelParse, ALongErrorMessageIsCutSafely)
{
    std::string json = "{\"ok\":false,\"message\":\"" + std::string(200, 'x') + "\"}";
    fuel_result_t r = parse(json);
    EXPECT_EQ(r.status, FUEL_PARSE_API_ERROR);
    EXPECT_EQ(strlen(r.message), (size_t) FUEL_MESSAGE_MAX - 1);
}

// ---- the price
// ------------------------------------------------------------------------------------

TEST(FuelPrice, TheThirdDecimalIsSplitOffLikeOnThePump)
{
    struct {
        float price;
        const char *main;
        const char *ninth;
    } cases[] = {{1.899f, "1.89", "9"}, {1.009f, "1.00", "9"},  {2.0f, "2.00", "0"},
                 {1.999f, "1.99", "9"}, {0.5f, "0.50", "0"},    {1.729f, "1.72", "9"},
                 {1.719f, "1.71", "9"}, {12.345f, "12.34", "5"}};
    for (const auto &c : cases) {
        char main_digits[8], ninth[2];
        fuel_format_price(c.price, main_digits, sizeof(main_digits), ninth, sizeof(ninth));
        EXPECT_STREQ(main_digits, c.main) << c.price;
        EXPECT_STREQ(ninth, c.ninth) << c.price;
    }
}

// ---- the page
// --------------------------------------------------------------------------------------

namespace
{

const int kBoardSizes[][2] = {{800, 480}, {480, 800}, {960, 540}, {1200, 1600}, {1872, 1404}};

fuel_screen_data_t page_of(const std::string &json, int count, fuel_type_t type = FUEL_DIESEL)
{
    fuel_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = FUEL_SCREEN_OK;
    data.type = type;
    data.radius_km = 5;
    fuel_parse(json.c_str(), true, count, &data.result);
    return data;
}

}  // namespace

TEST(FuelScreen, EveryNumberOfStationsFitsEveryPanel)
{
    std::string json = fixture("list-diesel-mixed.json");
    for (const auto &size : kBoardSizes) {
        for (int count = 1; count <= FUEL_MAX_STATIONS; count++) {
            for (bool german : {false, true}) {
                GuardedCanvas cv(size[0], size[1]);
                info_now_t now;
                info_now_from_date(2026, 9, 30, german, &now);
                now.hour = 14;
                now.minute = 5;
                fuel_screen_data_t data = page_of(json, count);
                ASSERT_EQ(data.result.count, count);
                fuel_screen_render(&cv.canvas, &now, &data);
                ASSERT_TRUE(cv.guards_intact()) << count << " " << size[0] << "x" << size[1];
                EXPECT_GT(cv.painted(), (size_t) 2000);
            }
        }
    }
}

TEST(FuelScreen, TheCheapestIsGreenTheHeaderYellowAndAllColoursArePalette)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    for (const auto &size : {std::pair<int, int>{800, 480}, std::pair<int, int>{480, 800}}) {
        GuardedCanvas cv(size.first, size.second);
        fuel_screen_data_t data = page_of(fixture("list-diesel-mixed.json"), 5);
        fuel_screen_render(&cv.canvas, &now, &data);
        std::set<uint32_t> colours = cv.colours();
        EXPECT_TRUE(colours.count(0x00FF00));
        EXPECT_TRUE(colours.count(0xFFFF00));
        for (uint32_t rgb : colours) {
            for (int shift : {16, 8, 0}) {
                uint32_t v = (rgb >> shift) & 0xFF;
                EXPECT_TRUE(v == 0 || v == 255) << std::hex << rgb;
            }
        }
    }
}

TEST(FuelScreen, TheAttributionIsAlwaysAtTheBottom)
{
    for (const auto &size : kBoardSizes) {
        GuardedCanvas cv(size[0], size[1]);
        info_now_t now;
        info_now_from_date(2026, 9, 30, true, &now);
        fuel_screen_data_t data = page_of(fixture("list-e5-demo.json"), 3, FUEL_E5);
        fuel_screen_render(&cv.canvas, &now, &data);
        int u = canvas_unit(&cv.canvas);
        int line = canvas_text_height(canvas_text_scale(&cv.canvas, 1));
        int ink = 0;
        for (int y = size[1] - u - line; y < size[1] - u; y++) {
            for (int x = 0; x < size[0]; x++) {
                ink += cv.blank(x, y) ? 0 : 1;
            }
        }
        EXPECT_GT(ink, 100) << size[0] << "x" << size[1];
    }
}

TEST(FuelScreen, EveryReasonHasItsOwnMessageInBothLanguages)
{
    std::set<std::string> seen;
    for (bool german : {false, true}) {
        for (fuel_screen_status_t status :
             {FUEL_SCREEN_NO_KEY, FUEL_SCREEN_NO_LOCATION, FUEL_SCREEN_NO_NETWORK,
              FUEL_SCREEN_FETCH_FAILED, FUEL_SCREEN_KEY_REFUSED, FUEL_SCREEN_NONE_FOUND}) {
            GuardedCanvas cv(800, 480);
            info_now_t now;
            info_now_from_date(2026, 9, 30, german, &now);
            fuel_screen_data_t data;
            memset(&data, 0, sizeof(data));
            data.status = status;
            strcpy(data.result.message, "apikey falsch");
            fuel_screen_render(&cv.canvas, &now, &data);
            EXPECT_TRUE(cv.guards_intact());
            EXPECT_GT(cv.painted(), (size_t) 300);
            EXPECT_TRUE(
                seen.insert(std::string((const char *) cv.canvas.rgb, 800 * 480 * 3)).second)
                << german << " " << status;
        }
    }
}

TEST(FuelScreen, TheServicesOwnWordsAreShownWhenItRefusesTheKey)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    GuardedCanvas a(800, 480), b(800, 480);
    fuel_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = FUEL_SCREEN_KEY_REFUSED;
    strcpy(data.result.message, "first text");
    fuel_screen_render(&a.canvas, &now, &data);
    strcpy(data.result.message, "another text");
    fuel_screen_render(&b.canvas, &now, &data);
    EXPECT_NE(memcmp(a.canvas.rgb, b.canvas.rgb, (size_t) 800 * 480 * 3), 0);
}

TEST(FuelScreen, AnOkPageWithoutStationsIsTheNoneFoundMessage)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    GuardedCanvas a(800, 480), b(800, 480);
    fuel_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = FUEL_SCREEN_OK;
    fuel_screen_render(&a.canvas, &now, &data);
    data.status = FUEL_SCREEN_NONE_FOUND;
    fuel_screen_render(&b.canvas, &now, &data);
    EXPECT_EQ(memcmp(a.canvas.rgb, b.canvas.rgb, (size_t) 800 * 480 * 3), 0);
}

TEST(FuelScreen, LongTextsStayInsideTheMargins)
{
    std::string long_json =
        "{\"ok\":true,\"stations\":[{\"brand\":\"A brand with a really very long name indeed\","
        "\"street\":\"An extremely long street name of a place\",\"place\":\"Another very long "
        "town\","
        "\"dist\":24.9,\"price\":1.999,\"isOpen\":true},{\"brand\":\"X\",\"price\":9.999,"
        "\"isOpen\":true,\"street\":\"\",\"place\":\"\",\"dist\":0}]}";
    for (const auto &size : kBoardSizes) {
        GuardedCanvas cv(size[0], size[1]);
        info_now_t now;
        info_now_from_date(2026, 9, 30, false, &now);
        fuel_screen_data_t data = page_of(long_json, 5);
        data.radius_km = 25;
        fuel_screen_render(&cv.canvas, &now, &data);
        ASSERT_TRUE(cv.guards_intact());
        int u = canvas_unit(&cv.canvas);
        int band = canvas_text_height(canvas_text_scale(&cv.canvas, 1)) + 2 * u;
        for (int y = band; y < size[1]; y++) {
            for (int x = size[0] - 2 * u; x < size[0]; x++) {
                ASSERT_TRUE(cv.blank(x, y)) << size[0] << "x" << size[1] << " at " << x << "," << y;
            }
        }
    }
}
