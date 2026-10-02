#include "fx_rates.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------------------------
// Currency codes and the request

int fx_parse_codes(const char *text, char codes[][FX_CODE_LEN], int max)
{
    int count = 0;
    if (!text) {
        return 0;
    }
    const char *p = text;
    while (*p && count < max) {
        while (*p == ',' || *p == ';' || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            p++;
        }
        const char *start = p;
        while (*p && *p != ',' && *p != ';' && *p != ' ' && *p != '\t' && *p != '\r' &&
               *p != '\n') {
            p++;
        }
        if (p - start != 3) {
            continue;
        }
        char code[FX_CODE_LEN];
        bool letters = true;
        for (int i = 0; i < 3; i++) {
            letters = letters && isalpha((unsigned char) start[i]);
            code[i] = (char) toupper((unsigned char) start[i]);
        }
        code[3] = '\0';
        if (!letters || strcmp(code, "EUR") == 0) {
            continue;
        }
        bool seen = false;
        for (int i = 0; i < count; i++) {
            seen = seen || strcmp(codes[i], code) == 0;
        }
        if (!seen) {
            memcpy(codes[count++], code, FX_CODE_LEN);
        }
    }
    return count;
}

bool fx_build_url(char codes[][FX_CODE_LEN], int count, int points, char *out, size_t out_len)
{
    if (count < 1 || count > FX_MAX_CURRENCIES) {
        return false;
    }
    char joined[FX_MAX_CURRENCIES * FX_CODE_LEN + 1];
    joined[0] = '\0';
    for (int i = 0; i < count; i++) {
        if (i > 0) {
            strcat(joined, "+");
        }
        strncat(joined, codes[i], 3);
    }
    if (points < 1) {
        points = 1;
    }
    if (points > FX_MAX_POINTS) {
        points = FX_MAX_POINTS;
    }
    int n = snprintf(out, out_len,
                     "https://data-api.ecb.europa.eu/service/data/EXR/D.%s.EUR.SP00.A"
                     "?lastNObservations=%d&format=csvdata&detail=dataonly",
                     joined, points);
    return n > 0 && (size_t) n < out_len;
}

// ---------------------------------------------------------------------------------------------
// The answer

// Splits a CSV line in place into fields (a quoted field may hold commas and doubled quotes).
static int split_csv_line(char *line, char **fields, int max)
{
    int count = 0;
    char *p = line;
    while (count < max) {
        if (*p == '"') {
            char *write = ++p;
            fields[count++] = write;
            while (*p) {
                if (*p == '"' && p[1] == '"') {
                    *write++ = '"';
                    p += 2;
                } else if (*p == '"') {
                    p++;
                    break;
                } else {
                    *write++ = *p++;
                }
            }
            *write = '\0';
            if (*p == ',') {
                p++;
                continue;
            }
            break;
        }
        fields[count++] = p;
        while (*p && *p != ',') {
            p++;
        }
        if (*p == ',') {
            *p++ = '\0';
            continue;
        }
        break;
    }
    return count;
}

static fx_series_t *series_for(fx_series_t *out, int *count, int max, const char *code)
{
    for (int i = 0; i < *count; i++) {
        if (strcmp(out[i].code, code) == 0) {
            return &out[i];
        }
    }
    if (*count >= max) {
        return NULL;
    }
    fx_series_t *series = &out[(*count)++];
    memset(series, 0, sizeof(*series));
    memcpy(series->code, code, FX_CODE_LEN);
    return series;
}

// Puts a point in date order; the oldest point goes when the series is full.
static void add_point(fx_series_t *series, const char *date, float value)
{
    int pos = 0;
    while (pos < series->count && strcmp(series->dates[pos], date) < 0) {
        pos++;
    }
    if (pos < series->count && strcmp(series->dates[pos], date) == 0) {
        series->values[pos] = value;  // the same day twice: the later line wins
        return;
    }
    if (series->count == FX_MAX_POINTS) {
        if (pos == 0) {
            return;  // older than everything we keep
        }
        memmove(&series->dates[0], &series->dates[1], (size_t) (FX_MAX_POINTS - 1) * FX_DATE_LEN);
        memmove(&series->values[0], &series->values[1],
                (size_t) (FX_MAX_POINTS - 1) * sizeof(float));
        series->count--;
        pos--;
    }
    memmove(&series->dates[pos + 1], &series->dates[pos],
            (size_t) (series->count - pos) * FX_DATE_LEN);
    memmove(&series->values[pos + 1], &series->values[pos],
            (size_t) (series->count - pos) * sizeof(float));
    memcpy(series->dates[pos], date, FX_DATE_LEN - 1);
    series->dates[pos][FX_DATE_LEN - 1] = '\0';
    series->values[pos] = value;
    series->count++;
}

int fx_parse_csv(const char *csv, fx_series_t *out, int max)
{
    int series_count = 0;
    if (!csv || max < 1) {
        return 0;
    }
    int col_code = -1, col_date = -1, col_value = -1;
    const char *p = csv;
    bool header_seen = false;
    while (*p) {
        const char *end = strchr(p, '\n');
        size_t len = end ? (size_t) (end - p) : strlen(p);
        char line[512];
        if (len >= sizeof(line)) {
            len = sizeof(line) - 1;
        }
        memcpy(line, p, len);
        line[len] = '\0';
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ')) {
            line[--len] = '\0';
        }
        p = end ? end + 1 : p + strlen(p);
        if (len == 0) {
            continue;
        }
        char *fields[40];
        int n = split_csv_line(line, fields, 40);
        if (!header_seen) {
            for (int i = 0; i < n; i++) {
                if (strcmp(fields[i], "CURRENCY") == 0) {
                    col_code = i;
                } else if (strcmp(fields[i], "TIME_PERIOD") == 0) {
                    col_date = i;
                } else if (strcmp(fields[i], "OBS_VALUE") == 0) {
                    col_value = i;
                }
            }
            if (col_code < 0 || col_date < 0 || col_value < 0) {
                return 0;  // not the answer we asked for
            }
            header_seen = true;
            continue;
        }
        if (n <= col_code || n <= col_date || n <= col_value) {
            continue;
        }
        const char *code = fields[col_code];
        const char *date = fields[col_date];
        const char *value_text = fields[col_value];
        if (strlen(code) != 3 || strlen(date) != FX_DATE_LEN - 1 || value_text[0] == '\0') {
            continue;
        }
        char *stop = NULL;
        float value = strtof(value_text, &stop);
        if (stop == value_text || *stop != '\0' || !(value > 0.0f)) {
            continue;  // "NaN", empty or nonsense
        }
        fx_series_t *series = series_for(out, &series_count, max, code);
        if (series) {
            add_point(series, date, value);
        }
    }
    // a currency that came without any usable value is not a series
    int kept = 0;
    for (int i = 0; i < series_count; i++) {
        if (out[i].count > 0) {
            if (kept != i) {
                out[kept] = out[i];
            }
            kept++;
        }
    }
    return kept;
}

int fx_order_series(fx_series_t *series, int count, char codes[][FX_CODE_LEN], int code_count)
{
    fx_series_t ordered[FX_MAX_CURRENCIES];
    int kept = 0;
    for (int c = 0; c < code_count && kept < FX_MAX_CURRENCIES; c++) {
        for (int i = 0; i < count; i++) {
            if (strcmp(series[i].code, codes[c]) == 0) {
                ordered[kept++] = series[i];
                break;
            }
        }
    }
    for (int i = 0; i < kept; i++) {
        series[i] = ordered[i];
    }
    return kept;
}

// Days since 1970-01-01 of a calendar date (Howard Hinnant's days-from-civil).
static long civil_days(int year, int month, int day)
{
    long y = year - (month <= 2 ? 1 : 0);
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int fx_drop_stale(fx_series_t *series, int count, int year, int month, int day, int max_age_days)
{
    long today = civil_days(year, month, day);
    int kept = 0;
    for (int i = 0; i < count; i++) {
        int y = 0, m = 0, d = 0;
        bool fresh = false;
        if (series[i].count > 0 &&
            sscanf(series[i].dates[series[i].count - 1], "%d-%d-%d", &y, &m, &d) == 3) {
            fresh = today - civil_days(y, m, d) <= max_age_days;
        }
        if (fresh) {
            if (kept != i) {
                series[kept] = series[i];
            }
            kept++;
        }
    }
    return kept;
}

float fx_period_change_percent(const fx_series_t *series)
{
    if (!series || series->count < 2) {
        return 0.0f;
    }
    float first = series->values[0];
    float last = series->values[series->count - 1];
    return first > 0.0f ? (last - first) / first * 100.0f : 0.0f;
}

float fx_change_percent(const fx_series_t *series)
{
    if (!series || series->count < 2) {
        return 0.0f;
    }
    float last = series->values[series->count - 1];
    float previous = series->values[series->count - 2];
    return previous > 0.0f ? (last - previous) / previous * 100.0f : 0.0f;
}

void fx_format_rate(float value, char *out, size_t out_len)
{
    int decimals = value < 10.0f ? 4 : (value < 100.0f ? 3 : 2);
    snprintf(out, out_len, "%.*f", decimals, (double) value);
}
