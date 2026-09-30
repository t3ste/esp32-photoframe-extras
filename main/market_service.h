#ifndef MARKET_SERVICE_H
#define MARKET_SERVICE_H

#include <stdbool.h>

#include "screen_markets.h"

/**
 * @file market_service.h
 * @brief The data of the markets page (build option `market-quotes`): reads the symbols and keys of
 * the settings, asks the sources in the order of market_plan() (Yahoo Finance, Twelve Data, Alpha
 * Vantage; the first one that answers wins), counts the requests of the quota-limited ones per day,
 * and keeps the last good answers in a text file on the storage, so a wake without network - or a
 * symbol that no source answers today - still shows the last known price (drawn in blue).
 *
 * The requests hold the personal API keys: they are never logged.
 */

/** @brief The symbols used while none are set: a stock, an ETF, an index and a crypto currency. */
#define MARKET_DEFAULT_SYMBOLS "AAPL, EUNL.DE, ^GDAXI, BTC-EUR"

/**
 * @brief Fills the data of the page: status, one row per symbol (a row without data if no source
 * and no cache had the symbol).
 *
 * @param wifi_connected Whether this wake has a network; without one only the cache is used.
 */
void market_service_load(markets_screen_data_t *out, bool wifi_connected);

#endif
