// The pure parts of the travel time (main/route_time.c): the requests, the readers of the TomTom
// and HERE answers (host_tests/data/route), the checks that decide whether an address was
// recognised, and the rule that tells when a time is too long.

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include "route_time.h"
}

namespace
{

std::string fixture(const std::string &name)
{
    std::ifstream file(std::string(ROUTE_TEST_DATA_DIR) + "/" + name, std::ios::binary);
    EXPECT_TRUE(file.good()) << name;
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

std::string replaced(std::string text, const std::string &from, const std::string &to)
{
    size_t at = text.find(from);
    EXPECT_NE(at, std::string::npos) << "not in the fixture: " << from;
    if (at != std::string::npos) {
        text.replace(at, from.size(), to);
    }
    return text;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Requests

TEST(RouteUrl, EncodesWhatNeedsEncoding)
{
    char out[64];
    ASSERT_TRUE(route_url_encode("Pariser Platz 1, Berlin", out, sizeof(out)));
    EXPECT_STREQ(out, "Pariser%20Platz%201%2C%20Berlin");
    ASSERT_TRUE(route_url_encode("a-b_c.d~e", out, sizeof(out)));
    EXPECT_STREQ(out, "a-b_c.d~e");
    ASSERT_TRUE(
        route_url_encode("K\xC3\xB6nigstra\xC3\x9F"
                         "e",
                         out, sizeof(out)));
    EXPECT_STREQ(out, "K%C3%B6nigstra%C3%9Fe");
    ASSERT_TRUE(route_url_encode("a/b?c&d=e#f+g%h", out, sizeof(out)));
    EXPECT_STREQ(out, "a%2Fb%3Fc%26d%3De%23f%2Bg%25h");  // nothing can open a new parameter
    ASSERT_TRUE(route_url_encode("", out, sizeof(out)));
    EXPECT_STREQ(out, "");
}

TEST(RouteUrl, ANeedlessLongTextIsRefusedNotCut)
{
    char out[8];
    EXPECT_FALSE(route_url_encode("0123456789", out, sizeof(out)));
    EXPECT_STREQ(out, "");
    EXPECT_FALSE(route_url_encode("a b", out, 4));  // 'a' + "%20" + NUL needs 5
    EXPECT_FALSE(route_url_encode(nullptr, out, sizeof(out)));
    EXPECT_FALSE(route_url_encode("a", nullptr, 4));
    EXPECT_FALSE(route_url_encode("a", out, 0));
}

TEST(RouteUrl, TheTomtomAddressSearch)
{
    char url[ROUTE_URL_MAX];
    ASSERT_TRUE(
        route_tomtom_geocode_url("Pariser Platz 1, Berlin", "de-DE", "KEY123", url, sizeof(url)));
    EXPECT_STREQ(url,
                 "https://api.tomtom.com/search/2/geocode/Pariser%20Platz%201%2C%20Berlin.json"
                 "?key=KEY123&limit=5&language=de-DE");
    ASSERT_TRUE(route_tomtom_geocode_url("x", nullptr, "K", url, sizeof(url)));
    EXPECT_STREQ(url, "https://api.tomtom.com/search/2/geocode/x.json?key=K&limit=5");
    EXPECT_FALSE(
        route_tomtom_geocode_url("", "de-DE", "K", url, sizeof(url)));  // nothing to look for
    EXPECT_FALSE(route_tomtom_geocode_url("x", "de-DE", "", url, sizeof(url)));  // no key
    EXPECT_FALSE(route_tomtom_geocode_url("x", "de-DE", nullptr, url, sizeof(url)));
    char tiny[20];
    EXPECT_FALSE(route_tomtom_geocode_url("x", "de-DE", "K", tiny, sizeof(tiny)));
}

TEST(RouteUrl, TheTomtomRoute)
{
    char url[ROUTE_URL_MAX];
    ASSERT_TRUE(
        route_tomtom_route_url(52.5251, 13.3694, 52.5219, 13.4132, "KEY", url, sizeof(url)));
    EXPECT_STREQ(url,
                 "https://api.tomtom.com/routing/1/calculateRoute/"
                 "52.525100,13.369400:52.521900,13.413200/json"
                 "?key=KEY&traffic=true&travelMode=car&computeTravelTimeFor=all&"
                 "routeRepresentation=summaryOnly");
    EXPECT_TRUE(route_tomtom_route_url(-33.8688, 151.2093, -33.9, 151.1, "K", url, sizeof(url)));
    EXPECT_NE(std::string(url).find("-33.868800,151.209300:-33.900000,151.100000"),
              std::string::npos);
    EXPECT_FALSE(
        route_tomtom_route_url(91.0, 0, 0, 0, "K", url, sizeof(url)));  // not a place on earth
    EXPECT_FALSE(route_tomtom_route_url(0, 181.0, 0, 0, "K", url, sizeof(url)));
    EXPECT_FALSE(route_tomtom_route_url(0, 0, 0, 0, "", url, sizeof(url)));
}

TEST(RouteUrl, TheHereRequests)
{
    char url[ROUTE_URL_MAX];
    ASSERT_TRUE(route_here_geocode_url("Pariser Platz 1", "de", "KEY", url, sizeof(url)));
    EXPECT_STREQ(url,
                 "https://geocode.search.hereapi.com/v1/"
                 "geocode?q=Pariser%20Platz%201&limit=5&lang=de&apiKey=KEY");
    ASSERT_TRUE(route_here_route_url(52.5251, 13.3694, 52.5219, 13.4132,
                                     "2026-10-03T07:30:00+02:00", "KEY", url, sizeof(url)));
    EXPECT_STREQ(url,
                 "https://router.hereapi.com/v8/routes?transportMode=car&origin=52.525100,13.369400"
                 "&destination=52.521900,13.413200&return=summary"
                 "&departureTime=2026-10-03T07%3A30%3A00%2B02%3A00&apiKey=KEY");
    ASSERT_TRUE(
        route_here_route_url(1, 2, 3, 4, nullptr, "KEY", url, sizeof(url)));  // without a time
    EXPECT_EQ(std::string(url).find("departureTime"), std::string::npos);
    EXPECT_FALSE(route_here_route_url(1, 2, 3, 4, "t", nullptr, url, sizeof(url)));
}

TEST(RouteUrl, TheKeyIsTheOnlyThingThatIsNotEncoded)
{
    // a key is letters and digits; whatever else is typed as a place stays inside its own parameter
    char url[ROUTE_URL_MAX];
    ASSERT_TRUE(route_tomtom_geocode_url("a&key=evil", "de-DE", "KEY", url, sizeof(url)));
    EXPECT_EQ(std::string(url).find("&key=evil"), std::string::npos);
    EXPECT_NE(std::string(url).find("a%26key%3Devil"), std::string::npos);
}

TEST(RouteUrl, WhatAKeyLooksLike)
{
    EXPECT_TRUE(route_key_valid("0123456789abcdefABCDEF0123456789"));  // 32, like a TomTom key
    EXPECT_TRUE(route_key_valid("aB3-dE_6gH9jK2mN5pQ8sT1vW4yZ7aB0cD3eF6gH9jK"));  // like a HERE key
    EXPECT_TRUE(route_key_valid("0123456789abcdef"));                             // the shortest
    EXPECT_FALSE(route_key_valid("0123456789abcde"));                             // too short
    EXPECT_FALSE(route_key_valid(std::string(105, 'a').c_str()));
    EXPECT_TRUE(route_key_valid(std::string(104, 'a').c_str()));
    EXPECT_FALSE(
        route_key_valid("0123456789abcdef&key=evil"));  // nothing that can open a parameter
    EXPECT_FALSE(route_key_valid("0123456789abcdef 123"));
    EXPECT_FALSE(route_key_valid("0123456789abcde\n1"));
    EXPECT_FALSE(route_key_valid("0123456789abcde%2F"));
    EXPECT_FALSE(route_key_valid(""));
    EXPECT_FALSE(route_key_valid(nullptr));
}

TEST(RouteTime, TheIsoTimeWithItsOffset)
{
    char out[40];
    ASSERT_TRUE(route_format_iso_time(2026, 10, 3, 7, 30, 5, 7200, out, sizeof(out)));
    EXPECT_STREQ(out, "2026-10-03T07:30:05+02:00");
    ASSERT_TRUE(
        route_format_iso_time(2026, 1, 9, 23, 59, 0, -(5 * 3600 + 30 * 60), out, sizeof(out)));
    EXPECT_STREQ(out, "2026-01-09T23:59:00-05:30");
    ASSERT_TRUE(route_format_iso_time(2026, 1, 9, 0, 0, 0, 0, out, sizeof(out)));
    EXPECT_STREQ(out, "2026-01-09T00:00:00+00:00");
    EXPECT_FALSE(route_format_iso_time(2026, 13, 1, 0, 0, 0, 0, out, sizeof(out)));
    EXPECT_FALSE(route_format_iso_time(2026, 1, 1, 24, 0, 0, 0, out, sizeof(out)));
    EXPECT_FALSE(route_format_iso_time(2026, 1, 1, 0, 0, 0, 20 * 3600, out, sizeof(out)));
    char tiny[10];
    EXPECT_FALSE(route_format_iso_time(2026, 1, 1, 0, 0, 0, 0, tiny, sizeof(tiny)));
}

// ---------------------------------------------------------------------------------------------
// Answers

TEST(RouteTomtom, OnlyAddressesAndStreetsAreOffered)
{
    std::string json = fixture("tomtom-geocode-address.json");
    route_place_t places[ROUTE_CANDIDATES_MAX];
    int n = route_tomtom_parse_geocode(json.c_str(), json.size(), places, ROUTE_CANDIDATES_MAX);
    ASSERT_EQ(n, 2);  // the town and the crossing are left out
    EXPECT_STREQ(places[0].label, "Pariser Platz 1, 10117 Berlin");
    EXPECT_EQ(places[0].level, ROUTE_LEVEL_ADDRESS);
    EXPECT_NEAR(places[0].lat, 52.51627, 1e-6);
    EXPECT_NEAR(places[0].lon, 13.37783, 1e-6);
    EXPECT_NEAR(places[0].score, 0.97f, 1e-6);
    EXPECT_STREQ(places[1].label, "Pariser Platz, 10117 Berlin");
    EXPECT_EQ(places[1].level, ROUTE_LEVEL_STREET);
}

TEST(RouteTomtom, NoPlacesIsNoCandidates)
{
    std::string json = fixture("tomtom-geocode-none.json");
    route_place_t places[ROUTE_CANDIDATES_MAX];
    EXPECT_EQ(route_tomtom_parse_geocode(json.c_str(), json.size(), places, ROUTE_CANDIDATES_MAX),
              0);
    std::string error = fixture("tomtom-error-401.json");
    EXPECT_EQ(route_tomtom_parse_geocode(error.c_str(), error.size(), places, ROUTE_CANDIDATES_MAX),
              0);
}

TEST(RouteTomtom, ABetterMatchComesFirstWhateverTheOrder)
{
    std::string json = fixture("tomtom-geocode-address.json");
    // the street scores higher than the address now: it is offered first
    json = replaced(json, "\"score\":0.97", "\"score\":0.50");
    route_place_t places[ROUTE_CANDIDATES_MAX];
    ASSERT_EQ(route_tomtom_parse_geocode(json.c_str(), json.size(), places, ROUTE_CANDIDATES_MAX),
              2);
    EXPECT_EQ(places[0].level, ROUTE_LEVEL_STREET);
    EXPECT_EQ(places[1].level, ROUTE_LEVEL_ADDRESS);
}

TEST(RouteTomtom, TheListIsCutAndDuplicatesAreLeftOut)
{
    std::string json = fixture("tomtom-geocode-address.json");
    route_place_t places[ROUTE_CANDIDATES_MAX];
    EXPECT_EQ(route_tomtom_parse_geocode(json.c_str(), json.size(), places, 1), 1);
    json = replaced(json, "\"freeformAddress\":\"Pariser Platz, 10117 Berlin\"",
                    "\"freeformAddress\":\"Pariser Platz 1, 10117 Berlin\"");
    EXPECT_EQ(route_tomtom_parse_geocode(json.c_str(), json.size(), places, ROUTE_CANDIDATES_MAX),
              1);
}

TEST(RouteTomtom, AResultThatIsNotUsableIsSkippedNotTrusted)
{
    route_place_t places[ROUTE_CANDIDATES_MAX];
    std::string base = fixture("tomtom-geocode-address.json");
    // no position, a position outside the earth, no label, a text for a number
    std::vector<std::string> broken = {
        replaced(base, "\"position\":{\"lat\":52.51627,\"lon\":13.37783}", "\"position\":{}"),
        replaced(base, "\"lat\":52.51627", "\"lat\":152.5"),
        replaced(base, "\"lon\":13.37783", "\"lon\":\"13.37\""),
        replaced(base, "\"freeformAddress\":\"Pariser Platz 1, 10117 Berlin\"",
                 "\"freeformAddress\":\"\""),
    };
    for (const std::string &json : broken) {
        int n = route_tomtom_parse_geocode(json.c_str(), json.size(), places, ROUTE_CANDIDATES_MAX);
        EXPECT_EQ(n, 1) << json.substr(0, 60);  // only the street is left
        if (n == 1) {
            EXPECT_EQ(places[0].level, ROUTE_LEVEL_STREET);
        }
    }
}

TEST(RouteTomtom, ALongLabelIsCutAtACharacter)
{
    std::string json = fixture("tomtom-geocode-address.json");
    std::string longname(300, 'x');
    longname.replace(94, 3,
                     "\xC3\xBC"
                     "x");  // a u-umlaut straddling the limit
    json = replaced(json, "Pariser Platz 1, 10117 Berlin", longname);
    route_place_t places[ROUTE_CANDIDATES_MAX];
    ASSERT_GE(route_tomtom_parse_geocode(json.c_str(), json.size(), places, ROUTE_CANDIDATES_MAX),
              1);
    size_t len = strlen(places[0].label);
    EXPECT_LT(len, (size_t) ROUTE_TEXT_MAX);
    for (size_t i = 0; i < len; i++) {  // every multi-byte character is whole
        unsigned char c = (unsigned char) places[0].label[i];
        if (c >= 0xC0) {
            ASSERT_LT(i + 1, len);
            EXPECT_EQ(((unsigned char) places[0].label[i + 1]) & 0xC0, 0x80u);
        }
    }
}

TEST(RouteTomtom, TheRouteWithAndWithoutTraffic)
{
    std::string json = fixture("tomtom-route.json");
    route_leg_t leg;
    ASSERT_TRUE(route_tomtom_parse_route(json.c_str(), json.size(), &leg));
    EXPECT_EQ(leg.seconds, 780);
    EXPECT_EQ(leg.free_seconds, 600);
    EXPECT_EQ(leg.meters, 4250);
    EXPECT_EQ(route_minutes(leg.seconds), 13);
    EXPECT_EQ(route_minutes(leg.free_seconds), 10);
}

TEST(RouteTomtom, WithoutTheFreeFlowTimeItIsTheTimeLessTheDelay)
{
    std::string json = fixture("tomtom-route-no-free.json");
    route_leg_t leg;
    ASSERT_TRUE(route_tomtom_parse_route(json.c_str(), json.size(), &leg));
    EXPECT_EQ(leg.seconds, 780);
    EXPECT_EQ(leg.free_seconds, 600);
    json = replaced(json, ",\"trafficDelayInSeconds\":180", "");
    ASSERT_TRUE(route_tomtom_parse_route(json.c_str(), json.size(), &leg));
    EXPECT_EQ(leg.free_seconds, 0);  // unknown
}

TEST(RouteTomtom, NoRouteIsNotATime)
{
    route_leg_t leg;
    for (const char *name : {"tomtom-error-401.json", "tomtom-error-no-route.json"}) {
        std::string json = fixture(name);
        EXPECT_FALSE(route_tomtom_parse_route(json.c_str(), json.size(), &leg)) << name;
    }
    EXPECT_FALSE(route_tomtom_parse_route("{\"routes\":[]}", 13, &leg));
    EXPECT_FALSE(route_tomtom_parse_route("not json", 8, &leg));
    EXPECT_FALSE(route_tomtom_parse_route(nullptr, 0, &leg));
    std::string zero = replaced(fixture("tomtom-route.json"), "\"travelTimeInSeconds\":780",
                                "\"travelTimeInSeconds\":0");
    EXPECT_FALSE(route_tomtom_parse_route(zero.c_str(), zero.size(), &leg));
    std::string text = replaced(fixture("tomtom-route.json"), "\"travelTimeInSeconds\":780",
                                "\"travelTimeInSeconds\":\"780\"");
    EXPECT_FALSE(route_tomtom_parse_route(text.c_str(), text.size(), &leg));
}

TEST(RouteHere, OnlyAddressesAndStreetsAreOffered)
{
    std::string json = fixture("here-geocode-address.json");
    route_place_t places[ROUTE_CANDIDATES_MAX];
    int n = route_here_parse_geocode(json.c_str(), json.size(), places, ROUTE_CANDIDATES_MAX);
    ASSERT_EQ(n, 2);  // the town and the sight are left out
    EXPECT_STREQ(places[0].label, "Pariser Platz 1, 10117 Berlin, Deutschland");
    EXPECT_EQ(places[0].level, ROUTE_LEVEL_ADDRESS);
    EXPECT_NEAR(places[0].lat, 52.51628, 1e-6);
    EXPECT_NEAR(places[0].lon, 13.37785, 1e-6);
    EXPECT_NEAR(places[0].score, 1.0f, 1e-6);
    EXPECT_EQ(places[1].level, ROUTE_LEVEL_STREET);
    EXPECT_NEAR(places[1].score, 0.82f, 1e-6);
}

TEST(RouteHere, NoItemsIsNoCandidatesAndTheTitleIsTheFallbackLabel)
{
    route_place_t places[ROUTE_CANDIDATES_MAX];
    std::string none = fixture("here-geocode-none.json");
    EXPECT_EQ(route_here_parse_geocode(none.c_str(), none.size(), places, ROUTE_CANDIDATES_MAX), 0);
    std::string error = fixture("here-error-401.json");
    EXPECT_EQ(route_here_parse_geocode(error.c_str(), error.size(), places, ROUTE_CANDIDATES_MAX),
              0);

    std::string json = fixture("here-geocode-address.json");
    json = replaced(json, "\"address\":{\"label\":\"Pariser Platz 1, 10117 Berlin, Deutschland\",",
                    "\"address\":{");
    ASSERT_GE(route_here_parse_geocode(json.c_str(), json.size(), places, ROUTE_CANDIDATES_MAX), 1);
    EXPECT_STREQ(places[0].label, "Pariser Platz 1, 10117 Berlin, Deutschland");  // from the title
}

TEST(RouteHere, TheRouteWithAndWithoutTraffic)
{
    std::string json = fixture("here-route.json");
    route_leg_t leg;
    ASSERT_TRUE(route_here_parse_route(json.c_str(), json.size(), &leg));
    EXPECT_EQ(leg.seconds, 790);
    EXPECT_EQ(leg.free_seconds, 610);
    EXPECT_EQ(leg.meters, 4310);
}

TEST(RouteHere, SectionsAreAddedUp)
{
    std::string json = fixture("here-route-two-sections.json");
    route_leg_t leg;
    ASSERT_TRUE(route_here_parse_route(json.c_str(), json.size(), &leg));
    EXPECT_EQ(leg.seconds, 800);
    EXPECT_EQ(leg.free_seconds, 660);
    EXPECT_EQ(leg.meters, 5000);
    // one section without a free-flow time: the sum of it is not known
    json = replaced(json, ",\"baseDuration\":260", "");
    ASSERT_TRUE(route_here_parse_route(json.c_str(), json.size(), &leg));
    EXPECT_EQ(leg.seconds, 800);
    EXPECT_EQ(leg.free_seconds, 0);
}

TEST(RouteHere, NoRouteIsNotATime)
{
    route_leg_t leg;
    for (const char *name : {"here-error-401.json", "here-error-no-route.json"}) {
        std::string json = fixture(name);
        EXPECT_FALSE(route_here_parse_route(json.c_str(), json.size(), &leg)) << name;
    }
    EXPECT_FALSE(route_here_parse_route("{\"routes\":[]}", 13, &leg));
    EXPECT_FALSE(route_here_parse_route("{\"routes\":[{\"sections\":[]}]}", 28, &leg));
    EXPECT_FALSE(route_here_parse_route("", 0, &leg));
    // a section without a duration spoils the whole route
    std::string json = replaced(fixture("here-route-two-sections.json"), "\"duration\":300,", "");
    EXPECT_FALSE(route_here_parse_route(json.c_str(), json.size(), &leg));
}

TEST(RouteReaders, OnlyAnObjectIsAnAnswer)
{
    std::string json = fixture("tomtom-route.json");
    EXPECT_TRUE(route_json_valid(json.c_str(), json.size()));
    EXPECT_TRUE(route_json_valid("{}", 2));
    EXPECT_FALSE(route_json_valid("[]", 2));
    EXPECT_FALSE(route_json_valid("<html>Bad gateway</html>", 24));
    EXPECT_FALSE(route_json_valid("", 0));
    EXPECT_FALSE(route_json_valid(nullptr, 5));
    EXPECT_FALSE(route_json_valid("{\"a\":", 5));  // cut off
}

TEST(RouteError, TheProvidersOwnWords)
{
    char text[80];
    std::string tomtom = fixture("tomtom-error-401.json");
    route_error_text(tomtom.c_str(), tomtom.size(), text, sizeof(text));
    EXPECT_STREQ(text, "You are missing valid authentication credentials");
    std::string here = fixture("here-error-401.json");
    route_error_text(here.c_str(), here.size(), text, sizeof(text));
    EXPECT_STREQ(text, "apiKey invalid. apiKey not found.");
    std::string no_route = fixture("here-error-no-route.json");
    route_error_text(no_route.c_str(), no_route.size(), text, sizeof(text));
    EXPECT_STREQ(text, "Couldn't find a route.");
    route_error_text("<html>502</html>", 16, text, sizeof(text));
    EXPECT_STREQ(text, "");
    route_error_text(tomtom.c_str(), tomtom.size(), text, 10);  // cut, never overrun
    EXPECT_EQ(strlen(text), 9u);
    route_error_text(tomtom.c_str(), tomtom.size(), nullptr, 0);  // nothing to write to
}

TEST(RouteReaders, CutOffOrChangedAnswersNeverCrash)
{
    const char *names[] = {"tomtom-geocode-address.json", "tomtom-route.json",
                           "here-geocode-address.json", "here-route.json",
                           "here-route-two-sections.json"};
    route_place_t places[ROUTE_CANDIDATES_MAX];
    route_leg_t leg;
    char text[40];
    for (const char *name : names) {
        std::string json = fixture(name);
        for (size_t cut = 0; cut < json.size(); cut += 7) {
            route_tomtom_parse_geocode(json.c_str(), cut, places, ROUTE_CANDIDATES_MAX);
            route_here_parse_geocode(json.c_str(), cut, places, ROUTE_CANDIDATES_MAX);
            route_tomtom_parse_route(json.c_str(), cut, &leg);
            route_here_parse_route(json.c_str(), cut, &leg);
            route_error_text(json.c_str(), cut, text, sizeof(text));
        }
        unsigned seed = 12345;
        for (int round = 0; round < 300; round++) {  // a changed byte
            std::string changed = json;
            changed[rand_r(&seed) % changed.size()] = (char) (rand_r(&seed) % 256);
            route_tomtom_parse_geocode(changed.c_str(), changed.size(), places,
                                       ROUTE_CANDIDATES_MAX);
            route_here_parse_geocode(changed.c_str(), changed.size(), places, ROUTE_CANDIDATES_MAX);
            route_tomtom_parse_route(changed.c_str(), changed.size(), &leg);
            route_here_parse_route(changed.c_str(), changed.size(), &leg);
            route_error_text(changed.c_str(), changed.size(), text, sizeof(text));
        }
    }
    std::string huge(200 * 1024, ' ');  // more than a sensible answer: refused
    EXPECT_EQ(route_tomtom_parse_geocode(huge.c_str(), huge.size(), places, ROUTE_CANDIDATES_MAX),
              0);
    EXPECT_FALSE(route_here_parse_route(huge.c_str(), huge.size(), &leg));
}

// ---------------------------------------------------------------------------------------------
// Decisions

TEST(RouteDecision, TheDistanceBetweenTwoPlaces)
{
    EXPECT_NEAR(route_distance_m(52.5251, 13.3694, 52.5219, 13.4132), 3000.0, 150.0);  // about 3 km
    EXPECT_NEAR(route_distance_m(0, 0, 0, 1), 111195.0, 200.0);  // a degree on the equator
    EXPECT_DOUBLE_EQ(route_distance_m(52.5, 13.4, 52.5, 13.4), 0.0);
    EXPECT_NEAR(route_distance_m(52.5, 13.4, -52.5, -166.6), 20015000.0,
                30000.0);  // the far side of the earth
}

TEST(RoutePlausible, ARealRouteIsPlausible)
{
    route_leg_t leg = {780, 600, 4250};
    EXPECT_TRUE(route_plausible(&leg, 3000.0));
    route_leg_t slow_jam = {3 * 3600, 600, 4250};
    EXPECT_TRUE(route_plausible(&slow_jam, 3000.0));  // a bad jam is still a route
}

TEST(RoutePlausible, WhatCannotBeARouteIsRefused)
{
    route_leg_t leg = {780, 600, 4250};
    EXPECT_FALSE(route_plausible(nullptr, 3000.0));
    EXPECT_FALSE(route_plausible(&leg, 10.0));  // the same place twice
    EXPECT_FALSE(route_plausible(&leg, 29.9));
    route_leg_t none = {0, 0, 4250};
    EXPECT_FALSE(route_plausible(&none, 3000.0));
    route_leg_t no_length = {780, 600, 0};
    EXPECT_FALSE(route_plausible(&no_length, 3000.0));
    route_leg_t day = {13 * 3600, 0, 400000};
    EXPECT_FALSE(route_plausible(&day, 300000.0));  // more than 12 hours
    route_leg_t short_cut = {300, 200, 1000};
    EXPECT_FALSE(route_plausible(&short_cut, 3000.0));  // shorter than the straight line
    route_leg_t detour = {2000, 1800, 40000};
    EXPECT_FALSE(route_plausible(&detour, 3000.0));  // 13 times the straight line
    route_leg_t rocket = {60, 60, 9000};
    EXPECT_FALSE(route_plausible(&rocket, 8000.0));  // 150 m/s
}

TEST(RouteDecision, WhenATimeIsTooLong)
{
    // reference 30 minutes, 10 %, at least 5 minutes: red above 35 minutes
    EXPECT_FALSE(route_is_over(30 * 60, 30, 10, 5));
    EXPECT_FALSE(route_is_over(33 * 60, 30, 10, 5));  // 10 % but not 5 minutes
    EXPECT_FALSE(route_is_over(35 * 60, 30, 10, 5));  // exactly 5 minutes: not more
    EXPECT_TRUE(route_is_over(35 * 60 + 1, 30, 10, 5));
    EXPECT_TRUE(route_is_over(50 * 60, 30, 10, 5));
    // reference 60 minutes: 10 % are 6 minutes, more than the 5: red above 66 minutes
    EXPECT_FALSE(route_is_over(66 * 60, 60, 10, 5));
    EXPECT_TRUE(route_is_over(66 * 60 + 1, 60, 10, 5));
    EXPECT_FALSE(route_is_over(64 * 60, 60, 10, 5));
}

TEST(RouteDecision, ShorterIsNeverTooLong)
{
    EXPECT_FALSE(route_is_over(20 * 60, 30, 10, 5));
    EXPECT_FALSE(route_is_over(1, 30, 0, 0));
}

TEST(RouteDecision, TheLimitsAreOptional)
{
    EXPECT_TRUE(route_is_over(31 * 60, 30, 0, 0));     // any excess
    EXPECT_FALSE(route_is_over(30 * 60, 30, 0, 0));    // none is none
    EXPECT_TRUE(route_is_over(33 * 60, 30, 5, 0));     // only the percentage counts
    EXPECT_TRUE(route_is_over(36 * 60, 30, 0, 5));     // only the minutes count
    EXPECT_FALSE(route_is_over(36 * 60, 30, 100, 0));  // a hundred percent: twice the time
    EXPECT_TRUE(route_is_over(61 * 60, 30, 100, 0));
}

TEST(RouteDecision, WithoutAReferenceNothingIsTooLong)
{
    EXPECT_FALSE(route_is_over(90 * 60, 0, 10, 5));
    EXPECT_FALSE(route_is_over(90 * 60, -3, 10, 5));
    EXPECT_FALSE(route_is_over(0, 30, 10, 5));
    EXPECT_FALSE(route_is_over(-5, 30, 10, 5));
    EXPECT_TRUE(route_is_over(90 * 60, 30, -10, -5));  // negative limits count as none
}

TEST(RouteDecision, NoOverflowForHugeValues)
{
    EXPECT_TRUE(route_is_over(2000000000, 1000, 100, 60));
    EXPECT_FALSE(route_is_over(1000 * 60, 1000, 100, 60));
}

TEST(RouteDecision, MinutesAreRoundedAndNeverZero)
{
    EXPECT_EQ(route_minutes(0), 1);
    EXPECT_EQ(route_minutes(29), 1);
    EXPECT_EQ(route_minutes(30), 1);
    EXPECT_EQ(route_minutes(89), 1);
    EXPECT_EQ(route_minutes(90), 2);
    EXPECT_EQ(route_minutes(28 * 60), 28);
    EXPECT_EQ(route_minutes(28 * 60 + 29), 28);
    EXPECT_EQ(route_minutes(28 * 60 + 30), 29);
    EXPECT_EQ(route_minutes(125 * 60), 125);
}
