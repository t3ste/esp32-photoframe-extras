#include "market_quotes.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "json_scan.h"

const char *market_provider_name(market_provider_t provider)
{
    switch (provider) {
    case MARKET_TWELVEDATA:
        return "Twelve Data";
    case MARKET_ALPHAVANTAGE:
        return "Alpha Vantage";
    case MARKET_YAHOO:
    default:
        return "Yahoo Finance";
    }
}

// ---------------------------------------------------------------------------------------------
// Small helpers

// Copies text into a buffer, cut at a character boundary.
static void copy_text(char *dest, size_t size, const char *src)
{
    size_t len = src ? strlen(src) : 0;
    if (len > size - 1) {
        len = size - 1;
        while (len > 0 && ((unsigned char) src[len] & 0xC0) == 0x80) {
            len--;
        }
    }
    if (len > 0) {
        memcpy(dest, src, len);
    }
    dest[len] = '\0';
}

static bool ends_with(const char *text, const char *suffix)
{
    size_t a = strlen(text), b = strlen(suffix);
    return a >= b && strcmp(text + a - b, suffix) == 0;
}

static bool all_letters(const char *text, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (!isalpha((unsigned char) text[i])) {
            return false;
        }
    }
    return len > 0;
}

// Percent-encodes everything but letters, digits and "-._~".
static bool encode_component(const char *text, char *out, size_t out_len)
{
    size_t used = 0;
    for (const char *p = text; *p; p++) {
        unsigned char c = (unsigned char) *p;
        if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') {
            if (used + 2 > out_len) {
                return false;
            }
            out[used++] = (char) c;
        } else {
            if (used + 4 > out_len) {
                return false;
            }
            used += (size_t) snprintf(out + used, 4, "%%%02X", c);
        }
    }
    out[used] = '\0';
    return true;
}

static bool symbol_char(char c)
{
    return isalnum((unsigned char) c) || (c != '\0' && strchr(".-^=_", c) != NULL);
}

// A symbol as it may go into a request: letters, digits and . - ^ = _ / (the slash of "EUR/USD").
static bool request_symbol_valid(const char *symbol)
{
    size_t len = symbol ? strlen(symbol) : 0;
    if (len == 0 || len >= MARKET_SYMBOL_LEN) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (!symbol_char(symbol[i]) && symbol[i] != '/') {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// Symbols, keys and requests

int market_parse_symbols(const char *text, char symbols[][MARKET_SYMBOL_LEN], int max)
{
    int count = 0;
    if (!text) {
        return 0;
    }
    const char *p = text;
    while (*p && count < max) {
        while (*p == ',' || *p == ';' || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
        }
        const char *start = p;
        while (*p && *p != ',' && *p != ';' && *p != ' ' && *p != '\t' && *p != '\r' &&
               *p != '\n') {
            p++;
        }
        size_t len = (size_t) (p - start);
        if (len == 0 || len >= MARKET_SYMBOL_LEN) {
            continue;
        }
        char symbol[MARKET_SYMBOL_LEN];
        bool valid = true;
        for (size_t i = 0; i < len; i++) {
            valid = valid && symbol_char(start[i]);
            symbol[i] = (char) toupper((unsigned char) start[i]);
        }
        symbol[len] = '\0';
        if (!valid) {
            continue;
        }
        bool seen = false;
        for (int i = 0; i < count; i++) {
            seen = seen || strcmp(symbols[i], symbol) == 0;
        }
        if (!seen) {
            memcpy(symbols[count++], symbol, len + 1);
        }
    }
    return count;
}

bool market_key_valid(const char *key)
{
    size_t len = key ? strlen(key) : 0;
    if (len < 4 || len > MARKET_KEY_MAX) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (!isalnum((unsigned char) key[i])) {
            return false;
        }
    }
    return true;
}

bool market_yahoo_url(const char *symbol, char *out, size_t out_len)
{
    char encoded[3 * MARKET_SYMBOL_LEN];
    if (!request_symbol_valid(symbol) || strchr(symbol, '/') ||
        !encode_component(symbol, encoded, sizeof(encoded))) {  // Yahoo's notation has no slash
        return false;
    }
    int n = snprintf(out, out_len,
                     "https://query1.finance.yahoo.com/v8/finance/chart/%s?range=1mo&interval=1d",
                     encoded);
    return n > 0 && (size_t) n < out_len;
}

bool market_twelvedata_symbol(const char *symbol, char *out, size_t out_len)
{
    if (!request_symbol_valid(symbol) || strchr(symbol, '/') || symbol[0] == '^' ||
        ends_with(symbol, "=F")) {
        return false;
    }
    size_t len = strlen(symbol);
    char mapped[MARKET_SYMBOL_LEN + 2];
    if (ends_with(symbol, "=X")) {  // "EURUSD=X" -> "EUR/USD"
        if (len != 8 || !all_letters(symbol, 6)) {
            return false;
        }
        snprintf(mapped, sizeof(mapped), "%.3s/%.3s", symbol, symbol + 3);
    } else if (strchr(symbol, '=')) {
        return false;
    } else if (strchr(symbol, '.')) {
        return false;  // an exchange suffix: not on the free plan
    } else {
        const char *dash = strchr(symbol, '-');
        if (dash) {
            size_t base = (size_t) (dash - symbol);
            size_t quote = strlen(dash + 1);
            if (quote == 3 && base >= 2 && base <= 5 && all_letters(symbol, base) &&
                all_letters(dash + 1, 3)) {  // crypto "BTC-EUR" -> "BTC/EUR"
                snprintf(mapped, sizeof(mapped), "%.*s/%s", (int) base, symbol, dash + 1);
            } else if (quote == 1 && isalpha((unsigned char) dash[1])) {  // share class
                snprintf(mapped, sizeof(mapped), "%.*s.%s", (int) base, symbol, dash + 1);
            } else {
                return false;
            }
        } else {
            snprintf(mapped, sizeof(mapped), "%s", symbol);
        }
    }
    if (strlen(mapped) + 1 > out_len) {
        return false;
    }
    strcpy(out, mapped);
    return true;
}

bool market_twelvedata_url(const char *td_symbol, const char *key, char *out, size_t out_len)
{
    char encoded[3 * MARKET_SYMBOL_LEN];
    if (!request_symbol_valid(td_symbol) || !market_key_valid(key) ||
        !encode_component(td_symbol, encoded, sizeof(encoded))) {
        return false;
    }
    int n = snprintf(out, out_len,
                     "https://api.twelvedata.com/time_series?symbol=%s&interval=1day&outputsize=%d"
                     "&apikey=%s",
                     encoded, MARKET_MAX_POINTS, key);
    return n > 0 && (size_t) n < out_len;
}

bool market_alphavantage_symbol(const char *symbol, char *out, size_t out_len)
{
    if (!request_symbol_valid(symbol) || strchr(symbol, '/') || symbol[0] == '^' ||
        strchr(symbol, '=')) {
        return false;
    }
    static const struct {
        const char *yahoo;
        const char *alphavantage;
    } suffixes[] = {{"L", "LON"},  {"DE", "DEX"}, {"TO", "TRT"}, {"V", "TRV"},
                    {"BO", "BSE"}, {"SS", "SHH"}, {"SZ", "SHZ"}};
    const char *dot = strrchr(symbol, '.');
    char mapped[MARKET_SYMBOL_LEN + 2];
    if (dot) {
        bool found = false;
        for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
            if (strcmp(dot + 1, suffixes[i].yahoo) == 0) {
                snprintf(mapped, sizeof(mapped), "%.*s.%s", (int) (dot - symbol), symbol,
                         suffixes[i].alphavantage);
                found = true;
            }
        }
        if (!found) {
            return false;
        }
    } else {
        const char *dash = strchr(symbol, '-');
        if (dash && strlen(dash + 1) == 3) {
            return false;  // crypto: another function of the service
        }
        snprintf(mapped, sizeof(mapped), "%s", symbol);
    }
    if (strlen(mapped) + 1 > out_len) {
        return false;
    }
    strcpy(out, mapped);
    return true;
}

bool market_alphavantage_url(const char *av_symbol, const char *key, char *out, size_t out_len)
{
    char encoded[3 * MARKET_SYMBOL_LEN];
    if (!request_symbol_valid(av_symbol) || !market_key_valid(key) ||
        !encode_component(av_symbol, encoded, sizeof(encoded))) {
        return false;
    }
    int n = snprintf(out, out_len,
                     "https://www.alphavantage.co/query?function=TIME_SERIES_DAILY&symbol=%s"
                     "&apikey=%s",
                     encoded, key);
    return n > 0 && (size_t) n < out_len;
}

// ---------------------------------------------------------------------------------------------
// The answers

static void series_init(market_series_t *series, market_provider_t provider)
{
    memset(series, 0, sizeof(*series));
    series->provider = provider;
}

// Appends a point at the end; a full series drops its oldest point.
static void append_point(market_series_t *series, const char *date, float value)
{
    if (series->count == MARKET_MAX_POINTS) {
        memmove(series->dates[0], series->dates[1],
                (size_t) (MARKET_MAX_POINTS - 1) * MARKET_DATE_LEN);
        memmove(&series->values[0], &series->values[1], (MARKET_MAX_POINTS - 1) * sizeof(float));
        series->count--;
    }
    memcpy(series->dates[series->count], date, MARKET_DATE_LEN - 1);
    series->dates[series->count][MARKET_DATE_LEN - 1] = '\0';
    series->values[series->count] = value;
    series->count++;
}

static void reverse_points(market_series_t *series)
{
    for (int i = 0, j = series->count - 1; i < j; i++, j--) {
        char date[MARKET_DATE_LEN];
        memcpy(date, series->dates[i], MARKET_DATE_LEN);
        memcpy(series->dates[i], series->dates[j], MARKET_DATE_LEN);
        memcpy(series->dates[j], date, MARKET_DATE_LEN);
        float value = series->values[i];
        series->values[i] = series->values[j];
        series->values[j] = value;
    }
}

// "YYYY-MM-DD" of a time in seconds since 1970 (Howard Hinnant's civil-from-days).
static void date_from_seconds(long long seconds, char out[MARKET_DATE_LEN])
{
    long long days = seconds >= 0 ? seconds / 86400 : -((-seconds + 86399) / 86400);
    days += 719468;
    long long era = (days >= 0 ? days : days - 146096) / 146097;
    long long doe = days - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = yoe + era * 400;
    long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp = (5 * doy + 2) / 153;
    int day = (int) (doy - (153 * mp + 2) / 5 + 1);
    int month = (int) (mp < 10 ? mp + 3 : mp - 9);
    if (month <= 2) {
        y++;
    }
    char text[32];  // roomy on purpose: the compiler cannot know the year is four digits
    snprintf(text, sizeof(text), "%04d-%02d-%02d", (int) (y % 10000), month, day);
    memcpy(out, text, MARKET_DATE_LEN - 1);
    out[MARKET_DATE_LEN - 1] = 0;
}

// A price a parser accepts: positive and not absurd (also keeps the float conversion in range).
static bool plausible_price(double value)
{
    return value > 0.0 && value < 1e12;
}

// A time in seconds since 1970 a parser accepts (1970 to 2100); a NaN fails the comparison too.
static bool plausible_seconds(double value)
{
    return value >= 0.0 && value < 4.1e9;
}

static const char *string_of(const cJSON *object, const char *key)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive((cJSON *) object, key);
    return (cJSON_IsString(item) && item->valuestring) ? item->valuestring : NULL;
}

market_parse_status_t market_parse_yahoo(const char *json, market_series_t *out)
{
    series_init(out, MARKET_YAHOO);
    cJSON *root = json ? cJSON_Parse(json) : NULL;
    const cJSON *chart = cJSON_GetObjectItemCaseSensitive(root, "chart");
    if (!cJSON_IsObject(chart)) {
        cJSON_Delete(root);
        return MARKET_PARSE_BAD;
    }
    const cJSON *error = cJSON_GetObjectItemCaseSensitive((cJSON *) chart, "error");
    if (cJSON_IsObject(error)) {
        const char *code = string_of(error, "code");
        market_parse_status_t status =
            (code && strcmp(code, "Not Found") == 0) ? MARKET_PARSE_NOT_FOUND : MARKET_PARSE_BAD;
        cJSON_Delete(root);
        return status;
    }
    const cJSON *results = cJSON_GetObjectItemCaseSensitive((cJSON *) chart, "result");
    const cJSON *result = cJSON_IsArray(results) ? cJSON_GetArrayItem(results, 0) : NULL;
    const cJSON *meta = cJSON_GetObjectItemCaseSensitive((cJSON *) result, "meta");
    const cJSON *stamps = cJSON_GetObjectItemCaseSensitive((cJSON *) result, "timestamp");
    const cJSON *indicators = cJSON_GetObjectItemCaseSensitive((cJSON *) result, "indicators");
    const cJSON *quotes = cJSON_GetObjectItemCaseSensitive((cJSON *) indicators, "quote");
    const cJSON *quote = cJSON_IsArray(quotes) ? cJSON_GetArrayItem(quotes, 0) : NULL;
    const cJSON *closes = cJSON_GetObjectItemCaseSensitive((cJSON *) quote, "close");
    if (!cJSON_IsObject(meta) || !cJSON_IsArray(stamps) || !cJSON_IsArray(closes)) {
        cJSON_Delete(root);
        return MARKET_PARSE_BAD;
    }
    const cJSON *offset = cJSON_GetObjectItemCaseSensitive((cJSON *) meta, "gmtoffset");
    long long gmt_offset = 0;
    if (cJSON_IsNumber(offset) && offset->valuedouble > -86400.0 && offset->valuedouble < 86400.0) {
        gmt_offset = (long long) offset->valuedouble;
    }
    int n = cJSON_GetArraySize(stamps);
    if (cJSON_GetArraySize(closes) < n) {
        n = cJSON_GetArraySize(closes);
    }
    for (int i = 0; i < n; i++) {
        const cJSON *stamp = cJSON_GetArrayItem(stamps, i);
        const cJSON *close = cJSON_GetArrayItem(closes, i);
        if (!cJSON_IsNumber(stamp) || !cJSON_IsNumber(close) ||
            !plausible_seconds(stamp->valuedouble) || !plausible_price(close->valuedouble)) {
            continue;  // a day without a price (holiday, no trade)
        }
        char date[MARKET_DATE_LEN];
        date_from_seconds((long long) stamp->valuedouble + gmt_offset, date);
        append_point(out, date, (float) close->valuedouble);
    }
    const char *symbol = string_of(meta, "symbol");
    const char *name = string_of(meta, "longName");
    if (!name) {
        name = string_of(meta, "shortName");
    }
    copy_text(out->symbol, sizeof(out->symbol), symbol);
    copy_text(out->name, sizeof(out->name), name);
    copy_text(out->currency, sizeof(out->currency), string_of(meta, "currency"));
    cJSON_Delete(root);
    return out->count > 0 ? MARKET_PARSE_OK : MARKET_PARSE_BAD;
}

market_parse_status_t market_parse_twelvedata(const char *json, market_series_t *out)
{
    series_init(out, MARKET_TWELVEDATA);
    cJSON *root = json ? cJSON_Parse(json) : NULL;
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return MARKET_PARSE_BAD;
    }
    const char *status_text = string_of(root, "status");
    if (status_text && strcmp(status_text, "error") == 0) {
        const cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
        int value = cJSON_IsNumber(code) ? code->valueint : 0;
        cJSON_Delete(root);
        if (value == 401 || value == 403) {
            return MARKET_PARSE_KEY_REFUSED;
        }
        if (value == 429) {
            return MARKET_PARSE_RATE_LIMITED;
        }
        if (value == 400 || value == 404) {
            return MARKET_PARSE_NOT_FOUND;
        }
        return MARKET_PARSE_BAD;
    }
    const cJSON *meta = cJSON_GetObjectItemCaseSensitive(root, "meta");
    const cJSON *values = cJSON_GetObjectItemCaseSensitive(root, "values");
    if (!cJSON_IsArray(values)) {
        cJSON_Delete(root);
        return MARKET_PARSE_BAD;
    }
    // newest first: take the newest MARKET_MAX_POINTS, then turn them round
    const cJSON *entry;
    cJSON_ArrayForEach(entry, values)
    {
        if (out->count >= MARKET_MAX_POINTS) {
            break;
        }
        const char *date = string_of(entry, "datetime");
        const char *close = string_of(entry, "close");
        if (!date || !close || strlen(date) < 10) {
            continue;
        }
        char *stop = NULL;
        double value = strtod(close, &stop);
        if (stop == close || *stop != '\0' || !plausible_price(value)) {
            continue;
        }
        char day[MARKET_DATE_LEN];
        memcpy(day, date, MARKET_DATE_LEN - 1);
        day[MARKET_DATE_LEN - 1] = '\0';
        append_point(out, day, (float) value);
    }
    reverse_points(out);
    copy_text(out->symbol, sizeof(out->symbol), string_of(meta, "symbol"));
    const char *currency = string_of(meta, "currency");
    if (!currency) {  // forex and crypto give names ("US Dollar"): the code is behind the slash
        const char *slash = strchr(out->symbol, '/');
        currency = slash ? slash + 1 : NULL;
    }
    if (currency && strlen(currency) < MARKET_CURRENCY_LEN) {
        copy_text(out->currency, sizeof(out->currency), currency);
    }
    cJSON_Delete(root);
    return out->count > 0 ? MARKET_PARSE_OK : MARKET_PARSE_BAD;
}

// The text of a string member ("Key": "text") found in a JSON text, copied to `out`; false if
// missing.
static bool find_string_member(const char *json, const char *key, char *out, size_t out_len)
{
    char pattern[48];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) {
        return false;
    }
    p = json_skip_blanks(p + strlen(pattern));
    if (*p != ':') {
        return false;
    }
    p = json_skip_blanks(p + 1);
    if (*p != '"') {
        return false;
    }
    p++;
    size_t len = 0;
    while (p[len] && p[len] != '"' && len + 1 < out_len) {
        len++;
    }
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

static bool contains_ignoring_case(const char *text, const char *word)
{
    size_t n = strlen(word);
    for (const char *p = text; *p; p++) {
        if (strncasecmp(p, word, n) == 0) {
            return true;
        }
    }
    return false;
}

// An answer without daily prices: a message of the service.
static market_parse_status_t alphavantage_message(const char *json)
{
    if (strlen(json) > 4096) {
        return MARKET_PARSE_BAD;
    }
    char text[256];
    if (find_string_member(json, "Note", text, sizeof(text))) {
        return MARKET_PARSE_RATE_LIMITED;
    }
    if (find_string_member(json, "Information", text, sizeof(text))) {
        if (contains_ignoring_case(text, "demo") || contains_ignoring_case(text, "api key")) {
            return MARKET_PARSE_KEY_REFUSED;
        }
        return MARKET_PARSE_RATE_LIMITED;
    }
    if (find_string_member(json, "Error Message", text, sizeof(text))) {
        return contains_ignoring_case(text, "apikey") ? MARKET_PARSE_KEY_REFUSED
                                                      : MARKET_PARSE_NOT_FOUND;
    }
    return MARKET_PARSE_BAD;
}

market_parse_status_t market_parse_alphavantage(const char *json, market_series_t *out)
{
    series_init(out, MARKET_ALPHAVANTAGE);
    if (!json) {
        return MARKET_PARSE_BAD;
    }
    const char *key = strstr(json, "\"Time Series (Daily)\"");
    if (!key) {
        return alphavantage_message(json);
    }
    char symbol[MARKET_SYMBOL_LEN];
    if (find_string_member(json, "2. Symbol", symbol, sizeof(symbol))) {
        copy_text(out->symbol, sizeof(out->symbol), symbol);
    }
    const char *p = json_skip_blanks(key + strlen("\"Time Series (Daily)\""));
    if (*p != ':') {
        return MARKET_PARSE_BAD;
    }
    p = json_skip_blanks(p + 1);
    if (*p != '{') {
        return MARKET_PARSE_BAD;
    }
    p++;
    // one day at a time, newest first; an answer that was cut off still gives its beginning
    while (out->count < MARKET_MAX_POINTS) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ',') {
            p++;
        }
        if (*p != '"') {
            break;
        }
        const char *date = p + 1;
        const char *quote = strchr(date, '"');
        if (!quote || quote - date != MARKET_DATE_LEN - 1) {
            break;
        }
        p = json_skip_blanks(quote + 1);
        if (*p != ':') {
            break;
        }
        p = json_skip_blanks(p + 1);
        if (*p != '{') {
            break;
        }
        const char *end = json_object_end(p);
        if (!end) {
            break;
        }
        cJSON *day = cJSON_ParseWithLength(p, (size_t) (end - p) + 1);
        const char *close = day ? string_of(day, "4. close") : NULL;
        char *stop = NULL;
        double value = close ? strtod(close, &stop) : 0.0;
        if (close && stop != close && *stop == '\0' && plausible_price(value)) {
            char stamp[MARKET_DATE_LEN];
            memcpy(stamp, date, MARKET_DATE_LEN - 1);
            stamp[MARKET_DATE_LEN - 1] = '\0';
            append_point(out, stamp, (float) value);
        }
        cJSON_Delete(day);
        p = end + 1;
    }
    reverse_points(out);
    return out->count > 0 ? MARKET_PARSE_OK : MARKET_PARSE_BAD;
}

market_parse_status_t market_parse_answer(market_provider_t provider, int http_status,
                                          const char *body, market_series_t *out)
{
    series_init(out, provider);
    market_parse_status_t from_body = MARKET_PARSE_BAD;
    if (body && body[0]) {
        switch (provider) {
        case MARKET_TWELVEDATA:
            from_body = market_parse_twelvedata(body, out);
            break;
        case MARKET_ALPHAVANTAGE:
            from_body = market_parse_alphavantage(body, out);
            break;
        case MARKET_YAHOO:
        default:
            from_body = market_parse_yahoo(body, out);
            break;
        }
    }
    if (http_status == 200) {
        return from_body;
    }
    series_init(out, provider);  // no points from an error answer
    if (http_status == 401 || http_status == 403) {
        return MARKET_PARSE_KEY_REFUSED;
    }
    if (http_status == 429) {
        return MARKET_PARSE_RATE_LIMITED;
    }
    if (http_status == 404) {
        return MARKET_PARSE_NOT_FOUND;
    }
    // another status: only a message of the source in the body still tells what happened
    return (from_body == MARKET_PARSE_KEY_REFUSED || from_body == MARKET_PARSE_RATE_LIMITED ||
            from_body == MARKET_PARSE_NOT_FOUND)
               ? from_body
               : MARKET_PARSE_BAD;
}

float market_change_percent(const market_series_t *series)
{
    if (!series || series->count < 2) {
        return 0.0f;
    }
    float last = series->values[series->count - 1];
    float previous = series->values[series->count - 2];
    return previous > 0.0f ? (last - previous) / previous * 100.0f : 0.0f;
}

void market_format_price(const char *symbol, float value, char *out, size_t out_len)
{
    int decimals = 2;
    if (value >= 10000.0f) {
        decimals = 0;
    } else if (symbol && ends_with(symbol, "=X") && value < 20.0f) {
        decimals = 4;
    } else if (value < 0.01f) {
        decimals = 6;
    } else if (value < 1.0f) {
        decimals = 4;
    }
    snprintf(out, out_len, "%.*f", decimals, (double) value);
}

// ---------------------------------------------------------------------------------------------
// The quota

int market_quota_limit(market_provider_t provider)
{
    switch (provider) {
    case MARKET_TWELVEDATA:
        return 800;
    case MARKET_ALPHAVANTAGE:
        return 25;
    default:
        return 0;
    }
}

void market_quota_parse(const char *text, market_quota_t *quota)
{
    memset(quota, 0, sizeof(*quota));
    long day = 0;
    int used[MARKET_PROVIDER_COUNT] = {0};
    if (text && sscanf(text, "%ld,%d,%d,%d", &day, &used[0], &used[1], &used[2]) == 4 && day > 0) {
        quota->day = day;
        for (int i = 0; i < MARKET_PROVIDER_COUNT; i++) {
            quota->used[i] = used[i] < 0 ? 0 : used[i];
        }
    }
}

void market_quota_format(const market_quota_t *quota, char *out, size_t out_len)
{
    snprintf(out, out_len, "%ld,%d,%d,%d", quota->day, quota->used[0], quota->used[1],
             quota->used[2]);
}

void market_quota_roll(market_quota_t *quota, long today)
{
    if (quota->day != today) {
        memset(quota, 0, sizeof(*quota));
        quota->day = today;
    }
}

bool market_quota_allows(const market_quota_t *quota, market_provider_t provider)
{
    int limit = market_quota_limit(provider);
    return limit == 0 || quota->used[provider] < limit;
}

// ---------------------------------------------------------------------------------------------
// The plan

int market_plan(const market_options_t *options, const char *symbol, const market_quota_t *quota,
                market_provider_t order[MARKET_PROVIDER_COUNT])
{
    int count = 0;
    char mapped[MARKET_SYMBOL_LEN + 2];
    if (options->yahoo && request_symbol_valid(symbol) && !strchr(symbol, '/')) {
        order[count++] = MARKET_YAHOO;
    }
    if (options->twelvedata_key && market_quota_allows(quota, MARKET_TWELVEDATA) &&
        market_twelvedata_symbol(symbol, mapped, sizeof(mapped))) {
        order[count++] = MARKET_TWELVEDATA;
    }
    if (options->alphavantage_key && market_quota_allows(quota, MARKET_ALPHAVANTAGE) &&
        market_alphavantage_symbol(symbol, mapped, sizeof(mapped))) {
        order[count++] = MARKET_ALPHAVANTAGE;
    }
    return count;
}

// ---------------------------------------------------------------------------------------------
// The cache

#define CACHE_HEADER "PF-MARKETS 1\n"

size_t market_cache_encode(const market_series_t *series, int count, char *out, size_t out_len)
{
    size_t used = 0;
    int n = snprintf(out, out_len, CACHE_HEADER);
    if (n < 0 || (size_t) n >= out_len) {
        return 0;
    }
    used = (size_t) n;
    for (int i = 0; i < count; i++) {
        const market_series_t *s = &series[i];
        char name[MARKET_NAME_LEN];
        copy_text(name, sizeof(name), s->name);
        for (char *c = name; *c; c++) {
            if (*c == '|' || (unsigned char) *c < 0x20) {
                *c = ' ';
            }
        }
        n = snprintf(out + used, out_len - used, "S|%s|%s|%s|%d|%ld|%d\n", s->symbol, name,
                     s->currency, (int) s->provider, s->fetched, s->count);
        if (n < 0 || (size_t) n >= out_len - used) {
            return 0;
        }
        used += (size_t) n;
        for (int p = 0; p < s->count; p++) {
            n = snprintf(out + used, out_len - used, "%s %.8g\n", s->dates[p],
                         (double) s->values[p]);
            if (n < 0 || (size_t) n >= out_len - used) {
                return 0;
            }
            used += (size_t) n;
        }
    }
    return used;
}

// The next line of `p` (without its end) copied into `line`; false at the end of the text.
static bool next_line(const char **p, char *line, size_t line_len)
{
    if (**p == '\0') {
        return false;
    }
    size_t len = 0;
    while ((*p)[len] && (*p)[len] != '\n') {
        len++;
    }
    size_t copy = len < line_len - 1 ? len : line_len - 1;
    memcpy(line, *p, copy);
    line[copy] = '\0';
    *p += len;
    if (**p == '\n') {
        (*p)++;
    }
    return true;
}

// Splits `line` at '|' into up to `max` fields in place.
static int split_bars(char *line, char **fields, int max)
{
    int n = 0;
    char *p = line;
    while (n < max) {
        fields[n++] = p;
        char *bar = strchr(p, '|');
        if (!bar) {
            break;
        }
        *bar = '\0';
        p = bar + 1;
    }
    return n;
}

int market_cache_decode(const char *text, market_series_t *out, int max)
{
    int count = 0;
    if (!text || strncmp(text, CACHE_HEADER, strlen(CACHE_HEADER)) != 0) {
        return 0;
    }
    const char *p = text + strlen(CACHE_HEADER);
    char line[160];
    while (count < max && next_line(&p, line, sizeof(line))) {
        char *fields[8];
        if (line[0] != 'S' || line[1] != '|' || split_bars(line + 2, fields, 8) != 6) {
            break;
        }
        market_series_t series;
        memset(&series, 0, sizeof(series));
        copy_text(series.symbol, sizeof(series.symbol), fields[0]);
        copy_text(series.name, sizeof(series.name), fields[1]);
        copy_text(series.currency, sizeof(series.currency), fields[2]);
        int provider = atoi(fields[3]);
        long fetched = atol(fields[4]);
        int points = atoi(fields[5]);
        if (series.symbol[0] == '\0' || provider < 0 || provider >= MARKET_PROVIDER_COUNT ||
            points < 1 || points > MARKET_MAX_POINTS) {
            break;
        }
        series.provider = (market_provider_t) provider;
        series.fetched = fetched;
        bool complete = true;
        for (int i = 0; i < points; i++) {
            char *stop = NULL;
            if (!next_line(&p, line, sizeof(line)) || strlen(line) < 12 || line[10] != ' ') {
                complete = false;
                break;
            }
            float value = strtof(line + 11, &stop);
            if (stop == line + 11 || !plausible_price((double) value)) {
                complete = false;
                break;
            }
            line[10] = '\0';
            append_point(&series, line, value);
        }
        if (!complete) {
            break;
        }
        out[count++] = series;
    }
    return count;
}
