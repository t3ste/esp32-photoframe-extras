#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "glyph_extras.h"
#include "image_processor.h"
}

// The real sanitizer and text drawing of image_processor.c with the `glyphs` option on
// (FEATURE_GLYPHS): the nine characters stay characters, drawn from the font, everything else
// behaves as before.

namespace
{

std::string sanitize(const std::string &utf8)
{
    char out[256];
    image_processor_sanitize_ascii(utf8.c_str(), out, sizeof(out));
    return out;
}

constexpr int kW = 64, kH = 32;

// Draws `text` at (2, 2) in black on white and returns the buffer.
std::vector<uint8_t> draw(const std::string &text)
{
    std::vector<uint8_t> rgb((size_t) kW * kH * 3, 255);
    image_processor_draw_text(rgb.data(), kW, kH, 2, 2, text.c_str(), 0, 0, 0);
    return rgb;
}

bool ink(const std::vector<uint8_t> &rgb, int x, int y)
{
    return rgb[((size_t) y * kW + x) * 3] == 0;
}

}  // namespace

TEST(GlyphText, TheNineCharactersBecomeOneByteCodes)
{
    EXPECT_EQ(sanitize("\xC3\xA4"), "\x80");      // ä
    EXPECT_EQ(sanitize("\xC3\xB6"), "\x81");      // ö
    EXPECT_EQ(sanitize("\xC3\xBC"), "\x82");      // ü
    EXPECT_EQ(sanitize("\xC3\x84"), "\x83");      // Ä
    EXPECT_EQ(sanitize("\xC3\x96"), "\x84");      // Ö
    EXPECT_EQ(sanitize("\xC3\x9C"), "\x85");      // Ü
    EXPECT_EQ(sanitize("\xC3\x9F"), "\x86");      // ß
    EXPECT_EQ(sanitize("\xC2\xB0"), "\x87");      // °
    EXPECT_EQ(sanitize("\xE2\x82\xAC"), "\x88");  // €
}

TEST(GlyphText, ASentenceKeepsItsLettersAndDropsWhatHasNoGlyph)
{
    // "Größe 20°C, Käse 3€ - café ok 😀": é and the emoji have no glyph and are dropped, as before
    EXPECT_EQ(sanitize("Gr\xC3\xB6\xC3\x9F"
                       "e 20\xC2\xB0"
                       "C, K\xC3\xA4se 3\xE2\x82\xAC - caf\xC3\xA9 ok \xF0\x9F\x98\x80"),
              std::string("Gr\x81\x86"
                          "e 20\x87"
                          "C, K\x80se 3\x88 - caf ok "));
}

TEST(GlyphText, SanitizingTwiceChangesNothing)
{
    // text is sanitized where it is read (calendar, to-do, headlines) and again by the drawing
    // functions of the overlay bar - the second pass must keep the glyph codes
    const std::string once = sanitize(
        "K\xC3\xA4se \xC3\xB6\xC3\xBC \xC3\x84\xC3\x96\xC3\x9C \xC3\x9F 20\xC2\xB0"
        "C 3\xE2\x82\xAC caf\xC3\xA9");
    EXPECT_EQ(once,
              "K\x80se \x81\x82 \x83\x84\x85 \x86 20\x87"
              "C 3\x88 caf");
    EXPECT_EQ(sanitize(once), once);
    EXPECT_EQ(sanitize(sanitize(once)), once);
}

TEST(GlyphText, PlainAsciiIsUntouched)
{
    EXPECT_EQ(sanitize("Plain text, 123 ~!"), "Plain text, 123 ~!");
    EXPECT_EQ(sanitize(""), "");
}

TEST(GlyphText, EachGlyphIsOneCellWide)
{
    EXPECT_EQ(image_processor_measure_text_width("a\x80\x81\x82\x83\x84\x85\x86\x87\x88"),
              10 * IMAGE_PROCESSOR_FONT_WIDTH);
}

TEST(GlyphText, AnUmlautIsDrawnFromTheFontAndItsDots)
{
    std::vector<uint8_t> a = draw("a");
    std::vector<uint8_t> a_umlaut = draw("\x80");
    int differing_rows = 0;
    for (int y = 0; y < kH; y++) {
        bool same = true;
        for (int x = 0; x < kW; x++) {
            same = same && (ink(a, x, y) == ink(a_umlaut, x, y));
        }
        differing_rows += same ? 0 : 1;
    }
    EXPECT_EQ(differing_rows, 2);  // only the two rows of dots differ from a plain "a"
    // the dots of the glyph: rows 3-4 of the cell, columns 5-6 and 10-11 (the text starts at 2,2)
    EXPECT_TRUE(ink(a_umlaut, 2 + 5, 2 + 3));
    EXPECT_TRUE(ink(a_umlaut, 2 + 11, 2 + 4));
    EXPECT_FALSE(ink(a, 2 + 5, 2 + 3));
}

TEST(GlyphText, TheOtherGlyphsAreDrawn)
{
    for (unsigned code = GLYPH_EXTRA_FIRST; code <= GLYPH_EXTRA_LAST; code++) {
        std::string text(1, (char) code);
        std::vector<uint8_t> rgb = draw(text);
        int pixels = 0;
        for (int y = 0; y < kH; y++) {
            for (int x = 0; x < kW; x++) {
                pixels += ink(rgb, x, y) ? 1 : 0;
            }
        }
        EXPECT_GT(pixels, 8) << code;
    }
}

TEST(GlyphText, ANeighbourIsNotDisturbed)
{
    std::vector<uint8_t> pair = draw(
        "\x86"
        "B");
    std::vector<uint8_t> just_b = draw("B");
    for (int y = 0; y < kH; y++) {
        for (int x = 2 + IMAGE_PROCESSOR_FONT_WIDTH; x < kW; x++) {
            // the B of "ßB" is the B of "B", one cell to the right
            bool shifted = x - IMAGE_PROCESSOR_FONT_WIDTH >= 0
                               ? ink(just_b, x - IMAGE_PROCESSOR_FONT_WIDTH, y)
                               : false;
            EXPECT_EQ(ink(pair, x, y), shifted) << x << "," << y;
        }
    }
}

TEST(GlyphText, BytesWithoutAGlyphDrawNothing)
{
    std::vector<uint8_t> blank((size_t) kW * kH * 3, 255);
    EXPECT_EQ(draw("\x89"), blank);  // one above the last code
    EXPECT_EQ(draw("\xFF"), blank);
    EXPECT_EQ(draw("\x7F"), blank);
}
