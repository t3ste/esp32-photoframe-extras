#include "agenda_order.h"

#include <stdio.h>

bool agenda_priority_parse(const char *text, uint8_t order[AGENDA_CAL_COUNT])
{
    if (!text) {
        return false;
    }
    uint8_t found[AGENDA_CAL_COUNT];
    int count = 0;
    bool seen[AGENDA_CAL_COUNT] = {false};
    for (const char *p = text; *p; p++) {
        char c = *p;
        if (c >= 'a' && c <= 'e') {
            c = (char) (c - 'a' + 'A');
        }
        if (c < 'A' || c > 'E') {
            continue;  // a separator
        }
        int idx = c - 'A';
        if (seen[idx] || count >= AGENDA_CAL_COUNT) {
            return false;  // a calendar named twice
        }
        seen[idx] = true;
        found[count++] = (uint8_t) idx;
    }
    if (count != AGENDA_CAL_COUNT) {
        return false;
    }
    for (int i = 0; i < AGENDA_CAL_COUNT; i++) {
        order[i] = found[i];
    }
    return true;
}

void agenda_priority_format(const uint8_t order[AGENDA_CAL_COUNT], char out[AGENDA_PRIORITY_LEN])
{
    for (int i = 0; i < AGENDA_CAL_COUNT; i++) {
        out[i] = (char) ('A' + (order[i] < AGENDA_CAL_COUNT ? order[i] : 0));
    }
    out[AGENDA_CAL_COUNT] = '\0';
}

void agenda_priority_ranks(const uint8_t order[AGENDA_CAL_COUNT], uint8_t rank_of[AGENDA_CAL_COUNT])
{
    for (int i = 0; i < AGENDA_CAL_COUNT; i++) {
        rank_of[i] = AGENDA_CAL_COUNT - 1;
    }
    for (int i = 0; i < AGENDA_CAL_COUNT; i++) {
        if (order[i] < AGENDA_CAL_COUNT) {
            rank_of[order[i]] = (uint8_t) i;
        }
    }
}

static int cmp_i64(long long a, long long b)
{
    return (a > b) - (a < b);
}

int agenda_event_compare(const agenda_sort_key_t *a, const agenda_sort_key_t *b,
                         bool priority_first)
{
    int c;
    if (priority_first && (c = cmp_i64(a->rank, b->rank)) != 0) {
        return c;
    }
    if ((c = cmp_i64((long long) a->start, (long long) b->start)) != 0) {
        return c;
    }
    if (!priority_first && (c = cmp_i64(a->rank, b->rank)) != 0) {
        return c;
    }
    if ((c = cmp_i64(b->all_day, a->all_day)) != 0) {  // all-day first
        return c;
    }
    if ((c = cmp_i64((long long) a->end, (long long) b->end)) != 0) {
        return c;
    }
    if ((c = cmp_i64(a->cal, b->cal)) != 0) {
        return c;
    }
    return cmp_i64(a->seq, b->seq);
}

int agenda_select_by_rank(const uint8_t *ranks, int n, int capacity, bool *shown)
{
    int kept = 0;
    for (int i = 0; i < n; i++) {
        shown[i] = false;
    }
    if (capacity <= 0) {
        return 0;
    }
    uint8_t max_rank = 0;
    for (int i = 0; i < n; i++) {
        if (ranks[i] > max_rank) {
            max_rank = ranks[i];
        }
    }
    for (int rank = 0; rank <= max_rank && kept < capacity; rank++) {
        for (int i = 0; i < n && kept < capacity; i++) {
            if (ranks[i] == rank) {
                shown[i] = true;
                kept++;
            }
        }
    }
    return kept;
}

static bool is_leap(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

static int day_of_year(int y, int mon0, int mday)
{
    static const int before[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int doy = before[mon0] + mday;
    if (mon0 > 1 && is_leap(y)) {
        doy++;
    }
    return doy;
}

// 1 = Monday ... 7 = Sunday
static int iso_weekday(int y, int mon0, int mday)
{
    static const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int yy = mon0 < 2 ? y - 1 : y;
    int wd = (yy + yy / 4 - yy / 100 + yy / 400 + t[mon0] + mday) % 7;  // 0 = Sunday
    return wd == 0 ? 7 : wd;
}

// A year has 53 ISO weeks when it begins on a Thursday, or on a Wednesday in a leap year.
static int weeks_in_year(int y)
{
    int jan1 = iso_weekday(y, 0, 1);
    return (jan1 == 4 || (jan1 == 3 && is_leap(y))) ? 53 : 52;
}

int agenda_iso_week(int year_1900, int mon0, int mday)
{
    int y = year_1900 + 1900;
    if (mon0 < 0 || mon0 > 11) {
        mon0 = 0;
    }
    int week = (day_of_year(y, mon0, mday) - iso_weekday(y, mon0, mday) + 10) / 7;
    if (week < 1) {
        return weeks_in_year(y - 1);
    }
    if (week > weeks_in_year(y)) {
        return 1;
    }
    return week;
}

void agenda_week_text(char *out, size_t out_len, int week, bool german)
{
    snprintf(out, out_len, "%s%d", german ? "KW" : "CW", week);
}

void agenda_day_label(char *out, size_t out_len, const char *weekday, int mday, bool with_week,
                      int week, bool german)
{
    if (!with_week) {
        snprintf(out, out_len, "%s %d.", weekday, mday);
        return;
    }
    char text[8];
    agenda_week_text(text, sizeof(text), week, german);
    snprintf(out, out_len, "%s %d. %s", weekday, mday, text);
}
