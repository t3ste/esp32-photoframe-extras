#include "info_screens.h"

#include <string.h>
#include <time.h>

#include "board_hal.h"
#include "config.h"
#include "config_manager.h"
#include "display_manager.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "feature_config.h"
#include "image_processor.h"
#include "info_screens_core.h"
#include "screen_canvas.h"
#if FEATURE_CHORE_WHEEL
#include "screen_chore_wheel.h"
#endif
#if FEATURE_FACT_OF_THE_DAY
#include "fact_service.h"
#include "screen_fact.h"
#endif
#if FEATURE_FUEL_PRICES
#include <stdlib.h>

#include "fuel_prices.h"
#include "http_fetch.h"
#include "screen_fuel.h"
#include "weather.h"
#if FEATURE_ROUTE_TIME
#include "route_service.h"
#endif
#endif
#if FEATURE_FINANCE_SNAPSHOT
#include <stdlib.h>

#include "fx_rates.h"
#include "http_fetch.h"
#include "screen_finance.h"
#endif
#if FEATURE_MARKET_QUOTES
#include <stdlib.h>

#include "market_service.h"
#include "screen_markets.h"
#endif
#if FEATURE_WEATHER_SCREEN
#include <math.h>
#include <stdio.h>

#include "screen_weather.h"
#include "weather.h"
#endif
#if FEATURE_RECIPES
#include <stdlib.h>

#include "recipe_service.h"
#include "screen_recipe.h"
#endif

static const char *TAG = "info_screens";

static const char *const SCREEN_NAMES[INFO_SCREEN_COUNT] = {
    "agenda", "chore-wheel", "weather", "fact", "finance", "fuel", "markets", "recipe"};

const char *info_screen_name(int id)
{
    return (id >= 0 && id < INFO_SCREEN_COUNT) ? SCREEN_NAMES[id] : NULL;
}

int info_screen_id_from_name(const char *name)
{
    for (int i = 0; name && i < INFO_SCREEN_COUNT; i++) {
        if (strcmp(name, SCREEN_NAMES[i]) == 0) {
            return i;
        }
    }
    return -1;
}

uint32_t info_screens_compiled_mask(void)
{
    uint32_t mask = 1u << INFO_SCREEN_AGENDA;
#if FEATURE_CHORE_WHEEL
    mask |= 1u << INFO_SCREEN_CHORE_WHEEL;
#endif
#if FEATURE_WEATHER_SCREEN
    mask |= 1u << INFO_SCREEN_WEATHER;
#endif
#if FEATURE_FACT_OF_THE_DAY
    mask |= 1u << INFO_SCREEN_FACT;
#endif
#if FEATURE_FINANCE_SNAPSHOT
    mask |= 1u << INFO_SCREEN_FINANCE;
#endif
#if FEATURE_FUEL_PRICES
    mask |= 1u << INFO_SCREEN_FUEL;
#endif
#if FEATURE_MARKET_QUOTES
    mask |= 1u << INFO_SCREEN_MARKETS;
#endif
#if FEATURE_RECIPES
    mask |= 1u << INFO_SCREEN_RECIPE;
#endif
    return mask;
}

bool info_screens_extra_enabled(void)
{
    uint32_t extra = config_manager_get_info_screens_mask() & info_screens_compiled_mask() &
                     ~(1u << INFO_SCREEN_AGENDA);
    return extra != 0;
}

int info_screens_next(bool agenda_has_content)
{
    uint32_t stored = config_manager_get_info_screens_mask();
    uint32_t mask = info_screens_effective_mask(stored, info_screens_compiled_mask(), stored,
                                                agenda_has_content, INFO_SCREEN_AGENDA);
    uint32_t counter = config_manager_get_info_screens_rotation();
    int pick = info_rotation_pick(mask, counter, INFO_SCREEN_COUNT);
    if (pick < 0) {
        return INFO_SCREEN_AGENDA;
    }
    if (info_rotation_size(mask, INFO_SCREEN_COUNT) > 1) {
        config_manager_set_info_screens_rotation(counter + 1);
    }
    return pick;
}

#if FEATURE_SCHEDULE_PAGES
int info_screens_next_for(uint32_t mask, int schedule, bool agenda_has_content)
{
    // A schedule may only draw a page that is also currently ticked under "Information screens" -
    // otherwise a page the user has since switched off there would keep being drawn by a schedule
    // that was given it earlier. When nothing of this schedule's pages is available right now, -1
    // tells the caller to fall back to the shared rotation, the same as a schedule with no pages at
    // all.
    mask = info_screens_effective_mask(mask, info_screens_compiled_mask(),
                                       config_manager_get_info_screens_mask(), agenda_has_content,
                                       INFO_SCREEN_AGENDA);
    uint32_t counter = config_manager_get_sched_rotation(schedule);
    int pick = info_rotation_pick(mask, counter, INFO_SCREEN_COUNT);
    if (pick < 0) {
        return -1;
    }
    if (info_rotation_size(mask, INFO_SCREEN_COUNT) > 1) {
        config_manager_set_sched_rotation(schedule, (uint16_t) (counter + 1));
    }
    return pick;
}
#endif

// Writes the canvas as the file the display shows, and shows it.
static esp_err_t show_canvas(const canvas_t *canvas)
{
    image_format_t actual = IMAGE_FORMAT_PNG;
    esp_err_t err =
        image_processor_write_rgb_to_fmt(canvas->rgb, canvas->width, canvas->height,
                                         INFO_SCREEN_OUTPUT_PATH, IMAGE_FORMAT_PNG, &actual);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write the screen: %s", esp_err_to_name(err));
        return err;
    }
    return display_manager_show_image(INFO_SCREEN_OUTPUT_PATH);
}

#if FEATURE_WEATHER_SCREEN
// The forecast for the weather page - or the reason why there is none.
static void load_weather(weather_screen_data_t *out, bool wifi_connected)
{
    memset(out, 0, sizeof(*out));
    const char *place = config_manager_get_weather_location_name();
    const char *lat = config_manager_get_weather_lat();
    const char *lon = config_manager_get_weather_lon();
    bool have_place = (place && place[0]) || (lat && lat[0] && lon && lon[0]);
    if (!have_place) {
        out->status = WEATHER_SCREEN_NO_LOCATION;
        return;
    }
    if (place) {
        strncpy(out->place, place, sizeof(out->place) - 1);
    }
    if (!wifi_connected) {
        out->status = WEATHER_SCREEN_NO_NETWORK;
        return;
    }
    weather_forecast_t forecast;
    memset(&forecast, 0, sizeof(forecast));
    if (weather_fetch_forecast(&forecast, WEATHER_SCREEN_MAX_DAYS) != ESP_OK || !forecast.valid ||
        forecast.count < 1) {
        out->status = WEATHER_SCREEN_FETCH_FAILED;
        return;
    }
    out->status = WEATHER_SCREEN_OK;
    for (int i = 0; i < forecast.count && i < WEATHER_SCREEN_MAX_DAYS; i++) {
        weather_screen_day_t *day = &out->days[out->day_count];
        if (sscanf(forecast.days[i].date, "%d-%d-%d", &day->year, &day->month, &day->day) != 3) {
            continue;
        }
        day->temp_max = (int) lroundf(forecast.days[i].temp_max_c);
        day->temp_min = (int) lroundf(forecast.days[i].temp_min_c);
        day->code = forecast.days[i].weather_code;
        out->day_count++;
    }
    if (out->day_count < 1) {
        out->status = WEATHER_SCREEN_FETCH_FAILED;
    }
}
#endif

#if FEATURE_FINANCE_SNAPSHOT
#define FINANCE_DEFAULT_CURRENCIES "USD, GBP, CHF, JPY"
#define FINANCE_MAX_RATE_AGE_DAYS 14  // a rate older than this is a currency the ECB dropped

// The rates for the exchange-rate page - or the reason why there are none.
static void load_finance(finance_screen_data_t *out, const info_now_t *now, bool wifi_connected)
{
    memset(out, 0, sizeof(*out));
    char codes[FX_MAX_CURRENCIES][FX_CODE_LEN];
    int code_count = fx_parse_codes(config_manager_get_fx_currencies(), codes, FX_MAX_CURRENCIES);
    if (code_count == 0) {
        code_count = fx_parse_codes(FINANCE_DEFAULT_CURRENCIES, codes, FX_MAX_CURRENCIES);
    }
    if (!wifi_connected) {
        out->status = FINANCE_SCREEN_NO_NETWORK;
        return;
    }
    char url[256];
    char *body = NULL;
    if (!fx_build_url(codes, code_count, FX_MAX_POINTS, url, sizeof(url)) ||
        http_fetch_get(url, 15000, 32 * 1024, &body, NULL, NULL, NULL) != ESP_OK || !body) {
        out->status = FINANCE_SCREEN_FETCH_FAILED;
        free(body);
        return;
    }
    int count = fx_parse_csv(body, out->series, FX_MAX_CURRENCIES);
    free(body);
    count = fx_order_series(out->series, count, codes, code_count);
    count = fx_drop_stale(out->series, count, now->year, now->month, now->day,
                          FINANCE_MAX_RATE_AGE_DAYS);
    out->count = count;
    out->status = count > 0 ? FINANCE_SCREEN_OK : FINANCE_SCREEN_FETCH_FAILED;
    ESP_LOGI(TAG, "Exchange rates for %d of %d currencies", count, code_count);
}
#endif

#if FEATURE_FUEL_PRICES
// The prices for the fuel page - or the reason why there are none. The request holds the API key:
// it is never logged.
static void load_fuel(fuel_screen_data_t *out, bool wifi_connected)
{
    memset(out, 0, sizeof(*out));
    out->type = (fuel_type_t) config_manager_get_fuel_type();
    out->radius_km = config_manager_get_fuel_radius_km();
    const char *key = config_manager_get_fuel_api_key();
    if (key[0] == '\0') {
        out->status = FUEL_SCREEN_NO_KEY;
        return;
    }
    if (!wifi_connected) {
        out->status = FUEL_SCREEN_NO_NETWORK;
        return;
    }
    // the place is the weather's: coordinates, or a name that the weather module looks up once
    const char *lat = config_manager_get_weather_lat();
    const char *lon = config_manager_get_weather_lon();
    if ((!lat[0] || !lon[0]) && config_manager_get_weather_location_name()[0] != '\0') {
        weather_forecast_t forecast;
        memset(&forecast, 0, sizeof(forecast));
        weather_fetch_forecast(&forecast, 1);  // the lookup stores the coordinates
        lat = config_manager_get_weather_lat();
        lon = config_manager_get_weather_lon();
    }
    if (!lat[0] || !lon[0]) {
        out->status = FUEL_SCREEN_NO_LOCATION;
        return;
    }
    char url[320];
    char *body = NULL;
    if (!fuel_build_url(url, sizeof(url), lat, lon, out->radius_km, out->type, key) ||
        http_fetch_get(url, 15000, 96 * 1024, &body, NULL, NULL, NULL) != ESP_OK || !body) {
        out->status = FUEL_SCREEN_FETCH_FAILED;
        free(body);
        return;
    }
    fuel_parse(body, config_manager_get_fuel_hide_closed(), config_manager_get_fuel_count(),
               &out->result);
    free(body);
    switch (out->result.status) {
    case FUEL_PARSE_OK:
        out->status = FUEL_SCREEN_OK;
        break;
    case FUEL_PARSE_API_ERROR:
        out->status = FUEL_SCREEN_KEY_REFUSED;
        break;
    case FUEL_PARSE_EMPTY:
        out->status = FUEL_SCREEN_NONE_FOUND;
        break;
    default:
        out->status = FUEL_SCREEN_FETCH_FAILED;
        break;
    }
    ESP_LOGI(TAG, "Fuel prices: %d station(s)", out->result.count);
#if FEATURE_ROUTE_TIME
    // the travel time in the header: only with the places checked and an answer of the provider; a
    // failure leaves the header as it was
    if (out->status == FUEL_SCREEN_OK && config_manager_get_route_enabled()) {
        route_times_t times;
        route_status_t route_status = route_service_times(&times, wifi_connected);
        if (route_status == ROUTE_STATUS_OK) {
            int percent = config_manager_get_route_percent();
            int excess = config_manager_get_route_min_excess();
            out->route.shown = true;
            out->route.there_min = route_minutes(times.there.seconds);
            out->route.back_min = route_minutes(times.back.seconds);
            out->route.there_over = route_is_over(times.there.seconds,
                                                  config_manager_get_route_ref(0), percent, excess);
            out->route.back_over =
                route_is_over(times.back.seconds, config_manager_get_route_ref(1), percent, excess);
            snprintf(out->route.label, sizeof(out->route.label), "%s",
                     config_manager_get_route_label());
            ESP_LOGI(TAG, "Travel time: %d / %d min%s%s", out->route.there_min, out->route.back_min,
                     out->route.there_over ? ", there is long" : "",
                     out->route.back_over ? ", back is long" : "");
        } else {
            ESP_LOGW(TAG, "No travel time: %s", route_status_name(route_status));
        }
    }
#endif
}
#endif

#if FEATURE_RECIPES
// The recipe page: drawn for the orientation the user chose (a tall canvas for portrait on a wide
// panel and the other way round, turned into the panel's own layout like a photo in portrait
// orientation), with a recipe from the source or the last one. With no recipe at all the page is
// skipped - nothing is redrawn - unless it is the only page there is: then a message is drawn.
static esp_err_t render_recipe(canvas_t *native, const info_now_t *now, bool wifi_connected)
{
    bool landscape = config_manager_get_display_orientation() == DISPLAY_ORIENTATION_LANDSCAPE;
    int long_side = native->width > native->height ? native->width : native->height;
    int short_side = native->width > native->height ? native->height : native->width;
    int width = landscape ? long_side : short_side;
    int height = landscape ? short_side : long_side;

    canvas_t target = *native;
    uint8_t *turned = NULL;
    if (width != native->width || height != native->height) {
        turned = heap_caps_malloc((size_t) width * height * 3, MALLOC_CAP_SPIRAM);
        if (!turned) {
            return ESP_ERR_NO_MEM;
        }
        target.rgb = turned;
        target.width = width;
        target.height = height;
    }
    recipe_t *recipe = heap_caps_calloc(1, sizeof(*recipe), MALLOC_CAP_SPIRAM);
    recipe_screen_data_t *data = calloc(1, sizeof(*data));
    recipe_outcome_t *outcome = calloc(1, sizeof(*outcome));
    esp_err_t err = ESP_ERR_NO_MEM;
    if (recipe && data && outcome) {
        recipe_options_t options;
        config_manager_get_recipe_options(&options);
        recipe_result_t result = recipe_service_load(recipe, outcome, &options, wifi_connected,
                                                     width, height, landscape);
        data->options = options;
        data->grayscale = strncmp(BOARD_HAL_DISPLAY_TYPE, "gc", 2) == 0;
        err = ESP_OK;
        if (result == RECIPE_RESULT_NONE) {
            uint32_t mask = config_manager_get_info_screens_mask() & info_screens_compiled_mask();
            if (!(config_manager_get_agenda_todo_enabled() ||
                  config_manager_get_agenda_cal_enabled())) {
                mask &= ~(1u << INFO_SCREEN_AGENDA);
            }
            if (info_rotation_size(mask, INFO_SCREEN_COUNT) > 1) {
                ESP_LOGW(TAG, "No recipe: this page is skipped");
                err = ESP_ERR_NOT_FOUND;
            }
            data->status = wifi_connected ? RECIPE_SCREEN_NO_RECIPE : RECIPE_SCREEN_NO_NETWORK;
        } else {
            data->status = RECIPE_SCREEN_OK;
            data->recipe = recipe;
            data->warnings = outcome->warnings;
            data->photo = outcome->photo;
            data->photo_w = outcome->photo_w;
            data->photo_h = outcome->photo_h;
        }
        if (err == ESP_OK) {
            recipe_screen_render(&target, landscape, now, data);
            if (turned) {
                canvas_rotate_cw(&target, native);
            }
        }
    }
    free(outcome ? outcome->photo : NULL);
    free(outcome);
    free(data);
    heap_caps_free(recipe);
    heap_caps_free(turned);
    return err;
}
#endif

esp_err_t info_screens_show(int id, bool wifi_connected)
{
    (void) wifi_connected;
    size_t bytes = (size_t) BOARD_HAL_DISPLAY_WIDTH * BOARD_HAL_DISPLAY_HEIGHT * 3;
    uint8_t *rgb = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (!rgb) {
        ESP_LOGE(TAG, "Failed to allocate the %zu byte canvas", bytes);
        return ESP_ERR_NO_MEM;
    }
    canvas_t canvas = {
        .rgb = rgb, .width = BOARD_HAL_DISPLAY_WIDTH, .height = BOARD_HAL_DISPLAY_HEIGHT};

    time_t now_seconds;
    struct tm local;
    time(&now_seconds);
    localtime_r(&now_seconds, &local);
    info_now_t now;
    info_now_from_tm(&local, strcmp(config_manager_get_overlay_language(), "de") == 0, &now);

    esp_err_t err = ESP_ERR_NOT_FOUND;
    switch (id) {
#if FEATURE_CHORE_WHEEL
    case INFO_SCREEN_CHORE_WHEEL: {
        chore_config_t config;
        chore_config_parse(config_manager_get_chore_members(), config_manager_get_chore_tasks(),
                           &config);
        chore_wheel_render(&canvas, &now, &config);
        err = ESP_OK;
        break;
    }
#endif
#if FEATURE_WEATHER_SCREEN
    case INFO_SCREEN_WEATHER: {
        weather_screen_data_t data;
        load_weather(&data, wifi_connected);
        weather_screen_render(&canvas, &now, &data);
        err = ESP_OK;
        break;
    }
#endif
#if FEATURE_FACT_OF_THE_DAY
    case INFO_SCREEN_FACT: {
        fact_t fact;
        fact_service_pick(now.german, fact_day_number(now.year, now.month, now.day), &fact, NULL);
        fact_screen_render(&canvas, &now, &fact);
        err = ESP_OK;
        break;
    }
#endif
#if FEATURE_FINANCE_SNAPSHOT
    case INFO_SCREEN_FINANCE: {
        finance_screen_data_t *data = calloc(1, sizeof(*data));  // 1.9 KB: not on the task's stack
        if (!data) {
            err = ESP_ERR_NO_MEM;
            break;
        }
        load_finance(data, &now, wifi_connected);
        finance_screen_render(&canvas, &now, data);
        free(data);
        err = ESP_OK;
        break;
    }
#endif
#if FEATURE_FUEL_PRICES
    case INFO_SCREEN_FUEL: {
        fuel_screen_data_t *data = calloc(1, sizeof(*data));  // about 0.6 KB
        if (!data) {
            err = ESP_ERR_NO_MEM;
            break;
        }
        load_fuel(data, wifi_connected);
        fuel_screen_render(&canvas, &now, data);
        free(data);
        err = ESP_OK;
        break;
    }
#endif
#if FEATURE_RECIPES
    case INFO_SCREEN_RECIPE:
        err = render_recipe(&canvas, &now, wifi_connected);
        break;
#endif
#if FEATURE_MARKET_QUOTES
    case INFO_SCREEN_MARKETS: {
        markets_screen_data_t *data = calloc(1, sizeof(*data));  // 2.3 KB: not on the task's stack
        if (!data) {
            err = ESP_ERR_NO_MEM;
            break;
        }
        market_service_load(data, wifi_connected);
        markets_screen_render(&canvas, &now, data);
        free(data);
        err = ESP_OK;
        break;
    }
#endif
    default:
        ESP_LOGW(TAG, "No screen %d in this firmware", id);
        break;
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Showing the %s screen", info_screen_name(id));
        err = show_canvas(&canvas);
    }
    heap_caps_free(rgb);
    return err;
}
