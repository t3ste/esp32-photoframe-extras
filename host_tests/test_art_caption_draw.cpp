#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "guarded_canvas.h"

extern "C" {
#include "image_processor.h"
}

// The caption of the artworks mode drawn by the real image_processor.c: white text with a
// one-pixel black border at the bottom left, no bar, only the display palette's black and white,
// nothing outside its band, cut with "~" to the width of the picture.

namespace
{

constexpr uint32_t kRed = 0xFF0000, kWhite = 0xFFFFFF, kBlack = 0x000000;
constexpr int kMargin = 6, kGlyphW = 17, kGlyphH = 24;

struct Painted {
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
    size_t white = 0, black = 0, other = 0;
};

uint32_t pixel(const GuardedCanvas &c, int x, int y)
{
    const uint8_t *p = c.canvas.rgb + ((size_t) y * c.canvas.width + x) * 3;
    return (uint32_t) p[0] << 16 | (uint32_t) p[1] << 8 | p[2];
}

void fill_red(GuardedCanvas &c)
{
    for (int y = 0; y < c.canvas.height; y++) {
        for (int x = 0; x < c.canvas.width; x++) {
            uint8_t *p = c.canvas.rgb + ((size_t) y * c.canvas.width + x) * 3;
            p[0] = 255;
            p[1] = 0;
            p[2] = 0;
        }
    }
}

// What differs from the red background
Painted painted_of(const GuardedCanvas &c)
{
    Painted result;
    for (int y = 0; y < c.canvas.height; y++) {
        for (int x = 0; x < c.canvas.width; x++) {
            uint32_t value = pixel(c, x, y);
            if (value == kRed) {
                continue;
            }
            result.x0 = std::min(result.x0, x);
            result.y0 = std::min(result.y0, y);
            result.x1 = std::max(result.x1, x);
            result.y1 = std::max(result.y1, y);
            if (value == kWhite) {
                result.white++;
            } else if (value == kBlack) {
                result.black++;
            } else {
                result.other++;
            }
        }
    }
    return result;
}

Painted caption(int w, int h, const std::string &text, GuardedCanvas &c)
{
    fill_red(c);
    image_processor_draw_caption_outlined(c.canvas.rgb, w, h, text.c_str());
    return painted_of(c);
}

}  // namespace

TEST(ArtCaptionDraw, WhiteTextWithABlackBorderAtTheBottomLeft)
{
    GuardedCanvas c(400, 120);
    Painted p = caption(400, 120, "Hi", c);
    EXPECT_GT(p.white, 20u);
    EXPECT_GT(p.black, 20u);
    EXPECT_EQ(p.other, 0u);  // only the palette's black and white
    EXPECT_TRUE(c.guards_intact());
    // inside the band of one text line, one pixel wider on every side for the border
    int top = 120 - kGlyphH - kMargin;
    EXPECT_GE(p.x0, kMargin - 1);
    EXPECT_LE(p.x1, kMargin + 2 * kGlyphW);
    EXPECT_GE(p.y0, top - 1);
    EXPECT_LE(p.y1, top + kGlyphH);
}

TEST(ArtCaptionDraw, NothingOutsideTheBandChanges)
{
    GuardedCanvas c(400, 120);
    Painted p = caption(400, 120, "A caption of some length", c);
    ASSERT_GT(p.white, 0u);
    int top = 120 - kGlyphH - kMargin - 1;
    for (int y = 0; y < top; y++) {
        for (int x = 0; x < 400; x++) {
            ASSERT_EQ(pixel(c, x, y), kRed) << x << "," << y;
        }
    }
    for (int y = 0; y < 120; y++) {
        for (int x = p.x1 + 1; x < 400; x++) {
            ASSERT_EQ(pixel(c, x, y), kRed) << x << "," << y;
        }
    }
}

TEST(ArtCaptionDraw, TheBorderIsAroundEveryWhitePixel)
{
    GuardedCanvas c(300, 100);
    caption(300, 100, "Mm", c);
    // every white pixel has only white or black neighbours: the border closes the letters
    for (int y = 1; y < 99; y++) {
        for (int x = 1; x < 299; x++) {
            if (pixel(c, x, y) != kWhite) {
                continue;
            }
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    ASSERT_NE(pixel(c, x + dx, y + dy), kRed) << x << "," << y;
                }
            }
        }
    }
}

TEST(ArtCaptionDraw, ALongTextIsCutWithATildeToTheWidthOfThePicture)
{
    GuardedCanvas c(200, 80);  // (200 - 12) / 17 = 11 characters
    Painted p = caption(200, 80, std::string(40, 'M'), c);
    EXPECT_LE(p.x1, kMargin + 11 * kGlyphW);
    EXPECT_TRUE(c.guards_intact());

    GuardedCanvas fits(200, 80);
    Painted q = caption(200, 80, std::string(11, 'M'), fits);
    EXPECT_LE(p.x1,
              q.x1);  // the cut text (ten letters and the "~") is no wider than eleven letters
}

TEST(ArtCaptionDraw, AnUmlautIsOneCharacterWide)
{
    GuardedCanvas a(400, 80), b(400, 80);
    Painted with_umlaut = caption(400, 80, "K\xC3\xA4se", a);  // 4 characters
    Painted plain = caption(400, 80, "Kaese", b);              // 5 characters
    EXPECT_LT(with_umlaut.x1, plain.x1);
    EXPECT_EQ(with_umlaut.other, 0u);
}

TEST(ArtCaptionDraw, EmptyTextAndTooNarrowPicturesDoNothing)
{
    GuardedCanvas c(400, 80);
    EXPECT_EQ(caption(400, 80, "", c).white, 0u);
    GuardedCanvas narrow(40, 80);  // less than four characters wide
    EXPECT_EQ(caption(40, 80, "Caption", narrow).white, 0u);
    GuardedCanvas flat(400, 20);  // lower than the line: stays inside the picture
    caption(400, 20, "Caption", flat);
    EXPECT_TRUE(flat.guards_intact());
    image_processor_draw_caption_outlined(nullptr, 400, 80, "x");  // no crash
    image_processor_draw_caption_outlined(c.canvas.rgb, 0, 0, "x");
    EXPECT_TRUE(c.guards_intact());
}

TEST(ArtCaptionDraw, WhatTheFontCannotDrawLeavesNothing)
{
    GuardedCanvas c(400, 80);
    EXPECT_EQ(caption(400, 80, "\xF0\x9F\x98\x80", c).white, 0u);  // an emoji only
}
