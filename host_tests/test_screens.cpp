#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "render_screens_extra.h"

// The info screens: the logic of each and a render of all of them at the sizes of the boards. A
// canvas is drawn on with guard bytes around its buffer, so a stroke outside it is caught.

namespace
{

constexpr size_t kGuard = 4096;

struct GuardedCanvas {
    std::vector<uint8_t> memory;
    canvas_t canvas;
    GuardedCanvas(int w, int h) : memory((size_t) w * h * 3 + 2 * kGuard, 0xA5)
    {
        // white paper in the canvas itself, 0xA5 outside
        std::fill(memory.begin() + kGuard, memory.end() - kGuard, 255);
        canvas.rgb = memory.data() + kGuard;
        canvas.width = w;
        canvas.height = h;
    }
    bool guards_intact() const
    {
        for (size_t i = 0; i < kGuard; i++) {
            if (memory[i] != 0xA5 || memory[memory.size() - 1 - i] != 0xA5) {
                return false;
            }
        }
        return true;
    }
    size_t painted() const
    {
        size_t n = 0;
        for (size_t i = 0; i < (size_t) canvas.width * canvas.height; i++) {
            const uint8_t *p = canvas.rgb + i * 3;
            n += (p[0] != 255 || p[1] != 255 || p[2] != 255) ? 1 : 0;
        }
        return n;
    }
    std::set<uint32_t> colours() const
    {
        std::set<uint32_t> seen;
        for (size_t i = 0; i < (size_t) canvas.width * canvas.height; i++) {
            const uint8_t *p = canvas.rgb + i * 3;
            seen.insert((uint32_t) p[0] << 16 | (uint32_t) p[1] << 8 | p[2]);
        }
        return seen;
    }
};

const int kBoardSizes[][2] = {{800, 480}, {480, 800}, {960, 540}, {1200, 1600}, {1872, 1404}};

}  // namespace

// ---- chore wheel ------------------------------------------------------------------------------

TEST(ChoreWheel, ConfigIsParsedFromTheLists)
{
    chore_config_t c;
    chore_config_parse("Anna, Ben; Clara", "Bins\nDishes", &c);
    ASSERT_EQ(c.member_count, 3);
    EXPECT_STREQ(c.members[0], "Anna");
    EXPECT_STREQ(c.members[2], "Clara");
    ASSERT_EQ(c.task_count, 2);
    EXPECT_STREQ(c.tasks[1], "Dishes");
    chore_config_parse("a,b,c,d,e,f,g", "1,2,3,4,5,6,7,8", &c);
    EXPECT_EQ(c.member_count, CHORE_MAX_MEMBERS);
    EXPECT_EQ(c.task_count, CHORE_MAX_TASKS);
    chore_config_parse(nullptr, "", &c);
    EXPECT_EQ(c.member_count, 0);
    EXPECT_EQ(c.task_count, 0);
}

TEST(ChoreWheel, ChoresGoRoundTheMembersByWeek)
{
    // three members, four chores: chore t of week w belongs to (t + w) mod 3
    for (int week = 1; week <= 53; week++) {
        for (int task = 0; task < 4; task++) {
            EXPECT_EQ(chore_assignee(task, week, 3), (task + week) % 3);
        }
    }
    // next week everybody has the chore the member before them had this week
    for (int task = 0; task < 4; task++) {
        EXPECT_EQ(chore_assignee(task, 11, 3), (chore_assignee(task, 10, 3) + 1) % 3);
    }
    // in one week every member has as many chores as the others, give or take one
    int per_member[5] = {0};
    for (int task = 0; task < 6; task++) {
        per_member[chore_assignee(task, 40, 4)]++;
    }
    EXPECT_LE(*std::max_element(per_member, per_member + 4) -
                  *std::min_element(per_member, per_member + 4),
              1);
}

TEST(ChoreWheel, AssigneeEdgeCases)
{
    EXPECT_EQ(chore_assignee(0, 5, 0), -1);
    EXPECT_EQ(chore_assignee(0, 5, -2), -1);
    EXPECT_EQ(chore_assignee(2, 40, 1), 0);
    EXPECT_EQ(chore_assignee(-1, 0, 3), 2);  // never negative
}

TEST(ChoreWheel, MemberColoursAreThePalettePrimaries)
{
    EXPECT_EQ(chore_member_color(0).r, 255);
    EXPECT_EQ(chore_member_color(0).g, 0);
    std::set<uint32_t> distinct;
    for (int m = 0; m < CHORE_MAX_MEMBERS; m++) {
        canvas_color_t c = chore_member_color(m);
        distinct.insert((uint32_t) c.r << 16 | (uint32_t) c.g << 8 | c.b);
        for (uint8_t v : {c.r, c.g, c.b}) {
            EXPECT_TRUE(v == 0 || v == 255) << m;  // exactly what a 6-colour panel can show
        }
    }
    EXPECT_EQ(distinct.size(), (size_t) CHORE_MAX_MEMBERS);
    EXPECT_EQ(chore_member_color(5).r, chore_member_color(0).r);  // wraps
    EXPECT_EQ(chore_member_color(-1).r, chore_member_color(4).r);
}

TEST(ChoreWheel, EveryMemberColourAppearsOnTheScreen)
{
    GuardedCanvas cv(800, 480);
    draw_chore_wheel_english(&cv.canvas);
    std::set<uint32_t> colours = cv.colours();
    for (int m = 0; m < 4; m++) {
        canvas_color_t c = chore_member_color(m);
        EXPECT_TRUE(colours.count((uint32_t) c.r << 16 | (uint32_t) c.g << 8 | c.b)) << m;
    }
    EXPECT_TRUE(colours.count(0xFFFFFF));
    EXPECT_TRUE(colours.count(0x000000));
}

TEST(ChoreWheel, AllColoursAreExactPaletteColours)
{
    // no anti-aliasing, no greys: a colour panel and a grey panel quantize exact values
    for (const RenderCase &c : render_cases()) {
        GuardedCanvas cv(800, 480);
        c.draw(&cv.canvas);
        for (uint32_t rgb : cv.colours()) {
            for (int shift : {16, 8, 0}) {
                uint32_t v = (rgb >> shift) & 0xFF;
                EXPECT_TRUE(v == 0 || v == 255) << c.name << " " << std::hex << rgb;
            }
        }
    }
}

TEST(ChoreWheel, WeekMovesTheWheelAndTheCards)
{
    GuardedCanvas a(800, 480), b(800, 480);
    chore_config_t config;
    chore_config_parse("Anna, Ben, Clara", "Bins, Dishes, Plants", &config);
    info_now_t week40, week41;
    info_now_from_date(2026, 9, 30, false, &week40);
    info_now_from_date(2026, 10, 7, false, &week41);
    ASSERT_EQ(week41.iso_week, week40.iso_week + 1);
    chore_wheel_render(&a.canvas, &week40, &config);
    chore_wheel_render(&b.canvas, &week41, &config);
    EXPECT_NE(memcmp(a.canvas.rgb, b.canvas.rgb, (size_t) 800 * 480 * 3), 0);
    // the same week twice is the same picture
    GuardedCanvas again(800, 480);
    chore_wheel_render(&again.canvas, &week40, &config);
    EXPECT_EQ(memcmp(a.canvas.rgb, again.canvas.rgb, (size_t) 800 * 480 * 3), 0);
}

TEST(ChoreWheel, LanguageChangesTheText)
{
    GuardedCanvas en(800, 480), de(800, 480);
    chore_config_t config;
    chore_config_parse("Anna, Ben", "Bins, Dishes", &config);
    info_now_t english, german;
    info_now_from_date(2026, 9, 30, false, &english);
    info_now_from_date(2026, 9, 30, true, &german);
    chore_wheel_render(&en.canvas, &english, &config);
    chore_wheel_render(&de.canvas, &german, &config);
    EXPECT_NE(memcmp(en.canvas.rgb, de.canvas.rgb, (size_t) 800 * 480 * 3), 0);
}

TEST(ChoreWheel, NothingIsDrawnOutsideTheCanvas)
{
    for (const auto &size : kBoardSizes) {
        for (const RenderCase &c : render_cases()) {
            GuardedCanvas cv(size[0], size[1]);
            c.draw(&cv.canvas);
            EXPECT_TRUE(cv.guards_intact()) << c.name << " " << size[0] << "x" << size[1];
            EXPECT_GT(cv.painted(), (size_t) 500) << c.name << " " << size[0] << "x" << size[1];
        }
    }
}

TEST(ChoreWheel, EveryHouseholdSizeFitsEveryPanel)
{
    // 1..5 members and 1..6 chores at the smallest and the most awkward panels
    const int sizes[][2] = {{800, 480}, {480, 800}, {1200, 1600}};
    const char *members[] = {"A", "A,B", "A,B,C", "A,B,C,D", "A,B,C,D,E"};
    const char *tasks[] = {"One",
                           "One,Two",
                           "One,Two,Three",
                           "One,Two,Three,Four",
                           "One,Two,Three,Four,Five",
                           "One,Two,Three,Four,Five,Six"};
    for (const auto &size : sizes) {
        for (const char *m : members) {
            for (const char *t : tasks) {
                GuardedCanvas cv(size[0], size[1]);
                chore_config_t config;
                chore_config_parse(m, t, &config);
                info_now_t now;
                info_now_from_date(2026, 9, 30, false, &now);
                chore_wheel_render(&cv.canvas, &now, &config);
                EXPECT_TRUE(cv.guards_intact())
                    << m << " | " << t << " " << size[0] << "x" << size[1];
            }
        }
    }
}

TEST(ChoreWheel, LongNamesAreCutNotOverflowed)
{
    GuardedCanvas cv(800, 480);
    chore_config_t config;
    chore_config_parse("Bartholomew-Maximilian-Alexander-III, Eve",
                       "Clean the entire house from top to bottom thoroughly, Dishes", &config);
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    chore_wheel_render(&cv.canvas, &now, &config);
    EXPECT_TRUE(cv.guards_intact());
    // the right margin of the card column stays white (the text does not run out of the card)
    bool margin_clean = true;
    for (int y = 0; y < 480; y++) {
        for (int x = 800 - 6; x < 800; x++) {
            const uint8_t *p = cv.canvas.rgb + ((size_t) y * 800 + x) * 3;
            margin_clean = margin_clean && p[0] == 255 && p[1] == 255 && p[2] == 255;
        }
    }
    EXPECT_TRUE(margin_clean);
}

TEST(ChoreWheel, WithoutSetupItSaysSoInBothLanguages)
{
    GuardedCanvas en(800, 480), de(800, 480);
    chore_config_t none;
    chore_config_parse("", "", &none);
    info_now_t english, german;
    info_now_from_date(2026, 9, 30, false, &english);
    info_now_from_date(2026, 9, 30, true, &german);
    chore_wheel_render(&en.canvas, &english, &none);
    chore_wheel_render(&de.canvas, &german, &none);
    EXPECT_GT(en.painted(), (size_t) 100);
    EXPECT_GT(de.painted(), (size_t) 100);
    EXPECT_NE(memcmp(en.canvas.rgb, de.canvas.rgb, (size_t) 800 * 480 * 3), 0);
    // members but no chores (and the other way round) is "not set up" as well
    chore_config_t half;
    chore_config_parse("Anna", "", &half);
    GuardedCanvas h(800, 480);
    chore_wheel_render(&h.canvas, &english, &half);
    EXPECT_EQ(memcmp(h.canvas.rgb, en.canvas.rgb, (size_t) 800 * 480 * 3), 0);
}

// ---- weather ----------------------------------------------------------------------------------

namespace
{

// The WMO codes Open-Meteo can answer with (and the frame's other providers are mapped into).
const int kWmoCodes[] = {0,  1,  2,  3,  45, 48, 51, 53, 55, 56, 57, 61, 63, 65,
                         66, 67, 71, 73, 75, 77, 80, 81, 82, 85, 86, 95, 96, 99};

}  // namespace

TEST(WeatherScreen, EveryWmoCodeHasAKindAndWordsInBothLanguages)
{
    for (int code : kWmoCodes) {
        EXPECT_NE(weather_screen_kind(code), WEATHER_KIND_UNKNOWN) << code;
        std::string en = weather_screen_condition(code, false);
        std::string de = weather_screen_condition(code, true);
        EXPECT_NE(en, "Unknown") << code;
        EXPECT_NE(de, "Unbekannt") << code;
        EXPECT_NE(en, de) << code;
        EXPECT_LE(en.size(), 22u) << code;
        EXPECT_LE(de.size(), 24u) << code;  // umlauts count two bytes here
    }
    for (int code : {-1, 4, 44, 100, 123, 1000}) {
        EXPECT_EQ(weather_screen_kind(code), WEATHER_KIND_UNKNOWN) << code;
        EXPECT_STREQ(weather_screen_condition(code, false), "Unknown");
        EXPECT_STREQ(weather_screen_condition(code, true), "Unbekannt");
    }
}

TEST(WeatherScreen, KindsFollowTheWeather)
{
    EXPECT_EQ(weather_screen_kind(0), WEATHER_KIND_CLEAR);
    EXPECT_EQ(weather_screen_kind(2), WEATHER_KIND_PARTLY_CLOUDY);
    EXPECT_EQ(weather_screen_kind(3), WEATHER_KIND_OVERCAST);
    EXPECT_EQ(weather_screen_kind(45), WEATHER_KIND_FOG);
    EXPECT_EQ(weather_screen_kind(53), WEATHER_KIND_DRIZZLE);
    EXPECT_EQ(weather_screen_kind(65), WEATHER_KIND_RAIN);
    EXPECT_EQ(weather_screen_kind(82), WEATHER_KIND_RAIN);  // showers look like rain
    EXPECT_EQ(weather_screen_kind(67), WEATHER_KIND_FREEZING);
    EXPECT_EQ(weather_screen_kind(75), WEATHER_KIND_SNOW);
    EXPECT_EQ(weather_screen_kind(86), WEATHER_KIND_SNOW);
    EXPECT_EQ(weather_screen_kind(99), WEATHER_KIND_THUNDER);
}

TEST(WeatherScreen, GermanWordsHaveRealUmlauts)
{
    EXPECT_STREQ(weather_screen_condition(2, true), "Teils bew\xC3\xB6lkt");
    EXPECT_STREQ(weather_screen_condition(1, true),
                 "\xC3\x9C"
                 "berwiegend klar");
}

TEST(WeatherScreen, EveryIconStaysInItsBoxAndIsDrawn)
{
    for (int size : {24, 56, 100, 300}) {
        std::set<std::string> pictures;
        for (int k = WEATHER_KIND_CLEAR; k <= WEATHER_KIND_UNKNOWN; k++) {
            int side = size + 80;
            GuardedCanvas cv(side, side);
            weather_screen_draw_icon(&cv.canvas, side / 2, side / 2, size, (weather_kind_t) k);
            EXPECT_TRUE(cv.guards_intact()) << k << " at " << size;
            EXPECT_GT(cv.painted(), (size_t) size) << "kind " << k << " at " << size;
            // the outline may lie a pixel or two outside the nominal box
            int low = side / 2 - size / 2 - 2, high = side / 2 + size / 2 + 2;
            int stray = 0;
            for (int y = 0; y < side; y++) {
                for (int x = 0; x < side; x++) {
                    const uint8_t *p = cv.canvas.rgb + ((size_t) y * side + x) * 3;
                    bool ink = !(p[0] == 255 && p[1] == 255 && p[2] == 255);
                    if (ink && (x < low || x > high || y < low || y > high)) {
                        stray++;
                    }
                }
            }
            EXPECT_EQ(stray, 0) << "kind " << k << " at " << size;
            pictures.insert(std::string((const char *) cv.canvas.rgb, (size_t) side * side * 3));
        }
        EXPECT_EQ(pictures.size(), (size_t) WEATHER_KIND_UNKNOWN + 1) << "size " << size;
    }
}

TEST(WeatherScreen, IconsUseTheColoursOfTheWeather)
{
    auto colours_of = [](weather_kind_t kind) {
        GuardedCanvas cv(160, 160);
        weather_screen_draw_icon(&cv.canvas, 80, 80, 120, kind);
        return cv.colours();
    };
    EXPECT_TRUE(colours_of(WEATHER_KIND_CLEAR).count(0xFFFF00));  // a yellow sun
    EXPECT_TRUE(colours_of(WEATHER_KIND_PARTLY_CLOUDY).count(0xFFFF00));
    EXPECT_TRUE(colours_of(WEATHER_KIND_RAIN).count(0x0000FF));  // blue rain
    EXPECT_TRUE(colours_of(WEATHER_KIND_DRIZZLE).count(0x0000FF));
    EXPECT_TRUE(colours_of(WEATHER_KIND_THUNDER).count(0xFFFF00));  // a yellow bolt
    EXPECT_FALSE(colours_of(WEATHER_KIND_SNOW).count(0x0000FF));
    EXPECT_FALSE(colours_of(WEATHER_KIND_FOG).count(0xFFFF00));
}

TEST(WeatherScreen, TinyIconsAreLeftOut)
{
    GuardedCanvas cv(40, 40);
    weather_screen_draw_icon(&cv.canvas, 20, 20, 7, WEATHER_KIND_CLEAR);
    EXPECT_EQ(cv.painted(), (size_t) 0);
}

TEST(WeatherScreen, EachReasonHasItsOwnMessageInBothLanguages)
{
    std::set<std::string> seen;
    for (bool german : {false, true}) {
        for (weather_screen_status_t status :
             {WEATHER_SCREEN_NO_LOCATION, WEATHER_SCREEN_NO_NETWORK, WEATHER_SCREEN_FETCH_FAILED}) {
            GuardedCanvas cv(800, 480);
            info_now_t now;
            info_now_from_date(2026, 9, 30, german, &now);
            weather_screen_data_t data;
            memset(&data, 0, sizeof(data));
            data.status = status;
            weather_screen_render(&cv.canvas, &now, &data);
            EXPECT_GT(cv.painted(), (size_t) 300);
            EXPECT_TRUE(cv.guards_intact());
            EXPECT_TRUE(
                seen.insert(std::string((const char *) cv.canvas.rgb, 800 * 480 * 3)).second)
                << german << " " << status;
        }
    }
}

TEST(WeatherScreen, AnOkForecastWithoutDaysIsTreatedAsAFailedFetch)
{
    GuardedCanvas a(800, 480), b(800, 480);
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    weather_screen_data_t none;
    memset(&none, 0, sizeof(none));
    none.status = WEATHER_SCREEN_OK;
    weather_screen_render(&a.canvas, &now, &none);
    none.status = WEATHER_SCREEN_FETCH_FAILED;
    weather_screen_render(&b.canvas, &now, &none);
    EXPECT_EQ(memcmp(a.canvas.rgb, b.canvas.rgb, (size_t) 800 * 480 * 3), 0);
}

TEST(WeatherScreen, MoreDaysMeansMoreRows)
{
    static const int highs[] = {20, 21, 22, 23, 24}, lows[] = {10, 11, 12, 13, 14};
    static const int codes[] = {0, 1, 2, 3, 61};
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    size_t painted_before = 0;
    for (int count = 1; count <= 5; count++) {
        GuardedCanvas cv(800, 480);
        weather_screen_data_t data = weather_sample("Berlin", count, highs, lows, codes);
        weather_screen_render(&cv.canvas, &now, &data);
        EXPECT_TRUE(cv.guards_intact()) << count;
        EXPECT_GT(cv.painted(), painted_before) << count;
        painted_before = cv.painted();
    }
}

TEST(WeatherScreen, TheDateAndLanguageMoveThePicture)
{
    GuardedCanvas today(800, 480), other_day(800, 480), german(800, 480);
    draw_weather_english(&today.canvas);
    info_now_t now;
    info_now_from_date(2026, 10, 3, false, &now);  // the forecast starts three days before this
    static const int highs[] = {24, 22, 19, 17, 15}, lows[] = {12, 11, 9, 6, 2};
    static const int codes[] = {2, 61, 3, 95, 71};
    weather_screen_data_t data = weather_sample("Berlin", 5, highs, lows, codes);
    weather_screen_render(&other_day.canvas, &now, &data);
    draw_weather_german(&german.canvas);
    EXPECT_NE(memcmp(today.canvas.rgb, other_day.canvas.rgb, (size_t) 800 * 480 * 3), 0);
    EXPECT_NE(memcmp(today.canvas.rgb, german.canvas.rgb, (size_t) 800 * 480 * 3), 0);
}

TEST(WeatherScreen, ExtremeValuesAndLongPlacesFitEveryPanel)
{
    static const int highs[] = {-100, 100, 0, -9, 99}, lows[] = {-100, -99, -1, 100, -10};
    static const int codes[] = {45, 96, 1234, -5, 77};
    for (const auto &size : kBoardSizes) {
        for (bool german : {false, true}) {
            GuardedCanvas cv(size[0], size[1]);
            info_now_t now;
            info_now_from_date(2026, 9, 30, german, &now);
            weather_screen_data_t data =
                weather_sample("Llanfairpwllgwyngyllgogerychwyrndrobwllllantysiliogogogoch, Wales",
                               5, highs, lows, codes);
            weather_screen_render(&cv.canvas, &now, &data);
            EXPECT_TRUE(cv.guards_intact()) << size[0] << "x" << size[1];
            EXPECT_GT(cv.painted(), (size_t) 1000);
        }
    }
}

// ---- fact of the day --------------------------------------------------------------------------

namespace
{

// The code points of a UTF-8 string.
std::vector<unsigned> code_points(const std::string &utf8)
{
    std::vector<unsigned> out;
    for (size_t i = 0; i < utf8.size();) {
        unsigned char c = utf8[i];
        int extra = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : 3;
        unsigned cp = extra == 0 ? c : c & (0x3F >> extra);
        for (int k = 1; k <= extra && i + k < utf8.size(); k++) {
            cp = cp << 6 | (utf8[i + k] & 0x3F);
        }
        out.push_back(cp);
        i += 1 + extra;
    }
    return out;
}

}  // namespace

TEST(FactScreen, EveryBuiltInFactDrawsOnEveryPanelInBothLanguages)
{
    for (const auto &size : kBoardSizes) {
        for (int i = 0; i < fact_builtin_count(); i++) {
            for (bool german : {false, true}) {
                GuardedCanvas cv(size[0], size[1]);
                info_now_t now;
                info_now_from_date(2026, 9, 30, german, &now);
                fact_screen_render(&cv.canvas, &now, fact_builtin(i, german));
                ASSERT_TRUE(cv.guards_intact()) << i << " " << german << " " << size[0];
                EXPECT_GT(cv.painted(), (size_t) 2000) << i << " " << german;
                if (size[0] * size[1] > 960 * 540 && i % 8 != 0) {
                    continue;  // the palette check is slow on the big panels: a few facts there
                }
                for (uint32_t rgb : cv.colours()) {
                    for (int shift : {16, 8, 0}) {
                        uint32_t v = (rgb >> shift) & 0xFF;
                        ASSERT_TRUE(v == 0 || v == 255) << std::hex << rgb;
                    }
                }
            }
        }
    }
}

TEST(FactScreen, BuiltInFactsOnlyUseCharactersTheFrameCanDraw)
{
    // ä ö ü Ä Ö Ü ß ° € are the non-ASCII characters of the display font (glyphs option)
    const std::set<unsigned> drawable = {0xE4, 0xF6, 0xFC, 0xC4, 0xD6, 0xDC, 0xDF, 0xB0, 0x20AC};
    for (int i = 0; i < fact_builtin_count(); i++) {
        for (bool german : {false, true}) {
            const fact_t *f = fact_builtin(i, german);
            for (const char *field : {f->title, f->text, f->question}) {
                for (unsigned cp : code_points(field)) {
                    EXPECT_TRUE(cp < 0x80 || drawable.count(cp))
                        << "fact " << i << (german ? " de" : " en") << ": U+" << std::hex << cp;
                    EXPECT_FALSE(cp < 0x20) << "control character in fact " << i;
                }
                // nothing is lost when the text is made display text
                char shown[FACT_TEXT_MAX * 2];
                canvas_text_from_utf8(field, shown, sizeof(shown));
                EXPECT_EQ(strlen(shown), code_points(field).size())
                    << "fact " << i << ": " << field;
            }
        }
    }
}

TEST(FactScreen, TopicAndQuestionAreDrawnOnlyWhenTheFactHasThem)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    fact_t fact;
    memset(&fact, 0, sizeof(fact));
    strcpy(fact.text, "Just the fact.");
    GuardedCanvas plain(800, 480);
    fact_screen_render(&plain.canvas, &now, &fact);
    EXPECT_FALSE(plain.colours().count(0x0000FF));  // no blue topic pill
    EXPECT_FALSE(plain.colours().count(0xFFFF00));  // no yellow question box
    EXPECT_TRUE(plain.colours().count(0xFF0000));   // the red header is always there

    strcpy(fact.title, "Topic");
    GuardedCanvas with_title(800, 480);
    fact_screen_render(&with_title.canvas, &now, &fact);
    EXPECT_TRUE(with_title.colours().count(0x0000FF));
    EXPECT_FALSE(with_title.colours().count(0xFFFF00));

    strcpy(fact.question, "And a question?");
    GuardedCanvas with_question(800, 480);
    fact_screen_render(&with_question.canvas, &now, &fact);
    EXPECT_TRUE(with_question.colours().count(0xFFFF00));
}

TEST(FactScreen, TheHeaderFollowsLanguageAndDate)
{
    GuardedCanvas en(800, 480), de(800, 480), later(800, 480);
    fact_t fact;
    memset(&fact, 0, sizeof(fact));
    strcpy(fact.text, "Same fact.");
    info_now_t english, german, next_day;
    info_now_from_date(2026, 9, 30, false, &english);
    info_now_from_date(2026, 9, 30, true, &german);
    info_now_from_date(2026, 10, 1, false, &next_day);
    fact_screen_render(&en.canvas, &english, &fact);
    fact_screen_render(&de.canvas, &german, &fact);
    fact_screen_render(&later.canvas, &next_day, &fact);
    EXPECT_NE(memcmp(en.canvas.rgb, de.canvas.rgb, (size_t) 800 * 480 * 3), 0);
    EXPECT_NE(memcmp(en.canvas.rgb, later.canvas.rgb, (size_t) 800 * 480 * 3), 0);
}

TEST(FactScreen, ShortFactsAreDrawnBigLongOnesSmaller)
{
    auto ink_height = [](const std::string &text) {
        GuardedCanvas cv(800, 480);
        info_now_t now;
        info_now_from_date(2026, 9, 30, false, &now);
        fact_t fact;
        memset(&fact, 0, sizeof(fact));
        strncpy(fact.text, text.c_str(), sizeof(fact.text) - 1);
        fact_screen_render(&cv.canvas, &now, &fact);
        // the tallest run of black pixels in one column of the body (below the 48 px header)
        int tallest = 0;
        for (int x = 0; x < 800; x++) {
            int run = 0;
            for (int y = 60; y < 470; y++) {
                const uint8_t *p = cv.canvas.rgb + ((size_t) y * 800 + x) * 3;
                run = (p[0] == 0 && p[1] == 0 && p[2] == 0) ? run + 1 : 0;
                tallest = std::max(tallest, run);
            }
        }
        return tallest;
    };
    int short_height = ink_height("Tiny fact.");
    int long_height = ink_height(std::string(200, 'x') + " and more words to make it long");
    EXPECT_GT(short_height, long_height);
    EXPECT_GE(short_height, 40);  // at least twice the body text (a letter stem is 24 px per step)
}

TEST(FactScreen, TheLongestPossibleFactStaysInsideItsMargins)
{
    for (const auto &size : kBoardSizes) {
        GuardedCanvas cv(size[0], size[1]);
        draw_fact_long(&cv.canvas);
        EXPECT_TRUE(cv.guards_intact());
        int u = canvas_unit(&cv.canvas);
        int body_top = canvas_text_height(canvas_text_scale(&cv.canvas, 1)) + 2 * u;
        for (int y = body_top; y < size[1]; y++) {
            for (int x = size[0] - 2 * u; x < size[0]; x++) {
                const uint8_t *p = cv.canvas.rgb + ((size_t) y * size[0] + x) * 3;
                ASSERT_TRUE(p[0] == 255 && p[1] == 255 && p[2] == 255)
                    << size[0] << "x" << size[1] << " at " << x << "," << y;
            }
        }
        // and the bottom margin
        for (int y = size[1] - 2 * u; y < size[1]; y++) {
            for (int x = 0; x < size[0]; x++) {
                const uint8_t *p = cv.canvas.rgb + ((size_t) y * size[0] + x) * 3;
                ASSERT_TRUE(p[0] == 255 && p[1] == 255 && p[2] == 255)
                    << size[0] << "x" << size[1] << " at " << x << "," << y;
            }
        }
    }
}
