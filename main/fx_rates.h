#ifndef FX_RATES_H
#define FX_RATES_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @file fx_rates.h
 * @brief Exchange rates for the finance page (build option `finance-snapshot`): the request to the
 * ECB's data portal and the parser of its answer. The ECB publishes reference rates of the euro
 * against about 30 currencies every working day, free, without a key and with a stable interface,
 * which makes it the source of this page (stocks, crypto and commodities have no such source).
 * Pure C with no ESP-IDF dependency, so the host tests link it.
 *
 * The request is one GET for all currencies of the page:
 *   https://data-api.ecb.europa.eu/service/data/EXR/D.USD+GBP.EUR.SP00.A
 *     ?lastNObservations=30&format=csvdata&detail=dataonly
 * and the answer is CSV with one line per currency and working day:
 *   KEY,FREQ,CURRENCY,CURRENCY_DENOM,EXR_TYPE,EXR_SUFFIX,TIME_PERIOD,OBS_VALUE
 * A rate is the price of one euro in the currency.
 */

#define FX_MAX_CURRENCIES 4
#define FX_MAX_POINTS 30
#define FX_CODE_LEN 4  // three letters and the NUL
#define FX_DATE_LEN 11

typedef struct {
    char code[FX_CODE_LEN];
    int count;  // points, oldest first
    char dates[FX_MAX_POINTS][FX_DATE_LEN];
    float values[FX_MAX_POINTS];
} fx_series_t;

/**
 * @brief Reads a list of currency codes as a user types it ("usd, gbp; CHF"): separators are
 * commas, semicolons, spaces and line breaks; codes are three letters (made upper case), EUR is
 * left out (the rates are against it), doubles are dropped. Returns how many were written (at most
 * `max`); `codes` holds `max` strings of FX_CODE_LEN.
 */
int fx_parse_codes(const char *text, char codes[][FX_CODE_LEN], int max);

/**
 * @brief Builds the request for `count` currencies and the last `points` working days. Returns
 * false if there is no code or the buffer is too small.
 */
bool fx_build_url(char codes[][FX_CODE_LEN], int count, int points, char *out, size_t out_len);

/**
 * @brief Parses the CSV answer into one series per currency, in the order the currencies first
 * appear; the points of a series are sorted by date, rows without a value are skipped, and only the
 * newest FX_MAX_POINTS are kept. Quoted fields are handled. Returns the number of series (at most
 * `max`).
 */
int fx_parse_csv(const char *csv, fx_series_t *out, int max);

/**
 * @brief Puts the series in the order of the currency list the user typed (the ECB answers in the
 * order of its own keys) and drops those that are not in it. Returns the number of series left.
 */
int fx_order_series(fx_series_t *series, int count, char codes[][FX_CODE_LEN], int code_count);

/**
 * @brief Drops the series whose newest rate is more than `max_age_days` older than the given date:
 * the ECB still lists currencies it stopped publishing (RUB, HRK, BGN, ...), and their last rate
 * must not pass for a current one. Returns the number of series left, in their order.
 */
int fx_drop_stale(fx_series_t *series, int count, int year, int month, int day, int max_age_days);

/** @brief Change of the last point against the one before it, in percent (0 with fewer than two).
 */
float fx_change_percent(const fx_series_t *series);

/**
 * @brief The rate as text with a sensible number of decimals: 4 below 10, 3 below 100, 2 above
 * (1.1355 -> "1.1355", 162.34 -> "162.34").
 */
void fx_format_rate(float value, char *out, size_t out_len);

#endif
