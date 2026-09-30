#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "fonts.h"
#include "glyph_extras.h"
}

namespace
{

// One row of a glyph as text: '#' ink, '.' none, over the 17 pixels of the cell.
std::vector<std::string> art(const uint8_t *glyph)
{
    std::vector<std::string> rows;
    for (int r = 0; r < GLYPH_ROWS; r++) {
        std::string row;
        for (int c = 0; c < 17; c++) {
            row += (glyph[r * GLYPH_BYTES_PER_ROW + c / 8] & (0x80 >> (c % 8))) ? '#' : '.';
        }
        rows.push_back(row);
    }
    return rows;
}

std::vector<std::string> extra(uint8_t code)
{
    uint8_t glyph[GLYPH_BYTES];
    EXPECT_TRUE(glyph_extras_compose(code, Font24.table, glyph));
    return art(glyph);
}

std::vector<std::string> letter(char c)
{
    return art(Font24.table + (c - ' ') * GLYPH_BYTES);
}

int first_ink_row(const std::vector<std::string> &rows)
{
    for (size_t r = 0; r < rows.size(); r++) {
        if (rows[r].find('#') != std::string::npos) {
            return (int) r;
        }
    }
    return -1;
}

int last_ink_row(const std::vector<std::string> &rows)
{
    for (int r = (int) rows.size() - 1; r >= 0; r--) {
        if (rows[r].find('#') != std::string::npos) {
            return r;
        }
    }
    return -1;
}

}  // namespace

TEST(GlyphExtras, TheFontIsTheExpectedCell)
{
    ASSERT_EQ(Font24.Width, 17);
    ASSERT_EQ(Font24.Height, GLYPH_ROWS);
}

TEST(GlyphExtras, CodesAndCodePoints)
{
    EXPECT_EQ(glyph_extras_code_for_codepoint(0x00E4), GLYPH_A_UMLAUT);
    EXPECT_EQ(glyph_extras_code_for_codepoint(0x00F6), GLYPH_O_UMLAUT);
    EXPECT_EQ(glyph_extras_code_for_codepoint(0x00FC), GLYPH_U_UMLAUT);
    EXPECT_EQ(glyph_extras_code_for_codepoint(0x00C4), GLYPH_CAPITAL_A_UMLAUT);
    EXPECT_EQ(glyph_extras_code_for_codepoint(0x00D6), GLYPH_CAPITAL_O_UMLAUT);
    EXPECT_EQ(glyph_extras_code_for_codepoint(0x00DC), GLYPH_CAPITAL_U_UMLAUT);
    EXPECT_EQ(glyph_extras_code_for_codepoint(0x00DF), GLYPH_SHARP_S);
    EXPECT_EQ(glyph_extras_code_for_codepoint(0x00B0), GLYPH_DEGREE);
    EXPECT_EQ(glyph_extras_code_for_codepoint(0x20AC), GLYPH_EURO);
    EXPECT_EQ(glyph_extras_code_for_codepoint('a'), 0);
    EXPECT_EQ(glyph_extras_code_for_codepoint(0x00E9), 0);   // é: still dropped
    EXPECT_EQ(glyph_extras_code_for_codepoint(0x1F600), 0);  // an emoji
    for (unsigned c = 0; c < 256; c++) {
        EXPECT_EQ(glyph_extras_is_code((uint8_t) c),
                  c >= GLYPH_EXTRA_FIRST && c <= GLYPH_EXTRA_LAST)
            << c;
    }
    // every code in the range has a bitmap, and nothing outside it does
    uint8_t glyph[GLYPH_BYTES];
    for (unsigned c = GLYPH_EXTRA_FIRST; c <= GLYPH_EXTRA_LAST; c++) {
        EXPECT_TRUE(glyph_extras_compose((uint8_t) c, Font24.table, glyph)) << c;
    }
    EXPECT_FALSE(glyph_extras_compose(GLYPH_EXTRA_LAST + 1, Font24.table, glyph));
    EXPECT_FALSE(glyph_extras_compose('a', Font24.table, glyph));
}

TEST(GlyphExtras, UmlautsAreTheLetterWithTwoDots)
{
    const struct {
        uint8_t code;
        char base;
        int dots_top;
    } umlauts[] = {{GLYPH_A_UMLAUT, 'a', 3},         {GLYPH_O_UMLAUT, 'o', 3},
                   {GLYPH_U_UMLAUT, 'u', 3},         {GLYPH_CAPITAL_A_UMLAUT, 'A', 0},
                   {GLYPH_CAPITAL_O_UMLAUT, 'O', 0}, {GLYPH_CAPITAL_U_UMLAUT, 'U', 0}};
    for (const auto &u : umlauts) {
        std::vector<std::string> glyph = extra(u.code);
        std::vector<std::string> base = letter(u.base);
        // below the dots the glyph is exactly the letter of the font
        for (int r = u.dots_top + 2; r < GLYPH_ROWS; r++) {
            EXPECT_EQ(glyph[r], base[r]) << (int) u.code << " row " << r;
        }
        // the dots: 2x2 pixels twice, and a free row between them and the letter
        for (int r = u.dots_top; r < u.dots_top + 2; r++) {
            EXPECT_EQ(glyph[r], std::string(".....##...##.....")) << (int) u.code << " row " << r;
        }
        for (int r = 0; r < u.dots_top; r++) {
            EXPECT_EQ(glyph[r], std::string(17, '.')) << (int) u.code;
        }
        int letter_top = first_ink_row(base);
        EXPECT_GE(letter_top, u.dots_top + 3)
            << "a gap of one row between dots and letter: " << u.base;
    }
}

TEST(GlyphExtras, EveryGlyphStaysInsideTheCell)
{
    for (unsigned c = GLYPH_EXTRA_FIRST; c <= GLYPH_EXTRA_LAST; c++) {
        uint8_t glyph[GLYPH_BYTES];
        ASSERT_TRUE(glyph_extras_compose((uint8_t) c, Font24.table, glyph));
        for (int r = 0; r < GLYPH_ROWS; r++) {
            // the 17 pixels of a row are bits 7..0 of byte 0, 7..0 of byte 1 and bit 7 of byte 2
            EXPECT_EQ(glyph[r * GLYPH_BYTES_PER_ROW + 2] & 0x7F, 0) << c << " row " << r;
        }
        std::vector<std::string> rows = art(glyph);
        EXPECT_GE(first_ink_row(rows), 0) << c;
        EXPECT_LE(last_ink_row(rows), 19) << c;  // nothing below the descender area of the font
    }
}

TEST(GlyphExtras, SharpS)
{
    const std::vector<std::string> expected = {
        ".................", ".................", ".................", "....#######......",
        "...#########.....", "..###.....###....", "..###......###...", "..###......###...",
        "..###....#####...", "..###....#####...", "..###......#####.", "..###.......####.",
        "..###.......####.", "..###......#####.", "..###...#######..", "..###...######...",
        "..###............", ".................", ".................", ".................",
        ".................", ".................", ".................", "................."};
    EXPECT_EQ(extra(GLYPH_SHARP_S), expected);
}

TEST(GlyphExtras, DegreeSign)
{
    std::vector<std::string> rows = extra(GLYPH_DEGREE);
    EXPECT_EQ(first_ink_row(rows), 3);  // at the height of the capitals
    EXPECT_EQ(last_ink_row(rows), 7);
    EXPECT_EQ(rows[3], ".....#####.......");
    EXPECT_EQ(rows[5], "....##...##......");
}

TEST(GlyphExtras, EuroIsACWithTwoBars)
{
    std::vector<std::string> euro = extra(GLYPH_EURO);
    std::vector<std::string> c = letter('C');
    for (int r = 0; r < GLYPH_ROWS; r++) {
        if ((r >= 7 && r <= 8) || (r >= 11 && r <= 12)) {
            EXPECT_EQ(euro[r].substr(0, 10), std::string(10, '#')) << r;  // the bar
            EXPECT_EQ(euro[r].substr(10), c[r].substr(10)) << r;          // the rest as the C
        } else {
            EXPECT_EQ(euro[r], c[r]) << r;
        }
    }
}

// Not an assertion: the glyphs as pictures, in the test log (run with --gtest_filter=*Pictures*).
TEST(GlyphExtras, Pictures)
{
    for (unsigned code = GLYPH_EXTRA_FIRST; code <= GLYPH_EXTRA_LAST; code++) {
        std::vector<std::string> rows = extra((uint8_t) code);
        printf("--- 0x%02X\n", code);
        for (int r = 0; r < 20; r++) {
            printf("%2d %s\n", r, rows[r].c_str());
        }
    }
}
