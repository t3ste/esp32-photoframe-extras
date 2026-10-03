#include "route_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "config_manager.h"
#include "esp_log.h"
#include "http_fetch.h"

static const char *TAG = "route_service";

#define ROUTE_HTTP_TIMEOUT_MS 15000
#define ROUTE_MAX_BODY_BYTES (48 * 1024)
#define ROUTE_CACHE_SECONDS 300
#define ROUTE_CLOCK_VALID_AFTER 1700000000L  // Nov 2023: earlier means the clock was never set

typedef enum {
    PROVIDER_TOMTOM = 0,
    PROVIDER_HERE = 1,
    PROVIDER_COUNT,
} provider_t;

static const char *provider_name(provider_t provider)
{
    return provider == PROVIDER_TOMTOM ? "TomTom" : "HERE";
}

static const char *provider_key(provider_t provider)
{
    return provider == PROVIDER_TOMTOM ? config_manager_get_route_key_tomtom()
                                       : config_manager_get_route_key_here();
}

static bool provider_usable(provider_t provider)
{
    return route_key_valid(provider_key(provider));
}

const char *route_status_name(route_status_t status)
{
    switch (status) {
    case ROUTE_STATUS_OK:
        return "ok";
    case ROUTE_STATUS_NO_KEY:
        return "no_key";
    case ROUTE_STATUS_NO_PLACES:
        return "no_places";
    case ROUTE_STATUS_NO_NETWORK:
        return "no_network";
    case ROUTE_STATUS_KEY_REFUSED:
        return "key_refused";
    case ROUTE_STATUS_QUOTA:
        return "quota";
    case ROUTE_STATUS_NOT_FOUND:
        return "not_found";
    case ROUTE_STATUS_IMPLAUSIBLE:
        return "implausible";
    case ROUTE_STATUS_FAILED:
    default:
        return "failed";
    }
}

// ---------------------------------------------------------------------------------------------
// One request

typedef enum {
    FETCH_OK,
    FETCH_NO_ANSWER,    // the server did not answer
    FETCH_KEY_REFUSED,  // 401, 403
    FETCH_QUOTA,        // 429
    FETCH_REJECTED,     // 400, 404, 422: the request found nothing (no such address, no route)
    FETCH_FAILED,       // anything else
} fetch_result_t;

// A GET that asks once (a retry would spend the quota again). On any result but FETCH_OK the body
// is gone and `message` holds the provider's words, if it had any.
static fetch_result_t fetch(const char *url, char **body, size_t *len, char *message,
                            size_t message_len)
{
    *body = NULL;
    *len = 0;
    message[0] = '\0';
    int status = 0;
    esp_err_t err = http_fetch_get_once(url, ROUTE_HTTP_TIMEOUT_MS, ROUTE_MAX_BODY_BYTES, body, len,
                                        &status, NULL);
    if (err != ESP_OK) {
        free(*body);
        *body = NULL;
        return FETCH_NO_ANSWER;
    }
    if (status == 200 && *body) {
        return FETCH_OK;
    }
    if (*body) {
        route_error_text(*body, *len, message, message_len);
        free(*body);
        *body = NULL;
    }
    if (status == 401 || status == 403) {
        return FETCH_KEY_REFUSED;
    }
    if (status == 429) {
        return FETCH_QUOTA;
    }
    if (status == 400 || status == 404 || status == 422) {
        return FETCH_REJECTED;
    }
    return FETCH_FAILED;
}

// What went wrong with the providers that were asked
typedef struct {
    bool tried;
    bool refused;
    bool quota;
    bool not_found;
    char message[96];
} problems_t;

static void note(problems_t *problems, provider_t provider, fetch_result_t result,
                 const char *message)
{
    switch (result) {
    case FETCH_KEY_REFUSED:
        problems->refused = true;
        break;
    case FETCH_QUOTA:
        problems->quota = true;
        break;
    case FETCH_REJECTED:
        problems->not_found = true;
        break;
    default:
        break;
    }
    ESP_LOGW(TAG, "%s: %s", provider_name(provider),
             result == FETCH_NO_ANSWER     ? "no answer"
             : result == FETCH_KEY_REFUSED ? "the key was refused"
             : result == FETCH_QUOTA       ? "no requests left"
             : result == FETCH_REJECTED    ? "nothing found"
                                           : "the answer could not be used");
    if (message && message[0]) {
        snprintf(problems->message, sizeof(problems->message), "%s", message);
    }
}

static route_status_t problems_status(const problems_t *problems)
{
    if (!problems->tried) {
        return ROUTE_STATUS_NO_KEY;
    }
    if (problems->not_found) {
        return ROUTE_STATUS_NOT_FOUND;
    }
    if (problems->refused) {
        return ROUTE_STATUS_KEY_REFUSED;
    }
    if (problems->quota) {
        return ROUTE_STATUS_QUOTA;
    }
    return ROUTE_STATUS_FAILED;
}

static bool german(void)
{
    return strcmp(config_manager_get_overlay_language(), "de") == 0;
}

// The moment of now as the providers write times ("2026-10-03T07:30:00+02:00"); "" if the clock was
// never set (HERE then answers without the traffic).
static void iso_now(char *out, size_t out_len)
{
    out[0] = '\0';
    time_t now = time(NULL);
    if (now < ROUTE_CLOCK_VALID_AFTER) {
        return;
    }
    struct tm local, utc;
    localtime_r(&now, &local);
    gmtime_r(&now, &utc);
    int days = local.tm_yday - utc.tm_yday;
    if (local.tm_year != utc.tm_year) {
        days = local.tm_year > utc.tm_year ? 1 : -1;
    }
    int offset = days * 86400 + (local.tm_hour - utc.tm_hour) * 3600 +
                 (local.tm_min - utc.tm_min) * 60 + (local.tm_sec - utc.tm_sec);
    route_format_iso_time(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour,
                          local.tm_min, local.tm_sec, offset, out, out_len);
}

// ---------------------------------------------------------------------------------------------
// Addresses

route_status_t route_service_geocode(const char *text, route_place_t *out, int max, int *count,
                                     char *source, size_t source_len, char *message,
                                     size_t message_len)
{
    *count = 0;
    if (source_len > 0) {
        source[0] = '\0';
    }
    if (message_len > 0) {
        message[0] = '\0';
    }
    if (!text || text[0] == '\0') {
        return ROUTE_STATUS_NOT_FOUND;
    }
    problems_t problems;
    memset(&problems, 0, sizeof(problems));
    for (int p = 0; p < PROVIDER_COUNT; p++) {
        provider_t provider = (provider_t) p;
        if (!provider_usable(provider)) {
            continue;
        }
        char url[ROUTE_URL_MAX];
        bool built = provider == PROVIDER_TOMTOM
                         ? route_tomtom_geocode_url(text, german() ? "de-DE" : "en-GB",
                                                    provider_key(provider), url, sizeof(url))
                         : route_here_geocode_url(text, german() ? "de" : "en",
                                                  provider_key(provider), url, sizeof(url));
        if (!built) {
            continue;  // a text too long to ask for
        }
        problems.tried = true;
        char *body = NULL;
        size_t len = 0;
        char words[96];
        fetch_result_t result = fetch(url, &body, &len, words, sizeof(words));
        if (result != FETCH_OK) {
            note(&problems, provider, result, words);
            continue;
        }
        int found = provider == PROVIDER_TOMTOM ? route_tomtom_parse_geocode(body, len, out, max)
                                                : route_here_parse_geocode(body, len, out, max);
        bool readable = route_json_valid(body, len);
        free(body);
        if (found > 0) {
            *count = found;
            if (source_len > 0) {
                snprintf(source, source_len, "%s", provider_name(provider));
            }
            ESP_LOGI(TAG, "%d place(s) from %s", found, provider_name(provider));
            return ROUTE_STATUS_OK;
        }
        // answered, but no address or street in it - or not an answer of the service at all
        note(&problems, provider, readable ? FETCH_REJECTED : FETCH_FAILED, NULL);
    }
    if (message_len > 0) {
        snprintf(message, message_len, "%s", problems.message);
    }
    return problems_status(&problems);
}

// ---------------------------------------------------------------------------------------------
// Routes

// One direction: the providers in order, the first that gives a time wins.
static route_status_t route_one(double from_lat, double from_lon, double to_lat, double to_lon,
                                route_leg_t *leg, char *source, size_t source_len, char *message,
                                size_t message_len)
{
    problems_t problems;
    memset(&problems, 0, sizeof(problems));
    char depart[40];
    iso_now(depart, sizeof(depart));
    for (int p = 0; p < PROVIDER_COUNT; p++) {
        provider_t provider = (provider_t) p;
        if (!provider_usable(provider)) {
            continue;
        }
        char url[ROUTE_URL_MAX];
        bool built = provider == PROVIDER_TOMTOM
                         ? route_tomtom_route_url(from_lat, from_lon, to_lat, to_lon,
                                                  provider_key(provider), url, sizeof(url))
                         : route_here_route_url(from_lat, from_lon, to_lat, to_lon, depart,
                                                provider_key(provider), url, sizeof(url));
        if (!built) {
            continue;
        }
        problems.tried = true;
        char *body = NULL;
        size_t len = 0;
        char words[96];
        fetch_result_t result = fetch(url, &body, &len, words, sizeof(words));
        if (result != FETCH_OK) {
            note(&problems, provider, result, words);
            continue;
        }
        route_leg_t found;
        bool ok = provider == PROVIDER_TOMTOM ? route_tomtom_parse_route(body, len, &found)
                                              : route_here_parse_route(body, len, &found);
        free(body);
        if (ok) {
            *leg = found;
            if (source_len > 0) {
                snprintf(source, source_len, "%s", provider_name(provider));
            }
            return ROUTE_STATUS_OK;
        }
        note(&problems, provider, FETCH_FAILED, NULL);  // an answer without a route
    }
    if (message_len > 0) {
        snprintf(message, message_len, "%s", problems.message);
    }
    return problems_status(&problems);
}

static route_status_t route_both(double from_lat, double from_lon, double to_lat, double to_lon,
                                 route_times_t *out)
{
    memset(out, 0, sizeof(*out));
    route_status_t status = route_one(from_lat, from_lon, to_lat, to_lon, &out->there, out->source,
                                      sizeof(out->source), out->message, sizeof(out->message));
    if (status != ROUTE_STATUS_OK) {
        return status;
    }
    char other_source[12];
    status = route_one(to_lat, to_lon, from_lat, from_lon, &out->back, other_source,
                       sizeof(other_source), out->message, sizeof(out->message));
    return status;
}

route_status_t route_service_check(const char *from_text, const route_place_t *from,
                                   const char *to_text, const route_place_t *to, route_times_t *out)
{
    memset(out, 0, sizeof(*out));
    if (!from || !to || from->level < ROUTE_LEVEL_STREET || to->level < ROUTE_LEVEL_STREET) {
        return ROUTE_STATUS_NO_PLACES;
    }
    if (!provider_usable(PROVIDER_TOMTOM) && !provider_usable(PROVIDER_HERE)) {
        return ROUTE_STATUS_NO_KEY;
    }
    route_status_t status = route_both(from->lat, from->lon, to->lat, to->lon, out);
    if (status != ROUTE_STATUS_OK) {
        return status;
    }
    double straight = route_distance_m(from->lat, from->lon, to->lat, to->lon);
    if (!route_plausible(&out->there, straight) || !route_plausible(&out->back, straight)) {
        ESP_LOGW(TAG, "The route between the two places cannot be right (%d m, %d s there)",
                 out->there.meters, out->there.seconds);
        return ROUTE_STATUS_IMPLAUSIBLE;
    }
    // everything held: the places are taken over, and only now the check counts
    config_manager_set_route_place(0, from_text, from->label, from->lat, from->lon);
    config_manager_set_route_place(1, to_text, to->label, to->lat, to->lon);
    config_manager_set_route_checked(true);
    route_service_forget();
    ESP_LOGI(TAG, "Places checked: %d min there, %d min back (%s)",
             route_minutes(out->there.seconds), route_minutes(out->back.seconds), out->source);
    return ROUTE_STATUS_OK;
}

// ---------------------------------------------------------------------------------------------
// The times for the fuel page

static struct {
    bool valid;
    time_t at;
    double lat[2], lon[2];
    route_times_t times;
} cache;

void route_service_forget(void)
{
    cache.valid = false;
}

route_status_t route_service_times(route_times_t *out, bool wifi_connected)
{
    memset(out, 0, sizeof(*out));
    double lat[2], lon[2];
    if (!config_manager_get_route_checked() ||
        !config_manager_get_route_point(0, &lat[0], &lon[0]) ||
        !config_manager_get_route_point(1, &lat[1], &lon[1])) {
        return ROUTE_STATUS_NO_PLACES;
    }
    if (!provider_usable(PROVIDER_TOMTOM) && !provider_usable(PROVIDER_HERE)) {
        return ROUTE_STATUS_NO_KEY;
    }
    if (!wifi_connected) {
        return ROUTE_STATUS_NO_NETWORK;
    }
    time_t now = time(NULL);
    if (cache.valid && now >= cache.at && now - cache.at < ROUTE_CACHE_SECONDS &&
        cache.lat[0] == lat[0] && cache.lon[0] == lon[0] && cache.lat[1] == lat[1] &&
        cache.lon[1] == lon[1]) {
        *out = cache.times;
        return ROUTE_STATUS_OK;
    }
    route_status_t status = route_both(lat[0], lon[0], lat[1], lon[1], out);
    if (status != ROUTE_STATUS_OK) {
        return status;
    }
    cache.valid = true;
    cache.at = now;
    cache.lat[0] = lat[0];
    cache.lon[0] = lon[0];
    cache.lat[1] = lat[1];
    cache.lon[1] = lon[1];
    cache.times = *out;
    return ROUTE_STATUS_OK;
}
