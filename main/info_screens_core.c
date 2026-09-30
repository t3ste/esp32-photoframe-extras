#include "info_screens_core.h"

#include <stdio.h>
#include <string.h>

static bool is_leap(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

// Days since 1970-01-01 (Howard Hinnant's algorithm).
static long days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned) (y - era * 400);
    unsigned doy = (unsigned) ((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long) doe - 719468;
}

int info_days_between(const char *from_iso, const char *to_iso)
{
    int y1, m1, d1, y2, m2, d2;
    if (!from_iso || !to_iso || sscanf(from_iso, "%4d-%2d-%2d", &y1, &m1, &d1) != 3 ||
        sscanf(to_iso, "%4d-%2d-%2d", &y2, &m2, &d2) != 3 || m1 < 1 || m1 > 12 || m2 < 1 ||
        m2 > 12 || d1 < 1 || d1 > 31 || d2 < 1 || d2 > 31) {
        return -1;
    }
    long days = days_from_civil(y2, m2, d2) - days_from_civil(y1, m1, d1);
    return (days < 0 || days > 100000) ? -1 : (int) days;
}

bool info_format_stamp(bool german, int year, int month, int day, int hour, int minute, char *out,
                       size_t out_len)
{
    if (out_len == 0) {
        return false;
    }
    out[0] = '\0';
    if (year < 2024 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 ||
        hour > 23 || minute < 0 || minute > 59) {
        return false;
    }
    if (german) {
        snprintf(out, out_len, "Stand %02d.%02d. %02d:%02d", day, month, hour, minute);
    } else {
        snprintf(out, out_len, "Updated %d %.3s %02d:%02d", day, info_month_name(month, false),
                 hour, minute);
    }
    return true;
}

bool info_format_stamp_now(const info_now_t *now, char *out, size_t out_len)
{
    return info_format_stamp(now->german, now->year, now->month, now->day, now->hour, now->minute,
                             out, out_len);
}

void info_format_span(int days, bool german, char *out, size_t out_len)
{
    if (out_len == 0) {
        return;
    }
    out[0] = '\0';
    if (days >= 1) {
        snprintf(out, out_len, "%d %s", days > 9999 ? 9999 : days, german ? "T" : "d");
    }
}

int info_weekday(int year, int month, int day)
{
    long days = days_from_civil(year, month, day);
    int w = (int) ((days + 4) % 7);  // 1970-01-01 was a Thursday
    return w < 0 ? w + 7 : w;
}

static int day_of_year(int y, int m, int d)
{
    static const int before[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int doy = before[m - 1] + d;
    if (m > 2 && is_leap(y)) {
        doy++;
    }
    return doy;
}

// The weekday of 31 December decides whether a year has 52 or 53 ISO weeks.
static int weeks_in_year(int y)
{
    int p_this = (y + y / 4 - y / 100 + y / 400) % 7;
    int py = y - 1;
    int p_prev = (py + py / 4 - py / 100 + py / 400) % 7;
    return (p_this == 4 || p_prev == 3) ? 53 : 52;
}

int info_iso_week(int year, int month, int day, int *iso_year)
{
    int wd = info_weekday(year, month, day);
    if (wd == 0) {
        wd = 7;  // Monday = 1 .. Sunday = 7
    }
    int week = (day_of_year(year, month, day) - wd + 10) / 7;
    int y = year;
    if (week < 1) {
        y = year - 1;
        week = weeks_in_year(y);
    } else if (week > weeks_in_year(year)) {
        y = year + 1;
        week = 1;
    }
    if (iso_year) {
        *iso_year = y;
    }
    return week;
}

void info_now_from_tm(const struct tm *local, bool german, info_now_t *out)
{
    out->german = german;
    out->year = local->tm_year + 1900;
    out->month = local->tm_mon + 1;
    out->day = local->tm_mday;
    out->wday = local->tm_wday;
    out->hour = local->tm_hour;
    out->minute = local->tm_min;
    out->iso_week = info_iso_week(out->year, out->month, out->day, &out->iso_year);
}

void info_now_from_date(int year, int month, int day, bool german, info_now_t *out)
{
    out->german = german;
    out->year = year;
    out->month = month;
    out->day = day;
    out->wday = info_weekday(year, month, day);
    out->hour = 12;
    out->minute = 0;
    out->iso_week = info_iso_week(year, month, day, &out->iso_year);
}

const char *info_weekday_name(int wday, bool german)
{
    static const char *const en[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                     "Thursday", "Friday", "Saturday"};
    static const char *const de[] = {"Sonntag",    "Montag",  "Dienstag", "Mittwoch",
                                     "Donnerstag", "Freitag", "Samstag"};
    if (wday < 0 || wday > 6) {
        return "?";
    }
    return german ? de[wday] : en[wday];
}

const char *info_weekday_short(int wday, bool german)
{
    static const char *const en[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char *const de[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
    if (wday < 0 || wday > 6) {
        return "?";
    }
    return german ? de[wday] : en[wday];
}

const char *info_month_name(int month, bool german)
{
    static const char *const en[] = {"January",   "February", "March",    "April",
                                     "May",       "June",     "July",     "August",
                                     "September", "October",  "November", "December"};
    static const char *const de[] = {"Januar",    "Februar", "M\xC3\xA4rz", "April",
                                     "Mai",       "Juni",    "Juli",        "August",
                                     "September", "Oktober", "November",    "Dezember"};
    if (month < 1 || month > 12) {
        return "?";
    }
    return german ? de[month - 1] : en[month - 1];
}

int info_rotation_size(uint32_t mask, int bit_count)
{
    int n = 0;
    for (int i = 0; i < bit_count && i < 32; i++) {
        n += (mask >> i) & 1u;
    }
    return n;
}

int info_rotation_pick(uint32_t mask, uint32_t counter, int bit_count)
{
    int n = info_rotation_size(mask, bit_count);
    if (n == 0) {
        return -1;
    }
    uint32_t want = counter % (uint32_t) n;
    for (int i = 0; i < bit_count && i < 32; i++) {
        if ((mask >> i) & 1u) {
            if (want == 0) {
                return i;
            }
            want--;
        }
    }
    return -1;
}

int info_parse_list(const char *text, char *names, size_t name_len, int max)
{
    int count = 0;
    if (!text || name_len < 2 || max <= 0) {
        return 0;
    }
    const char *p = text;
    while (*p != '\0' && count < max) {
        const char *end = p;
        while (*end != '\0' && *end != ',' && *end != ';' && *end != '\n' && *end != '\r') {
            end++;
        }
        const char *start = p;
        const char *stop = end;
        while (start < stop && (*start == ' ' || *start == '\t')) {
            start++;
        }
        while (stop > start && (stop[-1] == ' ' || stop[-1] == '\t')) {
            stop--;
        }
        if (stop > start) {
            size_t n = (size_t) (stop - start);
            if (n > name_len - 1) {
                n = name_len - 1;
                // do not end in the middle of a UTF-8 character
                while (n > 0 && ((unsigned char) start[n] & 0xC0) == 0x80) {
                    n--;
                }
            }
            memcpy(names + (size_t) count * name_len, start, n);
            names[(size_t) count * name_len + n] = '\0';
            count++;
        }
        p = *end == '\0' ? end : end + 1;
    }
    return count;
}
