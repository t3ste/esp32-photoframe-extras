// How the recipe page gets its recipe (main/recipe_engine.c): the tries, the choosing and the
// rules, with a made-up network (the invented answers of host_tests/data/recipe), a made-up picture
// decoder, clock and dice. What the maintainer asked for: three tries with the filters as they are,
// the filters relaxed only after those, recipes without a picture or with too much text skipped,
// and the last recipe with a warning when nothing else could be had.

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "recipe_engine.h"
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

bool contains(const std::string &text, const char *part)
{
    return text.find(part) != std::string::npos;
}

// The made-up world the engine runs in.
struct World {
    // answers: the first responder that returns true gives the body
    std::vector<std::function<bool(const std::string &, std::string *)>> responders;
    std::vector<std::string> requests;
    std::vector<unsigned> dice = {0};
    size_t dice_index = 0;
    unsigned long clock = 0;
    unsigned long ms_per_request = 0;
    bool decode_fails = false;

    // the last recipe
    bool have_last = false;
    recipe_t last;
    std::string last_jpeg;
    int saved = 0;
    std::string saved_title;
    std::string saved_jpeg;

    bool last_decodable = true;
    recipe_env_t env;

    World()
    {
        memset(&env, 0, sizeof(env));
        env.get = &World::get;
        env.decode = &World::decode;
        env.random = &World::random;
        env.now_ms = &World::now;
        env.load_last = &World::load_last;
        env.save_last = &World::save_last;
        env.ctx = this;
        env.mealdb_key = "";
        memset(&last, 0, sizeof(last));
    }

    static char *get(void *ctx, const char *url, size_t, size_t *len, int *status)
    {
        World *w = static_cast<World *>(ctx);
        w->requests.push_back(url);
        w->clock += w->ms_per_request;
        for (auto &responder : w->responders) {
            std::string body;
            if (responder(url, &body)) {
                *status = 200;
                *len = body.size();
                char *out = static_cast<char *>(malloc(body.size() + 1));
                memcpy(out, body.c_str(), body.size() + 1);
                return out;
            }
        }
        *status = 0;
        *len = 0;
        return nullptr;
    }

    static uint8_t *decode(void *ctx, const uint8_t *jpeg, size_t len, int box_w, int box_h, int *w,
                           int *h)
    {
        World *world = static_cast<World *>(ctx);
        if (world->decode_fails || len < 10 || memcmp(jpeg, "JPEG", 4) != 0) {
            return nullptr;
        }
        EXPECT_GT(box_w, 20);
        EXPECT_GT(box_h, 20);
        *w = 40;
        *h = 30;
        uint8_t *pixels = static_cast<uint8_t *>(malloc(40 * 30 * 3));
        memset(pixels, 128, 40 * 30 * 3);
        return pixels;
    }

    static unsigned random(void *ctx)
    {
        World *w = static_cast<World *>(ctx);
        unsigned v = w->dice[w->dice_index % w->dice.size()];
        w->dice_index++;
        return v;
    }

    static unsigned long now(void *ctx)
    {
        return static_cast<World *>(ctx)->clock;
    }

    static bool load_last(void *ctx, recipe_t *recipe, uint8_t **jpeg, size_t *len)
    {
        World *w = static_cast<World *>(ctx);
        if (!w->have_last) {
            return false;
        }
        *recipe = w->last;
        *jpeg = nullptr;
        *len = 0;
        if (!w->last_jpeg.empty()) {
            *jpeg = static_cast<uint8_t *>(malloc(w->last_jpeg.size()));
            memcpy(*jpeg, w->last_jpeg.data(), w->last_jpeg.size());
            *len = w->last_jpeg.size();
        }
        return true;
    }

    static void save_last(void *ctx, const recipe_t *recipe, const uint8_t *jpeg, size_t len)
    {
        World *w = static_cast<World *>(ctx);
        w->saved++;
        w->saved_title = recipe->title;
        w->saved_jpeg.assign(reinterpret_cast<const char *>(jpeg), jpeg ? len : 0);
    }

    // the fixtures as the services' answers
    void serve_day_page()
    {
        responders.push_back([](const std::string &url, std::string *body) {
            if (url == RECIPE_DAY_URL) {
                *body = fixture("chefkoch-day.html");
                return true;
            }
            return false;
        });
    }

    void serve_details(const std::string &file = "chefkoch-recipe.json")
    {
        responders.push_back([file](const std::string &url, std::string *body) {
            if (url.rfind("https://api.chefkoch.de/v2/recipes/", 0) == 0) {
                std::string text = fixture(file);
                // the id of the answer is the id asked for
                std::string id = url.substr(url.rfind('/') + 1);
                size_t at = text.find("\"id\": \"");
                if (at != std::string::npos) {
                    size_t from = at + 7, to = text.find('"', from);
                    text.replace(from, to - from, id);
                }
                *body = text;
                return true;
            }
            return false;
        });
    }

    void serve_pictures()
    {
        responders.push_back([](const std::string &url, std::string *body) {
            if (contains(url, "/bilder/") || contains(url, "/meals/")) {
                *body = std::string("JPEG") + std::string(300, 'x');
                return true;
            }
            return false;
        });
    }
};

recipe_canvas_t landscape_canvas()
{
    return {800, 480, true};
}

recipe_options_t options_for(int source)
{
    recipe_options_t options;
    recipe_options_defaults(&options);
    options.source = source;
    return options;
}

struct Fetched {
    recipe_t *recipe = new recipe_t;
    recipe_outcome_t out = {};
    recipe_result_t result;
    std::string history;
    ~Fetched()
    {
        free(out.photo);
        delete recipe;
    }
};

void run(Fetched &r, World &world, const recipe_options_t &options, bool network = true,
         recipe_canvas_t canvas = landscape_canvas())
{
    free(r.out.photo);  // the one of the run before
    r.out = {};
    r.out.recipe = r.recipe;
    char history[RECIPE_HISTORY_CAP];
    snprintf(history, sizeof(history), "%s", r.history.c_str());
    r.result =
        recipe_engine_run(&options, &canvas, &world.env, network, history, sizeof(history), &r.out);
    r.history = history;
}

int count_matching(const World &world, const char *part)
{
    int n = 0;
    for (const auto &url : world.requests) {
        n += contains(url, part);
    }
    return n;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// The recipe of the day
// ---------------------------------------------------------------------------------------------

TEST(RecipeEngineDay, TheRecipeOfTheVariantIsFetchedWithItsPicture)
{
    World world;
    world.serve_day_page();
    world.serve_details();
    world.serve_pictures();
    recipe_options_t options = options_for(RECIPE_SOURCE_DAY);
    options.variant = 1;
    Fetched r;
    run(r, world, options);
    ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
    EXPECT_EQ(r.out.warnings, 0u);
    EXPECT_EQ(r.out.tries, 1);
    EXPECT_EQ(r.out.requests, 3);        // the day page, the recipe, the picture
    EXPECT_STREQ(r.recipe->id, "3002");  // the second recipe of the page: the vegetarian one
    EXPECT_NE(r.out.photo, nullptr);
    EXPECT_EQ(r.out.photo_w, 40);
    EXPECT_EQ(world.requests[0], RECIPE_DAY_URL);
    EXPECT_EQ(world.requests[1], "https://api.chefkoch.de/v2/recipes/3002");
    EXPECT_TRUE(contains(world.requests[2], "crop-240x160"));  // the size for a box this wide
    EXPECT_EQ(world.saved, 1);
    EXPECT_EQ(world.saved_jpeg.size(), 304u);  // the bytes that were downloaded are kept with it
    EXPECT_EQ(r.history, "");  // the recipe of the day is not a "seen lately" matter
}

TEST(RecipeEngineDay, EachVariantIsItsOwnPositionOnThePage)
{
    for (int variant = 0; variant < 3; variant++) {
        World world;
        world.serve_day_page();
        world.serve_details();
        world.serve_pictures();
        recipe_options_t options = options_for(RECIPE_SOURCE_DAY);
        options.variant = variant;
        Fetched r;
        run(r, world, options);
        ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
        EXPECT_EQ(std::string(r.recipe->id), std::to_string(3001 + variant));
    }
}

TEST(RecipeEngineDay, ARecipeWithoutAPictureIsSkippedForTheSameVariantOfAnEarlierDay)
{
    World world;
    world.serve_day_page();
    // today's recipe (3001) has no picture, the one three places on (3004) has
    world.responders.push_back([](const std::string &url, std::string *body) {
        if (url == "https://api.chefkoch.de/v2/recipes/3001") {
            *body = fixture("chefkoch-recipe-no-image.json");
            return true;
        }
        return false;
    });
    world.serve_details();
    world.serve_pictures();
    Fetched r;
    run(r, world, options_for(RECIPE_SOURCE_DAY));
    ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
    EXPECT_STREQ(r.recipe->id, "3004");
    EXPECT_EQ(r.out.warnings, 0u);
}

TEST(RecipeEngineDay, ARecipeWithTooMuchTextIsSkipped)
{
    World world;
    world.serve_day_page();
    world.responders.push_back([](const std::string &url, std::string *body) {
        if (url == "https://api.chefkoch.de/v2/recipes/3001") {
            *body = fixture("chefkoch-recipe-long.json");
            return true;
        }
        return false;
    });
    world.serve_details();
    world.serve_pictures();
    Fetched r;
    run(r, world, options_for(RECIPE_SOURCE_DAY));
    ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
    EXPECT_STREQ(r.recipe->id, "3004");
}

TEST(RecipeEngineDay, ARecipeWhosePictureCannotBeHadIsSkipped)
{
    World world;
    world.serve_day_page();
    world.serve_details();
    int pictures = 0;
    world.responders.push_back([&pictures](const std::string &url, std::string *body) {
        if (contains(url, "/bilder/")) {
            if (pictures++ == 0) {
                return false;  // no answer for the first picture
            }
            *body = std::string("JPEG") + std::string(300, 'x');
            return true;
        }
        return false;
    });
    Fetched r;
    run(r, world, options_for(RECIPE_SOURCE_DAY));
    ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
    EXPECT_STREQ(r.recipe->id, "3004");
}

TEST(RecipeEngineDay, WithPicturesSwitchedOffNoPictureIsFetchedAndNoneIsNeeded)
{
    World world;
    world.serve_day_page();
    world.serve_details("chefkoch-recipe-no-image.json");
    recipe_options_t options = options_for(RECIPE_SOURCE_DAY);
    options.image = false;
    Fetched r;
    run(r, world, options);
    ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
    EXPECT_EQ(r.out.photo, nullptr);
    EXPECT_EQ(r.out.requests, 2);
    EXPECT_STREQ(r.recipe->id, "3001");
}

TEST(RecipeEngineDay, ThreeTriesThenTheLastRecipeWithAWarning)
{
    World world;  // the day page never answers
    world.have_last = true;
    world.last = *[] {
        static recipe_t r;
        memset(&r, 0, sizeof(r));
        snprintf(r.title, sizeof(r.title), "Das letzte Rezept");
        snprintf(r.ingredients[0], RECIPE_INGREDIENT_LEN, "Salz");
        r.ingredient_count = 1;
        snprintf(r.text, sizeof(r.text), "Kochen.");
        snprintf(r.image_url, sizeof(r.image_url), "https://img.example.invalid/x/<format>/y.jpg");
        r.german = true;
        return &r;
    }();
    world.last_jpeg = std::string("JPEG") + std::string(100, 'z');
    Fetched r;
    run(r, world, options_for(RECIPE_SOURCE_DAY));
    ASSERT_EQ(r.result, RECIPE_RESULT_LAST);
    EXPECT_EQ(r.out.tries, 3);
    EXPECT_EQ(count_matching(world, "rezept-des-tages"),
              3);  // three tries, no relaxing for a source that is down
    EXPECT_STREQ(r.recipe->title, "Das letzte Rezept");
    EXPECT_TRUE(r.out.warnings & RECIPE_WARN_LAST_RECIPE);
    EXPECT_FALSE(r.out.warnings & RECIPE_WARN_NO_NETWORK);
    EXPECT_NE(r.out.photo, nullptr);  // the picture of the last recipe is shown again
    EXPECT_EQ(world.saved, 0);        // the last recipe is not saved over itself
}

// ---------------------------------------------------------------------------------------------
// The search
// ---------------------------------------------------------------------------------------------

namespace
{

recipe_options_t search_options()
{
    recipe_options_t options = options_for(RECIPE_SOURCE_SEARCH);
    options.category = 8;  // Suppe
    options.meal = 1;      // Hauptspeise
    return options;
}

void serve_search(World &world, const std::string &list = "chefkoch-search.json")
{
    world.responders.push_back([list](const std::string &url, std::string *body) {
        if (url.rfind("https://api.chefkoch.de/v2/recipes?", 0) == 0) {
            *body = fixture(list);
            return true;
        }
        return false;
    });
}

}  // namespace

TEST(RecipeEngineSearch, AskForAPageAtARandomOffsetAndTakeACandidate)
{
    World world;
    world.dice = {137, 2};  // the offset, then the start among the candidates
    serve_search(world);
    world.serve_details();
    world.serve_pictures();
    Fetched r;
    run(r, world, search_options());
    ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
    EXPECT_TRUE(contains(world.requests[0], "&limit=20&offset=137&"));
    EXPECT_TRUE(contains(world.requests[0], "query=Suppe%20Hauptspeise&"));
    EXPECT_EQ(r.out.tries, 1);
    EXPECT_EQ(r.out.stage, 0);
    EXPECT_EQ(r.out.warnings, 0u);
    // usable with a picture: 1001, 1005, 1006; the dice said to start at the third
    EXPECT_STREQ(r.recipe->id, "1006");
    EXPECT_EQ(r.history, "1006");
}

TEST(RecipeEngineSearch, PremiumRejectedPictureLessAndRecentlySeenRecipesAreNotAsked)
{
    World world;
    world.dice = {0, 0};
    serve_search(world);
    world.serve_details();
    world.serve_pictures();
    Fetched r;
    r.history = "1001";
    run(r, world, search_options());
    ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
    for (const char *skipped : {"/1001", "/1002", "/1003", "/1004"}) {
        for (const auto &url : world.requests) {
            EXPECT_FALSE(url.size() >= 5 && url.substr(url.size() - 5) == skipped) << url;
        }
    }
    EXPECT_STREQ(r.recipe->id, "1005");
    EXPECT_EQ(r.history, "1001\n1005");
}

TEST(RecipeEngineSearch, TheRecipesShownLatelyAreRemembered)
{
    World world;
    world.dice = {0, 0};
    serve_search(world);
    world.serve_details();
    world.serve_pictures();
    Fetched r;
    std::vector<std::string> seen;
    for (int i = 0; i < 3; i++) {
        run(r, world, search_options());
        ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
        seen.push_back(r.recipe->id);
    }
    // three of three usable recipes, none twice
    EXPECT_NE(seen[0], seen[1]);
    EXPECT_NE(seen[1], seen[2]);
    EXPECT_NE(seen[0], seen[2]);
    // all three are in the list now: the fourth try finds nothing new and relaxes... then gives up
    // on this list (the same answer): the last recipe
    world.have_last = true;
    world.last = *r.recipe;
    run(r, world, search_options());
    EXPECT_EQ(r.result, RECIPE_RESULT_LAST);
}

TEST(RecipeEngineSearch, SimpleMeansTheSimplestRecipesOnly)
{
    World world;
    world.dice = {0, 0};
    serve_search(world);
    world.serve_details();
    world.serve_pictures();
    recipe_options_t options = search_options();
    options.property = 1;  // Einfach
    Fetched r;
    run(r, world, options);
    ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
    EXPECT_STREQ(r.recipe->id, "1001");  // difficulty 1; 1005 is normal, 1006 demanding
}

TEST(RecipeEngineSearch, TheFiltersAreRelaxedOnlyAfterThreeTriesWithThem)
{
    World world;
    world.dice = {0, 0};
    // the answers have recipes only for a question without the category word
    world.responders.push_back([](const std::string &url, std::string *body) {
        if (url.rfind("https://api.chefkoch.de/v2/recipes?", 0) == 0) {
            *body = contains(url, "Suppe") ? "{\"count\": 0, \"results\": []}"
                                           : fixture("chefkoch-search.json");
            return true;
        }
        return false;
    });
    world.serve_details();
    world.serve_pictures();
    Fetched r;
    run(r, world, search_options());
    ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
    // three tries with every filter (they find no recipe), then stage 1 (still has the category),
    // then stage 2 (no category: found)
    EXPECT_EQ(r.out.tries, 5);
    EXPECT_EQ(r.out.stage, 2);
    EXPECT_TRUE(r.out.warnings & RECIPE_WARN_RELAXED);
    std::vector<std::string> lists;
    for (const auto &url : world.requests) {
        if (url.rfind("https://api.chefkoch.de/v2/recipes?", 0) == 0) {
            lists.push_back(url);
        }
    }
    ASSERT_EQ(lists.size(), 5u);
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(contains(lists[i], "query=Suppe%20Hauptspeise&"))
            << i;  // the filters as set, three times
    }
    EXPECT_TRUE(contains(lists[3], "query=Suppe&"));  // the meal dropped first
    EXPECT_TRUE(contains(lists[4], "query=&"));       // then the category
}

TEST(RecipeEngineSearch, EverythingRelaxedIsNoFilterAtAll)
{
    World world;
    world.dice = {0, 0};
    world.responders.push_back([](const std::string &url, std::string *body) {
        if (url.rfind("https://api.chefkoch.de/v2/recipes?", 0) == 0) {
            // only the question with nothing but the order has an answer
            bool bare = contains(url, "query=&") && !contains(url, "maximumTime") &&
                        !contains(url, "minimumRating");
            *body = bare ? fixture("chefkoch-search.json") : "{\"count\": 0, \"results\": []}";
            return true;
        }
        return false;
    });
    world.serve_details();
    world.serve_pictures();
    recipe_options_t options = search_options();
    options.max_time = 1;
    options.min_rating = 4;
    snprintf(options.query, sizeof(options.query), "Kartoffel");
    Fetched r;
    run(r, world, options);
    ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
    EXPECT_EQ(r.out.tries, 6);
    EXPECT_EQ(r.out.stage, 3);
    EXPECT_TRUE(r.out.warnings & RECIPE_WARN_RELAXED);
}

TEST(RecipeEngineSearch, AnUnreachableSourceIsNotRelaxedAndGivesTheLastRecipe)
{
    World world;  // nothing answers
    world.have_last = true;
    memset(&world.last, 0, sizeof(world.last));
    snprintf(world.last.title, sizeof(world.last.title), "Vorher");
    snprintf(world.last.ingredients[0], RECIPE_INGREDIENT_LEN, "Salz");
    world.last.ingredient_count = 1;
    snprintf(world.last.text, sizeof(world.last.text), "Kochen.");
    world.last.german = true;
    Fetched r;
    run(r, world, search_options());
    ASSERT_EQ(r.result, RECIPE_RESULT_LAST);
    EXPECT_EQ(r.out.tries, 3);
    EXPECT_EQ(world.requests.size(), 3u);  // one request a try: nothing was relaxed
    EXPECT_TRUE(r.out.warnings & RECIPE_WARN_LAST_RECIPE);
    EXPECT_FALSE(r.out.warnings & RECIPE_WARN_RELAXED);
    EXPECT_EQ(r.out.photo, nullptr);
    EXPECT_TRUE(r.out.warnings & RECIPE_WARN_NO_IMAGE);  // pictures are on and there is none
}

TEST(RecipeEngineSearch, WithoutANetworkNoRequestIsMade)
{
    World world;
    world.have_last = true;
    memset(&world.last, 0, sizeof(world.last));
    snprintf(world.last.title, sizeof(world.last.title), "Vorher");
    Fetched r;
    run(r, world, search_options(), false);
    ASSERT_EQ(r.result, RECIPE_RESULT_LAST);
    EXPECT_TRUE(world.requests.empty());
    EXPECT_EQ(r.out.tries, 0);
    EXPECT_TRUE(r.out.warnings & RECIPE_WARN_NO_NETWORK);
    EXPECT_TRUE(r.out.warnings & RECIPE_WARN_LAST_RECIPE);
}

TEST(RecipeEngineSearch, NothingAtAllWhenThereWasNeverALastRecipe)
{
    World world;
    Fetched r;
    run(r, world, search_options());
    EXPECT_EQ(r.result, RECIPE_RESULT_NONE);
    EXPECT_EQ(r.out.tries, 3);
    run(r, world, search_options(), false);
    EXPECT_EQ(r.result, RECIPE_RESULT_NONE);
}

TEST(RecipeEngineSearch, APageBeyondTheEndOfTheResultsIsAskedAgainInsideThem)
{
    World world;
    world.dice = {900, 5, 0};
    // 25 results: a page at offset 900 is empty, the answer still says how many there are
    world.responders.push_back([](const std::string &url, std::string *body) {
        if (url.rfind("https://api.chefkoch.de/v2/recipes?", 0) == 0) {
            *body = contains(url, "offset=900") ? "{\"count\": 25, \"results\": []}"
                                                : fixture("chefkoch-search.json");
            return true;
        }
        return false;
    });
    world.serve_details();
    world.serve_pictures();
    Fetched r;
    run(r, world, search_options());
    ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
    EXPECT_EQ(r.out.tries, 1);  // one try, with two list requests
    EXPECT_TRUE(contains(world.requests[0], "offset=900"));
    EXPECT_TRUE(contains(world.requests[1], "offset=5&"));  // inside the 25 results: 5 + 20 = 25
}

TEST(RecipeEngineSearch, TheFirstOffsetIsAnywhereInTheReachOfTheSearch)
{
    for (unsigned dice : {0u, 1u, 500u, 979u, 980u, 5000u, 99999u}) {
        World world;
        world.dice = {dice, 0};
        serve_search(world);
        world.serve_details();
        world.serve_pictures();
        Fetched r;
        run(r, world, search_options());
        ASSERT_FALSE(world.requests.empty());
        const std::string &url = world.requests[0];
        size_t at = url.find("&offset=");
        ASSERT_NE(at, std::string::npos);
        int offset = atoi(url.c_str() + at + 8);
        EXPECT_GE(offset, 0);
        EXPECT_LE(offset + RECIPE_SEARCH_PAGE, RECIPE_SEARCH_DEPTH) << dice;
    }
}

// ---------------------------------------------------------------------------------------------
// TheMealDB
// ---------------------------------------------------------------------------------------------

namespace
{

void serve_mealdb(World &world)
{
    world.responders.push_back([](const std::string &url, std::string *body) {
        if (contains(url, "/filter.php")) {
            *body = fixture("mealdb-list.json");
            return true;
        }
        if (contains(url, "/lookup.php") || contains(url, "/random.php")) {
            *body = fixture("mealdb-meal.json");
            return true;
        }
        return false;
    });
}

recipe_options_t mealdb_options(int category)
{
    recipe_options_t options = options_for(RECIPE_SOURCE_MEALDB);
    options.mealdb_category = category;
    return options;
}

}  // namespace

TEST(RecipeEngineMealDb, ACategoryListAndALookup)
{
    World world;
    world.dice = {0};
    serve_mealdb(world);
    world.serve_pictures();
    Fetched r;
    run(r, world, mealdb_options(10));  // Seafood
    ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
    EXPECT_EQ(world.requests[0], "https://www.themealdb.com/api/json/v1/1/filter.php?c=Seafood");
    EXPECT_TRUE(contains(world.requests[1], "/1/lookup.php?i=500"));
    EXPECT_TRUE(contains(world.requests[2], "/small"));
    EXPECT_FALSE(r.recipe->german);
    EXPECT_STREQ(r.recipe->source, "TheMealDB");
    EXPECT_EQ(r.history, "5001");
}

TEST(RecipeEngineMealDb, NoCategoryIsARandomRecipe)
{
    World world;
    serve_mealdb(world);
    world.serve_pictures();
    Fetched r;
    run(r, world, mealdb_options(0));
    ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
    EXPECT_EQ(world.requests[0], "https://www.themealdb.com/api/json/v1/1/random.php");
}

TEST(RecipeEngineMealDb, TheUsersKeyIsUsed)
{
    World world;
    world.env.mealdb_key = "mykey123";
    serve_mealdb(world);
    world.serve_pictures();
    Fetched r;
    run(r, world, mealdb_options(10));
    ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
    EXPECT_TRUE(contains(world.requests[0], "/v1/mykey123/filter.php"));
    EXPECT_TRUE(contains(world.requests[1], "/v1/mykey123/lookup.php"));
}

TEST(RecipeEngineMealDb, ACategoryThatHasNothingIsRelaxedToAnyRecipeAfterThreeTries)
{
    World world;
    world.responders.push_back([](const std::string &url, std::string *body) {
        if (contains(url, "/filter.php")) {
            *body = fixture("mealdb-none.json");
            return true;
        }
        return false;
    });
    serve_mealdb(world);
    world.serve_pictures();
    Fetched r;
    run(r, world, mealdb_options(10));
    ASSERT_EQ(r.result, RECIPE_RESULT_NEW);
    EXPECT_EQ(r.out.tries, 4);
    EXPECT_EQ(r.out.stage, 1);
    EXPECT_TRUE(r.out.warnings & RECIPE_WARN_RELAXED);
    EXPECT_EQ(count_matching(world, "filter.php"), 3);
    EXPECT_EQ(count_matching(world, "random.php"), 1);
}

// ---------------------------------------------------------------------------------------------
// Limits
// ---------------------------------------------------------------------------------------------

TEST(RecipeEngineLimits, NoFurtherTryIsMadeAfterTheTimeIsUp)
{
    World world;
    world.ms_per_request = 60000;  // a minute a request
    world.responders.push_back([](const std::string &url, std::string *body) {
        if (url.rfind("https://api.chefkoch.de/v2/recipes?", 0) == 0) {
            *body = "{\"count\": 0, \"results\": []}";
            return true;
        }
        return false;
    });
    Fetched r;
    run(r, world, search_options());
    EXPECT_EQ(r.result, RECIPE_RESULT_NONE);
    EXPECT_LT(r.out.tries, 6);
    EXPECT_LE(r.out.tries, 2);
}

TEST(RecipeEngineLimits, ABadSourceNeverCausesAFloodOfRequests)
{
    World world;
    world.dice = {0, 0};
    serve_search(world);
    // every recipe is a premium recipe: nothing can be shown
    world.serve_details("chefkoch-recipe-premium.json");
    world.serve_pictures();
    Fetched r;
    run(r, world, search_options());
    EXPECT_EQ(r.result, RECIPE_RESULT_NONE);
    EXPECT_LE(r.out.requests, 6 * (1 + RECIPE_MAX_CANDIDATES) + 2);
    EXPECT_EQ(r.out.requests, static_cast<int>(world.requests.size()));
}

TEST(RecipeEngineLimits, TheListOfRecipesSeenLatelyStaysShort)
{
    World world;
    world.dice = {0};
    // every list has fresh ids
    int counter = 0;
    world.responders.push_back([&counter](const std::string &url, std::string *body) {
        if (url.rfind("https://api.chefkoch.de/v2/recipes?", 0) == 0) {
            std::string text = fixture("chefkoch-search.json");
            for (const char *id : {"1001", "1005", "1006"}) {
                size_t at = text.find(std::string("\"id\": \"") + id + "\"");
                text.replace(at, 12, "\"id\": \"" + std::to_string(9000 + counter++) + "\"");
            }
            *body = text;
            return true;
        }
        return false;
    });
    world.serve_details();
    world.serve_pictures();
    Fetched r;
    for (int i = 0; i < 40; i++) {
        run(r, world, search_options());
        ASSERT_EQ(r.result, RECIPE_RESULT_NEW) << i;
        ASSERT_LT(r.history.size(), static_cast<size_t>(RECIPE_HISTORY_CAP)) << i;
    }
    int lines = 1;
    for (char c : r.history) {
        lines += c == '\n';
    }
    EXPECT_LE(lines, RECIPE_HISTORY_KEEP);
    EXPECT_GE(lines, RECIPE_HISTORY_KEEP - 1);
}

TEST(RecipeEngineLimits, BadArgumentsGiveNothing)
{
    World world;
    Fetched r;
    recipe_options_t options = options_for(RECIPE_SOURCE_DAY);
    char history[16] = "";
    recipe_canvas_t canvas = landscape_canvas();
    r.out.recipe = r.recipe;
    EXPECT_EQ(
        recipe_engine_run(nullptr, &canvas, &world.env, true, history, sizeof(history), &r.out),
        RECIPE_RESULT_NONE);
    EXPECT_EQ(
        recipe_engine_run(&options, nullptr, &world.env, true, history, sizeof(history), &r.out),
        RECIPE_RESULT_NONE);
    EXPECT_EQ(recipe_engine_run(&options, &canvas, nullptr, true, history, sizeof(history), &r.out),
              RECIPE_RESULT_NONE);
    recipe_outcome_t no_recipe = {};
    EXPECT_EQ(recipe_engine_run(&options, &canvas, &world.env, true, history, sizeof(history),
                                &no_recipe),
              RECIPE_RESULT_NONE);
    EXPECT_TRUE(world.requests.empty());
}

TEST(RecipeEngineLimits, ThePictureOfTheLastRecipeThatCannotBeDecodedIsReportedMissing)
{
    World world;
    world.have_last = true;
    memset(&world.last, 0, sizeof(world.last));
    snprintf(world.last.title, sizeof(world.last.title), "Vorher");
    snprintf(world.last.image_url, sizeof(world.last.image_url),
             "https://img.example.invalid/x/<format>/y.jpg");
    world.last.german = true;
    world.last_jpeg = "garbage that is no jpeg";
    Fetched r;
    run(r, world, search_options(), false);
    ASSERT_EQ(r.result, RECIPE_RESULT_LAST);
    EXPECT_EQ(r.out.photo, nullptr);
    EXPECT_TRUE(r.out.warnings & RECIPE_WARN_NO_IMAGE);
}
