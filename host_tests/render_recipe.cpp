// Host render harness of the recipe page: draws invented recipes at the panel sizes of the boards,
// in both orientations, and writes PNG files, so the layout can be looked at without a frame.
//
//   render_recipe <output directory> [case]
//
// The recipes are invented (host_tests/data/recipe and the ones below); the picture is made up.

#include <png.h>
#include <sys/stat.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "recipe_layout.h"
#include "recipe_qr.h"
#include "recipe_source.h"
#include "recipe_text.h"
}

namespace
{

bool write_png(const std::string &path, const std::vector<uint8_t> &rgb, int w, int h)
{
    FILE *fp = fopen(path.c_str(), "wb");
    if (!fp) {
        return false;
    }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png_create_info_struct(png);
    if (setjmp(png_jmpbuf(png))) {
        fclose(fp);
        return false;
    }
    png_init_io(png, fp);
    png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    for (int y = 0; y < h; y++) {
        png_write_row(png, const_cast<uint8_t *>(rgb.data()) + (size_t) y * w * 3);
    }
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    return true;
}

std::string fixture(const std::string &name)
{
    std::ifstream file(std::string(RECIPE_TEST_DATA_DIR) + "/" + name, std::ios::binary);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

// A made-up picture of a bowl on a cloth: smooth colours and some detail, to see the dither.
std::vector<uint8_t> make_photo(int w, int h)
{
    std::vector<uint8_t> rgb((size_t) w * h * 3);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double dx = (x - w * 0.5) / (w * 0.5), dy = (y - h * 0.52) / (h * 0.5);
            double d = std::sqrt(dx * dx + dy * dy);
            int r = 40 + (int) (30 * std::sin(x * 0.11)) + y / 6;
            int g = 90 + (int) (25 * std::sin(y * 0.13)) + y / 8;
            int b = 150 + (int) (30 * std::sin((x + y) * 0.07));
            if (d < 0.78) {  // the bowl
                r = g = b = 235 - (int) (60 * d);
            }
            if (d < 0.62) {  // the soup
                r = 215 + (int) (25 * std::sin(x * 0.5) * std::sin(y * 0.5));
                g = 120 + (int) (50 * (1 - d));
                b = 40;
            }
            if (d < 0.5 && ((x * 7 + y * 13) % 29) < 4) {  // herbs
                r = 40;
                g = 140;
                b = 50;
            }
            uint8_t *p = &rgb[((size_t) y * w + x) * 3];
            p[0] = (uint8_t) (r < 0 ? 0 : (r > 255 ? 255 : r));
            p[1] = (uint8_t) (g < 0 ? 0 : (g > 255 ? 255 : g));
            p[2] = (uint8_t) (b < 0 ? 0 : (b > 255 ? 255 : b));
        }
    }
    return rgb;
}

void set_text(recipe_t *recipe, const char *utf8_text)
{
    // the preparation as the readers make it: cleaned, in paragraphs
    std::string cleaned(8192, '\0'), paragraphs(8192, '\0');
    cleaned.resize(recipe_text_clean(utf8_text, &cleaned[0], cleaned.size(), nullptr));
    paragraphs.resize(recipe_text_paragraphs(cleaned.c_str(), &paragraphs[0], paragraphs.size()));
    snprintf(recipe->text, sizeof(recipe->text), "%s", paragraphs.c_str());
}

void add_ingredient(recipe_t *recipe, const char *utf8)
{
    recipe_text_clean(utf8, recipe->ingredients[recipe->ingredient_count++], RECIPE_INGREDIENT_LEN,
                      nullptr);
}

recipe_t german_recipe()
{
    recipe_t recipe;
    recipe_parse_chefkoch_recipe(fixture("chefkoch-recipe.json").c_str(),
                                 "Chefkoch \xE2\x80\x93 Rezept des Tages", &recipe);
    // more to read, like a real one: a longer preparation and a few more ingredients
    add_ingredient(&recipe, "Knoblauchzehe(n) (2)");
    add_ingredient(&recipe,
                   "Kreuzk\xC3\xBC"
                   "mmel (1 TL)");
    add_ingredient(&recipe,
                   "Chilifl\xC3\xB6"
                   "cken (1 Prise)");
    set_text(&recipe,
             "Das Gem\xC3\xBCse waschen und in kleine W\xC3\xBCrfel schneiden. Die Zwiebel und den "
             "Knoblauch fein hacken.\n"
             "In einem Topf etwas \xC3\x96l erhitzen und die Zwiebel darin glasig d\xC3\xBCnsten. "
             "Das Gem\xC3\xBCse zugeben "
             "und kurz mitbraten. Mit der Br\xC3\xBChe abl\xC3\xB6schen.\n"
             "Alles ca. 15 Min. leise k\xC3\xB6"
             "cheln lassen, bis das Gem\xC3\xBCse weich ist. Die H\xC3\xA4lfte der Suppe "
             "p\xC3\xBC"
             "rieren und wieder zur\xC3\xBC"
             "ckgeben. Mit Salz, Pfeffer und Kreuzk\xC3\xBC"
             "mmel abschmecken.\n"
             "Zum Servieren mit Petersilie bestreuen und mit frischem Brot reichen.\n"
             "Tipp: Die Suppe l\xC3\xA4sst sich gut einfrieren. Dann vor dem Servieren langsam "
             "erw\xC3\xA4rmen und "
             "eventuell mit etwas Br\xC3\xBChe verd\xC3\xBCnnen.");
    return recipe;
}

recipe_t german_long_title()
{
    recipe_t recipe = german_recipe();
    recipe_text_clean(
        "\xC3\x9C"
        "berbackene Kartoffeln mit Kr\xC3\xA4uterquark, ger\xC3\xB6steten Kichererbsen und "
        "Joghurtsauce",
        recipe.title, sizeof(recipe.title), nullptr);
    recipe_text_clean("Hauptspeise", recipe.category, sizeof(recipe.category), nullptr);
    return recipe;
}

recipe_t english_recipe()
{
    recipe_t recipe;
    recipe_parse_mealdb_recipe(fixture("mealdb-meal.json").c_str(), &recipe);
    return recipe;
}

recipe_t long_recipe()
{
    recipe_t recipe;
    recipe_parse_chefkoch_recipe(fixture("chefkoch-recipe-long.json").c_str(), "Chefkoch", &recipe);
    return recipe;
}

struct Case {
    const char *name;
    int w, h;
    bool landscape;
    recipe_t (*make)();
    bool image;
    bool photo;
    bool qr;
    unsigned warnings;
    bool grayscale;
};

const Case kCases[] = {
    {"landscape-de", 800, 480, true, german_recipe, true, true, false, 0, false},
    {"landscape-de-qr", 800, 480, true, german_recipe, true, true, true, 0, false},
    {"landscape-de-long-title", 800, 480, true, german_long_title, true, true, true,
     RECIPE_WARN_RELAXED, false},
    {"landscape-en-qr", 800, 480, true, english_recipe, true, true, true, 0, false},
    {"landscape-de-cut", 800, 480, true, long_recipe, true, true, true, RECIPE_WARN_LAST_RECIPE,
     false},
    {"landscape-de-noimage", 800, 480, true, german_recipe, true, false, false,
     RECIPE_WARN_NO_IMAGE, false},
    {"landscape-de-nopicture-option", 800, 480, true, german_recipe, false, false, true, 0, false},
    {"landscape-de-gray", 800, 480, true, german_recipe, true, true, true, 0, true},
    {"portrait-de", 480, 800, false, german_recipe, true, true, false, 0, false},
    {"portrait-de-qr-cut", 480, 800, false, long_recipe, true, true, true, 0, false},
    {"portrait-en-qr", 480, 800, false, english_recipe, true, true, true, 0, false},
    {"portrait-de-long-title", 480, 800, false, german_long_title, true, true, true,
     RECIPE_WARN_RELAXED | RECIPE_WARN_LAST_RECIPE, false},
    {"xiao-ee03-1872x1404-de", 1872, 1404, true, german_recipe, true, true, true, 0, true},
    {"reterminal-e1004-1200x1600-portrait-de", 1200, 1600, false, german_recipe, true, true, true,
     0, false},
    {"reterminal-e1004-1600x1200-landscape-de", 1600, 1200, true, german_recipe, true, true, true,
     0, false},
    {"m5paper-960x540-de", 960, 540, true, german_recipe, true, true, true, 0, true},
};

}  // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: render_recipe <output directory> [case]\n");
        return 2;
    }
    std::string dir = argv[1];
    mkdir(dir.c_str(), 0755);
    const char *only = argc > 2 ? argv[2] : nullptr;
    int failures = 0;
    for (const Case &c : kCases) {
        if (only && strcmp(only, c.name) != 0) {
            continue;
        }
        recipe_t recipe = c.make();
        recipe_qr_t qr;
        bool have_qr = false;
        if (c.qr) {
            char url[RECIPE_URL_MAX];
            if (recipe.german) {
                recipe_chefkoch_short_url(recipe.id[0] ? recipe.id : "35591011885735", url,
                                          sizeof(url));
            } else {
                recipe_mealdb_short_url(recipe.id, url, sizeof(url));
            }
            have_qr = recipe_qr_encode(url, &qr);
        }
        recipe_layout_input_t input = {};
        input.show_image = c.image;
        input.have_photo = c.photo;
        input.qr = have_qr ? &qr : nullptr;
        input.warnings = c.warnings;
        recipe_layout_t *layout =
            static_cast<recipe_layout_t *>(calloc(1, sizeof(recipe_layout_t)));
        bool fits = recipe_layout_build(&recipe, &input, c.w, c.h, c.landscape, layout);
        std::vector<uint8_t> rgb((size_t) c.w * c.h * 3);
        canvas_t canvas = {rgb.data(), c.w, c.h};
        std::vector<uint8_t> photo = make_photo(360, 240);
        recipe_layout_draw(&canvas, &recipe, layout, have_qr ? &qr : nullptr,
                           c.photo ? photo.data() : nullptr, 360, 240, c.grayscale);
        std::string path = dir + "/recipe-" + c.name + ".png";
        if (!write_png(path, rgb, c.w, c.h)) {
            fprintf(stderr, "cannot write %s\n", path.c_str());
            failures++;
            free(layout);
            continue;
        }
        printf("%-44s %4dx%-4d fits=%d scale=%d ing=%dpx body=%dpx lines: %d ing, %d body%s\n",
               c.name, c.w, c.h, fits, layout->scale, layout->ing_px, layout->body_px,
               layout->ing_count, layout->body_count, layout->cut ? "  (text cut)" : "");
        free(layout);
    }
    return failures == 0 ? 0 : 1;
}
