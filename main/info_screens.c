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
#if FEATURE_WEATHER_SCREEN
#include <math.h>
#include <stdio.h>

#include "screen_weather.h"
#include "weather.h"
#endif

static const char *TAG = "info_screens";

static const char *const SCREEN_NAMES[INFO_SCREEN_COUNT] = {"agenda", "chore-wheel", "weather",
                                                            "fact"};

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
    uint32_t mask = config_manager_get_info_screens_mask() & info_screens_compiled_mask();
    if (!agenda_has_content) {
        mask &= ~(1u << INFO_SCREEN_AGENDA);
    }
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
