#include "screen_fuel.h"

#include <stdio.h>
#include <string.h>

#include "screen_digits.h"

static void message(canvas_t *canvas, const info_now_t *now, const fuel_screen_data_t *data)
{
    static const char *const en[][2] = {
        {"", ""},
        {"No API key for the fuel prices", "Settings > Agenda > Information screens"},
        {"No place for the fuel prices", "Set it under Overlays > Weather"},
        {"No network", "Fuel prices arrive once WiFi is up"},
        {"Fuel prices not available", "Trying again next time"},
        {"The service refused the request", ""},
        {"No open station with a price", "Try a larger radius"},
    };
    static const char *const de[][2] = {
        {"", ""},
        {"Kein API-Schl\xC3\xBC"
         "ssel f\xC3\xBC"
         "r die Spritpreise",
         "Einstellungen > Agenda > Information screens"},
        {"Kein Ort f\xC3\xBC"
         "r die Spritpreise",
         "Einstellen unter Overlays > Weather"},
        {"Kein Netz", "Preise kommen, sobald WLAN da ist"},
        {"Spritpreise nicht erreichbar",
         "Neuer Versuch beim n\xC3\xA4"
         "chsten Mal"},
        {"Der Dienst hat die Anfrage abgelehnt", ""},
        {"Keine offene Tankstelle mit Preis",
         "Gr\xC3\xB6\xC3\x9F"
         "eren Umkreis versuchen"},
    };
    int index = (int) data->status;
    if (index < FUEL_SCREEN_NO_KEY || index > FUEL_SCREEN_NONE_FOUND) {
        index = FUEL_SCREEN_FETCH_FAILED;
    }
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    char lines[2][CANVAS_WRAP_LINE_MAX];
    for (int i = 0; i < 2; i++) {
        char raw[CANVAS_WRAP_LINE_MAX];
        const char *text = now->german ? de[index][i] : en[index][i];
        if (i == 1 && text[0] == '\0' && index == FUEL_SCREEN_KEY_REFUSED) {
            text = data->result.message;  // what the service says, in its own words
        }
        canvas_text_from_utf8(text, raw, sizeof(raw));
        canvas_text_fit(raw, canvas->width - 4 * u, s, lines[i], sizeof(lines[i]));
    }
    canvas_fill(canvas, CANVAS_WHITE);
    int y = canvas->height / 2 - canvas_text_height(s);
    canvas_text_centered(canvas, canvas->width / 2, y, lines[0], s, CANVAS_BLACK);
    canvas_text_centered(canvas, canvas->width / 2, y + canvas_text_height(s) + u, lines[1], s,
                         CANVAS_BLACK);
}

// A number with one decimal: "1.5" ("1,5" in German).
static void one_decimal(float value, bool german, char *out, size_t out_len)
{
    int tenths = (int) (value * 10.0f + 0.5f);
    snprintf(out, out_len, "%d%c%d", tenths / 10, german ? ',' : '.', tenths % 10);
}

// The price the way the pumps show it: "1.72" in big digits and the third decimal small at the top.
typedef struct {
    char digits[8];
    char ninth[4];
    int digits_h, small_scale, total_w;
} price_layout_t;

// Fits the price into a box of `height` and at most `max_width`.
static void price_layout(const canvas_t *canvas, float price, int height, int max_width,
                         price_layout_t *layout)
{
    char ninth[2];
    fuel_format_price(price, layout->digits, sizeof(layout->digits), ninth, sizeof(ninth));
    canvas_text_from_utf8(ninth, layout->ninth, sizeof(layout->ninth));
    int s = canvas_text_scale(canvas, 1);
    int u = canvas_unit(canvas);
    layout->small_scale = height >= 24 * s * 3 ? s * 2 : s;
    int small_w = canvas_text_width(layout->ninth, layout->small_scale);
    layout->digits_h = canvas_big_fit_height(layout->digits, max_width - small_w - u / 2, height);
    layout->total_w = canvas_big_width(layout->digits, layout->digits_h) + u / 2 + small_w;
}

// Draws the price with its right edge at `right`, centred vertically in the box from `top`.
static void price_draw(canvas_t *canvas, const price_layout_t *layout, int right, int top,
                       int height)
{
    int u = canvas_unit(canvas);
    int x = right - layout->total_w;
    int y = top + (height - layout->digits_h) / 2;
    canvas_big_text(canvas, x, y, layout->digits, layout->digits_h, CANVAS_BLACK);
    canvas_text(canvas, x + canvas_big_width(layout->digits, layout->digits_h) + u / 2, y,
                layout->ninth, layout->small_scale, CANVAS_BLACK);
}

static void draw_row(canvas_t *canvas, int x, int y, int w, int h, int rank,
                     const fuel_station_t *station, const info_now_t *now)
{
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    int line = canvas_text_height(s);
    canvas_color_t accent = rank == 0 ? CANVAS_GREEN : CANVAS_BLACK;
    canvas_frame(canvas, x, y, w, h, 1 + u / 12, CANVAS_BLACK);
    if (rank == 0) {
        canvas_rect(canvas, x, y, u, h, CANVAS_GREEN);  // the bar of the cheapest
    }

    char name[FUEL_NAME_MAX * 2], place[FUEL_PLACE_MAX * 2], distance[32], distance_text[48];
    canvas_text_from_utf8(station->name, name, sizeof(name));
    canvas_text_from_utf8(station->place, place, sizeof(place));
    one_decimal(station->dist_km, now->german, distance, sizeof(distance));
    snprintf(distance_text, sizeof(distance_text), "%s km", distance);
    char number[16];
    snprintf(number, sizeof(number), "%d", rank + 1);

    bool wide = w >= 6 * h;
    int badge_r = (wide ? h * 26 / 100 : line * 6 / 10);
    int badge_x = x + 2 * u + badge_r;
    char fitted[CANVAS_WRAP_LINE_MAX];
    if (wide) {
        int price_h = h * 62 / 100;
        price_layout_t price;
        price_layout(canvas, station->price, price_h, w / 3, &price);
        int price_right = x + w - u;
        price_draw(canvas, &price, price_right, y + (h - price_h) / 2, price_h);
        int text_x = badge_x + badge_r + u;
        int text_w = price_right - price.total_w - u - text_x;
        int dist_w = canvas_text_width(distance_text, s);
        bool two_lines = h >= 2 * line + u;
        int first_y = two_lines ? y + (h - 2 * line) / 2 : y + (h - line) / 2;
        canvas_disc(canvas, badge_x, y + h / 2, badge_r, accent);
        canvas_text_centered(canvas, badge_x, y + h / 2 - line / 2, number, s,
                             rank == 0 ? CANVAS_BLACK : CANVAS_WHITE);
        canvas_text_fit(name, text_w - dist_w - u, s, fitted, sizeof(fitted));
        canvas_text(canvas, text_x, first_y, fitted, s, CANVAS_BLACK);
        canvas_text_right(canvas, text_x + text_w, first_y, distance_text, s, CANVAS_BLACK);
        if (two_lines) {
            canvas_text_fit(place, text_w, s, fitted, sizeof(fitted));
            canvas_text(canvas, text_x, first_y + line, fitted, s, CANVAS_BLACK);
        }
    } else {
        // narrow: the name and the distance on the first line, the price big under them on the
        // right and the town on its left
        int pad = u / 2 + 1;
        int first_y = y + pad;
        int text_x = badge_x + badge_r + u;
        int dist_w = canvas_text_width(distance_text, s);
        canvas_disc(canvas, badge_x, first_y + line / 2, badge_r, accent);
        canvas_text_centered(canvas, badge_x, first_y, number, s,
                             rank == 0 ? CANVAS_BLACK : CANVAS_WHITE);
        canvas_text_fit(name, x + w - u - dist_w - u - text_x, s, fitted, sizeof(fitted));
        canvas_text(canvas, text_x, first_y, fitted, s, CANVAS_BLACK);
        canvas_text_right(canvas, x + w - u, first_y, distance_text, s, CANVAS_BLACK);

        int price_top = first_y + line + pad;
        int price_room = y + h - pad - price_top;
        int price_h = price_room * 78 / 100;
        price_layout_t price;
        price_layout(canvas, station->price, price_h, w / 2, &price);
        price_draw(canvas, &price, x + w - u, price_top + (price_room - price_h) / 2, price_h);
        canvas_text_fit(place, x + w - u - price.total_w - u - text_x, s, fitted, sizeof(fitted));
        canvas_text(canvas, text_x, price_top + (price_room - line) / 2, fitted, s, CANVAS_BLACK);
    }
}

void fuel_screen_render(canvas_t *canvas, const info_now_t *now, const fuel_screen_data_t *data)
{
    if (data->status != FUEL_SCREEN_OK || data->result.count < 1) {
        fuel_screen_data_t failed = *data;
        if (failed.status == FUEL_SCREEN_OK) {
            failed.status = FUEL_SCREEN_NONE_FOUND;
        }
        message(canvas, now, &failed);
        return;
    }
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    int line = canvas_text_height(s);
    canvas_fill(canvas, CANVAS_WHITE);

    // the header band: the fuel type on the left, the radius on the right
    int band_h = line + 2 * u;
    canvas_rect(canvas, 0, 0, canvas->width, band_h, CANVAS_YELLOW);
    char heading[CANVAS_WRAP_LINE_MAX], fitted[CANVAS_WRAP_LINE_MAX], radius[24];
    static const char *const type_names[] = {"SUPER E5", "SUPER E10", "DIESEL"};
    int type = (int) data->type;
    if (type < 0 || type > 2) {
        type = 0;
    }
    char raw[CANVAS_WRAP_LINE_MAX];
    snprintf(raw, sizeof(raw), "%s  %s", now->german ? "TANKPREISE" : "FUEL PRICES",
             type_names[type]);
    snprintf(radius, sizeof(radius), "%d km", data->radius_km);
    int radius_w = canvas_text_width(radius, s);
    if (canvas_text_width(raw, s) > canvas->width - 5 * u - radius_w) {
        snprintf(raw, sizeof(raw), "%s", type_names[type]);  // a narrow panel: just the fuel
    }
    canvas_text_from_utf8(raw, heading, sizeof(heading));
    canvas_text_fit(heading, canvas->width - 5 * u - radius_w, s, fitted, sizeof(fitted));
    canvas_text(canvas, 2 * u, u, fitted, s, CANVAS_BLACK);
    canvas_text_right(canvas, canvas->width - 2 * u, u, radius, s, CANVAS_BLACK);

    // the footer: the attribution the licence asks for, and when the prices were fetched
    char footer[CANVAS_WRAP_LINE_MAX], footer_raw[CANVAS_WRAP_LINE_MAX];
    for (int variant = 0; variant < 5; variant++) {
        if (variant == 0) {
            snprintf(footer_raw, sizeof(footer_raw), "%s tankerkoenig.de, CC BY 4.0, %02d:%02d",
                     now->german ? "Daten:" : "Data:", now->hour, now->minute);
        } else if (variant == 1) {
            snprintf(footer_raw, sizeof(footer_raw), "tankerkoenig.de CC BY 4.0 %02d:%02d",
                     now->hour, now->minute);
        } else if (variant == 2) {
            snprintf(footer_raw, sizeof(footer_raw), "tankerkoenig.de CC BY 4.0");
        } else if (variant == 3) {
            snprintf(footer_raw, sizeof(footer_raw), "tankerkoenig.de CC BY");
        } else {
            snprintf(footer_raw, sizeof(footer_raw), "tankerkoenig.de");
        }
        if (canvas_text_width(footer_raw, s) <= canvas->width - 4 * u) {
            break;
        }
    }
    canvas_text_from_utf8(footer_raw, footer, sizeof(footer));
    int footer_y = canvas->height - u - line;
    canvas_text_centered(canvas, canvas->width / 2, footer_y, footer, s, CANVAS_BLACK);

    // the rows, top aligned under the band, as high as if there were five
    int area_y = band_h + u;
    int area_h = footer_y - u - area_y;
    int gap = u / 2 + 1;
    int row_h = (area_h - gap * (FUEL_MAX_STATIONS - 1)) / FUEL_MAX_STATIONS;
    int count = data->result.count;
    int block_h = count * row_h + gap * (count - 1);
    int top = area_y + (area_h - block_h) / 2;
    for (int i = 0; i < count; i++) {
        draw_row(canvas, 2 * u, top + i * (row_h + gap), canvas->width - 4 * u, row_h, i,
                 &data->result.stations[i], now);
    }
}
