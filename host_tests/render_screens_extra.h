#ifndef RENDER_SCREENS_EXTRA_H
#define RENDER_SCREENS_EXTRA_H

// The render cases of the info screens: a name and a function that draws that screen with sample
// data. Shared by the render harness (pictures) and the screen tests (checks).

#include <vector>

extern "C" {
#include "info_screens_core.h"
#include "screen_canvas.h"
#include "screen_chore_wheel.h"
#include "screen_fact.h"
#include "screen_weather.h"
}

#include <cstring>
#include <string>

struct RenderCase {
    const char *name;
    void (*draw)(canvas_t *canvas);
};

inline void draw_chore_wheel_english(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    chore_config_t config;
    chore_config_parse(
        "Anna, Ben, Clara, Dave",
        "Take out the bins, Vacuum the living room, Water the plants, Wash the dishes", &config);
    chore_wheel_render(canvas, &now, &config);
}

inline void draw_chore_wheel_german(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 10, 7, true, &now);
    chore_config_t config;
    chore_config_parse("Anna, Bj\xC3\xB6rn, Cl\xC3\xA4ra, Dave, Eva",
                       "M\xC3\xBCll rausbringen, Wohnzimmer saugen, Blumen gie\xC3\x9F"
                       "en, "
                       "Geschirr sp\xC3\xBCle"
                       "n, Bad putzen, Einkaufen",
                       &config);
    chore_wheel_render(canvas, &now, &config);
}

inline void draw_chore_wheel_small(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 1, 1, false, &now);
    chore_config_t config;
    chore_config_parse("Sam, Alex", "Dishes, Trash", &config);
    chore_wheel_render(canvas, &now, &config);
}

inline void draw_chore_wheel_empty(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, true, &now);
    chore_config_t config;
    chore_config_parse("", "", &config);
    chore_wheel_render(canvas, &now, &config);
}

// A forecast: `count` days from 2026-09-30 with these highs, lows and WMO codes.
inline weather_screen_data_t weather_sample(const char *place, int count, const int highs[],
                                            const int lows[], const int codes[])
{
    weather_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = WEATHER_SCREEN_OK;
    strncpy(data.place, place, sizeof(data.place) - 1);
    data.day_count = count;
    for (int i = 0; i < count; i++) {
        data.days[i].year = 2026;
        data.days[i].month = 9 + (30 + i > 30 ? 1 : 0);
        data.days[i].day = 30 + i > 30 ? 30 + i - 30 : 30;
        data.days[i].temp_max = highs[i];
        data.days[i].temp_min = lows[i];
        data.days[i].code = codes[i];
    }
    return data;
}

inline void draw_weather_english(canvas_t *canvas)
{
    static const int highs[] = {24, 22, 19, 17, 15}, lows[] = {12, 11, 9, 6, 2};
    static const int codes[] = {2, 61, 3, 95, 71};
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    weather_screen_data_t data = weather_sample("Berlin", 5, highs, lows, codes);
    weather_screen_render(canvas, &now, &data);
}

inline void draw_weather_german(canvas_t *canvas)
{
    static const int highs[] = {31, 28, 5, -1, 0}, lows[] = {18, 16, 1, -6, -3};
    static const int codes[] = {0, 45, 80, 66, 99};
    info_now_t now;
    info_now_from_date(2026, 9, 30, true, &now);
    weather_screen_data_t data = weather_sample("K\xC3\xB6ln", 5, highs, lows, codes);
    weather_screen_render(canvas, &now, &data);
}

inline void draw_weather_cold(canvas_t *canvas)
{
    static const int highs[] = {-4, -12, -7, -3, 1}, lows[] = {-11, -18, -14, -9, -4};
    static const int codes[] = {73, 75, 71, 51, 1};
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    weather_screen_data_t data = weather_sample("", 5, highs, lows, codes);
    weather_screen_render(canvas, &now, &data);
}

inline void draw_weather_short(canvas_t *canvas)
{
    static const int highs[] = {100, 9}, lows[] = {-100, 3};
    static const int codes[] = {57, 123};
    info_now_t now;
    info_now_from_date(2026, 10, 1, false, &now);  // the forecast starts a day before "now"
    weather_screen_data_t data =
        weather_sample("A place with a really quite long name indeed", 2, highs, lows, codes);
    weather_screen_render(canvas, &now, &data);
}

inline void draw_weather_offline(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, true, &now);
    weather_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = WEATHER_SCREEN_NO_NETWORK;
    weather_screen_render(canvas, &now, &data);
}

inline void draw_weather_no_place(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    weather_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = WEATHER_SCREEN_NO_LOCATION;
    weather_screen_render(canvas, &now, &data);
}

inline void draw_fact_english(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    fact_screen_render(canvas, &now, fact_builtin(2, false));
}

inline void draw_fact_german(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, true, &now);
    fact_screen_render(canvas, &now, fact_builtin(14, true));
}

inline void draw_fact_plain(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 12, 24, false, &now);
    fact_t fact;
    memset(&fact, 0, sizeof(fact));
    strcpy(fact.text, "A short fact.");
    fact_screen_render(canvas, &now, &fact);
}

inline void draw_fact_long(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 3, 1, true, &now);
    fact_t fact;
    memset(&fact, 0, sizeof(fact));
    strcpy(fact.title, "A rather long topic that does not fit its pill on a small panel");
    std::string text, question;
    while (text.size() < FACT_TEXT_MAX - 14) {
        text += "Ein langes Wort. ";
    }
    while (question.size() < FACT_QUESTION_MAX - 12) {
        question += "Frage dazu? ";
    }
    strncpy(fact.text, text.c_str(), FACT_TEXT_MAX - 1);
    strncpy(fact.question, question.c_str(), FACT_QUESTION_MAX - 1);
    fact_screen_render(canvas, &now, &fact);
}

inline const std::vector<RenderCase> &render_cases()
{
    static const std::vector<RenderCase> cases = {
        {"chore-wheel-en", draw_chore_wheel_english},
        {"chore-wheel-de", draw_chore_wheel_german},
        {"chore-wheel-small", draw_chore_wheel_small},
        {"chore-wheel-empty", draw_chore_wheel_empty},
        {"weather-en", draw_weather_english},
        {"weather-de", draw_weather_german},
        {"weather-cold", draw_weather_cold},
        {"weather-short", draw_weather_short},
        {"weather-offline", draw_weather_offline},
        {"weather-no-place", draw_weather_no_place},
        {"fact-en", draw_fact_english},
        {"fact-de", draw_fact_german},
        {"fact-plain", draw_fact_plain},
        {"fact-long", draw_fact_long},
    };
    return cases;
}

#endif
