#ifndef FUEL_PRICES_H
#define FUEL_PRICES_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @file fuel_prices.h
 * @brief Fuel prices for the fuel page (build option `fuel-prices`): the request to the
 * Tankerkoenig API (Germany, prices of the Markttransparenzstelle fuer Kraftstoffe; free with a
 * personal key from https://creativecommons.tankerkoenig.de, licence CC BY 4.0) and the parser of
 * its answer. Only depends on cJSON, so the host tests link it.
 *
 * The request is one GET:
 *   https://creativecommons.tankerkoenig.de/json/list.php?lat=..&lng=..&rad=..&sort=price&type=..&apikey=..
 * and the answer is JSON: {"ok":true,"stations":[{"brand":..,"name":..,"street":..,"place":..,
 * "dist":1.5,"price":1.799,"isOpen":true}, ...]} - or {"ok":false,"message":".."}.
 */

#define FUEL_MAX_STATIONS 5
#define FUEL_NAME_MAX 40
#define FUEL_PLACE_MAX 48
#define FUEL_MESSAGE_MAX 64

typedef enum { FUEL_E5 = 0, FUEL_E10, FUEL_DIESEL } fuel_type_t;

typedef struct {
    char name[FUEL_NAME_MAX];    // the brand, else the name (UTF-8)
    char place[FUEL_PLACE_MAX];  // "street, town" (UTF-8)
    float dist_km;
    float price;  // euros per litre, three decimals
} fuel_station_t;

typedef enum {
    FUEL_PARSE_OK = 0,
    FUEL_PARSE_API_ERROR,  // the service refused ("ok": false), for instance a wrong key
    FUEL_PARSE_BAD_JSON,   // not the answer we asked for
    FUEL_PARSE_EMPTY       // no station with a price
} fuel_parse_status_t;

typedef struct {
    fuel_parse_status_t status;
    char message[FUEL_MESSAGE_MAX];  // the service's own text for FUEL_PARSE_API_ERROR
    int count;
    fuel_station_t stations[FUEL_MAX_STATIONS];  // the cheapest first
} fuel_result_t;

/** @brief "e5", "e10" or "diesel". */
const char *fuel_type_name(fuel_type_t type);

/** @brief The type of a name ("e5", "E10", "diesel"); `fallback` for anything else. */
fuel_type_t fuel_type_from_name(const char *name, fuel_type_t fallback);

/**
 * @brief Builds the request. `lat` and `lon` are the decimal degrees as text, `radius_km` is
 * clamped to 1..25. False if a value is missing or has characters that do not belong in it (the key
 * is letters, digits and dashes), or the buffer is too small.
 */
bool fuel_build_url(char *out, size_t out_len, const char *lat, const char *lon, int radius_km,
                    fuel_type_t type, const char *api_key);

/**
 * @brief Parses an answer: the up to `max` (at most FUEL_MAX_STATIONS) cheapest stations that have
 * a price, without the closed ones if `hide_closed`. Never shows a missing price as 0.
 */
void fuel_parse(const char *json, bool hide_closed, int max, fuel_result_t *out);

/**
 * @brief Splits a price the way the pumps show it: 1.899 -> "1.89" and "9" (the third decimal is
 * shown small). `main_digits` needs 8 bytes, `ninth` 2.
 */
void fuel_format_price(float price, char *main_digits, size_t main_len, char *ninth,
                       size_t ninth_len);

#endif
