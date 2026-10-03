#include "route_time.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"

#define ROUTE_JSON_MAX_BYTES (96 * 1024)

// ---------------------------------------------------------------------------------------------
// Requests

bool route_url_encode(const char *in, char *out, size_t out_len)
{
    static const char HEX[] = "0123456789ABCDEF";
    if (!in || !out || out_len == 0) {
        return false;
    }
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *) in; *p; p++) {
        bool plain = (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                     (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.' || *p == '~';
        size_t need = plain ? 1 : 3;
        if (o + need >= out_len) {
            out[0] = '\0';
            return false;
        }
        if (plain) {
            out[o++] = (char) *p;
        } else {
            out[o++] = '%';
            out[o++] = HEX[*p >> 4];
            out[o++] = HEX[*p & 15];
        }
    }
    out[o] = '\0';
    return true;
}

bool route_key_valid(const char *key)
{
    if (!key) {
        return false;
    }
    size_t len = strlen(key);
    if (len < 16 || len > 104) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        char c = key[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '-' || c == '_')) {
            return false;
        }
    }
    return true;
}

static bool finished(int written, size_t out_len)
{
    return written > 0 && (size_t) written < out_len;
}

static bool valid_point(double lat, double lon)
{
    return lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;
}

bool route_tomtom_geocode_url(const char *text, const char *language, const char *key, char *out,
                              size_t out_len)
{
    char encoded[ROUTE_TEXT_MAX * 3 + 1];
    if (!text || text[0] == '\0' || !key || key[0] == '\0' ||
        !route_url_encode(text, encoded, sizeof(encoded))) {
        return false;
    }
    char lang[56] = "";
    if (language && language[0]) {
        char encoded_lang[32];
        if (!route_url_encode(language, encoded_lang, sizeof(encoded_lang))) {
            return false;
        }
        snprintf(lang, sizeof(lang), "&language=%s", encoded_lang);
    }
    int n =
        snprintf(out, out_len, "https://api.tomtom.com/search/2/geocode/%s.json?key=%s&limit=%d%s",
                 encoded, key, ROUTE_CANDIDATES_MAX, lang);
    return finished(n, out_len);
}

bool route_tomtom_route_url(double from_lat, double from_lon, double to_lat, double to_lon,
                            const char *key, char *out, size_t out_len)
{
    if (!key || key[0] == '\0' || !valid_point(from_lat, from_lon) ||
        !valid_point(to_lat, to_lon)) {
        return false;
    }
    int n = snprintf(out, out_len,
                     "https://api.tomtom.com/routing/1/calculateRoute/%.6f,%.6f:%.6f,%.6f/json"
                     "?key=%s&traffic=true&travelMode=car&computeTravelTimeFor=all"
                     "&routeRepresentation=summaryOnly",
                     from_lat, from_lon, to_lat, to_lon, key);
    return finished(n, out_len);
}

bool route_here_geocode_url(const char *text, const char *language, const char *key, char *out,
                            size_t out_len)
{
    char encoded[ROUTE_TEXT_MAX * 3 + 1];
    if (!text || text[0] == '\0' || !key || key[0] == '\0' ||
        !route_url_encode(text, encoded, sizeof(encoded))) {
        return false;
    }
    char lang[56] = "";
    if (language && language[0]) {
        char encoded_lang[32];
        if (!route_url_encode(language, encoded_lang, sizeof(encoded_lang))) {
            return false;
        }
        snprintf(lang, sizeof(lang), "&lang=%s", encoded_lang);
    }
    int n = snprintf(out, out_len,
                     "https://geocode.search.hereapi.com/v1/geocode?q=%s&limit=%d%s&apiKey=%s",
                     encoded, ROUTE_CANDIDATES_MAX, lang, key);
    return finished(n, out_len);
}

bool route_here_route_url(double from_lat, double from_lon, double to_lat, double to_lon,
                          const char *depart_iso, const char *key, char *out, size_t out_len)
{
    if (!key || key[0] == '\0' || !valid_point(from_lat, from_lon) ||
        !valid_point(to_lat, to_lon)) {
        return false;
    }
    char time[64] = "";
    if (depart_iso && depart_iso[0]) {
        char encoded[48];
        if (!route_url_encode(depart_iso, encoded, sizeof(encoded))) {
            return false;
        }
        snprintf(time, sizeof(time), "&departureTime=%s", encoded);
    }
    int n = snprintf(out, out_len,
                     "https://router.hereapi.com/v8/routes?transportMode=car"
                     "&origin=%.6f,%.6f&destination=%.6f,%.6f&return=summary%s&apiKey=%s",
                     from_lat, from_lon, to_lat, to_lon, time, key);
    return finished(n, out_len);
}

bool route_format_iso_time(int year, int month, int day, int hour, int minute, int second,
                           int utc_offset_sec, char *out, size_t out_len)
{
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 ||
        minute > 59 || second < 0 || second > 60 || utc_offset_sec < -14 * 3600 ||
        utc_offset_sec > 14 * 3600) {
        return false;
    }
    char sign = utc_offset_sec < 0 ? '-' : '+';
    int offset = utc_offset_sec < 0 ? -utc_offset_sec : utc_offset_sec;
    int n = snprintf(out, out_len, "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d", year, month, day,
                     hour, minute, second, sign, offset / 3600, (offset % 3600) / 60);
    return finished(n, out_len);
}

// ---------------------------------------------------------------------------------------------
// Answers

static cJSON *parse(const char *json, size_t len)
{
    if (!json || len == 0 || len > ROUTE_JSON_MAX_BYTES) {
        return NULL;
    }
    return cJSON_ParseWithLength(json, len);
}

static cJSON *member(const cJSON *object, const char *key)
{
    return object ? cJSON_GetObjectItemCaseSensitive((cJSON *) object, key) : NULL;
}

static const char *member_text(const cJSON *object, const char *key)
{
    const cJSON *item = member(object, key);
    return (item && cJSON_IsString(item) && item->valuestring) ? item->valuestring : NULL;
}

static bool member_number(const cJSON *object, const char *key, double *out)
{
    const cJSON *item = member(object, key);
    if (!item || !cJSON_IsNumber(item)) {
        return false;
    }
    *out = item->valuedouble;
    return true;
}

// Copies a text; one that is too long is cut at a character boundary, so a name never ends in half
// an umlaut.
static void copy_text(char *dest, size_t size, const char *text)
{
    size_t len = text ? strlen(text) : 0;
    if (len >= size) {
        len = size - 1;
        while (len > 0 && ((unsigned char) text[len] & 0xC0) == 0x80) {
            len--;
        }
    }
    if (len > 0) {
        memcpy(dest, text, len);
    }
    dest[len] = '\0';
}

// Adds a place unless it is a duplicate; false when the list is full.
static bool add_place(route_place_t *out, int *count, int max, const route_place_t *place)
{
    for (int i = 0; i < *count; i++) {
        if (strcmp(out[i].label, place->label) == 0) {
            return true;  // the same place twice
        }
    }
    if (*count >= max) {
        return false;
    }
    out[(*count)++] = *place;
    return true;
}

// Best match first (the provider's order among equals), addresses before streets among equals.
static void sort_places(route_place_t *places, int count)
{
    for (int i = 1; i < count; i++) {
        route_place_t item = places[i];
        int j = i - 1;
        while (j >= 0 && (places[j].score < item.score ||
                          (places[j].score == item.score && places[j].level < item.level))) {
            places[j + 1] = places[j];
            j--;
        }
        places[j + 1] = item;
    }
}

bool route_json_valid(const char *json, size_t len)
{
    cJSON *root = parse(json, len);
    bool valid = root && cJSON_IsObject(root);
    cJSON_Delete(root);
    return valid;
}

int route_tomtom_parse_geocode(const char *json, size_t len, route_place_t *out, int max)
{
    cJSON *root = parse(json, len);
    if (!root) {
        return 0;
    }
    int count = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, member(root, "results"))
    {
        const char *type = member_text(entry, "type");
        int level = 0;
        if (type && (strcmp(type, "Point Address") == 0 || strcmp(type, "Address Range") == 0)) {
            level = ROUTE_LEVEL_ADDRESS;
        } else if (type && strcmp(type, "Street") == 0) {
            level = ROUTE_LEVEL_STREET;
        }
        const cJSON *position = member(entry, "position");
        double lat, lon;
        if (level == 0 || !member_number(position, "lat", &lat) ||
            !member_number(position, "lon", &lon) || !valid_point(lat, lon)) {
            continue;
        }
        route_place_t place;
        memset(&place, 0, sizeof(place));
        const cJSON *address = member(entry, "address");
        const char *label = member_text(address, "freeformAddress");
        if (!label || label[0] == '\0') {
            continue;
        }
        copy_text(place.label, sizeof(place.label), label);
        place.lat = lat;
        place.lon = lon;
        place.level = level;
        double score;
        if (member_number(member(entry, "matchConfidence"), "score", &score) && score >= 0.0) {
            place.score = (float) (score > 1.0 ? 1.0 : score);
        }
        if (!add_place(out, &count, max, &place)) {
            break;
        }
    }
    cJSON_Delete(root);
    sort_places(out, count);
    return count;
}

int route_here_parse_geocode(const char *json, size_t len, route_place_t *out, int max)
{
    cJSON *root = parse(json, len);
    if (!root) {
        return 0;
    }
    int count = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, member(root, "items"))
    {
        const char *type = member_text(entry, "resultType");
        int level = 0;
        if (type && strcmp(type, "houseNumber") == 0) {
            level = ROUTE_LEVEL_ADDRESS;
        } else if (type && strcmp(type, "street") == 0) {
            level = ROUTE_LEVEL_STREET;
        }
        const cJSON *position = member(entry, "position");
        double lat, lon;
        if (level == 0 || !member_number(position, "lat", &lat) ||
            !member_number(position, "lng", &lon) || !valid_point(lat, lon)) {
            continue;
        }
        route_place_t place;
        memset(&place, 0, sizeof(place));
        const char *label = member_text(member(entry, "address"), "label");
        if (!label || label[0] == '\0') {
            label = member_text(entry, "title");
        }
        if (!label || label[0] == '\0') {
            continue;
        }
        copy_text(place.label, sizeof(place.label), label);
        place.lat = lat;
        place.lon = lon;
        place.level = level;
        double score;
        if (member_number(member(entry, "scoring"), "queryScore", &score) && score >= 0.0) {
            place.score = (float) (score > 1.0 ? 1.0 : score);
        }
        if (!add_place(out, &count, max, &place)) {
            break;
        }
    }
    cJSON_Delete(root);
    sort_places(out, count);
    return count;
}

static int whole(double value)
{
    return (value > 0.0 && value < 2.0e9) ? (int) (value + 0.5) : 0;
}

bool route_tomtom_parse_route(const char *json, size_t len, route_leg_t *out)
{
    cJSON *root = parse(json, len);
    if (!root) {
        return false;
    }
    bool ok = false;
    const cJSON *route = cJSON_GetArrayItem((cJSON *) member(root, "routes"), 0);
    const cJSON *summary = member(route, "summary");
    double seconds, free_seconds, delay, meters;
    if (member_number(summary, "travelTimeInSeconds", &seconds) && whole(seconds) > 0) {
        memset(out, 0, sizeof(*out));
        out->seconds = whole(seconds);
        if (member_number(summary, "noTrafficTravelTimeInSeconds", &free_seconds) &&
            whole(free_seconds) > 0) {
            out->free_seconds = whole(free_seconds);
        } else if (member_number(summary, "trafficDelayInSeconds", &delay) && delay >= 0.0 &&
                   whole(seconds - delay) > 0) {
            out->free_seconds = whole(seconds - delay);
        }
        if (member_number(summary, "lengthInMeters", &meters)) {
            out->meters = whole(meters);
        }
        ok = true;
    }
    cJSON_Delete(root);
    return ok;
}

bool route_here_parse_route(const char *json, size_t len, route_leg_t *out)
{
    cJSON *root = parse(json, len);
    if (!root) {
        return false;
    }
    bool ok = false;
    const cJSON *route = cJSON_GetArrayItem((cJSON *) member(root, "routes"), 0);
    const cJSON *section;
    double seconds_sum = 0.0, base_sum = 0.0, meters_sum = 0.0;
    bool have_base = true;
    int sections = 0;
    cJSON_ArrayForEach(section, member(route, "sections"))
    {
        const cJSON *summary = member(section, "summary");
        double duration, base, length;
        if (!member_number(summary, "duration", &duration) || duration < 0.0) {
            seconds_sum = 0.0;
            sections = 0;
            break;
        }
        seconds_sum += duration;
        if (member_number(summary, "baseDuration", &base) && base >= 0.0) {
            base_sum += base;
        } else {
            have_base = false;
        }
        if (member_number(summary, "length", &length) && length >= 0.0) {
            meters_sum += length;
        }
        sections++;
    }
    if (sections > 0 && whole(seconds_sum) > 0) {
        memset(out, 0, sizeof(*out));
        out->seconds = whole(seconds_sum);
        out->free_seconds = have_base ? whole(base_sum) : 0;
        out->meters = whole(meters_sum);
        ok = true;
    }
    cJSON_Delete(root);
    return ok;
}

void route_error_text(const char *json, size_t len, char *out, size_t out_len)
{
    if (!out || out_len == 0) {
        return;
    }
    out[0] = '\0';
    cJSON *root = parse(json, len);
    if (!root) {
        return;
    }
    const char *text = member_text(member(root, "detailedError"), "message");
    static const char *const KEYS[] = {"error_description", "cause", "title", "message"};
    for (size_t i = 0; (!text || text[0] == '\0') && i < sizeof(KEYS) / sizeof(KEYS[0]); i++) {
        text = member_text(root, KEYS[i]);
    }
    if (text) {
        copy_text(out, out_len, text);
    }
    cJSON_Delete(root);
}

// ---------------------------------------------------------------------------------------------
// Decisions

double route_distance_m(double lat1, double lon1, double lat2, double lon2)
{
    const double R = 6371000.0;
    const double rad = 3.14159265358979323846 / 180.0;
    double dlat = (lat2 - lat1) * rad;
    double dlon = (lon2 - lon1) * rad;
    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(lat1 * rad) * cos(lat2 * rad) * sin(dlon / 2) * sin(dlon / 2);
    return 2.0 * R * asin(sqrt(a < 1.0 ? a : 1.0));
}

bool route_plausible(const route_leg_t *leg, double straight_m)
{
    if (!leg || leg->seconds <= 0 || leg->seconds > 12 * 3600 || leg->meters <= 0) {
        return false;
    }
    if (straight_m < 30.0) {
        return false;  // the same place twice
    }
    if ((double) leg->meters < straight_m * 0.95 - 50.0) {
        return false;  // shorter than the straight line
    }
    if ((double) leg->meters > straight_m * 8.0 + 3000.0) {
        return false;  // an absurd detour
    }
    if ((double) leg->meters / (double) leg->seconds > 70.0) {
        return false;  // faster than a car can go
    }
    return true;
}

bool route_is_over(int seconds, int reference_min, int percent, int min_excess_min)
{
    if (reference_min <= 0 || seconds <= 0) {
        return false;
    }
    if (percent < 0) {
        percent = 0;
    }
    if (min_excess_min < 0) {
        min_excess_min = 0;
    }
    int64_t reference = (int64_t) reference_min * 60;
    int64_t excess = (int64_t) seconds - reference;
    return excess * 100 > reference * percent && excess > (int64_t) min_excess_min * 60;
}

int route_minutes(int seconds)
{
    int minutes = (seconds + 30) / 60;
    return minutes < 1 ? 1 : minutes;
}
