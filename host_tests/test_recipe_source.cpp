// The sources of the recipe page (main/recipe_source.c): the lists the user chooses from, the
// requests, the readers of the Chefkoch and TheMealDB answers (invented answers of the shape the
// services send, host_tests/data/recipe), and the choosing helpers.

#include <gtest/gtest.h>

#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "recipe_source.h"
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

std::string byte(unsigned char value)
{
    return std::string(1, static_cast<char>(value));
}

std::string search_url(const recipe_options_t &options, int stage = 0, int offset = 0,
                       int limit = 20)
{
    char url[512];
    EXPECT_TRUE(recipe_search_url(&options, stage, offset, limit, url, sizeof(url)));
    return url;
}

}  // namespace

TEST(RecipeLists, NamesAreFoundWithoutRegardToCase)
{
    EXPECT_EQ(recipe_list_index(RECIPE_VARIANTS, RECIPE_VARIANT_COUNT, "vegan"), 2);
    EXPECT_EQ(recipe_list_index(RECIPE_VARIANTS, RECIPE_VARIANT_COUNT, "CLASSIC"), 0);
    EXPECT_EQ(recipe_list_index(RECIPE_MEALDB_CATEGORIES, RECIPE_MEALDB_CATEGORY_COUNT, "seafood"),
              10);
    EXPECT_EQ(recipe_list_index(RECIPE_CATEGORIES, RECIPE_CATEGORY_COUNT, "Süßspeise"), 10);
    EXPECT_EQ(recipe_list_index(RECIPE_VARIANTS, RECIPE_VARIANT_COUNT, "nope"), -1);
    EXPECT_EQ(recipe_list_index(RECIPE_VARIANTS, RECIPE_VARIANT_COUNT, nullptr), -1);
}

TEST(RecipeLists, EveryFilterListStartsWithNoFilter)
{
    EXPECT_STREQ(RECIPE_PROPERTIES[0], "");
    EXPECT_STREQ(RECIPE_HEALTH[0], "");
    EXPECT_STREQ(RECIPE_CATEGORIES[0], "");
    EXPECT_STREQ(RECIPE_COUNTRIES[0], "");
    EXPECT_STREQ(RECIPE_MEALS[0], "");
    EXPECT_STREQ(RECIPE_MEALDB_CATEGORIES[0], "");
    EXPECT_EQ(RECIPE_MAX_MINUTES[0], 0);
    EXPECT_EQ(RECIPE_MIN_RATINGS[0], 0);
}

TEST(RecipeLists, NoEntryRepeatsAndNoneIsEmptyBeyondTheFirst)
{
    auto check = [](const char *const *list, int count) {
        std::set<std::string> seen;
        for (int i = 0; i < count; i++) {
            if (i > 0) {
                EXPECT_STRNE(list[i], "") << i;
            }
            EXPECT_TRUE(seen.insert(list[i]).second) << list[i];
        }
    };
    check(RECIPE_PROPERTIES, RECIPE_PROPERTY_COUNT);
    check(RECIPE_HEALTH, RECIPE_HEALTH_COUNT);
    check(RECIPE_CATEGORIES, RECIPE_CATEGORY_COUNT);
    check(RECIPE_COUNTRIES, RECIPE_COUNTRY_COUNT);
    check(RECIPE_MEALS, RECIPE_MEAL_COUNT);
    check(RECIPE_MEALDB_CATEGORIES, RECIPE_MEALDB_CATEGORY_COUNT);
    check(RECIPE_SORTS, RECIPE_SORT_COUNT);
}

TEST(RecipeLists, TheDefaultsAreTheRecipeOfTheDayWithNoFilter)
{
    recipe_options_t options;
    memset(&options, 0xAB, sizeof(options));
    recipe_options_defaults(&options);
    EXPECT_EQ(options.source, RECIPE_SOURCE_DAY);
    EXPECT_EQ(options.variant, 0);
    EXPECT_STREQ(options.query, "");
    EXPECT_EQ(options.property + options.health + options.category + options.country + options.meal,
              0);
    EXPECT_EQ(options.max_time, 0);
    EXPECT_EQ(options.min_rating, 0);
    EXPECT_EQ(options.sort, 0);
    EXPECT_TRUE(options.image);
    EXPECT_FALSE(options.qr);
}

TEST(RecipeSearchUrl, NoFilterIsOnlyTheOrderAndThePage)
{
    recipe_options_t options;
    recipe_options_defaults(&options);
    EXPECT_EQ(search_url(options),
              "https://api.chefkoch.de/v2/recipes?query=&limit=20&offset=0&orderBy=2");
    EXPECT_EQ(search_url(options, 0, 300, 10),
              "https://api.chefkoch.de/v2/recipes?query=&limit=10&offset=300&orderBy=2");
}

TEST(RecipeSearchUrl, EveryFilterIsInTheFirstStage)
{
    recipe_options_t options;
    recipe_options_defaults(&options);
    snprintf(options.query, sizeof(options.query), "Kartoffel");
    options.category = recipe_list_index(RECIPE_CATEGORIES, RECIPE_CATEGORY_COUNT, "Suppe");
    options.country = recipe_list_index(RECIPE_COUNTRIES, RECIPE_COUNTRY_COUNT, "Italien");
    options.meal = recipe_list_index(RECIPE_MEALS, RECIPE_MEAL_COUNT, "Hauptspeise");
    options.health = recipe_list_index(RECIPE_HEALTH, RECIPE_HEALTH_COUNT, "Vegan");
    options.property = recipe_list_index(RECIPE_PROPERTIES, RECIPE_PROPERTY_COUNT, "Preiswert");
    options.max_time = 2;    // 30 minutes
    options.min_rating = 3;  // 4
    options.sort = 1;
    EXPECT_EQ(search_url(options),
              "https://api.chefkoch.de/v2/"
              "recipes?query=Kartoffel%20Suppe%20Italien%20Hauptspeise%20Vegan%20Preiswert"
              "&limit=20&offset=0&orderBy=3&maximumTime=30&minimumRating=4");
}

TEST(RecipeSearchUrl, LaterStagesDropFiltersOneGroupAtATime)
{
    recipe_options_t options;
    recipe_options_defaults(&options);
    snprintf(options.query, sizeof(options.query), "Brot");
    options.category = 8;  // Suppe
    options.country = 2;   // Italien
    options.meal = 1;
    options.health = 2;
    options.max_time = 1;
    options.min_rating = 4;
    // stage 1: no meal, diet or property words
    EXPECT_EQ(search_url(options, 1),
              "https://api.chefkoch.de/v2/"
              "recipes?query=Brot%20Suppe%20Italien&limit=20&offset=0&orderBy=2"
              "&maximumTime=15&minimumRating=4.5");
    // stage 2: no category or country either
    EXPECT_EQ(search_url(options, 2),
              "https://api.chefkoch.de/v2/"
              "recipes?query=Brot&limit=20&offset=0&orderBy=2&maximumTime=15&minimumRating=4.5");
    // stage 3: nothing but the order
    EXPECT_EQ(search_url(options, 3),
              "https://api.chefkoch.de/v2/recipes?query=&limit=20&offset=0&orderBy=2");
}

TEST(RecipeSearchUrl, UmlautsArePercentEncodedAsUtf8)
{
    recipe_options_t options;
    recipe_options_defaults(&options);
    options.category = recipe_list_index(RECIPE_CATEGORIES, RECIPE_CATEGORY_COUNT, "Süßspeise");
    EXPECT_NE(search_url(options).find("query=S%C3%BC%C3%9Fspeise&"), std::string::npos);
    options.category =
        recipe_list_index(RECIPE_CATEGORIES, RECIPE_CATEGORY_COUNT, "Brot und Brötchen");
    EXPECT_NE(search_url(options).find("query=Brot%20und%20Br%C3%B6tchen&"), std::string::npos);
    options.category = 0;
    snprintf(options.query, sizeof(options.query), "a&b=c d");
    EXPECT_NE(search_url(options).find("query=a%26b%3Dc%20d&"), std::string::npos);
}

TEST(RecipeSearchUrl, TheSortOrdersMapToTheServersNumbers)
{
    recipe_options_t options;
    recipe_options_defaults(&options);
    options.sort = 0;
    EXPECT_NE(search_url(options).find("orderBy=2"), std::string::npos);
    options.sort = 1;
    EXPECT_NE(search_url(options).find("orderBy=3"), std::string::npos);
    options.sort = 2;
    EXPECT_NE(search_url(options).find("orderBy=1"), std::string::npos);
    options.sort = 99;  // an unknown order is the default one
    EXPECT_NE(search_url(options).find("orderBy=2"), std::string::npos);
}

TEST(RecipeSearchUrl, QuickMeansThirtyMinutesUnlessThereIsAShorterLimit)
{
    recipe_options_t options;
    recipe_options_defaults(&options);
    options.property = recipe_list_index(RECIPE_PROPERTIES, RECIPE_PROPERTY_COUNT, "Schnell");
    EXPECT_NE(search_url(options).find("&maximumTime=30"), std::string::npos);
    options.max_time = 1;  // 15
    EXPECT_NE(search_url(options).find("&maximumTime=15"), std::string::npos);
    options.max_time = 4;  // 120 is longer than quick: quick wins
    EXPECT_NE(search_url(options).find("&maximumTime=30"), std::string::npos);
    // stage 1 drops the property
    EXPECT_NE(search_url(options, 1).find("&maximumTime=120"), std::string::npos);
}

TEST(RecipeSearchUrl, SimpleAddsNoWordAndTheWordPropertiesDo)
{
    recipe_options_t options;
    recipe_options_defaults(&options);
    options.property = recipe_list_index(RECIPE_PROPERTIES, RECIPE_PROPERTY_COUNT, "Einfach");
    EXPECT_NE(search_url(options).find("query=&"), std::string::npos);
    options.property = recipe_list_index(RECIPE_PROPERTIES, RECIPE_PROPERTY_COUNT, "Basisrezepte");
    EXPECT_NE(search_url(options).find("query=Basisrezepte&"), std::string::npos);
}

TEST(RecipeSearchUrl, ABufferThatIsTooSmallIsRefused)
{
    recipe_options_t options;
    recipe_options_defaults(&options);
    char url[20];
    EXPECT_FALSE(recipe_search_url(&options, 0, 0, 20, url, sizeof(url)));
    EXPECT_FALSE(recipe_search_url(nullptr, 0, 0, 20, url, sizeof(url)));
    EXPECT_FALSE(recipe_search_url(&options, 0, 0, 20, url, 0));
}

TEST(RecipeUrls, Chefkoch)
{
    char url[256];
    ASSERT_TRUE(recipe_chefkoch_detail_url("35591011885735", url, sizeof(url)));
    EXPECT_STREQ(url, "https://api.chefkoch.de/v2/recipes/35591011885735");
    ASSERT_TRUE(recipe_chefkoch_short_url("35591011885735", url, sizeof(url)));
    EXPECT_STREQ(url, "https://www.chefkoch.de/rezepte/35591011885735/");
    EXPECT_FALSE(recipe_chefkoch_detail_url("12/../x", url, sizeof(url)));
    EXPECT_FALSE(recipe_chefkoch_detail_url("", url, sizeof(url)));
    EXPECT_FALSE(recipe_chefkoch_short_url("a b", url, sizeof(url)));
}

TEST(RecipeUrls, ChefkochPictureSizeFollowsTheBox)
{
    const char *tpl = "https://img.example.invalid/rezepte/1/bilder/7/<format>/name.jpg";
    char url[256];
    ASSERT_TRUE(recipe_chefkoch_image_url(tpl, 176, url, sizeof(url)));
    EXPECT_STREQ(url, "https://img.example.invalid/rezepte/1/bilder/7/crop-240x160/name.jpg");
    ASSERT_TRUE(recipe_chefkoch_image_url(tpl, 300, url, sizeof(url)));
    EXPECT_STREQ(url, "https://img.example.invalid/rezepte/1/bilder/7/crop-360x240/name.jpg");
    ASSERT_TRUE(recipe_chefkoch_image_url(tpl, 500, url, sizeof(url)));
    EXPECT_STREQ(url, "https://img.example.invalid/rezepte/1/bilder/7/crop-642x428/name.jpg");
    // an address without the placeholder is used as it is
    ASSERT_TRUE(
        recipe_chefkoch_image_url("https://img.example.invalid/x.jpg", 176, url, sizeof(url)));
    EXPECT_STREQ(url, "https://img.example.invalid/x.jpg");
    EXPECT_FALSE(recipe_chefkoch_image_url(nullptr, 176, url, sizeof(url)));
    char tiny[10];
    EXPECT_FALSE(recipe_chefkoch_image_url(tpl, 176, tiny, sizeof(tiny)));
}

TEST(RecipeUrls, MealDb)
{
    char url[256];
    ASSERT_TRUE(recipe_mealdb_list_url("1", "Seafood", url, sizeof(url)));
    EXPECT_STREQ(url, "https://www.themealdb.com/api/json/v1/1/filter.php?c=Seafood");
    ASSERT_TRUE(recipe_mealdb_lookup_url("1", "52772", url, sizeof(url)));
    EXPECT_STREQ(url, "https://www.themealdb.com/api/json/v1/1/lookup.php?i=52772");
    ASSERT_TRUE(recipe_mealdb_random_url("abc123", url, sizeof(url)));
    EXPECT_STREQ(url, "https://www.themealdb.com/api/json/v1/abc123/random.php");
    ASSERT_TRUE(recipe_mealdb_short_url("52772", url, sizeof(url)));
    EXPECT_STREQ(url, "https://www.themealdb.com/meal/52772");
    EXPECT_FALSE(recipe_mealdb_list_url("1", "", url, sizeof(url)));
    EXPECT_FALSE(recipe_mealdb_lookup_url("1", "5 7", url, sizeof(url)));
}

TEST(RecipeUrls, AnInvalidKeyIsReplacedByTheDevelopmentKey)
{
    char url[256];
    ASSERT_TRUE(recipe_mealdb_random_url("bad/key", url, sizeof(url)));
    EXPECT_STREQ(url, "https://www.themealdb.com/api/json/v1/1/random.php");
    ASSERT_TRUE(recipe_mealdb_random_url("", url, sizeof(url)));
    EXPECT_STREQ(url, "https://www.themealdb.com/api/json/v1/1/random.php");
    ASSERT_TRUE(recipe_mealdb_random_url(nullptr, url, sizeof(url)));
    EXPECT_STREQ(url, "https://www.themealdb.com/api/json/v1/1/random.php");
}

TEST(RecipeUrls, MealDbKeyAndPicture)
{
    EXPECT_TRUE(recipe_mealdb_key_valid("1"));
    EXPECT_TRUE(recipe_mealdb_key_valid("Abc123xyz"));
    EXPECT_FALSE(recipe_mealdb_key_valid(""));
    EXPECT_FALSE(recipe_mealdb_key_valid("a-b"));
    EXPECT_FALSE(recipe_mealdb_key_valid("a b"));
    EXPECT_FALSE(recipe_mealdb_key_valid("0123456789012345678901234"));  // 25
    char url[256];
    ASSERT_TRUE(
        recipe_mealdb_image_url("https://img.example.invalid/m/a.jpg", 176, url, sizeof(url)));
    EXPECT_STREQ(url, "https://img.example.invalid/m/a.jpg/small");
    ASSERT_TRUE(
        recipe_mealdb_image_url("https://img.example.invalid/m/a.jpg", 300, url, sizeof(url)));
    EXPECT_STREQ(url, "https://img.example.invalid/m/a.jpg/medium");
    EXPECT_FALSE(recipe_mealdb_image_url("", 176, url, sizeof(url)));
}

TEST(RecipeParseSearch, ReadsTheCandidatesAndTheTotal)
{
    recipe_candidate_t found[10];
    int total = 0;
    int count =
        recipe_parse_chefkoch_search(fixture("chefkoch-search.json").c_str(), found, 10, &total);
    ASSERT_EQ(count, 6);
    EXPECT_EQ(total, 4321);
    EXPECT_STREQ(found[0].id, "1001");
    EXPECT_STREQ(found[0].title, "Beispielsuppe mit Linsen");
    EXPECT_EQ(found[0].minutes, 15);
    EXPECT_EQ(found[0].rating_tenths, 47);
    EXPECT_EQ(found[0].difficulty, 1);
    EXPECT_TRUE(found[0].has_image);
    EXPECT_TRUE(found[0].usable);
}

TEST(RecipeParseSearch, PremiumRejectedAndPictureLessRecipesAreMarked)
{
    recipe_candidate_t found[10];
    recipe_parse_chefkoch_search(fixture("chefkoch-search.json").c_str(), found, 10, nullptr);
    EXPECT_FALSE(found[1].usable);  // premium
    EXPECT_FALSE(found[3].usable);  // rejected
    EXPECT_FALSE(found[2].has_image);
    EXPECT_TRUE(found[2].usable);
    EXPECT_EQ(found[4].rating_tenths, 0);  // no rating yet
    EXPECT_EQ(found[5].difficulty, 3);
    EXPECT_STREQ(found[5].title,
                 ("S" + byte(0xFC) + byte(0xDF) + "er K" + byte(0xE4) + "sekuchen").c_str());
}

TEST(RecipeParseSearch, StopsAtTheArrayAndAtMax)
{
    recipe_candidate_t found[3];
    EXPECT_EQ(
        recipe_parse_chefkoch_search(fixture("chefkoch-search.json").c_str(), found, 3, nullptr),
        3);
}

TEST(RecipeParseSearch, AnAnswerThatWasCutOffGivesWhatCameBefore)
{
    std::string json = fixture("chefkoch-search.json");
    size_t cut = json.find("\"id\": \"1004\"");
    ASSERT_NE(cut, std::string::npos);
    json.resize(cut);  // in the middle of the fourth result
    recipe_candidate_t found[10];
    int total = 0;
    EXPECT_EQ(recipe_parse_chefkoch_search(json.c_str(), found, 10, &total), 3);
    EXPECT_EQ(total, 4321);
}

TEST(RecipeParseSearch, GarbageAndEmptyAnswersGiveNothing)
{
    recipe_candidate_t found[4];
    int total = 7;
    EXPECT_EQ(recipe_parse_chefkoch_search("", found, 4, &total), 0);
    EXPECT_EQ(total, -1);
    EXPECT_EQ(recipe_parse_chefkoch_search("not json", found, 4, nullptr), 0);
    EXPECT_EQ(recipe_parse_chefkoch_search("{\"count\": 12, \"results\": []}", found, 4, &total),
              0);
    EXPECT_EQ(total, 12);
    EXPECT_EQ(recipe_parse_chefkoch_search(nullptr, found, 4, nullptr), 0);
    EXPECT_EQ(recipe_parse_chefkoch_search("{\"results\": [1, 2]}", found, 4, nullptr), 0);
}

TEST(RecipeParseSearch, AResultWithoutAnIdIsLeftOut)
{
    recipe_candidate_t found[4];
    const char *json =
        "{\"count\": 2, \"results\": [{\"recipe\": {\"title\": \"No id\"}}, "
        "{\"recipe\": {\"id\": \"77\", \"title\": \"Has id\", \"hasImage\": true}}]}";
    ASSERT_EQ(recipe_parse_chefkoch_search(json, found, 4, nullptr), 1);
    EXPECT_STREQ(found[0].id, "77");
}

TEST(RecipeParseDay, ReadsTheIdsInTheOrderOfThePage)
{
    char ids[16][RECIPE_ID_MAX];
    int count = recipe_parse_chefkoch_day(fixture("chefkoch-day.html").c_str(), ids, 16);
    ASSERT_EQ(count, 12);
    EXPECT_STREQ(ids[0], "3001");
    EXPECT_STREQ(ids[1], "3002");
    EXPECT_STREQ(ids[2], "3003");
    EXPECT_STREQ(ids[11], "3012");
    EXPECT_EQ(recipe_parse_chefkoch_day(fixture("chefkoch-day.html").c_str(), ids, 3), 3);
}

TEST(RecipeParseDay, TheVariantsAreTheFirstThreePositions)
{
    EXPECT_EQ(recipe_day_position(0), 0);
    EXPECT_EQ(recipe_day_position(1), 1);
    EXPECT_EQ(recipe_day_position(2), 2);
    EXPECT_EQ(recipe_day_position(7), 0);
    EXPECT_EQ(recipe_day_position(-1), 0);
}

TEST(RecipeParseDay, NoListOrACutOffListGivesNothing)
{
    char ids[4][RECIPE_ID_MAX];
    EXPECT_EQ(recipe_parse_chefkoch_day("<html>no list here</html>", ids, 4), 0);
    std::string html = fixture("chefkoch-day.html");
    size_t cut = html.find("</script>", html.find("application/ld+json"));
    ASSERT_NE(cut, std::string::npos);
    html.resize(cut - 10);  // the page ends inside the list
    EXPECT_EQ(recipe_parse_chefkoch_day(html.c_str(), ids, 4), 0);
    EXPECT_EQ(recipe_parse_chefkoch_day(nullptr, ids, 4), 0);
    EXPECT_EQ(
        recipe_parse_chefkoch_day("<script type=\"application/ld+json\">{broken</script>", ids, 4),
        0);
}

TEST(RecipeParseDay, AnItemWithoutARecipeAddressIsSkipped)
{
    char ids[4][RECIPE_ID_MAX];
    const char *html =
        "<script type=\"application/ld+json\">{\"itemListElement\": ["
        "{\"url\": \"https://x.invalid/about\"}, {\"url\": "
        "\"https://x.invalid/rezepte/42/Name.html\"},"
        "{\"url\": \"https://x.invalid/rezept/43/Other.html\"}]}</script>";
    ASSERT_EQ(recipe_parse_chefkoch_day(html, ids, 4), 2);
    EXPECT_STREQ(ids[0], "42");
    EXPECT_STREQ(ids[1], "43");
}

TEST(RecipeParseChefkoch, ReadsTheRecipe)
{
    recipe_t recipe;
    ASSERT_TRUE(recipe_parse_chefkoch_recipe(fixture("chefkoch-recipe.json").c_str(),
                                             "Chefkoch \xE2\x80\x93 Rezept des Tages", &recipe));
    EXPECT_EQ(std::string(recipe.title), "Beispiel-Gem" + byte(0xFC) + "sesuppe");
    EXPECT_STREQ(recipe.id, "2001");
    EXPECT_TRUE(recipe.german);
    EXPECT_EQ(std::string(recipe.source), "Chefkoch " + byte(0x96) + " Rezept des Tages");
    EXPECT_STREQ(recipe.category, "Suppen");  // the last breadcrumb with a name
    EXPECT_STREQ(recipe.time, "35 Min.");
    EXPECT_STREQ(recipe.url, "https://www.chefkoch.de/rezepte/2001/");  // the short form
    EXPECT_STREQ(
        recipe.image_url,
        "https://img.example.invalid/rezepte/2001/bilder/7/<format>/beispiel-gemuesesuppe.jpg");
    EXPECT_FALSE(recipe.text_cut);
}

TEST(RecipeParseChefkoch, TheIngredientsReadAsTheyAreWrittenInARecipe)
{
    recipe_t recipe;
    ASSERT_TRUE(
        recipe_parse_chefkoch_recipe(fixture("chefkoch-recipe.json").c_str(), "Chefkoch", &recipe));
    ASSERT_EQ(recipe.ingredient_count, 6);  // the one without a name is left out
    EXPECT_STREQ(recipe.ingredients[0], "Karotte(n) (3)");
    EXPECT_STREQ(recipe.ingredients[1], "Zwiebel(n), rote (1)");
    EXPECT_EQ(std::string(recipe.ingredients[2]),
              "Gem" + byte(0xFC) + "sebr" + byte(0xFC) + "he (750 ml)");
    EXPECT_EQ(std::string(recipe.ingredients[3]),
              byte(0xD6) + "l zum Braten");  // no amount: no brackets
    EXPECT_STREQ(recipe.ingredients[4], "Petersilie (0,5 Bund frisch)");
    EXPECT_STREQ(recipe.ingredients[5], "Salz und Pfeffer");
}

TEST(RecipeParseChefkoch, ThePreparationIsInParagraphs)
{
    recipe_t recipe;
    ASSERT_TRUE(
        recipe_parse_chefkoch_recipe(fixture("chefkoch-recipe.json").c_str(), "Chefkoch", &recipe));
    std::string text = recipe.text;
    int lines = 1;
    for (char c : text) {
        lines += c == '\n';
    }
    EXPECT_EQ(lines,
              4);  // 2 sentences, then 3 split into 2 + 1, then 2 ("ca." and "Min." end none)
    EXPECT_EQ(text.rfind("Das Gem" + byte(0xFC) + "se waschen", 0), 0u);
    EXPECT_NE(text.find("Alles ca. 15 Min. leise k" + byte(0xF6) + "cheln"), std::string::npos);
    EXPECT_EQ(text.find("  "), std::string::npos);
}

TEST(RecipeParseChefkoch, ARecipeWithNothingToShowIsRefused)
{
    recipe_t recipe;
    EXPECT_FALSE(recipe_parse_chefkoch_recipe(fixture("chefkoch-recipe-premium.json").c_str(),
                                              "Chefkoch", &recipe));
    EXPECT_FALSE(recipe_parse_chefkoch_recipe(fixture("chefkoch-recipe-no-text.json").c_str(),
                                              "Chefkoch", &recipe));
    EXPECT_FALSE(recipe_parse_chefkoch_recipe("{}", "Chefkoch", &recipe));
    EXPECT_FALSE(recipe_parse_chefkoch_recipe("not json", "Chefkoch", &recipe));
    EXPECT_FALSE(recipe_parse_chefkoch_recipe(nullptr, "Chefkoch", &recipe));
}

TEST(RecipeParseChefkoch, AWrappedAnswerIsAccepted)
{
    recipe_t recipe;
    std::string wrapped = "{\"recipe\": " + fixture("chefkoch-recipe.json") + "}";
    EXPECT_TRUE(recipe_parse_chefkoch_recipe(wrapped.c_str(), "Chefkoch", &recipe));
    EXPECT_STREQ(recipe.id, "2001");
}

TEST(RecipeParseChefkoch, WithoutAPictureTheAddressIsEmpty)
{
    recipe_t recipe;
    ASSERT_TRUE(recipe_parse_chefkoch_recipe(fixture("chefkoch-recipe-no-image.json").c_str(),
                                             "Chefkoch", &recipe));
    EXPECT_STREQ(recipe.image_url, "");
}

TEST(RecipeParseChefkoch, TooMuchTextIsCutAndSaid)
{
    recipe_t recipe;
    ASSERT_TRUE(recipe_parse_chefkoch_recipe(fixture("chefkoch-recipe-long.json").c_str(),
                                             "Chefkoch", &recipe));
    EXPECT_TRUE(recipe.text_cut);
    EXPECT_EQ(strlen(recipe.text), static_cast<size_t>(RECIPE_TEXT_MAX - 1));
}

TEST(RecipeParseMealDb, ReadsTheList)
{
    recipe_candidate_t found[10];
    int count = recipe_parse_mealdb_list(fixture("mealdb-list.json").c_str(), found, 10);
    ASSERT_EQ(count, 3);  // the one without an id is left out
    EXPECT_STREQ(found[0].id, "5001");
    EXPECT_STREQ(found[0].title, "Beispiel Stew");
    EXPECT_TRUE(found[0].has_image);
    EXPECT_FALSE(found[2].has_image);
    EXPECT_EQ(recipe_parse_mealdb_list(fixture("mealdb-none.json").c_str(), found, 10), 0);
    EXPECT_EQ(recipe_parse_mealdb_list("garbage", found, 10), 0);
    EXPECT_EQ(recipe_parse_mealdb_list(fixture("mealdb-list.json").c_str(), found, 2), 2);
}

TEST(RecipeParseMealDb, ReadsTheRecipeAndTidiesTheSteps)
{
    recipe_t recipe;
    ASSERT_TRUE(recipe_parse_mealdb_recipe(fixture("mealdb-meal.json").c_str(), &recipe));
    EXPECT_STREQ(recipe.title, "Example Cabbage Stew");
    EXPECT_STREQ(recipe.category, "Beef");
    EXPECT_STREQ(recipe.source, "TheMealDB");
    EXPECT_STREQ(recipe.id, "5001");
    EXPECT_FALSE(recipe.german);
    EXPECT_STREQ(recipe.time, "");
    EXPECT_STREQ(recipe.url, "https://www.themealdb.com/meal/5001");
    EXPECT_STREQ(recipe.image_url, "https://img.example.invalid/meals/a.jpg");
    ASSERT_EQ(recipe.ingredient_count, 5);
    EXPECT_STREQ(recipe.ingredients[0], "Beef (500g)");
    EXPECT_STREQ(recipe.ingredients[1], "Cabbage (1 Diced)");
    EXPECT_STREQ(recipe.ingredients[3], "Salt");  // a measure of blanks is no measure
    EXPECT_STREQ(recipe.ingredients[4], "Boiling Water (1 L)");
    // the steps that stood on a line of their own are joined with the text after them
    EXPECT_STREQ(recipe.text,
                 "1. Cooking\n"
                 "Add some oil to a pan. Fry the onions & the diced beef for 5 minutes.\n"
                 "2. Add the paprika and season with salt. Stir.\n"
                 "3. Simmer for 2.5 hours until the stew is ready.");
}

TEST(RecipeParseMealDb, NothingToShowIsRefused)
{
    recipe_t recipe;
    EXPECT_FALSE(recipe_parse_mealdb_recipe(fixture("mealdb-none.json").c_str(), &recipe));
    EXPECT_FALSE(recipe_parse_mealdb_recipe("{\"meals\": []}", &recipe));
    EXPECT_FALSE(recipe_parse_mealdb_recipe("garbage", &recipe));
    EXPECT_FALSE(recipe_parse_mealdb_recipe(nullptr, &recipe));
    EXPECT_FALSE(recipe_parse_mealdb_recipe(
        "{\"meals\": [{\"idMeal\": \"1\", \"strMeal\": \"Only a name\"}]}", &recipe));
}

TEST(RecipePick, TheOffsetStaysInsideTheReachAndTheResults)
{
    for (unsigned random = 0; random < 5000; random += 7) {
        int offset = recipe_pick_offset(385908, 20, random);
        EXPECT_GE(offset, 0);
        EXPECT_LE(offset + 20, RECIPE_SEARCH_DEPTH);
        offset = recipe_pick_offset(55, 20, random);
        EXPECT_LE(offset + 20, 55);
        offset = recipe_pick_offset(-1, 20, random);  // the total is not known yet
        EXPECT_LE(offset + 20, RECIPE_SEARCH_DEPTH);
    }
    EXPECT_EQ(recipe_pick_offset(10, 20, 123), 0);  // fewer results than a page
    EXPECT_EQ(recipe_pick_offset(0, 20, 123) + 20 <= RECIPE_SEARCH_DEPTH, true);
}

TEST(RecipePick, EveryOffsetCanComeUp)
{
    std::set<int> seen;
    for (unsigned random = 0; random < 200; random++) {
        seen.insert(recipe_pick_offset(25, 20, random));
    }
    EXPECT_EQ(seen.size(), 6u);  // 0..5
}

TEST(RecipeHistory, RemembersTheNewestAndFindsThem)
{
    char history[64] = "";
    recipe_history_add(history, sizeof(history), "111", 3);
    recipe_history_add(history, sizeof(history), "222", 3);
    recipe_history_add(history, sizeof(history), "333", 3);
    EXPECT_STREQ(history, "111\n222\n333");
    EXPECT_TRUE(recipe_history_has(history, "222"));
    EXPECT_FALSE(recipe_history_has(history, "22"));  // a part of an id is no id
    EXPECT_FALSE(recipe_history_has(history, "444"));
    recipe_history_add(history, sizeof(history), "444", 3);
    EXPECT_STREQ(history, "222\n333\n444");  // the oldest is gone
    EXPECT_FALSE(recipe_history_has(history, "111"));
}

TEST(RecipeHistory, AnIdSeenAgainMovesToTheEnd)
{
    char history[64] = "";
    recipe_history_add(history, sizeof(history), "1", 4);
    recipe_history_add(history, sizeof(history), "2", 4);
    recipe_history_add(history, sizeof(history), "3", 4);
    recipe_history_add(history, sizeof(history), "1", 4);
    EXPECT_STREQ(history, "2\n3\n1");
}

TEST(RecipeHistory, TheCapacityIsNeverExceeded)
{
    for (size_t cap = 2; cap < 40; cap++) {
        std::string buffer(cap + 4, 'Q');
        buffer[0] = '\0';
        for (int i = 0; i < 30; i++) {
            recipe_history_add(&buffer[0], cap, std::to_string(1000 + i).c_str(), 10);
            ASSERT_LT(strlen(buffer.c_str()), cap) << cap;
            ASSERT_EQ(buffer.substr(cap), std::string(4, 'Q'))
                << "written beyond the buffer at " << cap;
        }
    }
}

TEST(RecipeHistory, EmptyInputIsHarmless)
{
    char history[16] = "";
    recipe_history_add(history, sizeof(history), "", 3);
    recipe_history_add(history, sizeof(history), nullptr, 3);
    EXPECT_STREQ(history, "");
    EXPECT_FALSE(recipe_history_has(history, "1"));
    EXPECT_FALSE(recipe_history_has(nullptr, "1"));
    EXPECT_FALSE(recipe_history_has(history, ""));
}
