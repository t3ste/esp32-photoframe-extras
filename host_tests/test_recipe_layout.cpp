// The recipe page (main/recipe_layout.c, recipe_font.c, recipe_qr.c): the fonts, the QR code, the
// layout in both orientations at the sizes of the boards, the picture, and the rules the page keeps
// whatever the recipe: nothing outside the canvas, the text never under the picture or the code,
// every line inside its width, nothing lost when "everything fits". The recipes are invented.

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "recipe_font.h"
#include "recipe_layout.h"
#include "recipe_qr.h"
#include "recipe_source.h"
#include "recipe_text.h"
}

namespace
{

std::string fixture(const std::string &name)
{
    std::ifstream file(std::string(RECIPE_TEST_DATA_DIR) + "/" + name, std::ios::binary);
    EXPECT_TRUE(file.good()) << name;
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

recipe_t sample()
{
    recipe_t recipe;
    EXPECT_TRUE(
        recipe_parse_chefkoch_recipe(fixture("chefkoch-recipe.json").c_str(), "Chefkoch", &recipe));
    return recipe;
}

struct Panel {
    int w, h;
    bool landscape;
};

// the panels of the boards, in the orientations a user can choose
const Panel kPanels[] = {
    {800, 480, true},   {480, 800, false},   {960, 540, true},   {540, 960, false},
    {1872, 1404, true}, {1404, 1872, false}, {1600, 1200, true}, {1200, 1600, false},
};

struct Box {
    int x0, y0, x1, y1;
};

bool overlaps(const Box &a, const Box &b)
{
    return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
}

// The ink box of a line: its cell, from the top of the line to the bottom of the descenders.
Box line_box(const recipe_layout_t &l, const recipe_line_t &line, const recipe_font_t *font,
             const char *text)
{
    int w = recipe_font_text_width(font, text + line.start, line.len, l.scale);
    if (line.flags & RECIPE_LINE_ELLIPSIS) {
        w += recipe_font_char_width(font, 0x85) * l.scale;
    }
    return {line.x, line.y, line.x + w, line.y + (font->ascent + font->descent) * l.scale};
}

std::vector<Box> text_boxes(const recipe_t &recipe, const recipe_layout_t &l, bool bodies,
                            bool ingredients)
{
    std::vector<Box> boxes;
    if (bodies) {
        for (int i = 0; i < l.body_count; i++) {
            boxes.push_back(line_box(l, l.body[i], l.body_font, recipe.text));
        }
    }
    if (ingredients) {
        for (int i = 0; i < l.ing_count; i++) {
            boxes.push_back(
                line_box(l, l.ing[i], l.ing_font, recipe.ingredients[l.ing[i].ingredient]));
        }
    }
    return boxes;
}

recipe_layout_input_t input(bool image, bool photo, const recipe_qr_t *qr, unsigned warnings = 0)
{
    recipe_layout_input_t in = {};
    in.show_image = image;
    in.have_photo = photo;
    in.qr = qr;
    in.warnings = warnings;
    return in;
}

recipe_qr_t make_qr(const char *text = "https://www.chefkoch.de/rezepte/35591011885735/")
{
    recipe_qr_t qr;
    EXPECT_TRUE(recipe_qr_encode(text, &qr));
    return qr;
}

// ---- random recipes ----

const char *kWords[] = {"Zwiebel",
                        "Kartoffeln",
                        "Gem\xFC"
                        "se",
                        "k\xF6"
                        "cheln",
                        "\xD6"
                        "l",
                        "R\xFChren",
                        "Br\xFChe",
                        "ca.",
                        "Min.",
                        "Salz",
                        "Pfeffer",
                        "fein",
                        "hacken",
                        "Backofen",
                        "200",
                        "\xB0"
                        "C",
                        "Gem\xFC"
                        "sebr\xFChe",
                        "Kr\xE4uterquark",
                        "mit",
                        "und",
                        "den",
                        "die",
                        "das",
                        "Stunde",
                        "\xBD",
                        "Bund",
                        "frisch",
                        "1/2",
                        "Chilifl\xF6"
                        "cken",
                        "gebratene",
                        "Kichererbsen",
                        "abschmecken."};

std::string words(std::mt19937 &rng, int count)
{
    std::string text;
    for (int i = 0; i < count; i++) {
        if (i) {
            text += ' ';
        }
        text += kWords[rng() % (sizeof(kWords) / sizeof(kWords[0]))];
    }
    return text;
}

recipe_t random_recipe(std::mt19937 &rng)
{
    recipe_t recipe;
    memset(&recipe, 0, sizeof(recipe));
    recipe.german = rng() % 3 != 0;
    snprintf(recipe.title, sizeof(recipe.title), "%s", words(rng, 1 + rng() % 14).c_str());
    if (rng() % 4) {
        snprintf(recipe.category, sizeof(recipe.category), "%s", words(rng, 1 + rng() % 2).c_str());
    }
    if (rng() % 3) {
        recipe_text_format_minutes(10 + (int) (rng() % 200), recipe.german, recipe.time,
                                   sizeof(recipe.time));
    }
    snprintf(recipe.source, sizeof(recipe.source), "Chefkoch - Rezept des Tages");
    int ingredients = (int) (rng() % 28);
    for (int i = 0; i < ingredients; i++) {
        snprintf(recipe.ingredients[i], RECIPE_INGREDIENT_LEN, "%s (%d g)",
                 words(rng, 1 + rng() % 5).c_str(), 1 + (int) (rng() % 900));
    }
    recipe.ingredient_count = ingredients;
    std::string text;
    int paragraphs = (int) (rng() % 12);
    for (int i = 0; i < paragraphs; i++) {
        if (i) {
            text += '\n';
        }
        text += words(rng, 3 + rng() % 60);
    }
    if (rng() % 15 == 0) {
        text += "\nSuperkalifragilistischexpialigetisch" +
                std::string(70, 'x');  // a word wider than any line
    }
    snprintf(recipe.text, sizeof(recipe.text), "%s", text.c_str());
    return recipe;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// The fonts
// ---------------------------------------------------------------------------------------------

TEST(RecipeFont, EverySizeTheLayoutAsksForIsThere)
{
    for (int px : {12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 24, 30}) {
        EXPECT_NE(recipe_font_find(px, false), nullptr) << px;
    }
    EXPECT_NE(recipe_font_find(12, true), nullptr);
    EXPECT_NE(recipe_font_find(20, true), nullptr);
    EXPECT_EQ(recipe_font_find(11, false), nullptr);
    EXPECT_EQ(recipe_font_find(14, true), nullptr);
}

TEST(RecipeFont, TheFontsAreOrderedBySizeAndKnowTheirHeight)
{
    int previous = 0;
    for (int i = 0; i < RECIPE_FONT_COUNT; i++) {
        const recipe_font_t &font = RECIPE_FONTS[i];
        EXPECT_GT(font.ascent, 0);
        EXPECT_GT(font.descent, 0);
        EXPECT_GE(recipe_font_line_height(&font), font.px);
        if (!font.bold) {
            EXPECT_GT(font.px, previous) << i;
            previous = font.px;
        }
    }
}

TEST(RecipeFont, TheGermanLettersAndTheSignsOfRecipesHaveGlyphs)
{
    const recipe_font_t *font = recipe_font_find(16, false);
    ASSERT_NE(font, nullptr);
    for (unsigned char c :
         {0xE4, 0xF6, 0xFC, 0xC4, 0xD6, 0xDC, 0xDF, 0xE9, 0xE8, 0xE0, 0xB0, 0xBD,
          0xBC, 0xBE, 0x80, 0x84, 0x93, 0x94, 0x92, 0x96, 0x97, 0x85, 0x95, 0xD7}) {
        const recipe_glyph_t &glyph = font->glyphs[c - RECIPE_FONT_FIRST];
        EXPECT_GT(glyph.advance, 0) << std::hex << (int) c;
        EXPECT_GT(glyph.width, 0) << std::hex << (int) c;
        EXPECT_GT(glyph.height, 0) << std::hex << (int) c;
    }
    EXPECT_GT(recipe_font_char_width(font, ' '), 0);
}

TEST(RecipeFont, UnassignedCodesAndControlsAreEmpty)
{
    const recipe_font_t *font = recipe_font_find(16, false);
    for (unsigned char c : {0x81, 0x8D, 0x8F, 0x90, 0x9D}) {
        EXPECT_EQ(recipe_font_char_width(font, c), 0) << std::hex << (int) c;
    }
    EXPECT_EQ(recipe_font_char_width(font, 0x07), 0);
    EXPECT_EQ(recipe_font_char_width(font, 0x00), 0);
    EXPECT_EQ(recipe_font_char_width(nullptr, 'a'), 0);
}

TEST(RecipeFont, WidthIsTheSumOfTheAdvancesTimesTheScale)
{
    const recipe_font_t *font = recipe_font_find(15, false);
    const char *text =
        "Gem\xFC"
        "se, 3 Zwiebeln";
    int sum = 0;
    for (const char *p = text; *p; p++) {
        sum += recipe_font_char_width(font, (unsigned char) *p);
    }
    EXPECT_EQ(recipe_font_text_width(font, text, strlen(text), 1), sum);
    EXPECT_EQ(recipe_font_text_width(font, text, strlen(text), 3), sum * 3);
    EXPECT_EQ(recipe_font_text_width(font, text, 0, 2), 0);
    EXPECT_EQ(recipe_font_text_width(font, nullptr, 5, 2), 0);
    EXPECT_EQ(recipe_font_text_width(font, text, strlen(text), 0), sum);  // a scale under 1 is 1
}

TEST(RecipeFont, BoldIsWiderThanRegular)
{
    const recipe_font_t *regular = recipe_font_find(12, false);
    const recipe_font_t *bold = recipe_font_find(12, true);
    EXPECT_GT(recipe_font_text_width(bold, "ZUTATEN", 7, 1),
              recipe_font_text_width(regular, "ZUTATEN", 7, 1));
}

TEST(RecipeFont, DrawingSetsPixelsInsideTheLineAndClipsAtTheEdges)
{
    const recipe_font_t *font = recipe_font_find(20, false);
    std::vector<uint8_t> rgb(200 * 60 * 3, 255);
    canvas_t canvas = {rgb.data(), 200, 60};
    int end = recipe_font_draw(&canvas, font, 1, 10, 10, "Hallo", 5, CANVAS_BLACK);
    EXPECT_EQ(end, 10 + recipe_font_text_width(font, "Hallo", 5, 1));
    int dark = 0;
    for (int y = 0; y < 60; y++) {
        for (int x = 0; x < 200; x++) {
            if (rgb[((size_t) y * 200 + x) * 3] == 0) {
                dark++;
                EXPECT_GE(y, 10);
                EXPECT_LT(y, 10 + (font->ascent + font->descent));
                EXPECT_GE(x, 10);
                EXPECT_LT(x, end + 2);
            }
        }
    }
    EXPECT_GT(dark, 100);
    // far outside the canvas: nothing happens, nothing breaks
    recipe_font_draw(&canvas, font, 2, -500, -500, "Hallo", 5, CANVAS_BLACK);
    recipe_font_draw(&canvas, font, 2, 5000, 5000, "Hallo", 5, CANVAS_BLACK);
    recipe_font_draw(&canvas, font, 1, -15, 50, "Hallo", 5,
                     CANVAS_BLACK);  // cut at the left and the bottom
}

TEST(RecipeFont, TheScaleRepeatsEveryPixel)
{
    const recipe_font_t *font = recipe_font_find(12, false);
    std::vector<uint8_t> one(40 * 30 * 3, 255), two(80 * 60 * 3, 255);
    canvas_t c1 = {one.data(), 40, 30}, c2 = {two.data(), 80, 60};
    recipe_font_draw(&c1, font, 1, 4, 3, "Ag", 2, CANVAS_BLACK);
    recipe_font_draw(&c2, font, 2, 8, 6, "Ag", 2, CANVAS_BLACK);
    for (int y = 0; y < 60; y++) {
        for (int x = 0; x < 80; x++) {
            EXPECT_EQ(two[((size_t) y * 80 + x) * 3], one[((size_t) (y / 2) * 40 + x / 2) * 3])
                << x << "," << y;
        }
    }
}

// ---------------------------------------------------------------------------------------------
// The QR code
// ---------------------------------------------------------------------------------------------

TEST(RecipeQr, AShortAddressIsASmallCode)
{
    recipe_qr_t qr;
    ASSERT_TRUE(recipe_qr_encode("https://www.chefkoch.de/rezepte/35591011885735/", &qr));
    EXPECT_EQ(qr.size, 29);  // version 3: 47 bytes at the lowest error correction
    ASSERT_TRUE(recipe_qr_encode("https://www.themealdb.com/meal/52772", &qr));
    EXPECT_LE(qr.size, 29);
    EXPECT_GE(qr.size, 25);
}

TEST(RecipeQr, TheFinderPatternsAreThere)
{
    recipe_qr_t qr = make_qr();
    for (int i = 0; i < 7; i++) {
        EXPECT_TRUE(recipe_qr_module(&qr, i, 0));
        EXPECT_TRUE(recipe_qr_module(&qr, 0, i));
        EXPECT_TRUE(recipe_qr_module(&qr, qr.size - 1 - i, 0));
        EXPECT_TRUE(recipe_qr_module(&qr, 0, qr.size - 1 - i));
    }
    EXPECT_FALSE(recipe_qr_module(&qr, 1, 1));  // the white ring inside a finder pattern
    EXPECT_TRUE(recipe_qr_module(&qr, 3, 3));   // its centre
}

TEST(RecipeQr, OutsideTheCodeIsWhite)
{
    recipe_qr_t qr = make_qr();
    EXPECT_FALSE(recipe_qr_module(&qr, -1, 0));
    EXPECT_FALSE(recipe_qr_module(&qr, 0, -1));
    EXPECT_FALSE(recipe_qr_module(&qr, qr.size, 0));
    EXPECT_FALSE(recipe_qr_module(&qr, 0, qr.size));
    EXPECT_FALSE(recipe_qr_module(nullptr, 0, 0));
}

TEST(RecipeQr, NothingToEncodeOrTooMuchGivesNoCode)
{
    recipe_qr_t qr;
    EXPECT_FALSE(recipe_qr_encode("", &qr));
    EXPECT_EQ(qr.size, 0);
    EXPECT_FALSE(recipe_qr_encode(nullptr, &qr));
    EXPECT_FALSE(recipe_qr_encode("x", nullptr));
    std::string too_long(300, 'a');
    EXPECT_FALSE(recipe_qr_encode(too_long.c_str(), &qr));
    EXPECT_EQ(qr.size, 0);
    // the largest version holds 134 bytes at the lowest error correction
    std::string fits(120, 'b');
    EXPECT_TRUE(recipe_qr_encode(fits.c_str(), &qr));
    EXPECT_LE(qr.size, RECIPE_QR_MAX_SIZE);
}

TEST(RecipeQr, TheSameTextGivesTheSameCode)
{
    recipe_qr_t a = make_qr("https://example.invalid/a"), b = make_qr("https://example.invalid/a"),
                c = make_qr("https://example.invalid/b");
    EXPECT_EQ(a.size, b.size);
    EXPECT_EQ(memcmp(a.bits, b.bits, sizeof(a.bits)), 0);
    EXPECT_NE(memcmp(a.bits, c.bits, sizeof(a.bits)), 0);
}

// ---------------------------------------------------------------------------------------------
// The layout
// ---------------------------------------------------------------------------------------------

TEST(RecipeLayout, ARecipeFitsInTheBiggestSizeThatHoldsIt)
{
    recipe_t recipe = sample();
    recipe_layout_t *l = new recipe_layout_t;
    ASSERT_TRUE(recipe_layout_build(
        &recipe, &(const recipe_layout_input_t &) input(true, true, nullptr), 800, 480, true, l));
    EXPECT_FALSE(l->cut);
    EXPECT_GE(l->ing_px, 12);
    EXPECT_GE(l->body_px, 12);
    EXPECT_EQ(l->scale, 1);
    EXPECT_EQ(l->ing_count >= recipe.ingredient_count, true);
    delete l;
}

TEST(RecipeLayout, MoreTextNeverMakesTheFontBigger)
{
    recipe_t recipe = sample();
    int previous = 1000;
    std::string base = recipe.text;
    for (int copies = 1; copies <= 8; copies++) {
        std::string text;
        for (int i = 0; i < copies; i++) {
            text += (i ? "\n" : "") + base;
        }
        snprintf(recipe.text, sizeof(recipe.text), "%s", text.c_str());
        recipe_layout_t *l = new recipe_layout_t;
        recipe_layout_input_t in = input(true, true, nullptr);
        recipe_layout_build(&recipe, &in, 800, 480, true, l);
        EXPECT_LE(l->body_px, previous) << copies;
        previous = l->body_px;
        delete l;
    }
    EXPECT_EQ(previous, 12);  // eight copies are more than fits even in the smallest size
}

TEST(RecipeLayout, WhatDoesNotFitIsCutWithAnEllipsisAndSaid)
{
    recipe_t recipe;
    ASSERT_TRUE(recipe_parse_chefkoch_recipe(fixture("chefkoch-recipe-long.json").c_str(),
                                             "Chefkoch", &recipe));
    recipe_layout_t *l = new recipe_layout_t;
    recipe_layout_input_t in = input(true, true, nullptr);
    EXPECT_FALSE(recipe_layout_build(&recipe, &in, 800, 480, true, l));
    EXPECT_TRUE(l->cut);
    EXPECT_TRUE(l->body_cut);
    EXPECT_EQ(l->body_px, 12);  // the smallest size was used before anything was cut
    ASSERT_GT(l->body_count, 0);
    EXPECT_TRUE(l->body[l->body_count - 1].flags & RECIPE_LINE_ELLIPSIS);
    ASSERT_GE(l->warn_count, 1);
    EXPECT_NE(std::string(l->warn[0]).find("gek"),
              std::string::npos);  // "Text gekuerzt", first of the warnings
    delete l;
}

TEST(RecipeLayout, TextTheSourceCutIsShownAsCutEvenIfItFits)
{
    recipe_t recipe = sample();
    recipe.text_cut = true;
    recipe_layout_t *l = new recipe_layout_t;
    recipe_layout_input_t in = input(true, true, nullptr);
    EXPECT_FALSE(recipe_layout_build(&recipe, &in, 800, 480, true, l));
    EXPECT_TRUE(l->body_cut);
    EXPECT_TRUE(l->body[l->body_count - 1].flags & RECIPE_LINE_ELLIPSIS);
    delete l;
}

TEST(RecipeLayout, TheWarningsAreInTheCornerTwoAtMostWithTheCutOneFirst)
{
    recipe_t recipe = sample();
    recipe_layout_t *l = new recipe_layout_t;
    recipe_layout_input_t in = input(
        true, true, nullptr, RECIPE_WARN_RELAXED | RECIPE_WARN_NO_IMAGE | RECIPE_WARN_LAST_RECIPE);
    ASSERT_TRUE(recipe_layout_build(&recipe, &in, 800, 480, true, l));
    ASSERT_EQ(l->warn_count, 2);
    EXPECT_STREQ(l->warn[0], "! Letztes Rezept");
    EXPECT_STREQ(l->warn[1], "! Filter gelockert");
    recipe.german = false;
    ASSERT_TRUE(recipe_layout_build(&recipe, &in, 800, 480, true, l));
    EXPECT_STREQ(l->warn[0], "! Last recipe");
    EXPECT_STREQ(l->warn[1], "! Filters relaxed");
    recipe = sample();
    recipe.text_cut = true;
    recipe_layout_build(&recipe, &in, 800, 480, true, l);
    ASSERT_EQ(l->warn_count, 2);
    EXPECT_EQ(std::string(l->warn[0]), "! Text gek\xFCrzt");
    EXPECT_STREQ(l->warn[1], "! Letztes Rezept");
    delete l;
}

TEST(RecipeLayout, TheLabelsFollowTheLanguage)
{
    recipe_t recipe = sample();
    recipe_layout_t *l = new recipe_layout_t;
    recipe_layout_input_t in = input(true, true, nullptr);
    recipe_layout_build(&recipe, &in, 800, 480, true, l);
    EXPECT_STREQ(l->ing_label, "ZUTATEN");
    EXPECT_STREQ(l->body_label, "ZUBEREITUNG");
    EXPECT_EQ(std::string(l->source).rfind("Quelle: ", 0), 0u);
    recipe.german = false;
    recipe_layout_build(&recipe, &in, 800, 480, true, l);
    EXPECT_STREQ(l->ing_label, "INGREDIENTS");
    EXPECT_STREQ(l->body_label, "PREPARATION");
    EXPECT_EQ(std::string(l->source).rfind("Source: ", 0), 0u);
    delete l;
}

TEST(RecipeLayout, TheMetaLineHasCategoryAndTimeOrAWordForNone)
{
    recipe_t recipe = sample();
    recipe_layout_t *l = new recipe_layout_t;
    recipe_layout_input_t in = input(true, true, nullptr);
    recipe_layout_build(&recipe, &in, 800, 480, true, l);
    EXPECT_STREQ(l->meta, "Suppen | 35 Min.");
    recipe.time[0] = '\0';
    recipe_layout_build(&recipe, &in, 800, 480, true, l);
    EXPECT_STREQ(l->meta, "Suppen");
    recipe.category[0] = '\0';
    recipe_layout_build(&recipe, &in, 800, 480, true, l);
    EXPECT_STREQ(l->meta, "Rezept");
    recipe.german = false;
    recipe_layout_build(&recipe, &in, 800, 480, true, l);
    EXPECT_STREQ(l->meta, "Recipe");
    delete l;
}

TEST(RecipeLayout, ALongTitleTakesTwoLinesAndAVeryLongOneEndsWithAnEllipsis)
{
    recipe_t recipe = sample();
    recipe_layout_t *l = new recipe_layout_t;
    recipe_layout_input_t in = input(true, true, nullptr);
    snprintf(recipe.title, sizeof(recipe.title), "Kurz");
    recipe_layout_build(&recipe, &in, 800, 480, true, l);
    EXPECT_EQ(l->title_count, 1);
    EXPECT_EQ(l->title_font->px, 30);
    snprintf(recipe.title, sizeof(recipe.title), "%s",
             "Gebratene Nudeln mit Gem\xFC"
             "se, Sojasauce und knusprigem Tofu aus dem Ofen");
    recipe_layout_build(&recipe, &in, 800, 480, true, l);
    EXPECT_GE(l->title_count, 1);
    EXPECT_LE(l->title_count, 2);
    std::string many;
    for (int i = 0; i < 12; i++) {
        many += "Sehr lange Titelwoerter ";
    }
    snprintf(recipe.title, sizeof(recipe.title), "%s", many.c_str());  // as long as a title can be
    recipe_layout_build(&recipe, &in, 800, 480, true, l);
    EXPECT_EQ(l->title_count, 2);
    EXPECT_EQ(l->title_font->px, 19);
    EXPECT_FALSE(l->title[1].flags & RECIPE_LINE_ELLIPSIS);  // two wide lines hold it
    recipe_layout_build(&recipe, &in, 480, 800, false, l);   // two narrow ones do not
    EXPECT_EQ(l->title_count, 2);
    EXPECT_TRUE(l->title[1].flags & RECIPE_LINE_ELLIPSIS);
    EXPECT_LE(l->title[1].len, strlen(recipe.title));
    delete l;
}

TEST(RecipeLayout, InLandscapeThePreparationFlowsBesideAndBelowThePicture)
{
    recipe_t recipe = sample();
    // enough text to run past the picture
    std::string text = recipe.text;
    text += "\n" + text;
    snprintf(recipe.text, sizeof(recipe.text), "%s", text.c_str());
    recipe_layout_t *l = new recipe_layout_t;
    recipe_layout_input_t in = input(true, true, nullptr);
    recipe_layout_build(&recipe, &in, 800, 480, true, l);
    ASSERT_GT(l->image.w, 0);
    int beside = 0, below = 0;
    int narrow = 0, wide = 0;
    for (int i = 0; i < l->body_count; i++) {
        Box box = line_box(*l, l->body[i], l->body_font, recipe.text);
        if (l->body[i].y < l->image.y + l->image.h) {
            beside++;
            narrow = std::max(narrow, box.x1 - box.x0);
        } else {
            below++;
            wide = std::max(wide, box.x1 - box.x0);
        }
    }
    EXPECT_GT(beside, 2);
    EXPECT_GT(below, 2);
    EXPECT_GT(wide, narrow);  // the lines below are longer than the ones beside the picture
    EXPECT_LT(l->ing_head_x, l->body_head_x);
    EXPECT_EQ(l->ing_head_y, l->body_head_y);  // the two headings are on one line
    EXPECT_EQ(l->image.y, l->ing_head_y);      // and the picture is at their height
    delete l;
}

TEST(RecipeLayout, InPortraitThePreparationStartsBelowTheIngredientsAndThePicture)
{
    recipe_t recipe = sample();
    recipe_layout_t *l = new recipe_layout_t;
    recipe_layout_input_t in = input(true, true, nullptr);
    ASSERT_TRUE(recipe_layout_build(&recipe, &in, 480, 800, false, l));
    EXPECT_GT(l->body_head_y, l->image.y + l->image.h);
    int last_ing = 0;
    for (int i = 0; i < l->ing_count; i++) {
        last_ing = std::max(last_ing, (int) l->ing[i].y + l->ing_line_h);
    }
    EXPECT_GE(l->body_head_y, last_ing);
    EXPECT_EQ(l->body_head_x, l->ing_head_x);
    EXPECT_GT(l->image.x, l->ing_head_x);
    delete l;
}

TEST(RecipeLayout, WithoutThePictureTheTextTakesItsRoom)
{
    recipe_t recipe = sample();
    recipe_layout_t *with = new recipe_layout_t, *without = new recipe_layout_t;
    recipe_layout_input_t a = input(true, true, nullptr), b = input(false, false, nullptr);
    recipe_layout_build(&recipe, &a, 800, 480, true, with);
    recipe_layout_build(&recipe, &b, 800, 480, true, without);
    EXPECT_EQ(without->image.w, 0);
    EXPECT_LE(without->body_count, with->body_count);
    EXPECT_GE(without->body_px, with->body_px);
    delete with;
    delete without;
}

TEST(RecipeLayout, AMissingPictureLeavesANoteInItsBox)
{
    recipe_t recipe = sample();
    recipe_layout_t *l = new recipe_layout_t;
    recipe_layout_input_t in = input(true, false, nullptr);
    recipe_layout_build(&recipe, &in, 800, 480, true, l);
    EXPECT_GT(l->image.w, 0);
    EXPECT_TRUE(l->image_placeholder);
    delete l;
}

TEST(RecipeLayout, TheCodeIsInTheCornerAndAWholeNumberOfPixelsPerModule)
{
    recipe_t recipe = sample();
    recipe_qr_t qr = make_qr();
    recipe_layout_t *l = new recipe_layout_t;
    recipe_layout_input_t in = input(true, true, &qr);
    recipe_layout_build(&recipe, &in, 800, 480, true, l);
    EXPECT_EQ(l->qr_module_px, 2);  // 29 modules: the small code that was tried on a panel
    EXPECT_EQ(l->qr.w, l->qr.h);
    EXPECT_EQ(l->qr.w, 2 * (qr.size + 2));
    EXPECT_EQ(l->qr.x + l->qr.w, 800 - 18);
    EXPECT_LE(l->qr.y + l->qr.h, 480);
    recipe_layout_build(&recipe, &in, 1872, 1404, true, l);
    EXPECT_GE(l->qr_module_px, 4);
    delete l;
}

TEST(RecipeLayout, AnEmptyRecipeIsLaidOutWithoutTrouble)
{
    recipe_t recipe;
    memset(&recipe, 0, sizeof(recipe));
    recipe_layout_t *l = new recipe_layout_t;
    recipe_layout_input_t in = input(true, false, nullptr);
    EXPECT_TRUE(recipe_layout_build(&recipe, &in, 800, 480, true, l));
    EXPECT_EQ(l->ing_count, 0);
    EXPECT_EQ(l->body_count, 0);
    EXPECT_FALSE(recipe_layout_build(nullptr, &in, 800, 480, true, l));
    EXPECT_FALSE(recipe_layout_build(&recipe, nullptr, 800, 480, true, l));
    EXPECT_FALSE(recipe_layout_build(&recipe, &in, 50, 50, true, l));
    delete l;
}

TEST(RecipeLayout, TheFontScaleFollowsThePanel)
{
    recipe_t recipe = sample();
    recipe_layout_t *l = new recipe_layout_t;
    recipe_layout_input_t in = input(true, true, nullptr);
    recipe_layout_build(&recipe, &in, 800, 480, true, l);
    EXPECT_EQ(l->scale, 1);
    recipe_layout_build(&recipe, &in, 960, 540, true, l);
    EXPECT_EQ(l->scale, 1);
    recipe_layout_build(&recipe, &in, 1872, 1404, true, l);
    EXPECT_EQ(l->scale, 2);
    recipe_layout_build(&recipe, &in, 1200, 1600, false, l);
    EXPECT_EQ(l->scale, 2);
    delete l;
}

TEST(RecipeLayout, TheBracketsOfAnAmountStayTogetherWhenTheyCan)
{
    recipe_t recipe;
    memset(&recipe, 0, sizeof(recipe));
    recipe.german = true;
    snprintf(recipe.title, sizeof(recipe.title), "Test");
    snprintf(recipe.ingredients[0], RECIPE_INGREDIENT_LEN,
             "Gem\xFC"
             "sebr\xFChe mit Kr\xE4utern (750 ml)");
    recipe.ingredient_count = 1;
    snprintf(recipe.text, sizeof(recipe.text), "Kochen.");
    recipe_layout_t *l = new recipe_layout_t;
    recipe_layout_input_t in = input(false, false, nullptr);
    recipe_layout_build(&recipe, &in, 480, 800, false, l);
    for (int i = 0; i < l->ing_count; i++) {
        std::string line(recipe.ingredients[0] + l->ing[i].start, l->ing[i].len);
        int open = 0;
        for (char c : line) {
            open += c == '(';
            open -= c == ')';
        }
        EXPECT_EQ(open, 0) << "'" << line << "' splits the bracket";
    }
    delete l;
}

// ---------------------------------------------------------------------------------------------
// The rules the page keeps for any recipe
// ---------------------------------------------------------------------------------------------

TEST(RecipeLayoutRules, NothingLeavesTheCanvasAndTheTextNeverTouchesThePictureOrTheCode)
{
    std::mt19937 rng(12345);
    recipe_qr_t qr = make_qr();
    int built = 0;
    for (int round = 0; round < 400; round++) {
        recipe_t recipe = random_recipe(rng);
        const Panel &panel = kPanels[rng() % (sizeof(kPanels) / sizeof(kPanels[0]))];
        bool image = rng() % 4 != 0;
        recipe_layout_input_t in =
            input(image, rng() % 3 != 0, rng() % 2 ? &qr : nullptr, rng() % 16);
        recipe_layout_t *l = new recipe_layout_t;
        bool fits = recipe_layout_build(&recipe, &in, panel.w, panel.h, panel.landscape, l);
        EXPECT_EQ(fits, !l->cut);
        std::vector<Box> texts = text_boxes(recipe, *l, true, true);
        for (size_t i = 0; i < texts.size(); i++) {
            const Box &b = texts[i];
            ASSERT_GE(b.x0, 0) << "round " << round;
            ASSERT_GE(b.y0, 0) << "round " << round;
            ASSERT_LE(b.x1, panel.w) << "round " << round << " " << panel.w << "x" << panel.h;
            ASSERT_LE(b.y1, panel.h + 8)
                << "round " << round;  // the cell of the last line may reach into the margin
            if (l->image.w > 0) {
                Box image_box = {l->image.x, l->image.y, l->image.x + l->image.w,
                                 l->image.y + l->image.h};
                ASSERT_FALSE(overlaps(b, image_box)) << "text under the picture, round " << round;
            }
            if (l->qr.w > 0) {
                Box qr_box = {l->qr.x, l->qr.y, l->qr.x + l->qr.w, l->qr.y + l->qr.h};
                ASSERT_FALSE(overlaps(b, qr_box)) << "text over the code, round " << round;
            }
        }
        if (l->image.w > 0) {
            ASSERT_GE(l->image.x, 0);
            ASSERT_LE(l->image.x + l->image.w, panel.w);
            ASSERT_LE(l->image.y + l->image.h, panel.h);
        }
        if (l->qr.w > 0) {
            ASSERT_GE(l->qr.x, 0);
            ASSERT_LE(l->qr.x + l->qr.w, panel.w);
            ASSERT_LE(l->qr.y + l->qr.h, panel.h);
            if (l->image.w > 0) {
                Box a = {l->image.x, l->image.y, l->image.x + l->image.w, l->image.y + l->image.h};
                Box c = {l->qr.x, l->qr.y, l->qr.x + l->qr.w, l->qr.y + l->qr.h};
                ASSERT_FALSE(overlaps(a, c)) << "the picture and the code overlap, round " << round;
            }
        }
        delete l;
        built++;
    }
    EXPECT_EQ(built, 400);
}

TEST(RecipeLayoutRules, EveryLineIsInsideItsWidthAndTheLinesDoNotCollide)
{
    std::mt19937 rng(777);
    for (int round = 0; round < 300; round++) {
        recipe_t recipe = random_recipe(rng);
        const Panel &panel = kPanels[rng() % (sizeof(kPanels) / sizeof(kPanels[0]))];
        recipe_layout_input_t in = input(rng() % 4 != 0, true, nullptr);
        recipe_layout_t *l = new recipe_layout_t;
        recipe_layout_build(&recipe, &in, panel.w, panel.h, panel.landscape, l);
        for (int i = 0; i < l->body_count; i++) {
            Box b = line_box(*l, l->body[i], l->body_font, recipe.text);
            // a word wider than the line is cut between characters: it still fits
            ASSERT_LE(b.x1 - b.x0, l->body[i].width) << "round " << round << " line " << i;
            if (i > 0) {
                ASSERT_GE(l->body[i].y, l->body[i - 1].y + l->body_line_h) << "round " << round;
            }
        }
        for (int i = 0; i < l->ing_count; i++) {
            Box b = line_box(*l, l->ing[i], l->ing_font, recipe.ingredients[l->ing[i].ingredient]);
            ASSERT_LE(b.x1 - b.x0, l->ing[i].width) << "round " << round;
            if (i > 0) {
                ASSERT_GE(l->ing[i].y, l->ing[i - 1].y + l->ing_line_h) << "round " << round;
            }
        }
        delete l;
    }
}

// The lines of each paragraph, joined by spaces, are the paragraph: nothing lost, nothing doubled.
TEST(RecipeLayoutRules, WhenEverythingFitsNoWordIsMissing)
{
    std::mt19937 rng(4242);
    int fitted = 0;
    for (int round = 0; round < 300; round++) {
        recipe_t recipe = random_recipe(rng);
        // no word wider than a line here: those are cut between characters on purpose
        if (strstr(recipe.text, "Superkali")) {
            continue;
        }
        const Panel &panel = kPanels[rng() % (sizeof(kPanels) / sizeof(kPanels[0]))];
        recipe_layout_input_t in = input(rng() % 4 != 0, true, nullptr);
        recipe_layout_t *l = new recipe_layout_t;
        if (recipe_layout_build(&recipe, &in, panel.w, panel.h, panel.landscape, l)) {
            fitted++;
            std::string rebuilt;
            for (int i = 0; i < l->body_count; i++) {
                if (i) {
                    // lines of one paragraph are joined by a space, paragraphs by a line break
                    size_t prev_end = l->body[i - 1].start + l->body[i - 1].len;
                    rebuilt += (recipe.text[prev_end] == '\n') ? "\n" : " ";
                }
                rebuilt.append(recipe.text + l->body[i].start, l->body[i].len);
            }
            std::string expected = recipe.text;
            // the text as the layout sees it: single spaces
            std::string squeezed;
            for (char c : expected) {
                if (!(c == ' ' && !squeezed.empty() && squeezed.back() == ' ')) {
                    squeezed += c;
                }
            }
            ASSERT_EQ(rebuilt, squeezed) << "round " << round;
            // every ingredient has its lines, in order, the first with the bullet
            int items = 0;
            for (int i = 0; i < l->ing_count; i++) {
                items += (l->ing[i].flags & RECIPE_LINE_BULLET) ? 1 : 0;
            }
            int expected_items = 0;
            for (int i = 0; i < recipe.ingredient_count; i++) {
                expected_items += recipe.ingredients[i][0] ? 1 : 0;
            }
            ASSERT_EQ(items, expected_items) << "round " << round;
        }
        delete l;
    }
    EXPECT_GT(fitted, 50);
}

TEST(RecipeLayoutRules, WhenSomethingIsCutTheLastLineEndsWithAnEllipsisThatFits)
{
    std::mt19937 rng(99);
    int cut = 0;
    for (int round = 0; round < 300; round++) {
        recipe_t recipe = random_recipe(rng);
        const Panel &panel = kPanels[rng() % 4];  // the smaller panels
        recipe_layout_input_t in = input(true, true, nullptr);
        recipe_layout_t *l = new recipe_layout_t;
        if (!recipe_layout_build(&recipe, &in, panel.w, panel.h, panel.landscape, l)) {
            cut++;
            EXPECT_TRUE(l->cut);
            if (l->body_cut && l->body_count > 0) {
                const recipe_line_t &last = l->body[l->body_count - 1];
                ASSERT_TRUE(last.flags & RECIPE_LINE_ELLIPSIS) << round;
                Box b = line_box(*l, last, l->body_font, recipe.text);
                ASSERT_LE(b.x1 - b.x0, last.width) << "the ellipsis does not fit, round " << round;
            }
            if (l->ing_cut && l->ing_count > 0) {
                const recipe_line_t &last = l->ing[l->ing_count - 1];
                ASSERT_TRUE(last.flags & RECIPE_LINE_ELLIPSIS) << round;
            }
            // the smallest size was tried before anything was cut
            EXPECT_EQ(l->body_px, 12 * l->scale);
            EXPECT_EQ(l->ing_px, 12 * l->scale);
            EXPECT_GE(l->warn_count, 1);
        }
        delete l;
    }
    EXPECT_GT(cut, 20);
}

TEST(RecipeLayoutRules, TheLayoutIsTheSameEveryTime)
{
    recipe_t recipe = sample();
    recipe_qr_t qr = make_qr();
    recipe_layout_input_t in = input(true, true, &qr, RECIPE_WARN_RELAXED);
    recipe_layout_t *a = new recipe_layout_t, *b = new recipe_layout_t;
    recipe_layout_build(&recipe, &in, 800, 480, true, a);
    recipe_layout_build(&recipe, &in, 800, 480, true, b);
    EXPECT_EQ(memcmp(a, b, sizeof(*a)), 0);
    delete a;
    delete b;
}

// ---------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------

namespace
{

bool is_palette_color(const uint8_t *p)
{
    int r = p[0], g = p[1], b = p[2];
    return (r == 0 || r == 255) && (g == 0 || g == 255) && (b == 0 || b == 255) &&
           !(r == 0 && g == 255 && b == 255) && !(r == 255 && g == 0 && b == 255);
}

std::vector<uint8_t> gradient(int w, int h)
{
    std::vector<uint8_t> rgb((size_t) w * h * 3);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t *p = &rgb[((size_t) y * w + x) * 3];
            p[0] = (uint8_t) (x * 255 / (w - 1));
            p[1] = (uint8_t) (y * 255 / (h - 1));
            p[2] = (uint8_t) ((x + y) * 255 / (w + h - 2));
        }
    }
    return rgb;
}

}  // namespace

TEST(RecipeDraw, EveryPixelOfThePageIsAColourOfThePalette)
{
    recipe_t recipe = sample();
    recipe_qr_t qr = make_qr();
    std::vector<uint8_t> photo = gradient(360, 240);
    for (const Panel &panel : kPanels) {
        recipe_layout_input_t in = input(true, true, &qr, RECIPE_WARN_RELAXED);
        recipe_layout_t *l = new recipe_layout_t;
        recipe_layout_build(&recipe, &in, panel.w, panel.h, panel.landscape, l);
        std::vector<uint8_t> rgb((size_t) panel.w * panel.h * 3, 7);
        canvas_t canvas = {rgb.data(), panel.w, panel.h};
        recipe_layout_draw(&canvas, &recipe, l, &qr, photo.data(), 360, 240, false);
        for (size_t i = 0; i < rgb.size(); i += 3) {
            ASSERT_TRUE(is_palette_color(&rgb[i]))
                << panel.w << "x" << panel.h << " pixel " << i / 3 << ": " << (int) rgb[i] << ","
                << (int) rgb[i + 1] << "," << (int) rgb[i + 2];
        }
        delete l;
    }
}

TEST(RecipeDraw, OnAGrayscalePanelThePictureIsGrayAndTheRestIsBlackOrWhiteOrColour)
{
    recipe_t recipe = sample();
    std::vector<uint8_t> photo = gradient(300, 200);
    recipe_layout_input_t in = input(true, true, nullptr);
    recipe_layout_t *l = new recipe_layout_t;
    recipe_layout_build(&recipe, &in, 800, 480, true, l);
    std::vector<uint8_t> rgb(800 * 480 * 3);
    canvas_t canvas = {rgb.data(), 800, 480};
    recipe_layout_draw(&canvas, &recipe, l, nullptr, photo.data(), 300, 200, true);
    std::set<int> grays;
    int inset = l->mat + 1;
    for (int y = l->image.y + inset; y < l->image.y + l->image.h - inset; y++) {
        for (int x = l->image.x + inset; x < l->image.x + l->image.w - inset; x++) {
            const uint8_t *p = &rgb[((size_t) y * 800 + x) * 3];
            ASSERT_EQ(p[0], p[1]);
            ASSERT_EQ(p[1], p[2]);
            ASSERT_EQ(p[0] % 17, 0) << "not one of the sixteen grays";
            grays.insert(p[0]);
        }
    }
    EXPECT_GT(grays.size(), 8u);  // a gradient uses many of the sixteen
    delete l;
}

TEST(RecipeDraw, ThePictureStaysInsideItsBoxAndFillsIt)
{
    std::vector<uint8_t> photo = gradient(200, 100);  // wider than the box: cropped at both sides
    std::vector<uint8_t> rgb(120 * 90 * 3, 128);
    canvas_t canvas = {rgb.data(), 120, 90};
    recipe_draw_photo(&canvas, 20, 15, 80, 60, photo.data(), 200, 100, false);
    for (int y = 0; y < 90; y++) {
        for (int x = 0; x < 120; x++) {
            const uint8_t *p = &rgb[((size_t) y * 120 + x) * 3];
            bool inside = x >= 20 && x < 100 && y >= 15 && y < 75;
            if (inside) {
                ASSERT_TRUE(is_palette_color(p));
            } else {
                ASSERT_EQ(p[0], 128) << x << "," << y;  // untouched
            }
        }
    }
}

TEST(RecipeDraw, ThePictureKeepsItsLightsAndDarks)
{
    // a picture that is dark on the left and light on the right keeps that
    std::vector<uint8_t> photo(100 * 60 * 3);
    for (int y = 0; y < 60; y++) {
        for (int x = 0; x < 100; x++) {
            memset(&photo[((size_t) y * 100 + x) * 3], x < 50 ? 20 : 235, 3);
        }
    }
    std::vector<uint8_t> rgb(100 * 60 * 3, 0);
    canvas_t canvas = {rgb.data(), 100, 60};
    recipe_draw_photo(&canvas, 0, 0, 100, 60, photo.data(), 100, 60, false);
    int dark_left = 0, light_right = 0;
    for (int y = 0; y < 60; y++) {
        for (int x = 0; x < 100; x++) {
            const uint8_t *p = &rgb[((size_t) y * 100 + x) * 3];
            if (x < 50 && p[0] == 0 && p[1] == 0 && p[2] == 0) {
                dark_left++;
            }
            if (x >= 50 && p[0] == 255 && p[1] == 255 && p[2] == 255) {
                light_right++;
            }
        }
    }
    EXPECT_GT(dark_left, 50 * 60 * 8 / 10);
    EXPECT_GT(light_right, 50 * 60 * 8 / 10);
}

TEST(RecipeDraw, TinyAndHugeSourcesAndBadArgumentsAreSafe)
{
    std::vector<uint8_t> rgb(50 * 50 * 3, 9);
    canvas_t canvas = {rgb.data(), 50, 50};
    uint8_t pixel[3] = {200, 100, 50};
    recipe_draw_photo(&canvas, 5, 5, 40, 40, pixel, 1, 1,
                      false);  // one pixel stretched over the box
    std::vector<uint8_t> big = gradient(1000, 700);
    recipe_draw_photo(&canvas, 0, 0, 50, 50, big.data(), 1000, 700, false);
    recipe_draw_photo(&canvas, 0, 0, 0, 10, big.data(), 1000, 700, false);
    recipe_draw_photo(&canvas, 0, 0, 10, 10, nullptr, 10, 10, false);
    recipe_draw_photo(&canvas, 0, 0, 10, 10, big.data(), 0, 10, false);
    recipe_draw_photo(nullptr, 0, 0, 10, 10, big.data(), 10, 10, false);
    recipe_draw_photo(&canvas, 40, 40, 30, 30, big.data(), 1000, 700,
                      false);  // partly off the canvas
    SUCCEED();
}

TEST(RecipeDraw, TheTextIsBlackTheTitleRedTheMetaBlueAndTheRuleYellow)
{
    recipe_t recipe = sample();
    recipe_layout_input_t in = input(false, false, nullptr);
    recipe_layout_t *l = new recipe_layout_t;
    recipe_layout_build(&recipe, &in, 800, 480, true, l);
    std::vector<uint8_t> rgb(800 * 480 * 3);
    canvas_t canvas = {rgb.data(), 800, 480};
    recipe_layout_draw(&canvas, &recipe, l, nullptr, nullptr, 0, 0, false);
    auto count_in = [&](int x0, int y0, int x1, int y1, int r, int g, int b) {
        int n = 0;
        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                const uint8_t *p = &rgb[((size_t) y * 800 + x) * 3];
                n += p[0] == r && p[1] == g && p[2] == b;
            }
        }
        return n;
    };
    EXPECT_GT(count_in(0, 0, 800, l->meta_y, 255, 0, 0), 100);         // the title
    EXPECT_GT(count_in(0, l->meta_y, 400, l->rule_y, 0, 0, 255), 50);  // the category and time
    EXPECT_EQ(count_in(0, l->rule_y, 800, l->rule_y + l->rule_h, 255, 255, 0), 800 * l->rule_h);
    EXPECT_GT(count_in(0, l->ing_head_y + 40, 260, 480, 0, 0, 0), 200);   // the ingredients
    EXPECT_GT(count_in(0, l->ing_head_y + 40, 260, 480, 255, 0, 0), 20);  // their bullets
    delete l;
}
