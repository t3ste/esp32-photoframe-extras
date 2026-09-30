#ifndef SCREEN_MARKETS_H
#define SCREEN_MARKETS_H

#include "info_screens_core.h"
#include "market_quotes.h"
#include "screen_canvas.h"

/**
 * @file screen_markets.h
 * @brief The markets page (build option `market-quotes`): a blue header with the date of the newest
 * price, one row per symbol (stock, ETF, index, future, crypto, currency pair) with its name, the
 * last price in big digits, the change against the day before (green up, red down) and a sparkline
 * of the last 30 days, and a footer naming the sources the prices came from. A symbol without data
 * gets a row that says so; prices from the cache (the fetch failed) are drawn in blue.
 *
 * Pure drawing with no ESP-IDF dependency, so the host tests and the render harness link it.
 */

typedef enum {
    MARKETS_SCREEN_OK = 0,
    MARKETS_SCREEN_NO_NETWORK,   // the frame is offline on this wake and has nothing cached
    MARKETS_SCREEN_NO_SOURCE,    // Yahoo is off and no key is set
    MARKETS_SCREEN_FETCH_FAILED  // no source gave a price for any symbol
} markets_screen_status_t;

typedef struct {
    markets_screen_status_t status;
    int count;  // rows, 0..MARKET_MAX_SYMBOLS; a series with count 0 is a symbol without data
    market_series_t series[MARKET_MAX_SYMBOLS];
    // Local time of the newest fetch among the rows, for the note at the foot of the page ("Updated
    // 30 Sep 14:35"); all zero (no clock, or nothing fetched) leaves the note out.
    int updated_year, updated_month, updated_day, updated_hour, updated_minute;
} markets_screen_data_t;

/** @brief Draws the page (or a message when the data say there is nothing to show). */
void markets_screen_render(canvas_t *canvas, const info_now_t *now,
                           const markets_screen_data_t *data);

#endif
