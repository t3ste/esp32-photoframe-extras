#ifndef SCREEN_FUEL_H
#define SCREEN_FUEL_H

#include <stdbool.h>

#include "feature_config.h"
#include "fuel_prices.h"
#include "info_screens_core.h"
#include "screen_canvas.h"

/**
 * @file screen_fuel.h
 * @brief The fuel-price page (build option `fuel-prices`): a yellow header with the fuel type and
 * the search radius, one row per station - the cheapest first, with its brand, street and town,
 * distance and the price in big digits with the third decimal small like on the pump - and the
 * attribution of the data with the time of the retrieval at the bottom.
 *
 * Pure drawing with no ESP-IDF dependency, so the host tests and the render harness link it.
 */

typedef enum {
    FUEL_SCREEN_OK = 0,
    FUEL_SCREEN_NO_KEY,        // no API key in the settings
    FUEL_SCREEN_NO_LOCATION,   // no place / coordinates in the settings
    FUEL_SCREEN_NO_NETWORK,    // the frame is offline on this wake
    FUEL_SCREEN_FETCH_FAILED,  // the service did not answer
    FUEL_SCREEN_KEY_REFUSED,   // the service refused the key (or the request)
    FUEL_SCREEN_NONE_FOUND     // no open station with a price in the radius
} fuel_screen_status_t;

#if FEATURE_ROUTE_TIME
// The travel time in the header (build option `route-time`): both ways, in whole minutes, and
// whether each is longer than usual (drawn as a red block with a "!").
typedef struct {
    bool shown;  // false: the header has no travel time
    int there_min, back_min;
    bool there_over, back_over;
    char label[12];  // what the user calls the route; "" = none
} fuel_route_t;
#endif

typedef struct {
    fuel_screen_status_t status;
    fuel_type_t type;
    int radius_km;
    fuel_result_t result;  // for OK; result.message is the service's text for KEY_REFUSED
#if FEATURE_ROUTE_TIME
    fuel_route_t route;
#endif
} fuel_screen_data_t;

/** @brief Draws the page (or a message when the data say there is nothing to show). */
void fuel_screen_render(canvas_t *canvas, const info_now_t *now, const fuel_screen_data_t *data);

#endif
