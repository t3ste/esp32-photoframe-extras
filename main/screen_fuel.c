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
    char title[CANVAS_WRAP_LINE_MAX], raw[CANVAS_WRAP_LINE_MAX];
    canvas_text_from_utf8(now->german ? de[index][0] : en[index][0], raw, sizeof(raw));
    canvas_text_fit(raw, canvas->width - 4 * u, s, title, sizeof(title));
    // the second part may be long (the service's own words): wrapped to at most three lines
    const char *detail = now->german ? de[index][1] : en[index][1];
    if (detail[0] == '\0' && index == FUEL_SCREEN_KEY_REFUSED) {
        detail = data->result.message;
    }
    char detail_text[CANVAS_WRAP_LINE_MAX], lines[3][CANVAS_WRAP_LINE_MAX];
    canvas_text_from_utf8(detail, detail_text, sizeof(detail_text));
    int line_count =
        detail_text[0] ? canvas_text_wrap(detail_text, canvas->width - 4 * u, s, lines, 3) : 0;

    canvas_fill(canvas, CANVAS_WHITE);
    int line_h = canvas_text_height(s);
    int block_h = line_h + (line_count > 0 ? u + line_count * line_h : 0);
    int y = canvas->height / 2 - block_h / 2;
    canvas_text_centered(canvas, canvas->width / 2, y, title, s, CANVAS_BLACK);
    for (int i = 0; i < line_count; i++) {
        canvas_text_centered(canvas, canvas->width / 2, y + line_h + u + i * line_h, lines[i], s,
                             CANVAS_BLACK);
    }
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

#if FEATURE_ROUTE_TIME
// The forms of the travel time in the header, fullest first. Those up to ROUTE_FORM_NUMBERS say the
// unit ("min"): "Hin 28 min  Rück 31 min", "Hin 28  Rück 31 min", "28 / 31 min".
typedef enum {
    ROUTE_FORM_LABEL = 0,  // the name of the route in front of the first form
    ROUTE_FORM_UNIT_EACH,
    ROUTE_FORM_UNIT_ONCE,
    ROUTE_FORM_NUMBERS,
    ROUTE_FORM_WORDS,  // "Hin 28  Rück 31", no unit
    ROUTE_FORM_PLAIN,  // "28/31"
    ROUTE_FORM_COUNT
} route_form_t;

typedef struct {
    char prefix[CANVAS_WRAP_LINE_MAX + 4];
    char text[2][48];  // the two ways, with a "!" in front of one that is long
    int width[2];      // with the room of the red block around a long one
    const char *separator;
    const char *suffix;
    int total;
} route_layout_t;

// Fills the layout of a form; false if the form does not apply (a name of the route it has not
// got).
static bool route_layout(const fuel_route_t *route, bool german, int scale, int pad,
                         route_form_t form, route_layout_t *out)
{
    memset(out, 0, sizeof(*out));
    if (form == ROUTE_FORM_LABEL) {
        if (route->label[0] == '\0') {
            return false;
        }
        char label[CANVAS_WRAP_LINE_MAX];
        canvas_text_from_utf8(route->label, label, sizeof(label));
        snprintf(out->prefix, sizeof(out->prefix), "%s  ", label);
    }
    bool words = form <= ROUTE_FORM_UNIT_ONCE || form == ROUTE_FORM_WORDS;
    bool unit_each = form <= ROUTE_FORM_UNIT_EACH;
    bool unit_once = form == ROUTE_FORM_UNIT_ONCE || form == ROUTE_FORM_NUMBERS;
    const char *word[2] = {german ? "Hin" : "To", german ? "R\xC3\xBC"
                                                           "ck"
                                                         : "Back"};
    const bool over[2] = {route->there_over, route->back_over};
    const int minutes[2] = {route->there_min, route->back_min};
    for (int i = 0; i < 2; i++) {
        char raw[48], marked[56];
        snprintf(raw, sizeof(raw), words ? "%s %d%s" : "%.0s%d%s", word[i], minutes[i],
                 unit_each ? " min" : "");
        snprintf(marked, sizeof(marked), "%s%s", over[i] ? "!" : "", raw);
        canvas_text_from_utf8(marked, out->text[i], sizeof(out->text[i]));
        out->width[i] = canvas_text_width(out->text[i], scale) + (over[i] ? 2 * pad : 0);
    }
    out->separator = form == ROUTE_FORM_NUMBERS ? " / " : (form == ROUTE_FORM_PLAIN ? "/" : "  ");
    out->suffix = unit_once ? " min" : "";
    out->total = canvas_text_width(out->prefix, scale) + out->width[0] +
                 canvas_text_width(out->separator, scale) + out->width[1] +
                 canvas_text_width(out->suffix, scale);
    return true;
}

// The fullest form that fits `room` pixels, or -1.
static int route_best_form(const fuel_route_t *route, bool german, int scale, int pad, int room)
{
    route_layout_t layout;
    for (int form = ROUTE_FORM_LABEL; form < ROUTE_FORM_COUNT; form++) {
        if (route_layout(route, german, scale, pad, (route_form_t) form, &layout) &&
            layout.total <= room) {
            return form;
        }
    }
    return -1;
}

// The travel time in the free room of the header, from `x0` to `x1`: "Hin 28 min  Rück 31 min" (in
// English "To ... Back ..."), shorter where the room is short. A way that is longer than usual is a
// red block with white text and a "!" in front. Nothing is drawn if even the shortest form does not
// fit, so the header is never crowded.
static void draw_route(canvas_t *canvas, const fuel_route_t *route, bool german, int x0, int x1)
{
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    int pad = u / 2 + 1;
    int form = route_best_form(route, german, s, pad, x1 - x0);
    route_layout_t layout;
    if (form < 0 || !route_layout(route, german, s, pad, (route_form_t) form, &layout)) {
        return;
    }
    int text_y = u;
    int box_y = u / 3 + 1;
    int box_h = canvas_text_height(s) + 2 * u - 2 * box_y;
    const bool over[2] = {route->there_over, route->back_over};
    int x = x0 + (x1 - x0 - layout.total) / 2;
    x = canvas_text(canvas, x, text_y, layout.prefix, s, CANVAS_BLACK);  // returns the x after it
    for (int i = 0; i < 2; i++) {
        if (over[i]) {
            canvas_rect(canvas, x, box_y, layout.width[i], box_h, CANVAS_RED);
            canvas_text(canvas, x + pad, text_y, layout.text[i], s, CANVAS_WHITE);
        } else {
            canvas_text(canvas, x, text_y, layout.text[i], s, CANVAS_BLACK);
        }
        x += layout.width[i];
        if (i == 0) {
            x = canvas_text(canvas, x, text_y, layout.separator, s, CANVAS_BLACK);
        }
    }
    canvas_text(canvas, x, text_y, layout.suffix, s, CANVAS_BLACK);
}

// Whether the heading is better cut to the fuel type: when a fuller form of the travel time (with
// its words and its unit "min") fits then - the words "FUEL PRICES" are known from the page.
static bool route_wants_short_heading(const fuel_route_t *route, bool german, int scale, int pad,
                                      int unit, int right_edge, const char *full_heading,
                                      const char *short_heading)
{
    if (!route->shown) {
        return false;
    }
    int full = route_best_form(route, german, scale, pad,
                               right_edge - (4 * unit + canvas_text_width(full_heading, scale)));
    int shorter =
        route_best_form(route, german, scale, pad,
                        right_edge - (4 * unit + canvas_text_width(short_heading, scale)));
    return shorter >= 0 && (full < 0 || shorter < full);
}
#endif

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
#if FEATURE_ROUTE_TIME
    if (strcmp(raw, type_names[type]) != 0 &&
        route_wants_short_heading(&data->route, now->german, s, u / 2 + 1, u,
                                  canvas->width - 4 * u - radius_w, raw, type_names[type])) {
        snprintf(raw, sizeof(raw), "%s", type_names[type]);  // the travel time wants the room
    }
#endif
    canvas_text_from_utf8(raw, heading, sizeof(heading));
    canvas_text_fit(heading, canvas->width - 5 * u - radius_w, s, fitted, sizeof(fitted));
    canvas_text(canvas, 2 * u, u, fitted, s, CANVAS_BLACK);
    canvas_text_right(canvas, canvas->width - 2 * u, u, radius, s, CANVAS_BLACK);
#if FEATURE_ROUTE_TIME
    if (data->route.shown) {
        draw_route(canvas, &data->route, now->german, 4 * u + canvas_text_width(fitted, s),
                   canvas->width - 4 * u - radius_w);
    }
#endif

    // the footer: the attribution the licence asks for and when the prices were fetched, on one
    // line if they fit (the fullest wording of the attribution that leaves room for the time)
    char long_stamp[40], short_stamp[40];
    info_format_stamp_now(now, long_stamp, sizeof(long_stamp));
    info_format_stamp_short_now(now, short_stamp, sizeof(short_stamp));
    char wordings[5][CANVAS_WRAP_LINE_MAX];
    char with_word[CANVAS_WRAP_LINE_MAX];
    snprintf(with_word, sizeof(with_word), "%s tankerkoenig.de, CC BY 4.0",
             now->german ? "Daten:" : "Data:");
    canvas_text_from_utf8(with_word, wordings[0], sizeof(wordings[0]));
    canvas_text_from_utf8("tankerkoenig.de, CC BY 4.0", wordings[1], sizeof(wordings[1]));
    canvas_text_from_utf8("tankerkoenig.de CC BY 4.0", wordings[2], sizeof(wordings[2]));
    canvas_text_from_utf8("tankerkoenig.de CC BY", wordings[3], sizeof(wordings[3]));
    canvas_text_from_utf8("tankerkoenig.de", wordings[4], sizeof(wordings[4]));
    const char *sources[5] = {wordings[0], wordings[1], wordings[2], wordings[3], wordings[4]};
    char footer[CANVAS_NOTE_LINES_MAX][CANVAS_WRAP_LINE_MAX];
    int footer_n =
        canvas_note_lines(sources, 5, long_stamp, short_stamp, canvas->width - 4 * u, s, footer);
    int footer_y = canvas->height - u - footer_n * line;
    for (int i = 0; i < footer_n; i++) {
        canvas_text_centered(canvas, canvas->width / 2, footer_y + i * line, footer[i], s,
                             CANVAS_BLACK);
    }

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
