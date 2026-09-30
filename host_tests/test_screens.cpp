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
