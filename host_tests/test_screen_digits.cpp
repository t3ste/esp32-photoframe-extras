#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

extern "C" {
#include "screen_canvas.h"
#include "screen_digits.h"
}

namespace
{

struct Canvas {
    std::vector<uint8_t> pixels;
    canvas_t c;
    Canvas(int w, int h) : pixels((size_t) w * h * 3, 255)
    {
        c.rgb = pixels.data();
        c.width = w;
        c.height = h;
    }
    bool ink(int x, int y) const
    {
        const uint8_t *p = &pixels[((size_t) y * c.width + x) * 3];
        return !(p[0] == 255 && p[1] == 255 && p[2] == 255);
    }
    int count() const
    {
        int n = 0;
        for (int y = 0; y < c.height; y++) {
            for (int x = 0; x < c.width; x++) {
                n += ink(x, y) ? 1 : 0;
            }
        }
        return n;
    }
    std::string rows(int x0, int x1) const
    {
        std::string s;
        for (int y = 0; y < c.height; y++) {
            for (int x = x0; x < x1; x++) {
                s += ink(x, y) ? '#' : '.';
            }
            s += '\n';
        }
        return s;
    }
};

// The picture of one glyph drawn alone at a height.
std::string picture(char glyph, int height)
{
    char text[2] = {glyph, 0};
    int width = canvas_big_width(text, height);
    Canvas canvas(width + 8, height + 8);
    canvas_big_text(&canvas.c, 4, 4, text, height, CANVAS_BLACK);
    return canvas.rows(0, width + 8);
}

}  // namespace

TEST(ScreenDigits, EveryGlyphDrawsInkInsideItsOwnBox)
{
    const char *glyphs = "0123456789-+.:*";
    for (int height : {24, 60, 144, 400}) {
        for (const char *g = glyphs; *g; g++) {
            char text[2] = {*g, 0};
            int width = canvas_big_width(text, height);
            ASSERT_GT(width, 0) << *g;
            Canvas canvas(width + 40, height + 40);
            canvas_big_text(&canvas.c, 20, 20, text, height, CANVAS_BLACK);
            int inside = 0, outside = 0;
            for (int y = 0; y < canvas.c.height; y++) {
                for (int x = 0; x < canvas.c.width; x++) {
                    if (!canvas.ink(x, y)) {
                        continue;
                    }
                    bool in_box = x >= 20 && x < 20 + width && y >= 20 && y < 20 + height;
                    (in_box ? inside : outside)++;
                }
            }
            EXPECT_GT(inside, 0) << "glyph " << *g << " at " << height;
            EXPECT_EQ(outside, 0) << "glyph " << *g << " at " << height << " strays out of its box";
        }
    }
}

TEST(ScreenDigits, TheDigitsAreAllDifferent)
{
    std::set<std::string> seen;
    for (char d = '0'; d <= '9'; d++) {
        EXPECT_TRUE(seen.insert(picture(d, 90)).second) << "digit " << d << " repeats another";
    }
    EXPECT_EQ(seen.size(), 10u);
}

TEST(ScreenDigits, DigitsAreNeitherEmptyNorBlobs)
{
    for (char d = '0'; d <= '9'; d++) {
        char text[2] = {d, 0};
        int height = 120;
        int width = canvas_big_width(text, height);
        Canvas canvas(width, height);
        canvas_big_text(&canvas.c, 0, 0, text, height, CANVAS_BLACK);
        double share = (double) canvas.count() / (width * height);
        EXPECT_GT(share, 0.12) << d;
        EXPECT_LT(share, 0.55) << d;
    }
}

TEST(ScreenDigits, WidthIsTabularAndSkipsWhatItCannotDraw)
{
    int height = 100;
    int one = canvas_big_width("1", height);
    for (char d = '0'; d <= '9'; d++) {
        char text[2] = {d, 0};
        EXPECT_EQ(canvas_big_width(text, height), one) << d;
    }
    EXPECT_GT(canvas_big_width("12", height), 2 * one);
    EXPECT_EQ(canvas_big_width("1a2", height), canvas_big_width("12", height));
    EXPECT_EQ(canvas_big_width("", height), 0);
    EXPECT_EQ(canvas_big_width("abc", height), 0);
    EXPECT_EQ(canvas_big_width(nullptr, height), 0);
    EXPECT_GT(canvas_big_width("-12*", height), canvas_big_width("12", height));
}

TEST(ScreenDigits, TextReturnsTheEndAndCentresOnItsMiddle)
{
    Canvas canvas(400, 200);
    int end = canvas_big_text(&canvas.c, 10, 10, "2024", 100, CANVAS_BLACK);
    EXPECT_EQ(end, 10 + canvas_big_width("2024", 100));

    Canvas centred(400, 200);
    canvas_big_text_centered(&centred.c, 200, 10, "88", 100, CANVAS_BLACK);
    int left = 400, right = -1;
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < 400; x++) {
            if (centred.ink(x, y)) {
                left = x < left ? x : left;
                right = x > right ? x : right;
            }
        }
    }
    EXPECT_NEAR((left + right) / 2, 200, 2);
}

TEST(ScreenDigits, FitHeightShrinksUntilTheTextFits)
{
    int max_width = 300;
    int height = canvas_big_fit_height("-12*", max_width, 500);
    EXPECT_LE(canvas_big_width("-12*", height), max_width);
    EXPECT_GT(canvas_big_width("-12*", height + 1), max_width);
    EXPECT_EQ(canvas_big_fit_height("1", 1000, 40), 40);
    EXPECT_EQ(canvas_big_fit_height("123456789", 1, 40), 1);
}

TEST(ScreenDigits, ClipsAtTheEdgesAndNeverWritesOutsideTheBuffer)
{
    const int w = 60, h = 50, guard = 64;
    std::vector<uint8_t> buffer((size_t) w * h * 3 + 2 * guard, 0xAB);
    canvas_t canvas = {buffer.data() + guard, w, h};
    memset(canvas.rgb, 255, (size_t) w * h * 3);
    for (int x : {-70, -20, 0, 30, 55, 90}) {
        for (int y : {-100, -30, 0, 20, 45, 80}) {
            canvas_big_text(&canvas, x, y, "-88*", 90, CANVAS_BLACK);
        }
    }
    for (int i = 0; i < guard; i++) {
        ASSERT_EQ(buffer[i], 0xAB) << "before the buffer, byte " << i;
        ASSERT_EQ(buffer[guard + (size_t) w * h * 3 + i], 0xAB) << "after the buffer, byte " << i;
    }
}

TEST(ScreenDigits, TheStrokeGetsThickerWithTheHeight)
{
    auto ink_of = [](int height) {
        Canvas canvas(canvas_big_width("1", height), height);
        canvas_big_text(&canvas.c, 0, 0, "1", height, CANVAS_BLACK);
        return canvas.count();
    };
    EXPECT_LT(ink_of(40), ink_of(80));
    EXPECT_LT(ink_of(80), ink_of(160));
    // a tiny size still draws something visible
    EXPECT_GT(ink_of(12), 10);
}

// Prints the digits as pictures: --gtest_filter=*Pictures*
TEST(ScreenDigits, DISABLED_Pictures)
{
    for (const char *g = "0123456789-+.:*"; *g; g++) {
        printf("--- %c\n%s", *g, picture(*g, 44).c_str());
    }
}
