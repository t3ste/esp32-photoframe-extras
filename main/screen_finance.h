#ifndef SCREEN_FINANCE_H
#define SCREEN_FINANCE_H

#include "fx_rates.h"
#include "info_screens_core.h"
#include "screen_canvas.h"

/**
 * @file screen_finance.h
 * @brief The exchange-rate page (build option `finance-snapshot`): a blue header with the date of
 * the newest rates, one row per currency with its rate against the euro in big digits, the change
 * against the working day before (green up, red down) and a sparkline of the last 30 working days.
 *
 * Pure drawing with no ESP-IDF dependency, so the host tests and the render harness link it.
 */

typedef enum {
    FINANCE_SCREEN_OK = 0,
    FINANCE_SCREEN_NO_NETWORK,   // the frame is offline on this wake
    FINANCE_SCREEN_FETCH_FAILED  // the ECB did not answer, or the answer had no rates
} finance_screen_status_t;

typedef struct {
    finance_screen_status_t status;
    int count;  // 0..FX_MAX_CURRENCIES
    fx_series_t series[FX_MAX_CURRENCIES];
} finance_screen_data_t;

/** @brief Draws the page (or a message when the data say there is nothing to show). */
void finance_screen_render(canvas_t *canvas, const info_now_t *now,
                           const finance_screen_data_t *data);

#endif
