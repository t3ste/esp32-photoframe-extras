#ifndef ROUTE_TIME_H
#define ROUTE_TIME_H

/**
 * @file route_time.h
 * @brief The pure parts of the travel time on the fuel page (build option `route-time`): the
 * requests to TomTom and HERE (address -> coordinates, and the route between two places with the
 * current traffic), the readers of their answers, the rule that tells when a time is too long, and
 * the checks that decide whether an address was recognised. Pure C on top of cJSON, so the host
 * tests link it; the fetching is in route_service.c.
 *
 * The answers are read the way the providers document them (TomTom Search / Routing API, HERE
 * Geocoding & Search / Routing API v8); host_tests/data/route/README.md says where the test answers
 * come from. The requests hold the personal API keys and are never logged.
 */

#include <stdbool.h>
#include <stddef.h>

#define ROUTE_TEXT_MAX 96       // an address as typed or as found
#define ROUTE_CANDIDATES_MAX 5  // places offered when an address is not unique
#define ROUTE_URL_MAX 400

typedef enum {
    ROUTE_LEVEL_STREET = 1,   // a street without a house number
    ROUTE_LEVEL_ADDRESS = 2,  // an address with a house number
} route_level_t;

typedef struct {
    char label[ROUTE_TEXT_MAX];  // the place as the provider writes it
    double lat;
    double lon;
    int level;    // route_level_t
    float score;  // how well it matches the text, 0..1 (0 when the provider says nothing)
} route_place_t;

typedef struct {
    int seconds;       // the travel time with the traffic of now
    int free_seconds;  // without traffic (0 if the provider does not say)
    int meters;
} route_leg_t;

/** @brief Letters, digits, '-' and '_', 16 to 104 of them: what an API key of TomTom or HERE looks
 * like. */
bool route_key_valid(const char *key);

// ---- requests -----------------------------------------------------------------------------------

/** @brief Percent-encodes `in` for a URL path or query. False if `out` is too small. */
bool route_url_encode(const char *in, char *out, size_t out_len);

/** @brief The address search of TomTom: up to ROUTE_CANDIDATES_MAX places for a text. */
bool route_tomtom_geocode_url(const char *text, const char *language, const char *key, char *out,
                              size_t out_len);

/** @brief The route of TomTom between two places, summary only, with the traffic of now. */
bool route_tomtom_route_url(double from_lat, double from_lon, double to_lat, double to_lon,
                            const char *key, char *out, size_t out_len);

/** @brief The address search of HERE. */
bool route_here_geocode_url(const char *text, const char *language, const char *key, char *out,
                            size_t out_len);

/**
 * @brief The route of HERE between two places, summary only, for a departure at `depart_iso` (the
 * moment of now as "2026-10-03T07:30:00+02:00"; without a time HERE does not use the traffic).
 */
bool route_here_route_url(double from_lat, double from_lon, double to_lat, double to_lon,
                          const char *depart_iso, const char *key, char *out, size_t out_len);

/** @brief "2026-10-03T07:30:00+02:00" for a moment, from the broken-down local time and its UTC
 * offset in seconds. */
bool route_format_iso_time(int year, int month, int day, int hour, int minute, int second,
                           int utc_offset_sec, char *out, size_t out_len);

// ---- answers ------------------------------------------------------------------------------------

/**
 * @brief The places of an address search that are addresses or streets (towns, districts and
 * crossings are left out), best match first, at most `max`. Returns how many.
 */
int route_tomtom_parse_geocode(const char *json, size_t len, route_place_t *out, int max);
int route_here_parse_geocode(const char *json, size_t len, route_place_t *out, int max);

/** @brief Whether the bytes are a JSON object at all (an error page of a proxy is not). */
bool route_json_valid(const char *json, size_t len);

/** @brief The first route of an answer. False if there is none or it cannot be read. */
bool route_tomtom_parse_route(const char *json, size_t len, route_leg_t *out);
bool route_here_parse_route(const char *json, size_t len, route_leg_t *out);

/** @brief The provider's own words for an error answer (never anything of the request); "" if none.
 */
void route_error_text(const char *json, size_t len, char *out, size_t out_len);

// ---- decisions ----------------------------------------------------------------------------------

/** @brief The distance between two places in metres (great circle). */
double route_distance_m(double lat1, double lon1, double lat2, double lon2);

/**
 * @brief Whether a route between two places can be right: a time and a length that are positive,
 * not more than 12 hours, not shorter than the straight line between the places and not an absurd
 * detour of it, and two places that are not the same place.
 */
bool route_plausible(const route_leg_t *leg, double straight_m);

/**
 * @brief Whether a travel time is too long: more than `percent` over the reference and more than
 * `min_excess_min` minutes over it. A reference of 0 minutes (none set) is never exceeded.
 */
bool route_is_over(int seconds, int reference_min, int percent, int min_excess_min);

/** @brief The time in whole minutes for the display, at least 1: "28". */
int route_minutes(int seconds);

#endif
