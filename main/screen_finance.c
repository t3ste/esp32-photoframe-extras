#include "screen_finance.h"

#include <stdio.h>
#include <string.h>

#include "screen_digits.h"

#define FINANCE_MIN_ROWS 3  // fewer currencies get rows as high as if there were three

static void message(canvas_t *canvas, const info_now_t *now, finance_screen_status_t status)
{
    static const char *const en[][2] = {
        {"", ""},
        {"No network", "Rates arrive once WiFi is up"},
        {"Exchange rates not available", "Trying again next time"},
    };
    static const char *const de[][2] = {
        {"", ""},
        {"Kein Netz", "Kurse kommen, sobald WLAN da ist"},
        {"Wechselkurse nicht erreichbar",
         "Neuer Versuch beim n\xC3\xA4"
         "chsten Mal"},
    };
    int index = (status == FINANCE_SCREEN_NO_NETWORK) ? 1 : 2;
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    char lines[2][CANVAS_WRAP_LINE_MAX];
    for (int i = 0; i < 2; i++) {
        char raw[CANVAS_WRAP_LINE_MAX];
        canvas_text_from_utf8(now->german ? de[index][i] : en[index][i], raw, sizeof(raw));
        canvas_text_fit(raw, canvas->width - 4 * u, s, lines[i], sizeof(lines[i]));
    }
    canvas_fill(canvas, CANVAS_WHITE);
    int y = canvas->height / 2 - canvas_text_height(s);
    canvas_text_centered(canvas, canvas->width / 2, y, lines[0], s, CANVAS_BLACK);
    canvas_text_centered(canvas, canvas->width / 2, y + canvas_text_height(s) + u, lines[1], s,
                         CANVAS_BLACK);
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
static int span_days(const fx_series_t *series)
{
    return series->count > 0 ? info_days_between(series->dates[0], series->dates[series->count - 1])
                             : -1;
}

// The note beside the chart - the span and the change over it - in the most informative form that
// fits `max_w`. With `span_in_header` the span is not repeated here: only the change.
static void draw_chart_label(canvas_t *canvas, int x, int y, int max_w, bool german,
                             const fx_series_t *series, bool span_in_header)
{
    char labels[INFO_SPAN_LABELS_MAX][INFO_SPAN_LABEL_LEN];
    int n = info_span_labels(span_in_header ? 0 : span_days(series), series->count >= 2,
                             fx_period_change_percent(series), german, labels);
    const char *texts[INFO_SPAN_LABELS_MAX];
    for (int i = 0; i < n; i++) {
        texts[i] = labels[i];
    }
    canvas_text_first_fit(canvas, x, y, max_w, canvas_text_scale(canvas, 1), CANVAS_BLACK, texts,
                          n);
}

static void draw_row(canvas_t *canvas, int x, int y, int w, int h, bool german, bool span_in_header,
                     const fx_series_t *series)
{
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    int pad = u / 2 + 1;
    canvas_frame(canvas, x, y, w, h, 1 + u / 12, CANVAS_BLACK);

    float change = fx_change_percent(series);
    bool up = change > 0.004f, down = change < -0.004f;
    canvas_color_t trend = up ? CANVAS_GREEN : (down ? CANVAS_RED : CANVAS_BLACK);
    char rate[24], percent[16];
    fx_format_rate(series->values[series->count - 1], rate, sizeof(rate));
    snprintf(percent, sizeof(percent), "%+.2f%%", (double) change);

    int code_scale = s * 2;
    int code_w = canvas_text_width(series->code, code_scale);
    char caption[CANVAS_WRAP_LINE_MAX];
    canvas_text_from_utf8("1 EUR =", caption, sizeof(caption));
    bool wide = w >= 4 * h;

    if (wide) {
        int code_h = canvas_text_height(code_scale) + canvas_text_height(s);
        int code_y = y + (h - code_h) / 2;
        canvas_text(canvas, x + u, code_y, series->code, code_scale, CANVAS_BLACK);
        canvas_text(canvas, x + u, code_y + canvas_text_height(code_scale), caption, s,
                    CANVAS_BLACK);

        int rate_x = x + u + code_w + 2 * u;
        int spark_w = w * 34 / 100;
        int rate_w = x + w - u - spark_w - u - rate_x;
        int digits_h = canvas_big_fit_height(rate, rate_w, h * 58 / 100);
        canvas_big_text(canvas, rate_x, y + (h - digits_h) / 2, rate, digits_h, CANVAS_BLACK);

        int spark_x = x + w - u - spark_w;
        int text_h = canvas_text_height(s);
        canvas_sparkline(canvas, spark_x, y + pad, spark_w, h - 2 * pad - text_h - pad / 2,
                         series->values, series->count, trend);
        int text_y = y + h - pad - text_h;
        int text_w = canvas_text_width(percent, s);
        draw_chart_label(canvas, spark_x, text_y, spark_w - change_width(canvas, change) - u / 2,
                         german, series, span_in_header);
        canvas_text_right(canvas, x + w - u, text_y, percent, s, trend);
        if (up || down) {
            canvas_arrow(canvas, x + w - u - text_w - u, text_y + text_h / 2, text_h * 6 / 10, up,
                         trend);
        }
    } else {
        int top_h = h * 52 / 100;
        canvas_text(canvas, x + u, y + (top_h - canvas_text_height(code_scale)) / 2, series->code,
                    code_scale, CANVAS_BLACK);
        int rate_room = w - 2 * u - code_w - 2 * u;
        int digits_h = canvas_big_fit_height(rate, rate_room, top_h * 75 / 100);
        canvas_big_text(canvas, x + w - u - canvas_big_width(rate, digits_h),
                        y + (top_h - digits_h) / 2, rate, digits_h, CANVAS_BLACK);
        int bottom_y = y + top_h;
        int bottom_h = h - top_h - pad;
        int text_w = canvas_text_width(percent, s) + 3 * u;
        int text_h = canvas_text_height(s);
        canvas_sparkline(canvas, x + u, bottom_y, w - 2 * u - text_w, bottom_h - text_h,
                         series->values, series->count, trend);
        draw_chart_label(canvas, x + u, bottom_y + bottom_h - text_h,
                         w - 2 * u - change_width(canvas, change) - 2 * u, german, series,
                         span_in_header);
        int text_y = bottom_y + (bottom_h - text_h) / 2;
        canvas_text_right(canvas, x + w - u, text_y, percent, s, trend);
        if (up || down) {
            canvas_arrow(canvas, x + w - u - canvas_text_width(percent, s) - u, text_y + text_h / 2,
                         text_h * 6 / 10, up, trend);
        }
    }
}

void finance_screen_render(canvas_t *canvas, const info_now_t *now,
                           const finance_screen_data_t *data)
{
    if (data->status != FINANCE_SCREEN_OK || data->count < 1) {
        message(canvas, now,
                data->status != FINANCE_SCREEN_OK ? data->status : FINANCE_SCREEN_FETCH_FAILED);
        return;
    }
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    int line = canvas_text_height(s);
    canvas_fill(canvas, CANVAS_WHITE);

    // the header band, with the day of the newest rate (named, so that it is not taken for today's
    // date) and a yellow "!" when that day is older than the last trading day
    int band_h = line + 2 * u;
    canvas_rect(canvas, 0, 0, canvas->width, band_h, CANVAS_BLUE);
    char heading[CANVAS_WRAP_LINE_MAX], date[CANVAS_WRAP_LINE_MAX], fitted[CANVAS_WRAP_LINE_MAX];
    // all charts cover the same time: said once in the header ("EXCHANGE RATES 41 d"), not in every
    // row
    int common_span = -1;
    for (int i = 0; i < data->count; i++) {
        if (data->series[i].count < 1) {
            continue;
        }
        int days = span_days(&data->series[i]);
        common_span = (common_span == -1 || common_span == days) ? days : -2;
        if (common_span == -2) {
            break;
        }
    }
    char span_text[16];
    info_format_span(common_span > 0 ? common_span : 0, now->german, span_text, sizeof(span_text));
    canvas_text_from_utf8(now->german ? "WECHSELKURSE" : "EXCHANGE RATES", heading,
                          sizeof(heading));
    const fx_series_t *first = &data->series[0];
    // the heading names the span of the charts if they all share it and it fits beside the date;
    // the date has its word ("Rates") if that fits too
    char date_short[CANVAS_WRAP_LINE_MAX];
    date[0] = date_short[0] = '\0';
    bool late = false;
    if (first->count > 0) {
        const char *newest = first->dates[first->count - 1];
        late = info_trading_days_behind(newest, now) >= 1;
        info_format_day_label(INFO_DAY_RATE, now->german, newest, false, date_short,
                              sizeof(date_short));
        info_format_day_label(INFO_DAY_RATE, now->german, newest, true, date, sizeof(date));
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

    // the footer: the source and when the rates were fetched, on one line if they fit
    char long_stamp[40], short_stamp[40];
    info_format_stamp_now(now, long_stamp, sizeof(long_stamp));
    info_format_stamp_short_now(now, short_stamp, sizeof(short_stamp));
    char source_a[CANVAS_WRAP_LINE_MAX], source_b[CANVAS_WRAP_LINE_MAX];
    canvas_text_from_utf8(now->german ? "Quelle: EZB-Referenzkurse" : "Source: ECB reference rates",
                          source_a, sizeof(source_a));
    canvas_text_from_utf8(now->german ? "EZB-Referenzkurse" : "ECB reference rates", source_b,
                          sizeof(source_b));
    const char *sources[2] = {source_a, source_b};
    char footer[CANVAS_NOTE_LINES_MAX][CANVAS_WRAP_LINE_MAX];
    int footer_n =
        canvas_note_lines(sources, 2, long_stamp, short_stamp, canvas->width - 4 * u, s, footer);
    int footer_y = canvas->height - u - footer_n * line;
    for (int i = 0; i < footer_n; i++) {
        canvas_text_centered(canvas, canvas->width / 2, footer_y + i * line, footer[i], s,
                             CANVAS_BLACK);
    }

    // the rows, centred in the room between the band and the footer
    int area_y = band_h + u;
    int area_h = footer_y - u - area_y;
    int gap = u / 2 + 1;
    int slots = data->count < FINANCE_MIN_ROWS ? FINANCE_MIN_ROWS : data->count;
    int row_h = (area_h - gap * (slots - 1)) / slots;
    int block_h = data->count * row_h + gap * (data->count - 1);
    int top = area_y + (area_h - block_h) / 2;
    for (int i = 0; i < data->count; i++) {
        draw_row(canvas, 2 * u, top + i * (row_h + gap), canvas->width - 4 * u, row_h, now->german,
                 span_in_header, &data->series[i]);
    }
}
