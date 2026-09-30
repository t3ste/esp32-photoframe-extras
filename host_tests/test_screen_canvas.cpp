#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "fonts.h"
#include "glyph_extras.h"
#include "screen_canvas.h"
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
};

}  // namespace

TEST(ScreenCanvas, UnitAndTextScaleForTheBoards)
{
    const struct {
        int w, h, unit, scale;
    } boards[] = {{800, 480, 12, 1},
                  {480, 800, 12, 1},
                  {960, 540, 13, 1},
                  {1200, 1600, 30, 3},
                  {1872, 1404, 35, 3}};
    for (const auto &b : boards) {
        Canvas cv(b.w, b.h);
        EXPECT_EQ(canvas_unit(&cv.c), b.unit) << b.w << "x" << b.h;
        EXPECT_EQ(canvas_text_scale(&cv.c, 1), b.scale) << b.w << "x" << b.h;
        EXPECT_EQ(canvas_text_scale(&cv.c, 4), 4 * b.scale);
    }
    Canvas tiny(20, 20);
    EXPECT_EQ(canvas_unit(&tiny.c), 1);
    EXPECT_EQ(canvas_text_scale(&tiny.c, 1), 1);
}

TEST(ScreenCanvas, RectFillsAndClips)
{
    Canvas cv(20, 10);
    canvas_rect(&cv.c, 5, 2, 4, 3, CANVAS_RED);
    EXPECT_EQ(cv.count(), 12);
    EXPECT_TRUE(cv.ink(5, 2));
    EXPECT_TRUE(cv.ink(8, 4));
    EXPECT_FALSE(cv.ink(9, 4));
    EXPECT_EQ(cv.pixels[((size_t) 2 * 20 + 5) * 3], 255);
    EXPECT_EQ(cv.pixels[((size_t) 2 * 20 + 5) * 3 + 1], 0);

    Canvas edge(20, 10);
    canvas_rect(&edge.c, -3, -3, 6, 6, CANVAS_BLACK);   // only 3x3 inside
    canvas_rect(&edge.c, 18, 8, 10, 10, CANVAS_BLACK);  // only 2x2 inside
    EXPECT_EQ(edge.count(), 9 + 4);
    Canvas none(20, 10);
    canvas_rect(&none.c, 30, 30, 5, 5, CANVAS_BLACK);
    canvas_rect(&none.c, 2, 2, 0, 5, CANVAS_BLACK);
    canvas_rect(&none.c, 2, 2, -5, 5, CANVAS_BLACK);
    EXPECT_EQ(none.count(), 0);
}

TEST(ScreenCanvas, FrameIsOnlyTheOutline)
{
    Canvas cv(30, 20);
    canvas_frame(&cv.c, 2, 2, 20, 10, 2, CANVAS_BLACK);
    EXPECT_EQ(cv.count(), 20 * 10 - 16 * 6);
    EXPECT_TRUE(cv.ink(2, 2));
    EXPECT_TRUE(cv.ink(21, 11));
    EXPECT_FALSE(cv.ink(10, 6));
    Canvas thick(30, 20);
    canvas_frame(&thick.c, 0, 0, 6, 6, 5, CANVAS_BLACK);  // thicker than it can be: filled
    EXPECT_EQ(thick.count(), 36);
}

TEST(ScreenCanvas, DiscHasTheAreaOfACircle)
{
    for (int r : {5, 20, 60}) {
        Canvas cv(200, 200);
        canvas_disc(&cv.c, 100, 100, r, CANVAS_BLACK);
        double area = M_PI * r * r;
        EXPECT_NEAR(cv.count(), area, area * 0.06) << r;
        EXPECT_TRUE(cv.ink(100, 100));
        EXPECT_TRUE(cv.ink(100 + r, 100));
        EXPECT_FALSE(cv.ink(100 + r + 1, 100));
        EXPECT_FALSE(cv.ink(100 + r, 100 + r));  // the corner of the bounding box
    }
    Canvas none(50, 50);
    canvas_disc(&none.c, 25, 25, 0, CANVAS_BLACK);
    canvas_disc(&none.c, 25, 25, -3, CANVAS_BLACK);
    EXPECT_EQ(none.count(), 0);
    Canvas cut(50, 50);
    canvas_disc(&cut.c, 0, 0, 20, CANVAS_BLACK);  // a quarter is inside
    EXPECT_NEAR(cut.count(), M_PI * 400 / 4, 60);
}

TEST(ScreenCanvas, RingSectorCoversItsAngle)
{
    const int cx = 100, cy = 100, outer = 80, inner = 40;
    Canvas q(200, 200);
    canvas_ring_sector(&q.c, cx, cy, outer, inner, 0, 90, CANVAS_BLACK);  // 12 to 3 o'clock
    EXPECT_TRUE(q.ink(cx + 40, cy - 40));                                 // 45 degrees, radius 57
    EXPECT_TRUE(q.ink(cx + 1, cy - 60));                                  // just after 12 o'clock
    EXPECT_TRUE(q.ink(cx + 60, cy - 1));                                  // just before 3 o'clock
    EXPECT_FALSE(q.ink(cx - 40, cy - 40));                                // top left
    EXPECT_FALSE(q.ink(cx + 40, cy + 40));                                // bottom right
    EXPECT_FALSE(q.ink(cx + 10, cy - 10));                                // inside the hole
    EXPECT_FALSE(q.ink(cx + 100, cy));                                    // outside
    double quarter = M_PI * (outer * outer - inner * inner) / 4;
    EXPECT_NEAR(q.count(), quarter, quarter * 0.05);

    Canvas full(200, 200);
    canvas_ring_sector(&full.c, cx, cy, outer, inner, 0, 360, CANVAS_BLACK);
    double ring = M_PI * (outer * outer - inner * inner);
    EXPECT_NEAR(full.count(), ring, ring * 0.03);
    EXPECT_FALSE(full.ink(cx, cy));
}

TEST(ScreenCanvas, RingSectorsTileTheRing)
{
    // five sectors of 72 degrees, one colour each: every pixel of the ring is drawn by exactly one
    const int cx = 100, cy = 100;
    Canvas whole(200, 200), pieces(200, 200);
    canvas_ring_sector(&whole.c, cx, cy, 80, 40, 0, 360, CANVAS_BLACK);
    for (int i = 0; i < 5; i++) {
        canvas_ring_sector(&pieces.c, cx, cy, 80, 40, i * 72.0f, (i + 1) * 72.0f, CANVAS_BLACK);
    }
    int differing = 0;
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < 200; x++) {
            differing += whole.ink(x, y) != pieces.ink(x, y) ? 1 : 0;
        }
    }
    EXPECT_EQ(differing, 0);
}

TEST(ScreenCanvas, RingSectorWrapsAroundTheTop)
{
    Canvas cv(200, 200);
    canvas_ring_sector(&cv.c, 100, 100, 80, 40, 340, 380, CANVAS_BLACK);  // 340..20 degrees
    EXPECT_TRUE(cv.ink(100, 100 - 60));                                   // straight up
    EXPECT_TRUE(cv.ink(100 - 15, 100 - 60));                              // a little left of it
    EXPECT_TRUE(cv.ink(100 + 15, 100 - 60));                              // a little right of it
    EXPECT_FALSE(cv.ink(100 + 60, 100));                                  // 3 o'clock
    EXPECT_FALSE(cv.ink(100 - 60, 100));
    Canvas negative(200, 200);
    canvas_ring_sector(&negative.c, 100, 100, 80, 40, -20, 20, CANVAS_BLACK);
    EXPECT_EQ(negative.count(), cv.count());
}

TEST(ScreenCanvas, RingSectorDegenerateCases)
{
    Canvas cv(100, 100);
    canvas_ring_sector(&cv.c, 50, 50, 30, 10, 90, 90, CANVAS_BLACK);  // empty span
    canvas_ring_sector(&cv.c, 50, 50, 30, 10, 90, 45, CANVAS_BLACK);  // negative span
    canvas_ring_sector(&cv.c, 50, 50, 10, 30, 0, 360, CANVAS_BLACK);  // inner > outer
    canvas_ring_sector(&cv.c, 50, 50, 0, 0, 0, 360, CANVAS_BLACK);
    EXPECT_EQ(cv.count(), 0);
    Canvas disc(100, 100);
    canvas_ring_sector(&disc.c, 50, 50, 30, -5, 0, 360, CANVAS_BLACK);  // a negative hole is none
    EXPECT_NEAR(disc.count(), M_PI * 900, 100);
    Canvas cut(60, 60);
    canvas_ring_sector(&cut.c, 0, 0, 40, 20, 0, 360,
                       CANVAS_BLACK);  // mostly outside: clipped, no crash
    EXPECT_GT(cut.count(), 0);
}

TEST(ScreenCanvas, PillAndTriangle)
{
    Canvas cv(100, 40);
    canvas_pill(&cv.c, 10, 5, 60, 20, CANVAS_BLACK);
    EXPECT_TRUE(cv.ink(40, 15));  // middle
    EXPECT_TRUE(cv.ink(11, 15));  // the round left end
    EXPECT_FALSE(cv.ink(10, 5));  // the corner is cut
    EXPECT_FALSE(cv.ink(69, 5));
    EXPECT_NEAR(cv.count(), 40 * 20 + M_PI * 100, 40);
    Canvas narrow(100, 40);
    canvas_pill(&narrow.c, 10, 5, 10, 30, CANVAS_BLACK);  // taller than wide: a disc
    EXPECT_NEAR(narrow.count(), M_PI * 25, 20);

    Canvas tri(60, 60);
    canvas_triangle_down(&tri.c, 30, 40, 20, CANVAS_BLACK);
    EXPECT_TRUE(tri.ink(30, 40));   // the tip
    EXPECT_TRUE(tri.ink(30, 21));   // the middle of the top edge
    EXPECT_FALSE(tri.ink(30, 41));  // below the tip
    EXPECT_FALSE(tri.ink(15, 21));  // outside at the top
}

TEST(ScreenCanvas, TextAtScaleOneIsTheFontsGlyph)
{
    Canvas cv(60, 40);
    int end = canvas_text(&cv.c, 2, 3, "A", 1, CANVAS_BLACK);
    EXPECT_EQ(end, 2 + 17);
    const uint8_t *glyph = &Font24.table[('A' - ' ') * 24 * 3];
    for (int row = 0; row < 24; row++) {
        for (int col = 0; col < 17; col++) {
            bool font = glyph[row * 3 + col / 8] & (0x80 >> (col % 8));
            EXPECT_EQ(cv.ink(2 + col, 3 + row), font) << row << "," << col;
        }
    }
}

TEST(ScreenCanvas, TextScaleMultipliesEveryPixel)
{
    Canvas one(100, 100), three(100, 100);
    canvas_text(&one.c, 0, 0, "Bg", 1, CANVAS_BLACK);
    int end = canvas_text(&three.c, 0, 0, "Bg", 3, CANVAS_BLACK);
    EXPECT_EQ(end, 2 * 17 * 3);
    EXPECT_EQ(three.count(), one.count() * 9);
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 34; x++) {
            EXPECT_EQ(three.ink(x * 3 + 1, y * 3 + 2), one.ink(x, y));
        }
    }
    Canvas zero(100, 100), plain(100, 100);
    canvas_text(&zero.c, 0, 0, "B", 0, CANVAS_BLACK);  // a scale below 1 is 1
    canvas_text(&plain.c, 0, 0, "B", 1, CANVAS_BLACK);
    EXPECT_EQ(zero.pixels, plain.pixels);
}

TEST(ScreenCanvas, TextMetricsAndPlacement)
{
    EXPECT_EQ(canvas_text_width("Hello", 1), 5 * 17);
    EXPECT_EQ(canvas_text_width("Hello", 2), 5 * 17 * 2);
    EXPECT_EQ(canvas_text_width("", 3), 0);
    EXPECT_EQ(canvas_text_height(1), 24);
    EXPECT_EQ(canvas_text_height(4), 96);
    Canvas a(200, 60), b(200, 60);
    canvas_text_centered(&a.c, 100, 10, "abc", 1, CANVAS_BLACK);
    canvas_text(&b.c, 100 - 51 / 2, 10, "abc", 1, CANVAS_BLACK);
    EXPECT_EQ(a.pixels, b.pixels);
    Canvas r(200, 60), r2(200, 60);
    canvas_text_right(&r.c, 190, 10, "abc", 1, CANVAS_BLACK);
    canvas_text(&r2.c, 190 - 51, 10, "abc", 1, CANVAS_BLACK);
    EXPECT_EQ(r.pixels, r2.pixels);
}

TEST(ScreenCanvas, TextClipsAtTheEdgesAndSkipsUnknownBytes)
{
    Canvas cv(30, 30);
    canvas_text(&cv.c, -10, -10, "WW", 2, CANVAS_BLACK);  // partly outside: no crash
    canvas_text(&cv.c, 20, 20, "WW", 2, CANVAS_BLACK);
    EXPECT_GT(cv.count(), 0);
    Canvas blank(60, 30);
    canvas_text(&blank.c, 0, 0, "\x01\x7F\xFF", 1, CANVAS_BLACK);  // no glyph: the cells stay empty
    EXPECT_EQ(blank.count(), 0);
    Canvas moved(100, 30), ref(100, 30);
    canvas_text(&moved.c, 0, 0,
                "\x01"
                "A",
                1, CANVAS_BLACK);
    canvas_text(&ref.c, 17, 0, "A", 1, CANVAS_BLACK);  // but they take their width
    EXPECT_EQ(moved.pixels, ref.pixels);
}

TEST(ScreenCanvas, TextDrawsTheExtraGlyphs)
{
    Canvas cv(200, 40);
    canvas_text(&cv.c, 0, 0, "\x80\x86\x87\x88", 1, CANVAS_BLACK);
    for (int cell = 0; cell < 4; cell++) {
        int pixels = 0;
        for (int y = 0; y < 24; y++) {
            for (int x = cell * 17; x < cell * 17 + 17; x++) {
                pixels += cv.ink(x, y) ? 1 : 0;
            }
        }
        EXPECT_GT(pixels, 10) << cell;
    }
}

TEST(ScreenCanvas, FitCutsWithATilde)
{
    char out[64];
    canvas_text_fit("Short", 200, 1, out, sizeof(out));
    EXPECT_STREQ(out, "Short");
    canvas_text_fit("A much longer text", 17 * 8, 1, out, sizeof(out));  // 8 cells
    EXPECT_STREQ(out, "A much ~");
    canvas_text_fit("Text", 17 * 8, 2, out, sizeof(out));  // scale 2: 4 cells - exactly fits
    EXPECT_STREQ(out, "Text");
    canvas_text_fit("Texts", 17 * 8, 2, out, sizeof(out));
    EXPECT_STREQ(out, "Tex~");
    canvas_text_fit("Text", 10, 1, out, sizeof(out));  // not even one cell
    EXPECT_STREQ(out, "");
    char tiny[4];
    canvas_text_fit("Long text here", 1000, 1, tiny, sizeof(tiny));  // the buffer is the limit
    EXPECT_STREQ(tiny, "Lon");
}

TEST(ScreenCanvas, FitScalePicksTheLargestThatFits)
{
    EXPECT_EQ(canvas_text_fit_scale("12", 17 * 2 * 6, 6), 6);
    EXPECT_EQ(canvas_text_fit_scale("12", 17 * 2 * 4 + 5, 6), 4);
    EXPECT_EQ(canvas_text_fit_scale("123456", 20, 6), 1);  // does not fit at all: 1
    EXPECT_EQ(canvas_text_fit_scale("12", 1000, 3), 3);    // capped by max_scale
}

TEST(ScreenCanvas, WrapBreaksAtSpaces)
{
    char lines[8][CANVAS_WRAP_LINE_MAX];
    int n = canvas_text_wrap("the quick brown fox jumps over the lazy dog", 17 * 12, 1, lines, 8);
    ASSERT_EQ(n, 4);
    EXPECT_STREQ(lines[0], "the quick");
    EXPECT_STREQ(lines[1], "brown fox");
    EXPECT_STREQ(lines[2], "jumps over");
    EXPECT_STREQ(lines[3], "the lazy dog");
    for (int i = 0; i < n; i++) {
        EXPECT_LE((int) strlen(lines[i]), 12);
    }
}

TEST(ScreenCanvas, WrapCutsLongWordsAndMarksCutOffText)
{
    char lines[8][CANVAS_WRAP_LINE_MAX];
    int n = canvas_text_wrap("supercalifragilistic", 17 * 8, 1, lines, 8);
    ASSERT_EQ(n, 3);
    EXPECT_STREQ(lines[0], "supercal");
    EXPECT_STREQ(lines[1], "ifragili");
    EXPECT_STREQ(lines[2], "stic");

    n = canvas_text_wrap("one two three four five six seven", 17 * 8, 1, lines, 2);
    ASSERT_EQ(n, 2);
    EXPECT_STREQ(lines[0], "one two");
    EXPECT_STREQ(lines[1], "three~");  // cut: the last line ends with the mark
}

TEST(ScreenCanvas, WrapEdgeCases)
{
    char lines[4][CANVAS_WRAP_LINE_MAX];
    EXPECT_EQ(canvas_text_wrap("", 100, 1, lines, 4), 0);
    EXPECT_EQ(canvas_text_wrap("   ", 100, 1, lines, 4), 0);
    EXPECT_EQ(canvas_text_wrap("word", 100, 1, lines, 0), 0);
    int n = canvas_text_wrap("  padded   text  ", 17 * 20, 1, lines, 4);
    ASSERT_EQ(n, 1);
    EXPECT_STREQ(lines[0], "padded   text");
    n = canvas_text_wrap("a b", 1, 1, lines, 4);  // narrower than one cell: one glyph per line
    EXPECT_EQ(n, 2);
    EXPECT_STREQ(lines[0], "a");
    EXPECT_STREQ(lines[1], "b");
    // the scale changes what fits
    n = canvas_text_wrap("aaaa bbbb", 17 * 2 * 9, 2, lines, 4);  // nine cells of 34 pixels
    EXPECT_EQ(n, 1);
    n = canvas_text_wrap("aaaa bbbb", 17 * 2 * 9 - 1, 2, lines, 4);
    EXPECT_EQ(n, 2);
}

TEST(ScreenCanvas, SparklineStaysInItsBoxAndEndsInAColouredDisc)
{
    Canvas cv(200, 120);
    const float values[] = {1.0f, 3.0f, 2.0f, 5.0f, 4.0f, 6.0f};
    canvas_sparkline(&cv.c, 50, 30, 100, 60, values, 6, CANVAS_GREEN);
    int ink = 0, green = 0;
    for (int y = 0; y < 120; y++) {
        for (int x = 0; x < 200; x++) {
            if (cv.ink(x, y)) {
                ink++;
                bool inside = x >= 50 && x < 150 && y >= 30 && y < 90;
                EXPECT_TRUE(inside) << x << "," << y;
                const uint8_t *p = &cv.pixels[((size_t) y * 200 + x) * 3];
                green += (p[0] == 0 && p[1] == 255 && p[2] == 0) ? 1 : 0;
            }
        }
    }
    EXPECT_GT(ink, 100);
    EXPECT_GT(green, 4);
}

TEST(ScreenCanvas, SparklineNeedsTwoValuesAndABox)
{
    Canvas cv(100, 60);
    const float values[] = {1.0f, 2.0f};
    canvas_sparkline(&cv.c, 0, 0, 100, 60, values, 1, CANVAS_GREEN);
    canvas_sparkline(&cv.c, 0, 0, 100, 60, values, 0, CANVAS_GREEN);
    canvas_sparkline(&cv.c, 0, 0, 3, 60, values, 2, CANVAS_GREEN);
    canvas_sparkline(&cv.c, 0, 0, 100, 3, values, 2, CANVAS_GREEN);
    EXPECT_EQ(cv.count(), 0);
    canvas_sparkline(&cv.c, 0, 0, 100, 60, values, 2, CANVAS_GREEN);
    EXPECT_GT(cv.count(), 10);
}

TEST(ScreenCanvas, SparklineOfEqualValuesIsAFlatLineInTheMiddle)
{
    Canvas cv(100, 60);
    const float values[] = {5.0f, 5.0f, 5.0f, 5.0f};
    canvas_sparkline(&cv.c, 0, 0, 100, 60, values, 4, CANVAS_BLACK);
    int top = 60, bottom = 0;
    for (int y = 0; y < 60; y++) {
        for (int x = 0; x < 100; x++) {
            if (cv.ink(x, y)) {
                top = y < top ? y : top;
                bottom = y > bottom ? y : bottom;
            }
        }
    }
    EXPECT_LT(top, 30);
    EXPECT_GT(bottom, 28);
    EXPECT_LT(bottom - top, 12);
}
