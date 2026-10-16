#include <gtest/gtest.h>
#include <png.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "guarded_canvas.h"

extern "C" {
#include "climate.h"
#include "image_processor.h"
#include "text_outline.h"

// The settings the stubs of fork_stubs.c hand to image_processor.c
extern bool test_caption_invert_colors_enabled;
extern int test_overlay_text_style;
extern int test_overlay_text_border;
}

// The overlay and caption text without a bar (build option text-outline): the pure helpers of
// text_outline.c, and the real image_processor.c drawing the text with a border - the same
// letters in the same places as the bar, in the display palette's black and white only.

namespace
{

constexpr uint32_t kRed = 0xFF0000, kWhite = 0xFFFFFF, kBlack = 0x000000;
constexpr int kPad = 4, kGlyphH = 24;

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

struct Box {
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
    size_t count = 0;
};

// The pixels of one colour
Box box_of(const GuardedCanvas &c, uint32_t colour)
{
    Box b;
    for (int y = 0; y < c.canvas.height; y++) {
        for (int x = 0; x < c.canvas.width; x++) {
            if (pixel(c, x, y) == colour) {
                b.x0 = std::min(b.x0, x);
                b.y0 = std::min(b.y0, y);
                b.x1 = std::max(b.x1, x);
                b.y1 = std::max(b.y1, y);
                b.count++;
            }
        }
    }
    return b;
}

std::set<std::pair<int, int>> pixels_of(const GuardedCanvas &c, uint32_t colour)
{
    std::set<std::pair<int, int>> out;
    for (int y = 0; y < c.canvas.height; y++) {
        for (int x = 0; x < c.canvas.width; x++) {
            if (pixel(c, x, y) == colour) {
                out.insert({x, y});
            }
        }
    }
    return out;
}

// The chosen look, put back at the end of every test
class TextOutline : public ::testing::Test
{
   protected:
    void TearDown() override
    {
        test_caption_invert_colors_enabled = false;
        test_overlay_text_style = TEXT_STYLE_BAR;
        test_overlay_text_border = TEXT_BORDER_DEFAULT;
    }

    static void look(int style, int border, bool captions_follow)
    {
        test_overlay_text_style = style;
        test_overlay_text_border = border;
        test_caption_invert_colors_enabled = captions_follow;
    }

    // A caption in the look of the settings on a red canvas
    static void caption(GuardedCanvas &c, const std::string &text)
    {
        fill_red(c);
        image_processor_draw_caption_look(c.canvas.rgb, c.canvas.width, c.canvas.height,
                                          text.c_str(), false);
    }
};

// Reads an RGB PNG written by the overlay code
bool read_png(const std::string &path, int &w, int &h, std::vector<uint8_t> &rgb)
{
    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&image, path.c_str())) {
        return false;
    }
    image.format = PNG_FORMAT_RGB;
    w = (int) image.width;
    h = (int) image.height;
    rgb.resize(PNG_IMAGE_SIZE(image));
    bool ok = png_image_finish_read(&image, nullptr, rgb.data(), 0, nullptr) != 0;
    png_image_free(&image);
    return ok;
}

uint32_t rgb_at(const std::vector<uint8_t> &rgb, int w, int x, int y)
{
    const uint8_t *p = rgb.data() + ((size_t) y * w + x) * 3;
    return (uint32_t) p[0] << 16 | (uint32_t) p[1] << 8 | p[2];
}

// A red PNG, then the overlay lines and the caption drawn on it by the real overlay function
bool overlay_png(const std::string &path, int w, int h, const std::vector<std::string> &lines,
                 const char *exif_caption, std::vector<uint8_t> &out)
{
    GuardedCanvas c(w, h);
    fill_red(c);
    if (image_processor_write_rgb_to_png(c.canvas.rgb, w, h, path.c_str()) != ESP_OK) {
        return false;
    }
    std::vector<const char *> pointers;
    for (const std::string &line : lines) {
        pointers.push_back(line.c_str());
    }
    std::string mutable_path = path;
    mutable_path.reserve(path.size() + 8);
    char *buffer = mutable_path.data();
    if (image_processor_add_overlay_to_file(
            buffer, pointers.data(), (int) pointers.size(), false, false, 0, exif_caption, false,
            "", CLIMATE_CATEGORY_GOOD, false, "", CLIMATE_CATEGORY_GOOD) != ESP_OK) {
        return false;
    }
    int rw = 0, rh = 0;
    bool ok = read_png(path, rw, rh, out) && rw == w && rh == h;
    std::remove(path.c_str());
    return ok;
}

}  // namespace

TEST(TextOutlineHelpers, TheNamesAndTheStoredNumbers)
{
    EXPECT_STREQ(text_style_name(TEXT_STYLE_BAR), "bar");
    EXPECT_STREQ(text_style_name(TEXT_STYLE_WHITE), "white");
    EXPECT_STREQ(text_style_name(TEXT_STYLE_BLACK), "black");
    EXPECT_STREQ(text_style_name((text_style_t) 7), "bar");
    EXPECT_STREQ(text_style_name((text_style_t) -1), "bar");

    EXPECT_EQ(text_style_from_name("bar"), TEXT_STYLE_BAR);
    EXPECT_EQ(text_style_from_name("white"), TEXT_STYLE_WHITE);
    EXPECT_EQ(text_style_from_name("black"), TEXT_STYLE_BLACK);
    EXPECT_EQ(text_style_from_name("White"), -1);  // exact names only
    EXPECT_EQ(text_style_from_name(""), -1);
    EXPECT_EQ(text_style_from_name("red"), -1);
    EXPECT_EQ(text_style_from_name(nullptr), -1);

    EXPECT_EQ(text_style_clamp(0), TEXT_STYLE_BAR);
    EXPECT_EQ(text_style_clamp(2), TEXT_STYLE_BLACK);
    EXPECT_EQ(text_style_clamp(3), TEXT_STYLE_BAR);
    EXPECT_EQ(text_style_clamp(255), TEXT_STYLE_BAR);
    EXPECT_EQ(text_style_clamp(-5), TEXT_STYLE_BAR);

    EXPECT_EQ(text_border_clamp(0), 1);
    EXPECT_EQ(text_border_clamp(1), 1);
    EXPECT_EQ(text_border_clamp(2), 2);
    EXPECT_EQ(text_border_clamp(3), 2);
    EXPECT_EQ(text_border_clamp(-9), 1);

    EXPECT_TRUE(text_style_ink_white(TEXT_STYLE_WHITE));
    EXPECT_FALSE(text_style_ink_white(TEXT_STYLE_BLACK));
    EXPECT_FALSE(text_style_ink_white(TEXT_STYLE_BAR));
}

TEST(TextOutlineHelpers, OnePixelIsTheEightNeighbours)
{
    int8_t offsets[TEXT_BORDER_MAX_OFFSETS][2];
    ASSERT_EQ(text_border_offsets(1, offsets, TEXT_BORDER_MAX_OFFSETS), 8);
    std::set<std::pair<int, int>> seen;
    for (int i = 0; i < 8; i++) {
        seen.insert({offsets[i][0], offsets[i][1]});
    }
    EXPECT_EQ(seen.size(), 8u);
    EXPECT_EQ(seen.count({0, 0}), 0u);
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx != 0 || dy != 0) {
                EXPECT_EQ(seen.count({dx, dy}), 1u) << dx << "," << dy;
            }
        }
    }
}

TEST(TextOutlineHelpers, TwoPixelsAreTheSquareWithoutItsFourCorners)
{
    int8_t offsets[TEXT_BORDER_MAX_OFFSETS][2];
    ASSERT_EQ(text_border_offsets(2, offsets, TEXT_BORDER_MAX_OFFSETS), 20);
    std::set<std::pair<int, int>> seen;
    for (int i = 0; i < 20; i++) {
        seen.insert({offsets[i][0], offsets[i][1]});
    }
    EXPECT_EQ(seen.size(), 20u);
    EXPECT_EQ(seen.count({0, 0}), 0u);
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            bool corner = std::abs(dx) == 2 && std::abs(dy) == 2;
            bool expected = !(dx == 0 && dy == 0) && !corner;
            EXPECT_EQ(seen.count({dx, dy}), expected ? 1u : 0u) << dx << "," << dy;
        }
    }
    EXPECT_LE(TEXT_BORDER_MAX_OFFSETS, 24);
}

TEST(TextOutlineHelpers, TheOffsetsFitTheBufferAndTheWidthIsClamped)
{
    int8_t offsets[TEXT_BORDER_MAX_OFFSETS][2];
    EXPECT_EQ(text_border_offsets(2, offsets, 5), 5);  // the room given is never exceeded
    EXPECT_EQ(text_border_offsets(2, offsets, 0), 0);
    EXPECT_EQ(text_border_offsets(9, offsets, TEXT_BORDER_MAX_OFFSETS), 20);  // clamped to 2
    EXPECT_EQ(text_border_offsets(0, offsets, TEXT_BORDER_MAX_OFFSETS), 8);   // and to 1
}

TEST_F(TextOutline, WithTheBarAsStyleTheCaptionIsTheBarOfBefore)
{
    GuardedCanvas styled(400, 120), plain(400, 120);
    look(TEXT_STYLE_BAR, 2, true);
    caption(styled, "Hello world");
    fill_red(plain);
    image_processor_draw_caption(plain.canvas.rgb, 400, 120, "Hello world", false);
    EXPECT_EQ(memcmp(styled.canvas.rgb, plain.canvas.rgb, 400 * 120 * 3), 0);
    EXPECT_EQ(pixel(styled, 0, 119), kBlack);  // a bar, edge to edge
    EXPECT_EQ(pixel(styled, 399, 119), kBlack);
}

TEST_F(TextOutline, TheCaptionFollowsTheStyleOnlyWhenTheCaptionsFollowTheOverlay)
{
    GuardedCanvas bar(400, 120), plain(400, 120), text(400, 120);
    look(TEXT_STYLE_WHITE, 2, false);  // a style is chosen, the captions keep the bar
    caption(bar, "Hello world");
    fill_red(plain);
    image_processor_draw_caption(plain.canvas.rgb, 400, 120, "Hello world", false);
    EXPECT_EQ(memcmp(bar.canvas.rgb, plain.canvas.rgb, 400 * 120 * 3), 0);

    look(TEXT_STYLE_WHITE, 2, true);
    caption(text, "Hello world");
    EXPECT_NE(memcmp(text.canvas.rgb, plain.canvas.rgb, 400 * 120 * 3), 0);
    EXPECT_EQ(pixel(text, 0, 119), kRed);  // no bar
    EXPECT_EQ(pixel(text, 399, 119), kRed);
}

TEST_F(TextOutline, TheErrorBannerKeepsTheBarWhateverStyleIsChosen)
{
    GuardedCanvas c(400, 120);
    look(TEXT_STYLE_BLACK, 2, true);
    fill_red(c);
    image_processor_draw_caption(c.canvas.rgb, 400, 120, "No WiFi", false);  // what it calls
    EXPECT_EQ(pixel(c, 0, 119), kBlack);
    EXPECT_EQ(pixel(c, 399, 119), kBlack);
}

TEST_F(TextOutline, WhiteTextWithABlackBorderAndNothingElse)
{
    GuardedCanvas c(400, 120);
    look(TEXT_STYLE_WHITE, 2, true);
    caption(c, "Hello world");
    Box white = box_of(c, kWhite), black = box_of(c, kBlack), red = box_of(c, kRed);
    EXPECT_GT(white.count, 50u);
    EXPECT_GT(black.count, white.count);  // the border is more than the letters
    EXPECT_EQ(white.count + black.count + red.count,
              400u * 120u);  // only the palette and the canvas
    EXPECT_TRUE(c.guards_intact());
    // nothing above the text block or below it
    int block_top = 120 - (kGlyphH + 2 * kPad);
    EXPECT_GE(black.y0, block_top + kPad - 2);
    EXPECT_LE(black.y1, 119 - kPad + 2);
}

TEST_F(TextOutline, BlackTextWithAWhiteBorderIsTheSameWithTheColoursSwapped)
{
    GuardedCanvas white_style(400, 120), black_style(400, 120);
    look(TEXT_STYLE_WHITE, 2, true);
    caption(white_style, "Hello world");
    look(TEXT_STYLE_BLACK, 2, true);
    caption(black_style, "Hello world");
    EXPECT_EQ(pixels_of(white_style, kWhite), pixels_of(black_style, kBlack));  // the letters
    EXPECT_EQ(pixels_of(white_style, kBlack), pixels_of(black_style, kWhite));  // the borders
    EXPECT_EQ(pixels_of(white_style, kRed), pixels_of(black_style, kRed));
}

TEST_F(TextOutline, TheLettersStandWhereTheBarHasThem)
{
    // the bar's white letters and the white style's letters are the same pixels
    GuardedCanvas bar(400, 120), text(400, 120);
    fill_red(bar);
    image_processor_draw_caption(bar.canvas.rgb, 400, 120, "Fit in the same place", false);
    look(TEXT_STYLE_WHITE, 1, true);
    caption(text, "Fit in the same place");
    EXPECT_EQ(pixels_of(bar, kWhite), pixels_of(text, kWhite));
    EXPECT_GT(pixels_of(text, kWhite).size(), 100u);
}

TEST_F(TextOutline, EveryLetterPixelHasItsBorderAroundIt)
{
    for (int px = 1; px <= 2; px++) {
        for (int style : {TEXT_STYLE_WHITE, TEXT_STYLE_BLACK}) {
            GuardedCanvas c(400, 120);
            look(style, px, true);
            caption(c, "Mm Ag 0123");
            uint32_t ink = style == TEXT_STYLE_WHITE ? kWhite : kBlack;
            int8_t offsets[TEXT_BORDER_MAX_OFFSETS][2];
            int count = text_border_offsets(px, offsets, TEXT_BORDER_MAX_OFFSETS);
            size_t checked = 0;
            for (int y = 3; y < 117; y++) {
                for (int x = 3; x < 397; x++) {
                    if (pixel(c, x, y) != ink) {
                        continue;
                    }
                    checked++;
                    for (int k = 0; k < count; k++) {
                        ASSERT_NE(pixel(c, x + offsets[k][0], y + offsets[k][1]), kRed)
                            << "style " << style << " border " << px << " at " << x << "," << y;
                    }
                }
            }
            EXPECT_GT(checked, 100u);
        }
    }
}

TEST_F(TextOutline, TwoPixelsReachOnePixelFurtherThanOnePixel)
{
    GuardedCanvas one(400, 120), two(400, 120);
    look(TEXT_STYLE_WHITE, 1, true);
    caption(one, "Border");
    look(TEXT_STYLE_WHITE, 2, true);
    caption(two, "Border");
    Box a = box_of(one, kBlack), b = box_of(two, kBlack);
    EXPECT_EQ(b.x0, a.x0 - 1);
    EXPECT_EQ(b.x1, a.x1 + 1);
    EXPECT_EQ(b.y0, a.y0 - 1);
    EXPECT_EQ(b.y1, a.y1 + 1);
    EXPECT_GT(b.count, a.count);
    EXPECT_EQ(pixels_of(one, kWhite), pixels_of(two, kWhite));  // the same letters
}

TEST_F(TextOutline, AWeatherIconGetsTheBorderOfTheText)
{
    // \x01 is the first weather icon (WEATHER_ICON_MARKER_BASE): it is drawn like a letter
    for (int style : {TEXT_STYLE_WHITE, TEXT_STYLE_BLACK}) {
        GuardedCanvas c(400, 120);
        look(style, 2, true);
        caption(c, "\x01 sunny");
        uint32_t ink = style == TEXT_STYLE_WHITE ? kWhite : kBlack;
        uint32_t border = style == TEXT_STYLE_WHITE ? kBlack : kWhite;
        // the icon is the first cell of the line, 24 pixels wide
        int line_x = (400 - (24 + 6 * 17)) / 2;
        int line_y = 120 - (kGlyphH + 2 * kPad) + kPad;
        size_t ink_in_icon = 0, border_around = 0;
        for (int y = line_y - 2; y < line_y + kGlyphH + 2; y++) {
            for (int x = line_x - 2; x < line_x + 24 + 2; x++) {
                ink_in_icon += pixel(c, x, y) == ink ? 1 : 0;
                border_around += pixel(c, x, y) == border ? 1 : 0;
            }
        }
        EXPECT_GT(ink_in_icon, 40u) << style;
        EXPECT_GT(border_around, 60u) << style;  // a ring around the filled icon
        // and no pixel of the icon's ink touches the picture without a border in between
        int8_t offsets[TEXT_BORDER_MAX_OFFSETS][2];
        int count = text_border_offsets(2, offsets, TEXT_BORDER_MAX_OFFSETS);
        for (int y = line_y; y < line_y + kGlyphH; y++) {
            for (int x = line_x; x < line_x + 24; x++) {
                if (pixel(c, x, y) != ink) {
                    continue;
                }
                for (int k = 0; k < count; k++) {
                    ASSERT_NE(pixel(c, x + offsets[k][0], y + offsets[k][1]), kRed)
                        << style << " at " << x << "," << y;
                }
            }
        }
        EXPECT_TRUE(c.guards_intact());
    }
}

TEST_F(TextOutline, ASmallOrFlatPictureStaysInsideItsBuffer)
{
    for (int w : {40, 120, 400}) {
        for (int h : {10, 24, 40}) {
            GuardedCanvas c(w, h);
            look(TEXT_STYLE_WHITE, 2, true);
            caption(c, "A caption that does not fit \x01 anywhere");
            EXPECT_TRUE(c.guards_intact()) << w << "x" << h;
        }
    }
    image_processor_draw_caption_look(nullptr, 400, 120, "x", false);  // no crash
    GuardedCanvas c(400, 120);
    image_processor_draw_caption_look(c.canvas.rgb, 400, 120, "", false);
    image_processor_draw_caption_look(c.canvas.rgb, 400, 120, nullptr, false);
    EXPECT_TRUE(c.guards_intact());
}

TEST_F(TextOutline, TheOverlayLinesFollowTheStyleWithoutTheCaptionSetting)
{
    std::vector<uint8_t> bar, text;
    look(TEXT_STYLE_BAR, 2, false);
    ASSERT_TRUE(overlay_png("text_outline_bar.png", 400, 160, {"Wed sunny 16/24", "A headline"},
                            nullptr, bar));
    EXPECT_EQ(rgb_at(bar, 400, 0, 0), kBlack);  // the bar, edge to edge
    EXPECT_EQ(rgb_at(bar, 400, 399, 0), kBlack);

    look(TEXT_STYLE_WHITE, 2, false);  // the overlay follows the style by itself
    ASSERT_TRUE(overlay_png("text_outline_text.png", 400, 160, {"Wed sunny 16/24", "A headline"},
                            nullptr, text));
    EXPECT_EQ(rgb_at(text, 400, 0, 0), kRed);  // no bar
    EXPECT_EQ(rgb_at(text, 400, 399, 0), kRed);
    EXPECT_EQ(rgb_at(text, 400, 0, 159), kRed);

    // the white letters are where the bar has them
    size_t same = 0, white = 0;
    for (int y = 0; y < 160; y++) {
        for (int x = 0; x < 400; x++) {
            bool a = rgb_at(bar, 400, x, y) == kWhite, b = rgb_at(text, 400, x, y) == kWhite;
            white += b ? 1 : 0;
            same += (a && b) ? 1 : 0;
            ASSERT_EQ(a, b) << x << "," << y;
        }
    }
    EXPECT_GT(white, 100u);
    EXPECT_EQ(same, white);
}

TEST_F(TextOutline, TheCaptionOfAnOverlayIsTheBarUntilTheCaptionsFollow)
{
    std::vector<uint8_t> bar_caption, text_caption;
    look(TEXT_STYLE_WHITE, 2, false);
    ASSERT_TRUE(
        overlay_png("text_outline_cap1.png", 400, 160, {"Line"}, "12 May 2024", bar_caption));
    EXPECT_EQ(rgb_at(bar_caption, 400, 0, 0), kRed);      // the overlay line: text only
    EXPECT_EQ(rgb_at(bar_caption, 400, 0, 159), kBlack);  // the caption: still the bar
    EXPECT_EQ(rgb_at(bar_caption, 400, 399, 159), kBlack);

    look(TEXT_STYLE_WHITE, 2, true);
    ASSERT_TRUE(
        overlay_png("text_outline_cap2.png", 400, 160, {"Line"}, "12 May 2024", text_caption));
    EXPECT_EQ(rgb_at(text_caption, 400, 0, 159), kRed);  // text only
    EXPECT_EQ(rgb_at(text_caption, 400, 399, 159), kRed);
}

TEST_F(TextOutline, ThePlainBarFunctionsNeverChange)
{
    // image_processor_draw_overlay_bar() is the bar whatever is chosen
    std::vector<const char *> lines = {"Wed sunny 16/24"};
    GuardedCanvas a(400, 160), b(400, 160);
    fill_red(a);
    fill_red(b);
    look(TEXT_STYLE_BAR, 2, true);
    image_processor_draw_overlay_bar(a.canvas.rgb, 400, 160, lines.data(), 1, false, 0);
    look(TEXT_STYLE_BLACK, 2, true);
    image_processor_draw_overlay_bar(b.canvas.rgb, 400, 160, lines.data(), 1, false, 0);
    EXPECT_EQ(memcmp(a.canvas.rgb, b.canvas.rgb, 400 * 160 * 3), 0);
    EXPECT_EQ(pixel(a, 0, 0), kBlack);
}
