#ifndef SCREEN_WEATHER_H
#define SCREEN_WEATHER_H

#include <stdbool.h>

#include "info_screens_core.h"
#include "screen_canvas.h"

/**
 * @file screen_weather.h
 * @brief The weather page (build option `weather-screen`): today's condition with a big icon and
 * big temperature digits, and the next days as rows with icon, high and low.
 *
 * The data are the daily values the frame's weather module fetches (Open-Meteo, wttr.in or yr.no):
 * the day's high and low and a WMO weather code. Pure drawing with no ESP-IDF dependency, so the
 * host tests and the render harness link it; the icons are drawn with shapes at any size (colours
 * on the colour panels: a yellow sun, blue rain), the condition texts are whole words in English
 * and German.
 */

#define WEATHER_SCREEN_MAX_DAYS 5  // today and the four days after it
#define WEATHER_SCREEN_PLACE_MAX 40

typedef struct {
    int year, month, day;
    int temp_max, temp_min;  // degrees Celsius, rounded
    int code;                // WMO weather code
} weather_screen_day_t;

// Why there is nothing to show (weather_screen_data_t.status).
typedef enum {
    WEATHER_SCREEN_OK = 0,
    WEATHER_SCREEN_NO_LOCATION,  // no place set in the settings
    WEATHER_SCREEN_NO_NETWORK,   // the frame is offline on this wake
    WEATHER_SCREEN_FETCH_FAILED  // the weather service did not answer
} weather_screen_status_t;

typedef struct {
    weather_screen_status_t status;
    char place[WEATHER_SCREEN_PLACE_MAX];  // UTF-8, as typed in the settings (may be empty)
    int day_count;                         // 0..WEATHER_SCREEN_MAX_DAYS, first = today
    weather_screen_day_t days[WEATHER_SCREEN_MAX_DAYS];
} weather_screen_data_t;

// What an icon shows.
typedef enum {
    WEATHER_KIND_CLEAR,
    WEATHER_KIND_MOSTLY_CLEAR,
    WEATHER_KIND_PARTLY_CLOUDY,
    WEATHER_KIND_OVERCAST,
    WEATHER_KIND_FOG,
    WEATHER_KIND_DRIZZLE,
    WEATHER_KIND_RAIN,
    WEATHER_KIND_FREEZING,
    WEATHER_KIND_SNOW,
    WEATHER_KIND_THUNDER,
    WEATHER_KIND_UNKNOWN
} weather_kind_t;

/** @brief The kind of picture for a WMO weather code. */
weather_kind_t weather_screen_kind(int wmo_code);

/**
 * @brief The condition of a WMO weather code in whole words ("Light rain", "Leichter Regen"), UTF-8
 * (pass it through canvas_text_from_utf8() to draw it).
 */
const char *weather_screen_condition(int wmo_code, bool german);

/** @brief Draws the icon of a kind inside the square of side `size` centred on (cx, cy). */
void weather_screen_draw_icon(canvas_t *canvas, int cx, int cy, int size, weather_kind_t kind);

/** @brief Draws the whole page (or a message when the data say there is nothing to show). */
void weather_screen_render(canvas_t *canvas, const info_now_t *now,
                           const weather_screen_data_t *data);

#endif
