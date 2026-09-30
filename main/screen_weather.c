#include "screen_weather.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "screen_digits.h"

// ---------------------------------------------------------------------------------------------
// Codes and words

weather_kind_t weather_screen_kind(int code)
{
    switch (code) {
    case 0:
        return WEATHER_KIND_CLEAR;
    case 1:
        return WEATHER_KIND_MOSTLY_CLEAR;
    case 2:
        return WEATHER_KIND_PARTLY_CLOUDY;
    case 3:
        return WEATHER_KIND_OVERCAST;
    case 45:
    case 48:
        return WEATHER_KIND_FOG;
    case 51:
    case 53:
    case 55:
        return WEATHER_KIND_DRIZZLE;
    case 56:
    case 57:
    case 66:
    case 67:
        return WEATHER_KIND_FREEZING;
    case 61:
    case 63:
    case 65:
    case 80:
    case 81:
    case 82:
        return WEATHER_KIND_RAIN;
    case 71:
    case 73:
    case 75:
    case 77:
    case 85:
    case 86:
        return WEATHER_KIND_SNOW;
    case 95:
    case 96:
    case 99:
        return WEATHER_KIND_THUNDER;
    default:
        return WEATHER_KIND_UNKNOWN;
    }
}

const char *weather_screen_condition(int code, bool german)
{
    typedef struct {
        int code;
        const char *en;
        const char *de;
    } entry_t;
    static const entry_t table[] = {
        {0, "Clear sky", "Klarer Himmel"},
        {1, "Mostly clear",
         "\xC3\x9C"
         "berwiegend klar"},
        {2, "Partly cloudy", "Teils bew\xC3\xB6lkt"},
        {3, "Overcast", "Bedeckt"},
        {45, "Fog", "Nebel"},
        {48, "Freezing fog", "Reifnebel"},
        {51, "Light drizzle", "Leichter Nieselregen"},
        {53, "Drizzle", "Nieselregen"},
        {55, "Heavy drizzle", "Starker Nieselregen"},
        {56, "Freezing drizzle", "Gefr. Nieselregen"},
        {57, "Freezing drizzle", "Gefr. Nieselregen"},
        {61, "Light rain", "Leichter Regen"},
        {63, "Rain", "Regen"},
        {65, "Heavy rain", "Starkregen"},
        {66, "Freezing rain", "Gefrierender Regen"},
        {67, "Freezing rain", "Gefrierender Regen"},
        {71, "Light snow", "Leichter Schneefall"},
        {73, "Snow", "Schneefall"},
        {75, "Heavy snow", "Starker Schneefall"},
        {77, "Snow grains", "Schneegriesel"},
        {80, "Light showers", "Leichte Schauer"},
        {81, "Showers", "Schauer"},
        {82, "Heavy showers", "Starke Schauer"},
        {85, "Snow showers", "Schneeschauer"},
        {86, "Snow showers", "Schneeschauer"},
        {95, "Thunderstorm", "Gewitter"},
        {96, "Thunderstorm, hail", "Gewitter mit Hagel"},
        {99, "Thunderstorm, hail", "Gewitter mit Hagel"},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (table[i].code == code) {
            return german ? table[i].de : table[i].en;
        }
    }
    return german ? "Unbekannt" : "Unknown";
}

// ---------------------------------------------------------------------------------------------
// Icons

// Fills a simple polygon (even-odd rule) given as x, y pairs.
static void fill_polygon(canvas_t *canvas, const float *xy, int count, canvas_color_t color)
{
    float min_y = xy[1], max_y = xy[1];
    for (int i = 1; i < count; i++) {
        min_y = xy[2 * i + 1] < min_y ? xy[2 * i + 1] : min_y;
        max_y = xy[2 * i + 1] > max_y ? xy[2 * i + 1] : max_y;
    }
    for (int y = (int) floorf(min_y); y <= (int) ceilf(max_y); y++) {
        float crossings[16];
        int n = 0;
        float scan = (float) y + 0.5f;
        for (int i = 0; i < count && n < 16; i++) {
            int j = (i + 1) % count;
            float y0 = xy[2 * i + 1], y1 = xy[2 * j + 1];
            if ((y0 <= scan && y1 > scan) || (y1 <= scan && y0 > scan)) {
                float t = (scan - y0) / (y1 - y0);
                crossings[n++] = xy[2 * i] + t * (xy[2 * j] - xy[2 * i]);
            }
        }
        for (int a = 1; a < n; a++) {  // insertion sort, n is tiny
            float v = crossings[a];
            int b = a - 1;
            while (b >= 0 && crossings[b] > v) {
                crossings[b + 1] = crossings[b];
                b--;
            }
            crossings[b + 1] = v;
        }
        for (int a = 0; a + 1 < n; a += 2) {
            int x0 = (int) lroundf(crossings[a]);
            int x1 = (int) lroundf(crossings[a + 1]);
            canvas_rect(canvas, x0, y, x1 - x0 + 1, 1, color);
        }
    }
}

static int outline_of(int size)
{
    int o = size / 32;
    return o < 1 ? 1 : o;
}

static void sun(canvas_t *canvas, float cx, float cy, float radius, int o, bool rays)
{
    if (rays) {
        float inner = radius + (float) o + radius * 0.28f;
        float outer = inner + radius * 0.50f;
        for (int i = 0; i < 8; i++) {
            float a = (float) i * 3.14159265f / 4.0f;
            canvas_line(canvas, cx + inner * sinf(a), cy - inner * cosf(a), cx + outer * sinf(a),
                        cy - outer * cosf(a), o, CANVAS_BLACK);
        }
    }
    canvas_disc(canvas, (int) lroundf(cx), (int) lroundf(cy), (int) lroundf(radius) + o,
                CANVAS_BLACK);
    canvas_disc(canvas, (int) lroundf(cx), (int) lroundf(cy), (int) lroundf(radius), CANVAS_YELLOW);
}

// One pass of a cloud of width `w` around (cx, cy): three discs on a flat base, every shape grown
// by `grow` pixels - the black pass with the outline, then the white pass on top of it.
static void cloud_pass(canvas_t *canvas, float cx, float cy, float w, int grow,
                       canvas_color_t color)
{
    canvas_disc(canvas, (int) lroundf(cx - 0.22f * w), (int) lroundf(cy),
                (int) lroundf(0.17f * w) + grow, color);
    canvas_disc(canvas, (int) lroundf(cx - 0.02f * w), (int) lroundf(cy - 0.11f * w),
                (int) lroundf(0.25f * w) + grow, color);
    canvas_disc(canvas, (int) lroundf(cx + 0.24f * w), (int) lroundf(cy + 0.02f * w),
                (int) lroundf(0.18f * w) + grow, color);
    canvas_pill(canvas, (int) lroundf(cx - 0.39f * w) - grow, (int) lroundf(cy - 0.02f * w) - grow,
                (int) lroundf(0.81f * w) + 2 * grow, (int) lroundf(0.20f * w) + 2 * grow, color);
}

static void cloud(canvas_t *canvas, float cx, float cy, float w, int o)
{
    cloud_pass(canvas, cx, cy, w, o, CANVAS_BLACK);
    cloud_pass(canvas, cx, cy, w, 0, CANVAS_WHITE);
}

// The bottom of a cloud of width w at cy.
static float cloud_bottom(float cy, float w)
{
    return cy + 0.18f * w;
}

static void rain_lines(canvas_t *canvas, float cx, float top, float S, int count, int o,
                       canvas_color_t color)
{
    float spacing = 0.16f * S;
    float x = cx - spacing * (float) (count - 1) / 2.0f;
    for (int i = 0; i < count; i++) {
        float y = top + ((i % 2) ? 0.10f * S : 0.0f);
        canvas_line(canvas, x + spacing * (float) i + 0.05f * S, y,
                    x + spacing * (float) i - 0.02f * S, y + 0.17f * S, o, color);
    }
}

static void snow_flake(canvas_t *canvas, float cx, float cy, float r, int o)
{
    for (int i = 0; i < 3; i++) {
        float a = (float) i * 3.14159265f / 3.0f;
        canvas_line(canvas, cx - r * sinf(a), cy + r * cosf(a), cx + r * sinf(a), cy - r * cosf(a),
                    o > 1 ? o - 1 : 1, CANVAS_BLACK);
    }
}

static void bolt(canvas_t *canvas, float cx, float top, float height, int o)
{
    static const float shape[7][2] = {{0.60f, 0.00f}, {0.20f, 0.56f}, {0.48f, 0.56f},
                                      {0.34f, 1.00f}, {0.82f, 0.38f}, {0.54f, 0.38f},
                                      {0.72f, 0.00f}};
    float xy[14];
    for (int i = 0; i < 7; i++) {
        xy[2 * i] = cx + (shape[i][0] - 0.5f) * height * 0.80f;
        xy[2 * i + 1] = top + shape[i][1] * height;
    }
    fill_polygon(canvas, xy, 7, CANVAS_YELLOW);
    for (int i = 0; i < 7; i++) {
        int j = (i + 1) % 7;
        canvas_line(canvas, xy[2 * i], xy[2 * i + 1], xy[2 * j], xy[2 * j + 1], o > 2 ? o - 1 : 1,
                    CANVAS_BLACK);
    }
}

void weather_screen_draw_icon(canvas_t *canvas, int icx, int icy, int size, weather_kind_t kind)
{
    if (size < 8) {
        return;
    }
    float cx = (float) icx, cy = (float) icy, S = (float) size;
    int o = outline_of(size);
    switch (kind) {
    case WEATHER_KIND_CLEAR:
        sun(canvas, cx, cy, 0.24f * S, o, true);
        break;
    case WEATHER_KIND_MOSTLY_CLEAR:
        sun(canvas, cx - 0.06f * S, cy - 0.08f * S, 0.20f * S, o, true);
        cloud(canvas, cx + 0.12f * S, cy + 0.20f * S, 0.52f * S, o);
        break;
    case WEATHER_KIND_PARTLY_CLOUDY:
        sun(canvas, cx - 0.15f * S, cy - 0.17f * S, 0.15f * S, o, true);
        cloud(canvas, cx + 0.05f * S, cy + 0.12f * S, 0.74f * S, o);
        break;
    case WEATHER_KIND_OVERCAST:
        cloud(canvas, cx - 0.14f * S, cy - 0.10f * S, 0.58f * S, o);
        cloud(canvas, cx + 0.05f * S, cy + 0.10f * S, 0.80f * S, o);
        break;
    case WEATHER_KIND_FOG: {
        cloud(canvas, cx, cy - 0.14f * S, 0.72f * S, o);
        for (int i = 0; i < 3; i++) {
            float y = cy + (0.16f + 0.13f * (float) i) * S;
            float half = (i == 1 ? 0.28f : 0.34f) * S;
            canvas_line(canvas, cx - half, y, cx + half, y, o, CANVAS_BLACK);
        }
        break;
    }
    case WEATHER_KIND_DRIZZLE: {
        float w = 0.85f * S, y = cy - 0.14f * S;
        cloud(canvas, cx, y, w, o);
        float bottom = cloud_bottom(y, w);
        for (int row = 0; row < 2; row++) {
            for (int i = 0; i < 3; i++) {
                float x =
                    cx + ((float) i - 1.0f) * 0.20f * S + (row ? 0.10f * S : 0.0f) - 0.05f * S;
                canvas_disc(canvas, (int) lroundf(x),
                            (int) lroundf(bottom + (0.12f + 0.14f * (float) row) * S), o + o / 2,
                            CANVAS_BLUE);
            }
        }
        break;
    }
    case WEATHER_KIND_RAIN: {
        float w = 0.85f * S, y = cy - 0.14f * S;
        cloud(canvas, cx, y, w, o);
        rain_lines(canvas, cx, cloud_bottom(y, w) + 0.08f * S, S, 3, o, CANVAS_BLUE);
        break;
    }
    case WEATHER_KIND_FREEZING: {
        float w = 0.85f * S, y = cy - 0.14f * S;
        cloud(canvas, cx, y, w, o);
        float top = cloud_bottom(y, w) + 0.08f * S;
        rain_lines(canvas, cx - 0.14f * S, top, S, 2, o, CANVAS_BLUE);
        snow_flake(canvas, cx + 0.20f * S, top + 0.13f * S, 0.075f * S, o);
        break;
    }
    case WEATHER_KIND_SNOW: {
        float w = 0.85f * S, y = cy - 0.14f * S;
        cloud(canvas, cx, y, w, o);
        float bottom = cloud_bottom(y, w);
        snow_flake(canvas, cx - 0.22f * S, bottom + 0.19f * S, 0.075f * S, o);
        snow_flake(canvas, cx, bottom + 0.30f * S, 0.075f * S, o);
        snow_flake(canvas, cx + 0.22f * S, bottom + 0.19f * S, 0.075f * S, o);
        break;
    }
    case WEATHER_KIND_THUNDER: {
        float w = 0.85f * S, y = cy - 0.16f * S;
        cloud(canvas, cx, y, w, o);
        bolt(canvas, cx + 0.02f * S, cloud_bottom(y, w) - 0.05f * S, 0.44f * S, o);
        break;
    }
    default: {
        int r = (int) lroundf(0.36f * S);
        canvas_disc(canvas, icx, icy, r, CANVAS_BLACK);
        canvas_disc(canvas, icx, icy, r - o - 1, CANVAS_WHITE);
        int scale = size / 40 < 1 ? 1 : size / 40;
        canvas_text_centered(canvas, icx, icy - canvas_text_height(scale) / 2, "?", scale,
                             CANVAS_BLACK);
        break;
    }
    }
}

// ---------------------------------------------------------------------------------------------
// The page

static void temperature_text(int degrees, char *out, size_t out_len)
{
    snprintf(out, out_len, "%d*", degrees);
}

static void message(canvas_t *canvas, const info_now_t *now, weather_screen_status_t status)
{
    static const char *const en[][2] = {
        {"", ""},
        {"No place set for the weather", "Settings > Overlays > Weather"},
        {"No network", "Weather arrives once WiFi is up"},
        {"Weather service not reachable", "Trying again next time"},
    };
    static const char *const de[][2] = {
        {"", ""},
        {"Kein Ort f\xC3\xBCr das Wetter", "Einstellungen > Overlays > Weather"},
        {"Kein Netz", "Wetter kommt, sobald WLAN da ist"},
        {"Wetterdienst nicht erreichbar",
         "Neuer Versuch beim n\xC3\xA4"
         "chsten Mal"},
    };
    int index = (status >= 0 && status <= WEATHER_SCREEN_FETCH_FAILED) ? (int) status : 3;
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    char lines[2][CANVAS_WRAP_LINE_MAX];
    for (int i = 0; i < 2; i++) {
        char raw[CANVAS_WRAP_LINE_MAX];
        canvas_text_from_utf8(now->german ? de[index][i] : en[index][i], raw, sizeof(raw));
        canvas_text_fit(raw, canvas->width - 4 * u, s, lines[i], sizeof(lines[i]));
    }
    canvas_fill(canvas, CANVAS_WHITE);
    int icon = canvas->height / 4;
    weather_screen_draw_icon(canvas, canvas->width / 2, canvas->height / 2 - icon * 3 / 4, icon,
                             WEATHER_KIND_UNKNOWN);
    int y = canvas->height / 2 + icon / 3;
    canvas_text_centered(canvas, canvas->width / 2, y, lines[0], s, CANVAS_BLACK);
    canvas_text_centered(canvas, canvas->width / 2, y + canvas_text_height(s) + u, lines[1], s,
                         CANVAS_BLACK);
}

// The line with the day ("Wed 30 September") as display text: the longest of a few variants that
// fits `max_width` (the month is cut to three letters, then it is left out).
static void day_line(const info_now_t *now, const weather_screen_day_t *day, int scale,
                     int max_width, char *out, size_t out_len)
{
    char month_full[24], month_short[8], weekday[16], candidate[CANVAS_WRAP_LINE_MAX];
    canvas_text_from_utf8(info_month_name(day->month, now->german), month_full, sizeof(month_full));
    snprintf(month_short, sizeof(month_short), "%.3s", month_full);
    canvas_text_from_utf8(
        info_weekday_short(info_weekday(day->year, day->month, day->day), now->german), weekday,
        sizeof(weekday));
    const char *dot = now->german ? "." : "";
    for (int variant = 0; variant < 3; variant++) {
        if (variant == 0) {
            snprintf(candidate, sizeof(candidate), "%s %d%s %s", weekday, day->day, dot,
                     month_full);
        } else if (variant == 1) {
            snprintf(candidate, sizeof(candidate), "%s %d%s %s", weekday, day->day, dot,
                     month_short);
        } else {
            snprintf(candidate, sizeof(candidate), "%s %d%s", weekday, day->day, dot);
        }
        if (canvas_text_width(candidate, scale) <= max_width) {
            break;
        }
    }
    canvas_text_fit(candidate, max_width, scale, out, out_len);
}

static void draw_hero(canvas_t *canvas, int x, int y, int w, int h, const info_now_t *now,
                      const weather_screen_data_t *data)
{
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    const weather_screen_day_t *today = &data->days[0];
    int line = canvas_text_height(s);
    char text[CANVAS_WRAP_LINE_MAX], fitted[CANVAS_WRAP_LINE_MAX];

    // the header: the place, TODAY on the same line if it is today, and the date under them
    bool is_today =
        today->year == now->year && today->month == now->month && today->day == now->day;
    char marker[8];
    canvas_text_from_utf8(now->german ? "HEUTE" : "TODAY", marker, sizeof(marker));
    int marker_w = is_today ? canvas_text_width(marker, s) + 2 * u : 0;
    int header_y = y;
    if (data->place[0] != '\0') {
        canvas_text_from_utf8(data->place, text, sizeof(text));
        canvas_text_fit(text, w - marker_w, s, fitted, sizeof(fitted));
        canvas_text(canvas, x, header_y, fitted, s, CANVAS_BLACK);
    }
    if (is_today) {
        canvas_text_right(canvas, x + w, header_y, marker, s, CANVAS_RED);
    }
    if (data->place[0] != '\0' || is_today) {
        header_y += line + u / 2;
    }
    day_line(now, today, s, w, fitted, sizeof(fitted));
    canvas_text(canvas, x, header_y, fitted, s, CANVAS_BLACK);
    int body_top = header_y + line + u;

    // the footer: the condition in words
    int footer_y = y + h - line;
    canvas_text_from_utf8(weather_screen_condition(today->code, now->german), text, sizeof(text));
    canvas_text_fit(text, w, s, fitted, sizeof(fitted));
    canvas_text_centered(canvas, x + w / 2, footer_y, fitted, s, CANVAS_BLACK);

    int body_h = footer_y - u - body_top;
    if (body_h < 8 * u) {
        return;
    }
    // the high with the icon beside it, and the low under them - the group centred in the room
    char number[16], low[16];
    temperature_text(today->temp_max, number, sizeof(number));
    temperature_text(today->temp_min, low, sizeof(low));
    canvas_text_from_utf8(now->german ? "TIEF" : "LOW", text, sizeof(text));
    int label_w = canvas_text_width(text, s);

    int icon_size = body_h * 45 / 100 < w * 42 / 100 ? body_h * 45 / 100 : w * 42 / 100;
    int digits_w = w - icon_size - 2 * u;
    int digits_h = canvas_big_fit_height(number, digits_w, body_h * 50 / 100);
    int high_row_h = icon_size > digits_h ? icon_size : digits_h;
    int low_digits_h = canvas_big_fit_height(low, w - label_w - 3 * u, digits_h * 62 / 100);
    int group_h = high_row_h + 3 * u + low_digits_h;
    int top = body_top + (body_h - group_h) / 2;

    weather_screen_draw_icon(canvas, x + icon_size / 2, top + high_row_h / 2, icon_size,
                             weather_screen_kind(today->code));
    canvas_big_text(canvas,
                    x + icon_size + 2 * u + (digits_w - canvas_big_width(number, digits_h)) / 2,
                    top + (high_row_h - digits_h) / 2, number, digits_h, CANVAS_BLACK);

    int low_w = label_w + 2 * u + canvas_big_width(low, low_digits_h);
    int low_x = x + (w - low_w) / 2;
    int low_y = top + high_row_h + 3 * u;
    canvas_text(canvas, low_x, low_y + low_digits_h - line, text, s, CANVAS_BLACK);
    canvas_big_text(canvas, low_x + label_w + 2 * u, low_y, low, low_digits_h, CANVAS_BLUE);
}

static void draw_row(canvas_t *canvas, int x, int y, int w, int h, const info_now_t *now,
                     const weather_screen_day_t *day)
{
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    canvas_frame(canvas, x, y, w, h, 1 + u / 12, CANVAS_BLACK);

    char text[CANVAS_WRAP_LINE_MAX];
    canvas_text_from_utf8(
        info_weekday_short(info_weekday(day->year, day->month, day->day), now->german), text,
        sizeof(text));
    int text_x = x + u;
    canvas_text(canvas, text_x, y + (h - canvas_text_height(s)) / 2, text, s, CANVAS_BLACK);
    int after_text = text_x + canvas_text_width(text, s) + u;

    int icon = h * 72 / 100;
    int icon_x = after_text + icon / 2;
    weather_screen_draw_icon(canvas, icon_x, y + h / 2, icon, weather_screen_kind(day->code));

    char high[16], low[16];
    temperature_text(day->temp_max, high, sizeof(high));
    temperature_text(day->temp_min, low, sizeof(low));
    int room = x + w - u - (icon_x + icon / 2 + u);
    int digits_h = h * 52 / 100;
    while (digits_h > 8 &&
           canvas_big_width(high, digits_h) + canvas_big_width(low, digits_h) + 2 * u > room) {
        digits_h--;
    }
    int y_digits = y + (h - digits_h) / 2;
    int low_x = x + w - u - canvas_big_width(low, digits_h);
    int high_x = low_x - u - canvas_big_width(high, digits_h);
    canvas_big_text(canvas, high_x, y_digits, high, digits_h, CANVAS_BLACK);
    canvas_big_text(canvas, low_x, y_digits, low, digits_h, CANVAS_BLUE);
}

void weather_screen_render(canvas_t *canvas, const info_now_t *now,
                           const weather_screen_data_t *data)
{
    if (data->status != WEATHER_SCREEN_OK || data->day_count < 1) {
        message(canvas, now,
                data->status != WEATHER_SCREEN_OK ? data->status : WEATHER_SCREEN_FETCH_FAILED);
        return;
    }
    int u = canvas_unit(canvas);
    canvas_fill(canvas, CANVAS_WHITE);
    bool landscape = canvas->width >= canvas->height;

    int hero_x, hero_y, hero_w, hero_h, rows_x, rows_y, rows_w, rows_h;
    if (landscape) {
        hero_x = 2 * u;
        hero_y = 2 * u;
        hero_w = canvas->width * 42 / 100 - 2 * u;
        hero_h = canvas->height - 4 * u;
        rows_x = hero_x + hero_w + 2 * u;
        rows_y = 2 * u;
        rows_w = canvas->width - rows_x - 2 * u;
        rows_h = canvas->height - 4 * u;
    } else {
        hero_x = 2 * u;
        hero_y = 2 * u;
        hero_w = canvas->width - 4 * u;
        hero_h = canvas->height * 40 / 100 - 2 * u;
        rows_x = 2 * u;
        rows_y = hero_y + hero_h + u;
        rows_w = canvas->width - 4 * u;
        rows_h = canvas->height - rows_y - 2 * u;
    }
    draw_hero(canvas, hero_x, hero_y, hero_w, hero_h, now, data);

    int rows = data->day_count - 1;
    if (rows > 0) {
        int gap = u / 2 + 1;
        int row_h = (rows_h - gap * (WEATHER_SCREEN_MAX_DAYS - 2)) / (WEATHER_SCREEN_MAX_DAYS - 1);
        for (int i = 0; i < rows; i++) {
            draw_row(canvas, rows_x, rows_y + i * (row_h + gap), rows_w, row_h, now,
                     &data->days[i + 1]);
        }
    }
}
