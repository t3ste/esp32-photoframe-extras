// The device side of the travel time (main/route_service.c) on the PC: the settings and the HTTP
// helper are faked, the answers are those of host_tests/data/route.
#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "config.h"
#include "esp_err.h"
#include "route_service.h"
#include "route_time.h"
}

// ---- the fakes ---------------------------------------------------------------------------------

namespace
{

struct Reply {
    std::string url_part;  // the reply is for the first request whose address holds this
    esp_err_t err;
    int status;
    std::string body;
};

std::vector<Reply> g_replies;
std::vector<std::string> g_urls;

std::string g_key_tomtom, g_key_here;
std::string g_language = "en";
bool g_checked = false;
bool g_has_point[2] = {false, false};
double g_lat[2] = {0, 0}, g_lon[2] = {0, 0};

struct Stored {
    int which;
    std::string text, found;
    double lat, lon;
};
std::vector<Stored> g_stored;
int g_checked_calls = 0;

const char *kTomtomKey = "TOMTOMKEY0123456789abcdefABCDEF";  // 30 characters
const char *kHereKey = "HEREKEY-0123456789_abcdefABCDEF-xyz";

std::string fixture(const std::string &name)
{
    std::ifstream in(std::string(ROUTE_TEST_DATA_DIR) + "/" + name, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void reply(const std::string &part, int status, const std::string &body)
{
    g_replies.push_back({part, ESP_OK, status, body});
}

void no_answer(const std::string &part)
{
    g_replies.push_back({part, ESP_FAIL, 0, ""});
}

int calls(const char *part)
{
    int n = 0;
    for (const std::string &url : g_urls) {
        n += url.find(part) != std::string::npos ? 1 : 0;
    }
    return n;
}

const std::string kTomtomGeocode = "api.tomtom.com/search/2/geocode/";
const std::string kTomtomRoute = "api.tomtom.com/routing/1/calculateRoute/";
const std::string kHereGeocode = "geocode.search.hereapi.com/v1/geocode";
const std::string kHereRoute = "router.hereapi.com/v8/routes";

class RouteService : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        g_replies.clear();
        g_urls.clear();
        g_stored.clear();
        g_checked_calls = 0;
        g_key_tomtom = kTomtomKey;
        g_key_here = "";
        g_language = "en";
        g_checked = false;
        g_has_point[0] = g_has_point[1] = false;
        route_service_forget();
    }
    void places_checked()
    {
        g_checked = true;
        g_has_point[0] = g_has_point[1] = true;
        g_lat[0] = 52.5251;
        g_lon[0] = 13.3694;
        g_lat[1] = 52.5219;
        g_lon[1] = 13.4132;
    }
};

route_place_t place(const char *label, double lat, double lon, int level = ROUTE_LEVEL_ADDRESS)
{
    route_place_t p;
    memset(&p, 0, sizeof(p));
    snprintf(p.label, sizeof(p.label), "%s", label);
    p.lat = lat;
    p.lon = lon;
    p.level = level;
    return p;
}

}  // namespace

extern "C" {

const char *config_manager_get_route_key_tomtom(void)
{
    return g_key_tomtom.c_str();
}
const char *config_manager_get_route_key_here(void)
{
    return g_key_here.c_str();
}
const char *config_manager_get_overlay_language(void)
{
    return g_language.c_str();
}
bool config_manager_get_route_checked(void)
{
    return g_checked;
}
bool config_manager_get_route_point(int which, double *lat, double *lon)
{
    if (which < 0 || which > 1 || !g_has_point[which]) {
        return false;
    }
    *lat = g_lat[which];
    *lon = g_lon[which];
    return true;
}
void config_manager_set_route_place(int which, const char *text, const char *found, double lat,
                                    double lon)
{
    g_stored.push_back({which, text, found, lat, lon});
    g_has_point[which] = true;
    g_lat[which] = lat;
    g_lon[which] = lon;
}
void config_manager_set_route_checked(bool checked)
{
    g_checked = checked;
    g_checked_calls++;
}

esp_err_t http_fetch_get_once(const char *url, int timeout_ms, size_t max_response_bytes,
                              char **out_body, size_t *out_len, int *out_status,
                              const char *user_agent)
{
    (void) timeout_ms;
    (void) max_response_bytes;
    (void) user_agent;
    g_urls.push_back(url);
    *out_body = nullptr;
    *out_status = 0;
    if (out_len) {
        *out_len = 0;
    }
    for (const Reply &r : g_replies) {
        if (std::string(url).find(r.url_part) == std::string::npos) {
            continue;
        }
        if (r.err != ESP_OK) {
            return r.err;
        }
        *out_status = r.status;
        if (!r.body.empty()) {
            *out_body = (char *) malloc(r.body.size() + 1);
            memcpy(*out_body, r.body.c_str(), r.body.size() + 1);
            if (out_len) {
                *out_len = r.body.size();
            }
        }
        return ESP_OK;
    }
    return ESP_FAIL;  // nobody answers an address nobody planned for
}

}  // extern "C"

// ---------------------------------------------------------------------------------------------
// Addresses

TEST_F(RouteService, AnAddressGivesTheCandidatesOfTheFirstProvider)
{
    reply(kTomtomGeocode, 200, fixture("tomtom-geocode-address.json"));
    route_place_t places[ROUTE_CANDIDATES_MAX];
    int count = 0;
    char source[12], message[96];
    ASSERT_EQ(route_service_geocode("Pariser Platz 1, Berlin", places, ROUTE_CANDIDATES_MAX, &count,
                                    source, sizeof(source), message, sizeof(message)),
              ROUTE_STATUS_OK);
    EXPECT_EQ(count, 2);
    EXPECT_STREQ(source, "TomTom");
    ASSERT_EQ(g_urls.size(), 1u);
    EXPECT_NE(g_urls[0].find("Pariser%20Platz%201%2C%20Berlin.json"), std::string::npos);
    EXPECT_NE(g_urls[0].find(std::string("key=") + kTomtomKey), std::string::npos);
    EXPECT_NE(g_urls[0].find("language=en-GB"), std::string::npos);
}

TEST_F(RouteService, TheLanguageOfTheFrameIsAsked)
{
    g_language = "de";
    g_key_here = kHereKey;
    g_key_tomtom = "";
    reply(kHereGeocode, 200, fixture("here-geocode-address.json"));
    route_place_t places[ROUTE_CANDIDATES_MAX];
    int count = 0;
    char source[12], message[96];
    ASSERT_EQ(route_service_geocode("x", places, ROUTE_CANDIDATES_MAX, &count, source,
                                    sizeof(source), message, sizeof(message)),
              ROUTE_STATUS_OK);
    EXPECT_STREQ(source, "HERE");
    EXPECT_NE(g_urls[0].find("lang=de"), std::string::npos);
    EXPECT_EQ(g_urls[0].find("tomtom"), std::string::npos);  // no key: not asked
}

TEST_F(RouteService, ARefusedKeyIsFollowedByTheNextProvider)
{
    g_key_here = kHereKey;
    reply(kTomtomGeocode, 401, fixture("tomtom-error-401.json"));
    reply(kHereGeocode, 200, fixture("here-geocode-address.json"));
    route_place_t places[ROUTE_CANDIDATES_MAX];
    int count = 0;
    char source[12], message[96];
    ASSERT_EQ(route_service_geocode("x", places, ROUTE_CANDIDATES_MAX, &count, source,
                                    sizeof(source), message, sizeof(message)),
              ROUTE_STATUS_OK);
    EXPECT_STREQ(source, "HERE");
    EXPECT_EQ(g_urls.size(), 2u);
}

TEST_F(RouteService, AProviderThatFindsNothingIsFollowedByTheNext)
{
    g_key_here = kHereKey;
    reply(kTomtomGeocode, 200, fixture("tomtom-geocode-none.json"));
    reply(kHereGeocode, 200, fixture("here-geocode-address.json"));
    route_place_t places[ROUTE_CANDIDATES_MAX];
    int count = 0;
    char source[12], message[96];
    ASSERT_EQ(route_service_geocode("x", places, ROUTE_CANDIDATES_MAX, &count, source,
                                    sizeof(source), message, sizeof(message)),
              ROUTE_STATUS_OK);
    EXPECT_STREQ(source, "HERE");
}

TEST_F(RouteService, OnlyATownIsNoAddress)
{
    // a provider that knows only the town gives no candidate: the user is told so
    std::string town = fixture("tomtom-geocode-address.json");
    town.replace(town.find("\"Point Address\""), 15, "\"Geography\"");
    town.replace(town.find("\"Street\""), 8, "\"Geography\"");
    reply(kTomtomGeocode, 200, town);
    route_place_t places[ROUTE_CANDIDATES_MAX];
    int count = 7;
    char source[12], message[96];
    EXPECT_EQ(route_service_geocode("Berlin", places, ROUTE_CANDIDATES_MAX, &count, source,
                                    sizeof(source), message, sizeof(message)),
              ROUTE_STATUS_NOT_FOUND);
    EXPECT_EQ(count, 0);
    EXPECT_STREQ(source, "");
}

TEST_F(RouteService, WhatWentWrongIsToldInTheProvidersWords)
{
    route_place_t places[ROUTE_CANDIDATES_MAX];
    int count = 0;
    char source[12], message[96];
    reply(kTomtomGeocode, 401, fixture("tomtom-error-401.json"));
    EXPECT_EQ(route_service_geocode("x", places, ROUTE_CANDIDATES_MAX, &count, source,
                                    sizeof(source), message, sizeof(message)),
              ROUTE_STATUS_KEY_REFUSED);
    EXPECT_STREQ(message, "You are missing valid authentication credentials");

    g_replies.clear();
    reply(kTomtomGeocode, 429, "{}");
    EXPECT_EQ(route_service_geocode("x", places, ROUTE_CANDIDATES_MAX, &count, source,
                                    sizeof(source), message, sizeof(message)),
              ROUTE_STATUS_QUOTA);

    g_replies.clear();
    no_answer(kTomtomGeocode);
    EXPECT_EQ(route_service_geocode("x", places, ROUTE_CANDIDATES_MAX, &count, source,
                                    sizeof(source), message, sizeof(message)),
              ROUTE_STATUS_FAILED);

    g_replies.clear();
    reply(kTomtomGeocode, 500, "oops");
    EXPECT_EQ(route_service_geocode("x", places, ROUTE_CANDIDATES_MAX, &count, source,
                                    sizeof(source), message, sizeof(message)),
              ROUTE_STATUS_FAILED);
    g_replies.clear();
    reply(kTomtomGeocode, 200, "this is not json");
    EXPECT_EQ(route_service_geocode("x", places, ROUTE_CANDIDATES_MAX, &count, source,
                                    sizeof(source), message, sizeof(message)),
              ROUTE_STATUS_FAILED);  // not an answer of the service: not "nothing found"
}

TEST_F(RouteService, EveryRequestAsksOnce)
{
    // a retry of an answer would spend the quota again: one request per provider and no more
    reply(kTomtomGeocode, 429, "{}");
    route_place_t places[ROUTE_CANDIDATES_MAX];
    int count = 0;
    char source[12], message[96];
    route_service_geocode("x", places, ROUTE_CANDIDATES_MAX, &count, source, sizeof(source),
                          message, sizeof(message));
    EXPECT_EQ(g_urls.size(), 1u);
}

TEST_F(RouteService, WithoutAKeyNothingIsAsked)
{
    g_key_tomtom = "";
    g_key_here = "";
    route_place_t places[ROUTE_CANDIDATES_MAX];
    int count = 0;
    char source[12], message[96];
    EXPECT_EQ(route_service_geocode("x", places, ROUTE_CANDIDATES_MAX, &count, source,
                                    sizeof(source), message, sizeof(message)),
              ROUTE_STATUS_NO_KEY);
    EXPECT_TRUE(g_urls.empty());
    g_key_tomtom = "short";  // not a key: the same
    EXPECT_EQ(route_service_geocode("x", places, ROUTE_CANDIDATES_MAX, &count, source,
                                    sizeof(source), message, sizeof(message)),
              ROUTE_STATUS_NO_KEY);
    EXPECT_TRUE(g_urls.empty());
}

TEST_F(RouteService, NothingToLookForIsNothingFound)
{
    route_place_t places[ROUTE_CANDIDATES_MAX];
    int count = 0;
    char source[12], message[96];
    EXPECT_EQ(route_service_geocode("", places, ROUTE_CANDIDATES_MAX, &count, source,
                                    sizeof(source), message, sizeof(message)),
              ROUTE_STATUS_NOT_FOUND);
    EXPECT_EQ(route_service_geocode(nullptr, places, ROUTE_CANDIDATES_MAX, &count, source,
                                    sizeof(source), message, sizeof(message)),
              ROUTE_STATUS_NOT_FOUND);
    EXPECT_TRUE(g_urls.empty());
}

// ---------------------------------------------------------------------------------------------
// The check of two places

TEST_F(RouteService, TwoPlacesThatGiveARouteBothWaysAreTakenOver)
{
    reply(kTomtomRoute + "52.525100,13.369400:52.521900,13.413200", 200,
          fixture("tomtom-route.json"));
    reply(kTomtomRoute + "52.521900,13.413200:52.525100,13.369400", 200,
          fixture("tomtom-route.json"));
    route_place_t from = place("Berlin Hauptbahnhof", 52.5251, 13.3694);
    route_place_t to = place("Alexanderplatz", 52.5219, 13.4132);
    route_times_t times;
    ASSERT_EQ(route_service_check("hbf", &from, "alex", &to, &times), ROUTE_STATUS_OK);
    EXPECT_EQ(times.there.seconds, 780);
    EXPECT_EQ(times.back.seconds, 780);
    EXPECT_EQ(times.there.free_seconds, 600);
    EXPECT_STREQ(times.source, "TomTom");
    ASSERT_EQ(g_stored.size(), 2u);
    EXPECT_EQ(g_stored[0].which, 0);
    EXPECT_EQ(g_stored[0].text, "hbf");
    EXPECT_EQ(g_stored[0].found, "Berlin Hauptbahnhof");
    EXPECT_EQ(g_stored[1].which, 1);
    EXPECT_EQ(g_stored[1].text, "alex");
    EXPECT_TRUE(g_checked);
    EXPECT_EQ(calls("calculateRoute"), 2);
}

TEST_F(RouteService, ARouteThatCannotBeRightIsNotTakenOver)
{
    // the same place twice: a route of zero length
    reply(kTomtomRoute, 200, fixture("tomtom-route.json"));
    route_place_t same = place("Somewhere", 52.5251, 13.3694);
    route_times_t times;
    EXPECT_EQ(route_service_check("a", &same, "b", &same, &times), ROUTE_STATUS_IMPLAUSIBLE);
    EXPECT_TRUE(g_stored.empty());
    EXPECT_FALSE(g_checked);

    // two places far apart with a route as short as a block
    route_place_t far_away = place("Elsewhere", 48.1371, 11.5754);
    EXPECT_EQ(route_service_check("a", &same, "b", &far_away, &times), ROUTE_STATUS_IMPLAUSIBLE);
    EXPECT_TRUE(g_stored.empty());
}

TEST_F(RouteService, NoRouteBetweenThePlacesIsNotFound)
{
    reply(kTomtomRoute, 400, fixture("tomtom-error-no-route.json"));
    route_place_t from = place("A", 52.5251, 13.3694), to = place("B", 52.5219, 13.4132);
    route_times_t times;
    EXPECT_EQ(route_service_check("a", &from, "b", &to, &times), ROUTE_STATUS_NOT_FOUND);
    EXPECT_STREQ(times.message, "Error finding route: no route found between the points");
    EXPECT_TRUE(g_stored.empty());
}

TEST_F(RouteService, ARouteFallsBackToTheNextProvider)
{
    g_key_here = kHereKey;
    reply(kTomtomRoute, 429, "{}");
    reply(kHereRoute, 200, fixture("here-route.json"));
    route_place_t from = place("A", 52.5251, 13.3694), to = place("B", 52.5219, 13.4132);
    route_times_t times;
    ASSERT_EQ(route_service_check("a", &from, "b", &to, &times), ROUTE_STATUS_OK);
    EXPECT_STREQ(times.source, "HERE");
    EXPECT_EQ(times.there.seconds, 790);
    EXPECT_EQ(times.there.free_seconds, 610);
    // HERE only uses the traffic when it is told when: the moment of now is in the request
    EXPECT_NE(g_urls.back().find("departureTime=20"), std::string::npos);
}

TEST_F(RouteService, NoKeyNoPlacesNoCheck)
{
    route_place_t from = place("A", 52.5251, 13.3694), to = place("B", 52.5219, 13.4132);
    route_times_t times;
    g_key_tomtom = "";
    EXPECT_EQ(route_service_check("a", &from, "b", &to, &times), ROUTE_STATUS_NO_KEY);
    g_key_tomtom = kTomtomKey;
    EXPECT_EQ(route_service_check("a", nullptr, "b", &to, &times), ROUTE_STATUS_NO_PLACES);
    route_place_t coarse = place("Berlin", 52.5, 13.4, 0);  // not a street or an address
    EXPECT_EQ(route_service_check("a", &coarse, "b", &to, &times), ROUTE_STATUS_NO_PLACES);
    EXPECT_TRUE(g_urls.empty());
    EXPECT_TRUE(g_stored.empty());
}

// ---------------------------------------------------------------------------------------------
// The times for the page

TEST_F(RouteService, TheTimesNeedCheckedPlacesAKeyAndANetwork)
{
    route_times_t times;
    EXPECT_EQ(route_service_times(&times, true), ROUTE_STATUS_NO_PLACES);
    places_checked();
    g_key_tomtom = "";
    EXPECT_EQ(route_service_times(&times, true), ROUTE_STATUS_NO_KEY);
    g_key_tomtom = kTomtomKey;
    EXPECT_EQ(route_service_times(&times, false), ROUTE_STATUS_NO_NETWORK);
    EXPECT_TRUE(g_urls.empty());
    g_checked = false;
    EXPECT_EQ(route_service_times(&times, true),
              ROUTE_STATUS_NO_PLACES);  // places without the check
}

TEST_F(RouteService, TheTimesOfNowBothWays)
{
    places_checked();
    reply(kTomtomRoute + "52.525100,13.369400:52.521900,13.413200", 200,
          fixture("tomtom-route.json"));
    std::string back = fixture("tomtom-route.json");
    back.replace(back.find("\"travelTimeInSeconds\":780"), 25, "\"travelTimeInSeconds\":900");
    reply(kTomtomRoute + "52.521900,13.413200:52.525100,13.369400", 200, back);
    route_times_t times;
    ASSERT_EQ(route_service_times(&times, true), ROUTE_STATUS_OK);
    EXPECT_EQ(times.there.seconds, 780);
    EXPECT_EQ(times.back.seconds, 900);
}

TEST_F(RouteService, AnAnswerIsKeptForFiveMinutes)
{
    places_checked();
    reply(kTomtomRoute, 200, fixture("tomtom-route.json"));
    route_times_t times;
    ASSERT_EQ(route_service_times(&times, true), ROUTE_STATUS_OK);
    EXPECT_EQ(g_urls.size(), 2u);
    ASSERT_EQ(route_service_times(&times, true), ROUTE_STATUS_OK);  // a page drawn twice in a row
    EXPECT_EQ(g_urls.size(), 2u);
    EXPECT_EQ(times.there.seconds, 780);

    route_service_forget();  // the settings changed
    ASSERT_EQ(route_service_times(&times, true), ROUTE_STATUS_OK);
    EXPECT_EQ(g_urls.size(), 4u);

    g_lat[1] = 52.53;  // another destination: not the kept answer
    ASSERT_EQ(route_service_times(&times, true), ROUTE_STATUS_OK);
    EXPECT_EQ(g_urls.size(), 6u);
}

TEST_F(RouteService, AFailureIsNeverAnOldTime)
{
    places_checked();
    reply(kTomtomRoute, 200, fixture("tomtom-route.json"));
    route_times_t times;
    ASSERT_EQ(route_service_times(&times, true), ROUTE_STATUS_OK);
    route_service_forget();
    g_replies.clear();
    no_answer(kTomtomRoute);
    EXPECT_EQ(route_service_times(&times, true), ROUTE_STATUS_FAILED);
    EXPECT_EQ(times.there.seconds, 0);
    EXPECT_EQ(times.back.seconds, 0);
}

TEST_F(RouteService, TheWayBackFailingFailsTheWhole)
{
    places_checked();
    reply(kTomtomRoute + "52.525100,13.369400:52.521900,13.413200", 200,
          fixture("tomtom-route.json"));
    reply(kTomtomRoute + "52.521900,13.413200:52.525100,13.369400", 429, "{}");
    route_times_t times;
    EXPECT_EQ(route_service_times(&times, true), ROUTE_STATUS_QUOTA);
}

TEST(RouteStatus, TheNamesAreStable)
{
    EXPECT_STREQ(route_status_name(ROUTE_STATUS_OK), "ok");
    EXPECT_STREQ(route_status_name(ROUTE_STATUS_NO_KEY), "no_key");
    EXPECT_STREQ(route_status_name(ROUTE_STATUS_NO_PLACES), "no_places");
    EXPECT_STREQ(route_status_name(ROUTE_STATUS_NO_NETWORK), "no_network");
    EXPECT_STREQ(route_status_name(ROUTE_STATUS_KEY_REFUSED), "key_refused");
    EXPECT_STREQ(route_status_name(ROUTE_STATUS_QUOTA), "quota");
    EXPECT_STREQ(route_status_name(ROUTE_STATUS_NOT_FOUND), "not_found");
    EXPECT_STREQ(route_status_name(ROUTE_STATUS_IMPLAUSIBLE), "implausible");
    EXPECT_STREQ(route_status_name(ROUTE_STATUS_FAILED), "failed");
    EXPECT_STREQ(route_status_name((route_status_t) 99), "failed");
}
