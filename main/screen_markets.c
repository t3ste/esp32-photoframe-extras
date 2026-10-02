#include "screen_markets.h"

#include <stdio.h>
#include <string.h>

#include "screen_digits.h"

#define MARKETS_MIN_ROWS 3  // fewer symbols get rows as high as if there were three

static void message(canvas_t *canvas, const info_now_t *now, markets_screen_status_t status)
{
    static const char *const en[][2] = {
        {"", ""},
        {"No network", "Prices arrive once WiFi is up"},
        {"No data source", "Turn Yahoo on or enter an API key in the settings"},
        {"Prices not available", "Trying again next time"},
    };
    static const char *const de[][2] = {
        {"", ""},
        {"Kein Netz", "Kurse kommen, sobald WLAN da ist"},
        {"Keine Datenquelle", "Yahoo einschalten oder API-Key in den Einstellungen eintragen"},
        {"Kurse nicht erreichbar",
         "Neuer Versuch beim n\xC3\xA4"
         "chsten Mal"},
    };
    int index = (int) status;
    if (index < 1 || index > 3) {
        index = 3;
    }
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    int max_w = canvas->width - 4 * u;
    char raw[CANVAS_WRAP_LINE_MAX], heading[CANVAS_WRAP_LINE_MAX];
    char lines[3][CANVAS_WRAP_LINE_MAX];
    canvas_text_from_utf8(now->german ? de[index][0] : en[index][0], raw, sizeof(raw));
    canvas_text_fit(raw, max_w, s, heading, sizeof(heading));
    canvas_text_from_utf8(now->german ? de[index][1] : en[index][1], raw, sizeof(raw));
    int n = canvas_text_wrap(raw, max_w, s, lines, 3);

    canvas_fill(canvas, CANVAS_WHITE);
    int line_h = canvas_text_height(s);
    int block_h = line_h + u + n * line_h;
    int y = (canvas->height - block_h) / 2;
    canvas_text_centered(canvas, canvas->width / 2, y, heading, s, CANVAS_BLACK);
    for (int i = 0; i < n; i++) {
        canvas_text_centered(canvas, canvas->width / 2, y + line_h + u + i * line_h, lines[i], s,
                             CANVAS_BLACK);
    }
}

// The trend of a series: colour of the change text and whether an arrow is drawn.
static canvas_color_t trend_color(float change, bool *up, bool *down)
{
    *up = change > 0.004f;
    *down = change < -0.004f;
    return *up ? CANVAS_GREEN : (*down ? CANVAS_RED : CANVAS_BLACK);
}

static void draw_change(canvas_t *canvas, int right_x, int y, float change, canvas_color_t trend,
                        bool up, bool down)
{
    int s = canvas_text_scale(canvas, 1);
    int u = canvas_unit(canvas);
    char percent[16];
    snprintf(percent, sizeof(percent), "%+.2f%%", (double) change);
    int text_h = canvas_text_height(s);
    int text_w = canvas_text_width(percent, s);
    canvas_text_right(canvas, right_x, y, percent, s, trend);
    if (up || down) {
        canvas_arrow(canvas, right_x - text_w - u, y + text_h / 2, text_h * 6 / 10, up, trend);
    }
}

// The width the daily change takes at the right of a row: its text, the arrow and a gap.
static int change_width(const canvas_t *canvas, float change)
{
    int s = canvas_text_scale(canvas, 1);
    char percent[16];
    snprintf(percent, sizeof(percent), "%+.2f%%", (double) change);
    return canvas_text_width(percent, s) + canvas_unit(canvas) + canvas_text_height(s) * 6 / 10;
}

// The days a chart covers (-1 if it has no dates to say so).
static int span_days(const market_series_t *series)
{
    return series->count > 0 ? info_days_between(series->dates[0], series->dates[series->count - 1])
                             : -1;
}

// The note beside the chart - the span and the change over it - in the most informative form that
// fits `max_w`. With `span_in_header` the span is not repeated here: only the change.
static void draw_chart_label(canvas_t *canvas, int x, int y, int max_w, bool german,
                             const market_series_t *series, bool span_in_header)
{
    char labels[INFO_SPAN_LABELS_MAX][INFO_SPAN_LABEL_LEN];
    int n = info_span_labels(span_in_header ? 0 : span_days(series), series->count >= 2,
                             market_period_change_percent(series), german, labels);
    const char *texts[INFO_SPAN_LABELS_MAX];
    for (int i = 0; i < n; i++) {
        texts[i] = labels[i];
    }
    canvas_text_first_fit(canvas, x, y, max_w, canvas_text_scale(canvas, 1), CANVAS_BLACK, texts,
                          n);
}

// The width of the column that holds the symbol and the name of a row of `w` by `h` pixels.
static int title_width(int w, int h)
{
    return w >= 4 * h ? w * 34 / 100 : w * 42 / 100;
}

// The symbol (at `sym_scale`) with the name under it, in a column `col_w` wide; the name is left
// out if the two lines do not fit `avail_h`.
static void draw_title(canvas_t *canvas, int x, int y, int col_w, int avail_h, int sym_scale,
                       const market_series_t *series)
{
    int s = canvas_text_scale(canvas, 1);
    char symbol[CANVAS_WRAP_LINE_MAX], name[CANVAS_WRAP_LINE_MAX], fitted[CANVAS_WRAP_LINE_MAX];
    canvas_text_from_utf8(series->symbol, symbol, sizeof(symbol));
    canvas_text_from_utf8(series->name, name, sizeof(name));
    int sym_h = canvas_text_height(sym_scale);
    int name_h = canvas_text_height(s);
    bool show_name = name[0] != '\0' && sym_h + name_h <= avail_h;
    int block_h = show_name ? sym_h + name_h : sym_h;
    int top = y + (avail_h - block_h) / 2;
    canvas_text_fit(symbol, col_w, sym_scale, fitted, sizeof(fitted));
    canvas_text(canvas, x, top, fitted, sym_scale, CANVAS_BLACK);
    if (show_name) {
        canvas_text_fit(name, col_w, s, fitted, sizeof(fitted));
        canvas_text(canvas, x, top + sym_h, fitted, s, CANVAS_BLACK);
    }
}

// The price in big digits with the currency behind it, vertically centred in (y, h) and at most
// `max_w` wide.
static void draw_price(canvas_t *canvas, int x, int y, int h, int max_w, int max_digits_h,
                       const market_series_t *series, canvas_color_t color)
{
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    char price[24], currency[MARKET_CURRENCY_LEN * 2];
    market_format_price(series->symbol, series->values[series->count - 1], price, sizeof(price));
    canvas_text_from_utf8(series->currency, currency, sizeof(currency));
    int cur_w = currency[0] ? canvas_text_width(currency, s) + u / 2 + 1 : 0;
    int digits_h = canvas_big_fit_height(price, max_w - cur_w, max_digits_h);
    int digits_y = y + (h - digits_h) / 2;
    canvas_big_text(canvas, x, digits_y, price, digits_h, color);
    if (currency[0]) {
        int text_h = canvas_text_height(s);
        canvas_text(canvas, x + canvas_big_width(price, digits_h) + u / 2 + 1,
                    digits_y + digits_h - text_h, currency, s, color);
    }
}

static void draw_row(canvas_t *canvas, int x, int y, int w, int h, int sym_scale, bool german,
                     bool span_in_header, const market_series_t *series)
{
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    int pad = u / 2 + 1;
    int text_h = canvas_text_height(s);
    bool wide = w >= 4 * h;
    canvas_frame(canvas, x, y, w, h, 1 + u / 12, CANVAS_BLACK);

    if (series->count < 1) {  // a symbol nobody could serve
        char na[CANVAS_WRAP_LINE_MAX], fitted[CANVAS_WRAP_LINE_MAX];
        int col_w = title_width(w, h);
        draw_title(canvas, x + u, y, col_w, h, sym_scale, series);
        canvas_text_from_utf8("n/a", na, sizeof(na));
        canvas_text_fit(na, w - col_w - 4 * u, s, fitted, sizeof(fitted));
        canvas_text_right(canvas, x + w - u, y + (h - text_h) / 2, fitted, s, CANVAS_BLACK);
        return;
    }

    float change = market_change_percent(series);
    bool up, down;
    canvas_color_t trend = trend_color(change, &up, &down);
    canvas_color_t price_color = series->stale ? CANVAS_BLUE : CANVAS_BLACK;

    if (wide) {
        int col_w = title_width(w, h);
        int spark_w = w * 30 / 100;
        draw_title(canvas, x + u, y, col_w, h, sym_scale, series);

        int price_x = x + u + col_w + u;
        int price_w = x + w - u - spark_w - u - price_x;
        draw_price(canvas, price_x, y, h, price_w, h * 58 / 100, series, price_color);

        int spark_x = x + w - u - spark_w;
        canvas_sparkline(canvas, spark_x, y + pad, spark_w, h - 2 * pad - text_h - pad / 2,
                         series->values, series->count, trend);
        draw_chart_label(canvas, spark_x, y + h - pad - text_h,
                         spark_w - change_width(canvas, change) - u / 2, german, series,
                         span_in_header);
        draw_change(canvas, x + w - u, y + h - pad - text_h, change, trend, up, down);
    } else {
        int top_h = h * 52 / 100;
        int col_w = title_width(w, h);
        draw_title(canvas, x + u, y, col_w, top_h, sym_scale, series);
        int price_x = x + u + col_w + u;
        draw_price(canvas, price_x, y, top_h, x + w - u - price_x, top_h * 75 / 100, series,
                   price_color);

        int bottom_y = y + top_h;
        int bottom_h = h - top_h - pad;
        char percent[16];
        snprintf(percent, sizeof(percent), "%+.2f%%", (double) change);
        int text_w = canvas_text_width(percent, s) + 3 * u;
        canvas_sparkline(canvas, x + u, bottom_y, w - 2 * u - text_w, bottom_h - text_h,
                         series->values, series->count, trend);
        draw_chart_label(canvas, x + u, bottom_y + bottom_h - text_h,
                         w - 2 * u - change_width(canvas, change) - 2 * u, german, series,
                         span_in_header);
        draw_change(canvas, x + w - u, bottom_y + (bottom_h - text_h) / 2, change, trend, up, down);
    }
}

// The footer lines: the sources of the rows that have data and when they were fetched on one line
// ("Yahoo Finance, Twelve Data - 30 Sep 14:35"; wrapped over more lines on a narrow panel), and a
// line explaining the blue prices if a row is from the cache. Returns the number of lines written.
static int footer_lines(const info_now_t *now, const markets_screen_data_t *data, int rows,
                        int max_width, int scale, char lines[][CANVAS_WRAP_LINE_MAX])
{
    bool used[MARKET_PROVIDER_COUNT] = {false};
    bool stale = false;
    for (int i = 0; i < rows; i++) {
        if (data->series[i].count > 0) {
            used[data->series[i].provider % MARKET_PROVIDER_COUNT] = true;
            stale = stale || data->series[i].stale;
        }
    }
    char names[CANVAS_WRAP_LINE_MAX],
        with_word[CANVAS_WRAP_LINE_MAX + 16];  // the word and the names
    size_t n = 0;
    names[0] = '\0';
    for (int p = 0; p < MARKET_PROVIDER_COUNT && n < sizeof(names); p++) {
        if (used[p]) {
            n += (size_t) snprintf(names + n, sizeof(names) - n, "%s%s", n > 0 ? ", " : "",
                                   market_provider_name((market_provider_t) p));
        }
    }
    snprintf(with_word, sizeof(with_word), "%s%s", now->german ? "Quelle: " : "Source: ", names);
    char long_stamp[40], short_stamp[40];
    long_stamp[0] = short_stamp[0] = '\0';
    info_format_stamp(now->german, data->updated_year, data->updated_month, data->updated_day,
                      data->updated_hour, data->updated_minute, long_stamp, sizeof(long_stamp));
    info_format_stamp_short(now->german, data->updated_year, data->updated_month, data->updated_day,
                            data->updated_hour, data->updated_minute, short_stamp,
                            sizeof(short_stamp));
    char display[2][CANVAS_WRAP_LINE_MAX];
    canvas_text_from_utf8(with_word, display[0], sizeof(display[0]));
    canvas_text_from_utf8(names, display[1], sizeof(display[1]));
    const char *sources[2] = {display[0], display[1]};
    int count = canvas_note_lines(sources, 2, long_stamp, short_stamp, max_width, scale, lines);
    if (stale && count < 4) {
        canvas_text_from_utf8(now->german ? "Blau = zuletzt bekannt" : "Blue = last known",
                              display[0], sizeof(display[0]));
        canvas_text_fit(display[0], max_width, scale, lines[count], CANVAS_WRAP_LINE_MAX);
        count++;
    }
    return count;
}

void markets_screen_render(canvas_t *canvas, const info_now_t *now,
                           const markets_screen_data_t *data)
{
    bool any = false;
    for (int i = 0; i < data->count && i < MARKET_MAX_SYMBOLS; i++) {
        any = any || data->series[i].count > 0;
    }
    if (data->status != MARKETS_SCREEN_OK || !any) {
        message(canvas, now,
                data->status != MARKETS_SCREEN_OK ? data->status : MARKETS_SCREEN_FETCH_FAILED);
        return;
    }
    int rows = data->count > MARKET_MAX_SYMBOLS ? MARKET_MAX_SYMBOLS : data->count;
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    int line = canvas_text_height(s);
    canvas_fill(canvas, CANVAS_WHITE);

    // the header band, with the trading day of the newest price (named, so that it is not taken for
    // today's date) and a yellow "!" when that day is older than the last trading day
    int band_h = line + 2 * u;
    canvas_rect(canvas, 0, 0, canvas->width, band_h, CANVAS_BLUE);
    char heading[CANVAS_WRAP_LINE_MAX], date[CANVAS_WRAP_LINE_MAX], fitted[CANVAS_WRAP_LINE_MAX];
    // all charts cover the same time: said once in the header ("MARKETS 41 d"), not in every row
    int common_span = -1;
    for (int i = 0; i < rows; i++) {
        if (data->series[i].count < 1) {
            continue;
        }
        int days = span_days(&data->series[i]);
        common_span = (common_span == -1 || common_span == days) ? days : -2;
        if (common_span == -2) {
            break;
        }
    }
    const char *newest = NULL;
    for (int i = 0; i < rows; i++) {
        const market_series_t *series = &data->series[i];
        if (series->count > 0 &&
            (!newest || strcmp(series->dates[series->count - 1], newest) > 0)) {
            newest = series->dates[series->count - 1];
        }
    }
    // the heading names the span of the charts if they all share it and it fits beside the date;
    // the date has its word ("Close") if that fits too
    char span_text[16], date_short[CANVAS_WRAP_LINE_MAX];
    info_format_span(common_span > 0 ? common_span : 0, now->german, span_text, sizeof(span_text));
    snprintf(heading, sizeof(heading), "%s", now->german ? "KURSE" : "MARKETS");
    date[0] = date_short[0] = '\0';
    bool late = false;
    if (newest) {
        late = info_trading_days_behind(newest, now) >= 1;
        info_format_day_label(INFO_DAY_CLOSE, now->german, newest, false, date_short,
                              sizeof(date_short));
        info_format_day_label(INFO_DAY_CLOSE, now->german, newest, true, date, sizeof(date));
    }
    int avail = canvas->width - 5 * u;
    char with_span[CANVAS_WRAP_LINE_MAX + 24];  // the heading, two blanks and the span
    snprintf(with_span, sizeof(with_span), "%s  %s", heading, span_text);
    bool span_in_header =
        span_text[0] &&
        canvas_text_width(with_span, s) + canvas_header_label_width(date_short, late, s) <= avail;
    if (span_in_header) {
        snprintf(heading, sizeof(heading), "%.*s", (int) sizeof(heading) - 1, with_span);
    }
    if (canvas_text_width(heading, s) + canvas_header_label_width(date, late, s) > avail) {
        snprintf(date, sizeof(date), "%s",
                 date_short);  // a narrow panel: the date without the word
    }
    int date_w = canvas_header_label_width(date, late, s);
    canvas_text_fit(heading, canvas->width - 5 * u - date_w, s, fitted, sizeof(fitted));
    canvas_text(canvas, 2 * u, u, fitted, s, CANVAS_WHITE);
    canvas_header_label(canvas, canvas->width - 2 * u, u, s, date, late, CANVAS_WHITE);

    // the footer
    char footer[4][CANVAS_WRAP_LINE_MAX];
    int footer_n = footer_lines(now, data, rows, canvas->width - 4 * u, s, footer);
    int footer_y = canvas->height - u - footer_n * line;
    for (int i = 0; i < footer_n; i++) {
        canvas_text_centered(canvas, canvas->width / 2, footer_y + i * line, footer[i], s,
                             CANVAS_BLACK);
    }

    // the rows, centred in the room between the band and the footer
    int area_y = band_h + u;
    int area_h = footer_y - u - area_y;
    int gap = u / 2 + 1;
    int slots = rows < MARKETS_MIN_ROWS ? MARKETS_MIN_ROWS : rows;
    int row_h = (area_h - gap * (slots - 1)) / slots;
    int block_h = rows * row_h + gap * (rows - 1);
    int top = area_y + (area_h - block_h) / 2;
    int row_w = canvas->width - 4 * u;

    // one symbol size for all rows: the biggest at which the longest symbol fits its column
    int sym_scale = 2 * s;
    for (int i = 0; i < rows; i++) {
        char symbol[CANVAS_WRAP_LINE_MAX];
        canvas_text_from_utf8(data->series[i].symbol, symbol, sizeof(symbol));
        int fit = canvas_text_fit_scale(symbol, title_width(row_w, row_h), 2 * s);
        sym_scale = fit < sym_scale ? fit : sym_scale;
    }
    sym_scale = sym_scale < s ? s : sym_scale;

    for (int i = 0; i < rows; i++) {
        draw_row(canvas, 2 * u, top + i * (row_h + gap), row_w, row_h, sym_scale, now->german,
                 span_in_header, &data->series[i]);
    }
}
