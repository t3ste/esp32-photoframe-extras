#include "fuel_prices.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "json_scan.h"

const char *fuel_type_name(fuel_type_t type)
{
    switch (type) {
    case FUEL_E10:
        return "e10";
    case FUEL_DIESEL:
        return "diesel";
    case FUEL_E5:
    default:
        return "e5";
    }
}

fuel_type_t fuel_type_from_name(const char *name, fuel_type_t fallback)
{
    if (!name) {
        return fallback;
    }
    if (strcasecmp(name, "e5") == 0) {
        return FUEL_E5;
    }
    if (strcasecmp(name, "e10") == 0) {
        return FUEL_E10;
    }
    if (strcasecmp(name, "diesel") == 0) {
        return FUEL_DIESEL;
    }
    return fallback;
}

// A non-empty string of at most `max_len` characters that all come from `allowed`.
static bool only_allowed(const char *text, size_t max_len, const char *allowed)
{
    if (!text || text[0] == '\0' || strlen(text) > max_len) {
        return false;
    }
    for (const char *p = text; *p; p++) {
        if (!strchr(allowed, *p)) {
            return false;
        }
    }
    return true;
}

bool fuel_build_url(char *out, size_t out_len, const char *lat, const char *lon, int radius_km,
                    fuel_type_t type, const char *api_key)
{
    static const char *const number_chars = "0123456789.-+";
    static const char *const key_chars =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-";
    if (!only_allowed(lat, 16, number_chars) || !only_allowed(lon, 16, number_chars) ||
        !only_allowed(api_key, 64, key_chars) || strlen(api_key) < 8) {
        return false;
    }
    if (radius_km < 1) {
        radius_km = 1;
    }
    if (radius_km > 25) {
        radius_km = 25;
    }
    int n = snprintf(out, out_len,
                     "https://creativecommons.tankerkoenig.de/json/list.php"
                     "?lat=%s&lng=%s&rad=%d&sort=price&type=%s&apikey=%s",
                     lat, lon, radius_km, fuel_type_name(type), api_key);
    return n > 0 && (size_t) n < out_len;
}

// Copies a string member of a JSON object (empty if it is missing or not a string); the text is cut
// at a character boundary.
static void copy_string(const cJSON *object, const char *key, char *out, size_t out_len)
{
    out[0] = '\0';
    const cJSON *item = cJSON_GetObjectItemCaseSensitive((cJSON *) object, key);
    if (!cJSON_IsString(item) || !item->valuestring) {
        return;
    }
    size_t len = strlen(item->valuestring);
    if (len > out_len - 1) {
        len = out_len - 1;
        while (len > 0 && ((unsigned char) item->valuestring[len] & 0xC0) == 0x80) {
            len--;  // the cut would fall inside a character: leave it out
        }
    }
    memcpy(out, item->valuestring, len);
    out[len] = '\0';
}

// Appends `text` to `out` (of `size` bytes, `*used` filled), cut at a character boundary.
static void append_text(char *out, size_t size, size_t *used, const char *text)
{
    size_t len = strlen(text);
    if (*used + len > size - 1) {
        len = size - 1 - *used;
        while (len > 0 && ((unsigned char) text[len] & 0xC0) == 0x80) {
            len--;
        }
    }
    memcpy(out + *used, text, len);
    *used += len;
    out[*used] = '\0';
}

// Adds one station (a parsed JSON object) to the result if it has a price.
static void add_station(const cJSON *station, bool hide_closed, int max, fuel_result_t *out)
{
    const cJSON *price = cJSON_GetObjectItemCaseSensitive((cJSON *) station, "price");
    if (!cJSON_IsNumber(price) || !(price->valuedouble > 0.0)) {
        return;  // a missing price is never shown as 0
    }
    const cJSON *is_open = cJSON_GetObjectItemCaseSensitive((cJSON *) station, "isOpen");
    if (hide_closed && cJSON_IsFalse(is_open)) {
        return;
    }
    fuel_station_t entry;
    memset(&entry, 0, sizeof(entry));
    char brand[FUEL_NAME_MAX], name[FUEL_NAME_MAX], street[FUEL_NAME_MAX], town[FUEL_NAME_MAX];
    copy_string(station, "brand", brand, sizeof(brand));
    copy_string(station, "name", name, sizeof(name));
    copy_string(station, "street", street, sizeof(street));
    copy_string(station, "place", town, sizeof(town));
    memcpy(entry.name, brand[0] ? brand : name, FUEL_NAME_MAX);
    size_t used = 0;
    append_text(entry.place, sizeof(entry.place), &used, street);
    if (street[0] && town[0]) {
        append_text(entry.place, sizeof(entry.place), &used, ", ");
    }
    append_text(entry.place, sizeof(entry.place), &used, town);
    const cJSON *dist = cJSON_GetObjectItemCaseSensitive((cJSON *) station, "dist");
    entry.dist_km = cJSON_IsNumber(dist) ? (float) dist->valuedouble : 0.0f;
    entry.price = (float) price->valuedouble;

    // insert in price order (the nearer one first at the same price), keep the cheapest `max`
    int pos = out->count;
    while (pos > 0 && (out->stations[pos - 1].price > entry.price ||
                       (out->stations[pos - 1].price == entry.price &&
                        out->stations[pos - 1].dist_km > entry.dist_km))) {
        pos--;
    }
    if (pos >= max) {
        return;
    }
    int last = out->count < max ? out->count : max - 1;
    for (int i = last; i > pos; i--) {
        out->stations[i] = out->stations[i - 1];
    }
    out->stations[pos] = entry;
    if (out->count < max) {
        out->count++;
    }
}

#define FUEL_STATION_JSON_MAX 1536

// A small answer without a station list: an error message ("ok": false) or nonsense.
static void parse_error_answer(const char *json, fuel_result_t *out)
{
    out->status = FUEL_PARSE_BAD_JSON;
    if (strlen(json) > 4096) {
        return;
    }
    cJSON *root = cJSON_Parse(json);
    if (cJSON_IsObject(root) && cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(root, "ok"))) {
        out->status = FUEL_PARSE_API_ERROR;
        copy_string(root, "message", out->message, sizeof(out->message));
    }
    cJSON_Delete(root);
}

void fuel_parse(const char *json, bool hide_closed, int max, fuel_result_t *out)
{
    memset(out, 0, sizeof(*out));
    if (max > FUEL_MAX_STATIONS) {
        max = FUEL_MAX_STATIONS;
    }
    if (!json || json_skip_blanks(json)[0] != '{') {
        out->status = FUEL_PARSE_BAD_JSON;
        return;
    }
    // The stations are read one object at a time instead of building the tree of the whole answer
    // (up to 100 KB in a dense town), and an answer that was cut off still gives the stations
    // before the cut.
    const char *key = strstr(json, "\"stations\"");
    if (!key) {
        parse_error_answer(json, out);
        return;
    }
    const char *p = json_skip_blanks(key + strlen("\"stations\""));
    if (*p != ':') {
        out->status = FUEL_PARSE_BAD_JSON;
        return;
    }
    p = json_skip_blanks(p + 1);
    if (*p != '[') {
        out->status = FUEL_PARSE_BAD_JSON;
        return;
    }
    p++;
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ',') {
            p++;
        }
        if (*p != '{') {
            break;  // the end of the list, or of the text
        }
        const char *end = json_object_end(p);
        if (!end) {
            break;  // cut off inside a station
        }
        // one station at a time, parsed in place (a station is about 350 bytes; a huge one is
        // skipped)
        size_t length = (size_t) (end - p) + 1;
        if (length <= FUEL_STATION_JSON_MAX) {
            cJSON *station = cJSON_ParseWithLength(p, length);
            if (cJSON_IsObject(station)) {
                add_station(station, hide_closed, max, out);
            }
            cJSON_Delete(station);
        }
        p = end + 1;
    }
    out->status = out->count > 0 ? FUEL_PARSE_OK : FUEL_PARSE_EMPTY;
}

void fuel_format_price(float price, char *main_digits, size_t main_len, char *ninth,
                       size_t ninth_len)
{
    long thousandths = lroundf(price * 1000.0f);
    long cents = thousandths / 10;
    snprintf(main_digits, main_len, "%ld.%02ld", cents / 100, cents % 100);
    snprintf(ninth, ninth_len, "%ld", thousandths % 10);
}
