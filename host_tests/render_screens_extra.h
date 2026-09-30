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
#include "screen_finance.h"
#include "screen_fuel.h"
#include "screen_markets.h"
#include "screen_weather.h"
}

#include <cstdio>
#include <cstring>
#include <ctime>
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

// The ISO date `days_back` calendar days before 2026-09-30, for made-up series.
inline void sample_date(int days_back, char *out, size_t out_len)
{
    struct tm tm = {};
    tm.tm_year = 2026 - 1900;
    tm.tm_mon = 8;
    tm.tm_mday = 30 - days_back;
    tm.tm_hour = 12;
    mktime(&tm);
    snprintf(out, out_len, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

// A made-up series of `count` working days ending at `last`, moving by `drift` a day with a wobble.
inline fx_series_t fx_sample(const char *code, float last, float drift, int count = FX_MAX_POINTS)
{
    fx_series_t series;
    memset(&series, 0, sizeof(series));
    strcpy(series.code, code);
    series.count = count;
    for (int i = 0; i < count; i++) {
        int back = count - 1 - i;
        sample_date(back * 10 / 7, series.dates[i], FX_DATE_LEN);  // working days -> calendar days
        series.values[i] = last - drift * (float) back + last * 0.004f * (float) ((i * 7) % 5 - 2);
    }
    return series;
}

inline finance_screen_data_t finance_sample(int count)
{
    finance_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = FINANCE_SCREEN_OK;
    data.count = count;
    data.series[0] = fx_sample("USD", 1.1355f, 0.0009f);
    data.series[1] = fx_sample("GBP", 0.8674f, -0.0004f);
    data.series[2] = fx_sample("CHF", 0.9402f, 0.0001f);
    data.series[3] = fx_sample("JPY", 162.34f, 0.12f);
    return data;
}

inline void draw_finance_english(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    finance_screen_data_t data = finance_sample(4);
    finance_screen_render(canvas, &now, &data);
}

inline void draw_finance_german(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 3, 4, true, &now);
    finance_screen_data_t data = finance_sample(2);
    finance_screen_render(canvas, &now, &data);
}

inline void draw_finance_single(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    finance_screen_data_t data = finance_sample(1);
    data.series[0] = fx_sample("JPY", 162.34f, 0.0f, 2);  // two points, no drift
    finance_screen_render(canvas, &now, &data);
}

inline void draw_finance_offline(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, true, &now);
    finance_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = FINANCE_SCREEN_NO_NETWORK;
    finance_screen_render(canvas, &now, &data);
}

// A fuel page with `count` made-up stations, the cheapest first.
inline fuel_screen_data_t fuel_sample(int count, fuel_type_t type, int radius)
{
    static const char *const names[] = {"Aral", "Shell", "JET", "Esso", "Freie Tankstelle"};
    static const char *const streets[] = {"Hauptstr.", "Ringstrasse", "Am Markt", "Bahnhofstr.",
                                          "Industrieweg"};
    fuel_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = FUEL_SCREEN_OK;
    data.type = type;
    data.radius_km = radius;
    data.result.status = FUEL_PARSE_OK;
    data.result.count = count;
    for (int i = 0; i < count; i++) {
        fuel_station_t &station = data.result.stations[i];
        strcpy(station.name, names[i]);
        snprintf(station.place, sizeof(station.place), "%s, Musterstadt", streets[i]);
        station.dist_km = 0.8f + 1.3f * (float) i;
        station.price = 1.719f + 0.020f * (float) i;
    }
    return data;
}

inline void draw_fuel_english(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    now.hour = 14;
    now.minute = 35;
    fuel_screen_data_t data = fuel_sample(5, FUEL_DIESEL, 5);
    fuel_screen_render(canvas, &now, &data);
}

inline void draw_fuel_german(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, true, &now);
    now.hour = 7;
    now.minute = 5;
    fuel_screen_data_t data = fuel_sample(3, FUEL_E10, 12);
    strcpy(data.result.stations[0].name, "B\xC3\xA4ren-Tankstelle");
    fuel_screen_render(canvas, &now, &data);
}

inline void draw_fuel_refused(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, true, &now);
    fuel_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = FUEL_SCREEN_KEY_REFUSED;
    strcpy(data.result.message, "apikey nicht angegeben, falsch, oder im falschen Format");
    fuel_screen_render(canvas, &now, &data);
}

// A made-up market series of `count` days ending 2026-09-30, moving by `drift` a day with a wobble.
inline market_series_t market_sample(const char *symbol, const char *name, const char *currency,
                                     float last, float drift, market_provider_t provider,
                                     int count = MARKET_MAX_POINTS)
{
    market_series_t series;
    memset(&series, 0, sizeof(series));
    snprintf(series.symbol, sizeof(series.symbol), "%s", symbol);
    snprintf(series.name, sizeof(series.name), "%s", name);
    snprintf(series.currency, sizeof(series.currency), "%s", currency);
    series.provider = provider;
    series.count = count;
    for (int i = 0; i < count; i++) {
        int back = count - 1 - i;
        sample_date(back * 10 / 7, series.dates[i], MARKET_DATE_LEN);
        series.values[i] = last - drift * (float) back + last * 0.004f * (float) ((i * 7) % 5 - 2);
    }
    return series;
}

inline markets_screen_data_t markets_sample(int count)
{
    markets_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = MARKETS_SCREEN_OK;
    data.count = count;
    data.series[0] = market_sample("AAPL", "Apple Inc.", "USD", 329.4f, 0.6f, MARKET_YAHOO);
    data.series[1] = market_sample("EUNL.DE", "iShares Core MSCI World UCITS ETF USD (Acc)", "EUR",
                                   129.07f, 0.3f, MARKET_YAHOO);
    data.series[2] = market_sample("^GDAXI", "DAX P", "EUR", 25336.0f, -21.0f, MARKET_YAHOO);
    data.series[3] =
        market_sample("BTC-EUR", "Bitcoin EUR", "EUR", 75066.0f, 210.0f, MARKET_TWELVEDATA);
    return data;
}

// The note at the foot of a markets page: fetched on 2026-09-30 at 14:35.
inline void markets_stamped(markets_screen_data_t *data)
{
    data->updated_year = 2026;
    data->updated_month = 9;
    data->updated_day = 30;
    data->updated_hour = 14;
    data->updated_minute = 35;
}

inline void draw_markets_english(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    markets_screen_data_t data = markets_sample(4);
    markets_stamped(&data);
    markets_screen_render(canvas, &now, &data);
}

// German, two rows: the second from the cache, and a symbol nobody could serve
inline void draw_markets_german(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 3, 4, true, &now);
    markets_screen_data_t data = markets_sample(3);
    markets_stamped(&data);
    data.series[1].stale = true;
    data.series[1].provider = MARKET_ALPHAVANTAGE;
    data.series[2].count = 0;
    strcpy(data.series[2].name, "");
    markets_screen_render(canvas, &now, &data);
}

inline void draw_markets_single(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    markets_screen_data_t data = markets_sample(1);
    data.series[0] = market_sample("GC=F", "Gold Dec 26", "USD", 4231.2f, 0.0f, MARKET_YAHOO, 2);
    markets_screen_render(canvas, &now, &data);
}

inline void draw_markets_small_price(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, true, &now);
    markets_screen_data_t data = markets_sample(2);
    data.series[0] =
        market_sample("EURUSD=X", "EUR/USD", "USD", 1.1355f, 0.0009f, MARKET_TWELVEDATA);
    data.series[1] =
        market_sample("DOGE-EUR", "Dogecoin EUR", "EUR", 0.1234f, 0.0002f, MARKET_TWELVEDATA);
    markets_screen_render(canvas, &now, &data);
}

inline void draw_markets_offline(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, true, &now);
    markets_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = MARKETS_SCREEN_NO_NETWORK;
    markets_screen_render(canvas, &now, &data);
}

inline void draw_markets_no_source(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, true, &now);
    markets_screen_data_t data;
    memset(&data, 0, sizeof(data));
    data.status = MARKETS_SCREEN_NO_SOURCE;
    markets_screen_render(canvas, &now, &data);
}

inline void draw_markets_failed(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    markets_screen_data_t data = markets_sample(2);
    data.series[0].count = 0;
    data.series[1].count = 0;
    markets_screen_render(canvas, &now, &data);  // status OK but nothing to show: the message
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
        {"finance-en", draw_finance_english},
        {"finance-de", draw_finance_german},
        {"finance-single", draw_finance_single},
        {"finance-offline", draw_finance_offline},
        {"fuel-en", draw_fuel_english},
        {"fuel-de", draw_fuel_german},
        {"fuel-refused", draw_fuel_refused},
        {"markets-en", draw_markets_english},
        {"markets-de", draw_markets_german},
        {"markets-single", draw_markets_single},
        {"markets-small-price", draw_markets_small_price},
        {"markets-offline", draw_markets_offline},
        {"markets-no-source", draw_markets_no_source},
        {"markets-failed", draw_markets_failed},
    };
    return cases;
}

#endif
