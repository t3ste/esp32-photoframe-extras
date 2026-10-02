#include "market_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "config_manager.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "http_fetch.h"
#include "market_quotes.h"

static const char *TAG = "market_service";

#define MARKET_USER_AGENT "Mozilla/5.0 (compatible; esp32-photoframe)"
#define MARKET_HTTP_TIMEOUT_MS 15000
#define MARKET_MAX_BODY_BYTES (48 * 1024)  // Alpha Vantage's 100 days are about 21 KB
#define MARKET_CACHE_BUFFER_BYTES 4096     // four series of 30 days are about 3 KB
#define MARKET_CACHE_MAX_AGE_SECONDS (14L * 86400L)
#define MARKET_CLOCK_VALID_AFTER 1700000000L  // Nov 2023: earlier means the clock was never set

// ---------------------------------------------------------------------------------------------
// The cache file

// The whole text of a file, NULL if there is none. The caller frees it.
static char *read_text_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    char *text = malloc(MARKET_CACHE_BUFFER_BYTES + 1);
    if (!text) {
        fclose(file);
        return NULL;
    }
    size_t n = fread(text, 1, MARKET_CACHE_BUFFER_BYTES, file);
    fclose(file);
    text[n] = '\0';
    return text;
}

static void write_text_file(const char *path, const char *text, size_t len)
{
    FILE *file = fopen(path, "wb");
    if (!file) {
        ESP_LOGW(TAG, "The cache file cannot be written");
        return;
    }
    if (fwrite(text, 1, len, file) != len) {
        ESP_LOGW(TAG, "The cache file was not written completely");
    }
    fclose(file);
}

// ---------------------------------------------------------------------------------------------
// The requests

static bool build_url(market_provider_t provider, const char *symbol, char *url, size_t url_len)
{
    char mapped[MARKET_SYMBOL_LEN + 2];
    switch (provider) {
    case MARKET_TWELVEDATA:
        return market_twelvedata_symbol(symbol, mapped, sizeof(mapped)) &&
               market_twelvedata_url(mapped, config_manager_get_market_key_twelvedata(), url,
                                     url_len);
    case MARKET_ALPHAVANTAGE:
        return market_alphavantage_symbol(symbol, mapped, sizeof(mapped)) &&
               market_alphavantage_url(mapped, config_manager_get_market_key_alphavantage(), url,
                                       url_len);
    case MARKET_YAHOO:
    default:
        return market_yahoo_url(symbol, url, url_len);
    }
}

// Asks the sources for a symbol in the order of the plan; true and the series if one answered. A
// source that refuses the key, is out of quota or does not answer at all is left out for the rest
// of this run (`blocked`): asking again would only spend requests.
static bool fetch_symbol(const char *symbol, const market_options_t *options, market_quota_t *quota,
                         bool count_quota, bool blocked[MARKET_PROVIDER_COUNT],
                         market_series_t *out)
{
    market_provider_t order[MARKET_PROVIDER_COUNT];
    int n = market_plan(options, symbol, quota, order);
    for (int i = 0; i < n; i++) {
        market_provider_t provider = order[i];
        if (blocked[provider] || !market_quota_allows(quota, provider)) {
            continue;
        }
        char url[256];  // holds the API key: cleared after the request, never logged
        if (!build_url(provider, symbol, url, sizeof(url))) {
            continue;
        }
        if (count_quota && market_quota_limit(provider) > 0) {
            quota->used[provider]++;  // every request counts, also a failed one
        }
        char *body = NULL;
        int status = 0;
        esp_err_t err = http_fetch_get_once(url, MARKET_HTTP_TIMEOUT_MS, MARKET_MAX_BODY_BYTES,
                                            &body, NULL, &status, MARKET_USER_AGENT);
        memset(url, 0, sizeof(url));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "%s does not answer", market_provider_name(provider));
            blocked[provider] = true;
            continue;
        }
        market_parse_status_t result = market_parse_answer(provider, status, body, out);
        free(body);
        switch (result) {
        case MARKET_PARSE_OK:
            return true;
        case MARKET_PARSE_KEY_REFUSED:
            ESP_LOGW(TAG, "%s refused the key", market_provider_name(provider));
            blocked[provider] = true;
            break;
        case MARKET_PARSE_RATE_LIMITED:
            ESP_LOGW(TAG, "%s: too many requests", market_provider_name(provider));
            blocked[provider] = true;
            break;
        case MARKET_PARSE_NOT_FOUND:
            ESP_LOGI(TAG, "%s does not know %s", market_provider_name(provider), symbol);
            break;
        default:
            ESP_LOGW(TAG, "%s: unusable answer for %s (HTTP %d)", market_provider_name(provider),
                     symbol, status);
            break;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------------------------

void market_service_load(markets_screen_data_t *out, bool wifi_connected)
{
    memset(out, 0, sizeof(*out));

    char symbols[MARKET_MAX_SYMBOLS][MARKET_SYMBOL_LEN];
    int symbol_count =
        market_parse_symbols(config_manager_get_market_symbols(), symbols, MARKET_MAX_SYMBOLS);
    if (symbol_count == 0) {
        symbol_count = market_parse_symbols(MARKET_DEFAULT_SYMBOLS, symbols, MARKET_MAX_SYMBOLS);
    }
    market_options_t options = {
        .yahoo = config_manager_get_market_yahoo(),
        .twelvedata_key = market_key_valid(config_manager_get_market_key_twelvedata()),
        .alphavantage_key = market_key_valid(config_manager_get_market_key_alphavantage()),
    };
    if (!options.yahoo && !options.twelvedata_key && !options.alphavantage_key) {
        out->status = MARKETS_SCREEN_NO_SOURCE;
        return;
    }

    time_t now = time(NULL);
    bool clock_ok = now > MARKET_CLOCK_VALID_AFTER;

    market_series_t *cached =
        heap_caps_calloc(MARKET_MAX_SYMBOLS, sizeof(*cached), MALLOC_CAP_SPIRAM);
    int cached_count = 0;
    if (cached) {
        char *text = read_text_file(MARKET_CACHE_PATH);
        cached_count = market_cache_decode(text, cached, MARKET_MAX_SYMBOLS);
        free(text);
    }

    market_quota_t quota;
    market_quota_parse(config_manager_get_market_quota(), &quota);
    if (clock_ok) {
        market_quota_roll(&quota, (long) (now / 86400));
    }
    bool blocked[MARKET_PROVIDER_COUNT] = {false};
    char quota_before[MARKET_QUOTA_TEXT_MAX_LEN];
    market_quota_format(&quota, quota_before, sizeof(quota_before));
    bool any_fresh = false;

    out->count = symbol_count;
    for (int i = 0; i < symbol_count; i++) {
        market_series_t *series = &out->series[i];
        const market_series_t *old = NULL;
        for (int c = 0; c < cached_count; c++) {
            bool too_old = clock_ok && cached[c].fetched > 0 &&
                           (long) now - cached[c].fetched > MARKET_CACHE_MAX_AGE_SECONDS;
            if (!too_old && strcmp(cached[c].symbol, symbols[i]) == 0) {
                old = &cached[c];
            }
        }
        if (wifi_connected &&
            fetch_symbol(symbols[i], &options, &quota, clock_ok, blocked, series)) {
            snprintf(series->symbol, sizeof(series->symbol), "%s", symbols[i]);
            series->fetched = clock_ok ? (long) now : 0;
            series->stale = false;
            if (old && series->name[0] == '\0') {  // Twelve Data and Alpha Vantage give no name
                memcpy(series->name, old->name, sizeof(series->name));
            }
            if (old && series->currency[0] == '\0') {
                memcpy(series->currency, old->currency, sizeof(series->currency));
            }
            any_fresh = true;
            // the newest day of each chart, to see in the log how late an answer was
            ESP_LOGI(TAG, "%s from %s: %d points, the newest of %s", symbols[i],
                     market_provider_name(series->provider), series->count,
                     series->dates[series->count - 1]);
        } else if (old) {
            *series = *old;
            series->stale = true;
        } else {
            memset(series, 0, sizeof(*series));
            snprintf(series->symbol, sizeof(series->symbol), "%s", symbols[i]);
        }
    }

    bool any_data = false;
    for (int i = 0; i < symbol_count; i++) {
        any_data = any_data || out->series[i].count > 0;
    }
    if (any_data) {
        out->status = MARKETS_SCREEN_OK;
    } else {
        out->status = wifi_connected ? MARKETS_SCREEN_FETCH_FAILED : MARKETS_SCREEN_NO_NETWORK;
    }

    // when the newest price was fetched, in local time, for the note at the foot of the page
    // (nothing if the clock was never set: then no row has a time either)
    long newest = 0;
    for (int i = 0; i < symbol_count; i++) {
        if (out->series[i].count > 0 && out->series[i].fetched > newest) {
            newest = out->series[i].fetched;
        }
    }
    if (newest > MARKET_CLOCK_VALID_AFTER) {
        time_t moment = (time_t) newest;
        struct tm local;
        localtime_r(&moment, &local);
        out->updated_year = local.tm_year + 1900;
        out->updated_month = local.tm_mon + 1;
        out->updated_day = local.tm_mday;
        out->updated_hour = local.tm_hour;
        out->updated_minute = local.tm_min;
    }

    if (any_fresh) {  // remember what was good: the rows that have data, in the order shown
        market_series_t *keep =
            heap_caps_calloc(MARKET_MAX_SYMBOLS, sizeof(*keep), MALLOC_CAP_SPIRAM);
        char *text = malloc(MARKET_CACHE_BUFFER_BYTES);
        if (keep && text) {
            int keep_count = 0;
            for (int i = 0; i < symbol_count; i++) {
                if (out->series[i].count > 0) {
                    keep[keep_count++] = out->series[i];
                }
            }
            size_t len = market_cache_encode(keep, keep_count, text, MARKET_CACHE_BUFFER_BYTES);
            if (len > 0) {
                write_text_file(MARKET_CACHE_PATH, text, len);
            }
        }
        free(text);
        heap_caps_free(keep);
    }
    char quota_after[MARKET_QUOTA_TEXT_MAX_LEN];
    market_quota_format(&quota, quota_after, sizeof(quota_after));
    if (strcmp(quota_after, quota_before) != 0) {
        config_manager_set_market_quota(quota_after);
    }
    heap_caps_free(cached);

    int with_data = 0;
    for (int i = 0; i < symbol_count; i++) {
        with_data += out->series[i].count > 0 ? 1 : 0;
    }
    ESP_LOGI(TAG, "Markets: %d of %d symbols have data", with_data, symbol_count);
}
