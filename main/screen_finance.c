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

// "Tue 29 Sep" for an ISO date, as display text.
static void date_text(const info_now_t *now, const char *iso_date, char *out, size_t out_len)
{
    int year = 0, month = 0, day = 0;
    if (sscanf(iso_date, "%d-%d-%d", &year, &month, &day) != 3) {
        out[0] = '\0';
        return;
    }
    char weekday[16], month_name[24], month_short[8];
    canvas_text_from_utf8(info_weekday_short(info_weekday(year, month, day), now->german), weekday,
                          sizeof(weekday));
    canvas_text_from_utf8(info_month_name(month, now->german), month_name, sizeof(month_name));
    snprintf(month_short, sizeof(month_short), "%.3s", month_name);
    snprintf(out, out_len, "%s %d%s %s", weekday, day, now->german ? "." : "", month_short);
}

static void draw_row(canvas_t *canvas, int x, int y, int w, int h, const fx_series_t *series)
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
        canvas_sparkline(canvas, x + u, bottom_y, w - 2 * u - text_w, bottom_h, series->values,
                         series->count, trend);
        int text_h = canvas_text_height(s);
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

    // the header band, with the date of the newest rate
    int band_h = line + 2 * u;
    canvas_rect(canvas, 0, 0, canvas->width, band_h, CANVAS_BLUE);
    char heading[CANVAS_WRAP_LINE_MAX], date[CANVAS_WRAP_LINE_MAX], fitted[CANVAS_WRAP_LINE_MAX];
    canvas_text_from_utf8(now->german ? "WECHSELKURSE" : "EXCHANGE RATES", heading,
                          sizeof(heading));
    const fx_series_t *first = &data->series[0];
    date[0] = '\0';
    if (first->count > 0) {
        date_text(now, first->dates[first->count - 1], date, sizeof(date));
    }
    int date_w = canvas_text_width(date, s);
    canvas_text_fit(heading, canvas->width - 5 * u - date_w, s, fitted, sizeof(fitted));
    canvas_text(canvas, 2 * u, u, fitted, s, CANVAS_WHITE);
    canvas_text_right(canvas, canvas->width - 2 * u, u, date, s, CANVAS_WHITE);

    // the footer
    char footer[CANVAS_WRAP_LINE_MAX], footer_fitted[CANVAS_WRAP_LINE_MAX];
    canvas_text_from_utf8(now->german ? "Quelle: EZB-Referenzkurse" : "Source: ECB reference rates",
                          footer, sizeof(footer));
    canvas_text_fit(footer, canvas->width - 4 * u, s, footer_fitted, sizeof(footer_fitted));
    int footer_y = canvas->height - u - line;
    canvas_text_centered(canvas, canvas->width / 2, footer_y, footer_fitted, s, CANVAS_BLACK);

    // the rows, centred in the room between the band and the footer
    int area_y = band_h + u;
    int area_h = footer_y - u - area_y;
    int gap = u / 2 + 1;
    int slots = data->count < FINANCE_MIN_ROWS ? FINANCE_MIN_ROWS : data->count;
    int row_h = (area_h - gap * (slots - 1)) / slots;
    int block_h = data->count * row_h + gap * (data->count - 1);
    int top = area_y + (area_h - block_h) / 2;
    for (int i = 0; i < data->count; i++) {
        draw_row(canvas, 2 * u, top + i * (row_h + gap), canvas->width - 4 * u, row_h,
                 &data->series[i]);
    }
}
