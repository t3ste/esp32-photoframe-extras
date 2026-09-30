#ifndef MARKET_QUOTES_H
#define MARKET_QUOTES_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @file market_quotes.h
 * @brief Prices and price histories of stocks, ETFs, indices, futures, crypto and currency pairs
 * for the markets page (build option `market-quotes`): the symbol list, the requests to three data
 * sources and the parsers of their answers, a per-day request counter, and a text cache of the last
 * good answers. Only depends on cJSON, so the host tests link it.
 *
 * The three sources are tried in this order for each symbol (a source is left out if it has no key,
 * does not know the symbol's kind, or its daily quota is used up):
 *   1. Yahoo Finance chart endpoint - no key, worldwide, but unofficial (the user can switch it
 * off) https://query1.finance.yahoo.com/v8/finance/chart/<symbol>?range=1mo&interval=1d
 *   2. Twelve Data - free key, 800 requests a day; US stocks and ETFs, forex, crypto
 *        https://api.twelvedata.com/time_series?symbol=..&interval=1day&outputsize=30&apikey=..
 *   3. Alpha Vantage - free key, 25 requests a day; also European and Asian listings by suffix
 *        https://www.alphavantage.co/query?function=TIME_SERIES_DAILY&symbol=..&apikey=..
 * Symbols are written the Yahoo way ("AAPL", "EUNL.DE", "^GDAXI", "GC=F", "BTC-EUR", "EURUSD=X");
 * the other two get them translated (or skipped if there is no equivalent).
 */

#define MARKET_MAX_SYMBOLS 4
#define MARKET_MAX_POINTS 30
#define MARKET_SYMBOL_LEN 16
#define MARKET_NAME_LEN 32
#define MARKET_CURRENCY_LEN 5
#define MARKET_DATE_LEN 11
#define MARKET_KEY_MAX 64

typedef enum {
    MARKET_YAHOO = 0,
    MARKET_TWELVEDATA,
    MARKET_ALPHAVANTAGE,
    MARKET_PROVIDER_COUNT
} market_provider_t;

/** @brief "Yahoo Finance", "Twelve Data", "Alpha Vantage". */
const char *market_provider_name(market_provider_t provider);

typedef struct {
    char symbol[MARKET_SYMBOL_LEN];      // as the user typed it (Yahoo notation)
    char name[MARKET_NAME_LEN];          // UTF-8, may be empty
    char currency[MARKET_CURRENCY_LEN];  // "USD", "EUR", "GBp", ..., may be empty
    int count;                           // points, oldest first; 0 = no data
    char dates[MARKET_MAX_POINTS][MARKET_DATE_LEN];
    float values[MARKET_MAX_POINTS];
    market_provider_t provider;  // where the points came from
    bool stale;                  // from the cache because the fetch failed
    long fetched;                // seconds since 1970 when it was fetched (0 = unknown)
} market_series_t;

typedef enum {
    MARKET_PARSE_OK = 0,
    MARKET_PARSE_NOT_FOUND,     // the source does not know the symbol
    MARKET_PARSE_KEY_REFUSED,   // the key is wrong (or the demo key)
    MARKET_PARSE_RATE_LIMITED,  // the quota is used up
    MARKET_PARSE_BAD            // not the answer we asked for, or no usable point
} market_parse_status_t;

// ---- symbols and requests -----------------------------------------------------------------------

/**
 * @brief Reads a list of symbols as a user types it ("aapl, eunl.de; ^GDAXI"): separators are
 * commas, semicolons, spaces and line breaks; symbols are made upper case and may hold letters,
 * digits and
 * `. - ^ = _`, at most MARKET_SYMBOL_LEN - 1 characters; doubles are dropped. Returns how many were
 * written (at most `max`).
 */
int market_parse_symbols(const char *text, char symbols[][MARKET_SYMBOL_LEN], int max);

/**
 * @brief Whether a key looks like one: letters and digits, 4 to MARKET_KEY_MAX characters (4 so
 * that the services' public `demo` keys pass; real ones are 16 and 32).
 */
bool market_key_valid(const char *key);

/** @brief The Yahoo request for a symbol (the symbol is percent-encoded). False if it does not fit.
 */
bool market_yahoo_url(const char *symbol, char *out, size_t out_len);

/**
 * @brief The symbol as Twelve Data writes it: US stocks and ETFs as they are (a share class "BRK-B"
 * becomes "BRK.B"), currency pairs "EURUSD=X" -> "EUR/USD", crypto "BTC-EUR" -> "BTC/EUR". False
 * for what the free plan has no equivalent of (indices, futures, listings with an exchange suffix).
 */
bool market_twelvedata_symbol(const char *symbol, char *out, size_t out_len);

/** @brief The Twelve Data request. False if a value is not valid or the buffer is too small. */
bool market_twelvedata_url(const char *td_symbol, const char *key, char *out, size_t out_len);

/**
 * @brief The symbol as Alpha Vantage writes it: US stocks and ETFs as they are, exchange suffixes
 * translated (".L" -> ".LON", ".DE" -> ".DEX", ".TO" -> ".TRT", ".V" -> ".TRV", ".BO" -> ".BSE",
 * ".SS" ->
 * ".SHH", ".SZ" -> ".SHZ"). False for indices, futures, currency pairs, crypto and other suffixes.
 */
bool market_alphavantage_symbol(const char *symbol, char *out, size_t out_len);

/** @brief The Alpha Vantage request. False if a value is not valid or the buffer is too small. */
bool market_alphavantage_url(const char *av_symbol, const char *key, char *out, size_t out_len);

// ---- the answers --------------------------------------------------------------------------------

/**
 * @brief Reads the answers of the three sources into a series (oldest point first, at most the
 * newest MARKET_MAX_POINTS with a value; `symbol` is taken from the answer, the caller replaces it
 * with the user's notation). Alpha Vantage's answer is read one day at a time, newest first, so a
 * cut-off answer still gives its beginning.
 */
market_parse_status_t market_parse_yahoo(const char *json, market_series_t *out);
market_parse_status_t market_parse_twelvedata(const char *json, market_series_t *out);
market_parse_status_t market_parse_alphavantage(const char *json, market_series_t *out);

/**
 * @brief Reads the answer of a source given its HTTP status and body (NULL if there was none): the
 * status decides where the source says so (401 and 403 = key refused, 429 = quota, 404 = unknown
 * symbol), the body where it does not (Twelve Data and Alpha Vantage answer errors in the body,
 * Yahoo too). A 200 without a usable body, and any other status, are MARKET_PARSE_BAD.
 */
market_parse_status_t market_parse_answer(market_provider_t provider, int http_status,
                                          const char *body, market_series_t *out);

/** @brief Change of the last point against the one before it, in percent (0 with fewer than two).
 */
float market_change_percent(const market_series_t *series);

/**
 * @brief The price as text: no decimals from 10000, two from 1, four below 1 and six below 0.01; a
 * currency pair ("EURUSD=X") gets four decimals up to 20 (1.1355), the usual two above (USD/JPY).
 */
void market_format_price(const char *symbol, float value, char *out, size_t out_len);

// ---- the quota ----------------------------------------------------------------------------------

/** @brief Requests made today per source (Yahoo is not counted: it has no published limit). */
typedef struct {
    long day;  // days since 1970 (UTC) the counts are for
    int used[MARKET_PROVIDER_COUNT];
} market_quota_t;

/** @brief The daily limit of a source: 0 for none (Yahoo), 800 for Twelve Data, 25 for Alpha
 * Vantage. */
int market_quota_limit(market_provider_t provider);

/** @brief Reads "day,yahoo,twelvedata,alphavantage"; anything unreadable gives an empty quota. */
void market_quota_parse(const char *text, market_quota_t *quota);
void market_quota_format(const market_quota_t *quota, char *out, size_t out_len);

/** @brief Starts a new day: the counts are zero if `today` is not the day of the quota. */
void market_quota_roll(market_quota_t *quota, long today);

/** @brief Whether one more request to the source is within its limit. */
bool market_quota_allows(const market_quota_t *quota, market_provider_t provider);

// ---- which source for which symbol
// ---------------------------------------------------------------

typedef struct {
    bool yahoo;           // the user allows Yahoo
    bool twelvedata_key;  // a Twelve Data key is set
    bool alphavantage_key;
} market_options_t;

/**
 * @brief The sources to try for a symbol, in order (Yahoo, Twelve Data, Alpha Vantage), leaving out
 * those that are off, have no key, cannot serve this kind of symbol, or have no quota left. Returns
 * how many were written to `order` (MARKET_PROVIDER_COUNT at most).
 */
int market_plan(const market_options_t *options, const char *symbol, const market_quota_t *quota,
                market_provider_t order[MARKET_PROVIDER_COUNT]);

// ---- the cache ----------------------------------------------------------------------------------

/**
 * @brief Writes series as text ("PF-MARKETS 1", then per series a line with symbol, name, currency,
 * source, fetch time and count, then one "date value" line per point). Returns the length, or 0 if
 * it does not fit.
 */
size_t market_cache_encode(const market_series_t *series, int count, char *out, size_t out_len);

/** @brief Reads what market_cache_encode() wrote; a damaged text gives the series before the
 * damage. */
int market_cache_decode(const char *text, market_series_t *out, int max);

#endif
