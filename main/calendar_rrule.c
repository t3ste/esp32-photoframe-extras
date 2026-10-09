#include "calendar_rrule.h"

#if FEATURE_AGENDA_RRULE

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "esp_log.h"
#include "icalerror.h"
#include "icallimits.h"
#include "icalmemory.h"
#include "icalrecur.h"
#include "icaltime.h"
#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

static const char *TAG = "calendar_rrule";

// The checked rule that libical gets (UNTIL left out): "FREQ=MONTHLY;INTERVAL=2;BYDAY=2MO,-1FR;..."
#define RRULE_TEXT_MAX 320
// Items of a list part (BYDAY, BYMONTHDAY, ...) and the largest numbers a part may carry.
#define RRULE_LIST_MAX 62
#define RRULE_INTERVAL_MAX 1000
#define RRULE_COUNT_MAX 100000
// Libical's own search for the next instance of a rule that matches rarely or never ("30
// February"): its default is 100000 rounds.
#define RRULE_SEARCH_LIMIT 4000

enum {
    K_FREQ = 1 << 0,
    K_INTERVAL = 1 << 1,
    K_COUNT = 1 << 2,
    K_UNTIL = 1 << 3,
    K_WKST = 1 << 4,
    K_BYDAY = 1 << 5,
    K_BYMONTHDAY = 1 << 6,
    K_BYMONTH = 1 << 7,
    K_BYYEARDAY = 1 << 8,
    K_BYWEEKNO = 1 << 9,
    K_BYSETPOS = 1 << 10,
    K_BYHOUR = 1 << 11,
    K_BYMINUTE = 1 << 12,
    K_BYSECOND = 1 << 13,
};
#define K_ANY_BY                                                                             \
    (K_BYDAY | K_BYMONTHDAY | K_BYMONTH | K_BYYEARDAY | K_BYWEEKNO | K_BYHOUR | K_BYMINUTE | \
     K_BYSECOND)

typedef struct {
    char text[RRULE_TEXT_MAX];
    size_t len;
} rule_text_t;

static bool text_add(rule_text_t *t, const char *key, const char *value, size_t value_len)
{
    size_t key_len = strlen(key);
    size_t need = (t->len ? 1 : 0) + key_len + 1 + value_len;
    if (t->len + need + 1 > sizeof(t->text)) {
        return false;
    }
    if (t->len) {
        t->text[t->len++] = ';';
    }
    memcpy(t->text + t->len, key, key_len);
    t->len += key_len;
    t->text[t->len++] = '=';
    for (size_t i = 0; i < value_len; i++) {
        t->text[t->len++] = (char) toupper((unsigned char) value[i]);
    }
    t->text[t->len] = '\0';
    return true;
}

static bool key_is(const char *key, size_t key_len, const char *name)
{
    return strlen(name) == key_len && strncasecmp(key, name, key_len) == 0;
}

// A decimal number with an optional sign, at most 6 digits, nothing else.
static bool read_int(const char *s, size_t n, long *out, bool *signed_out)
{
    size_t i = 0;
    bool neg = false;
    if (i < n && (s[i] == '+' || s[i] == '-')) {
        neg = (s[i] == '-');
        if (signed_out) {
            *signed_out = true;
        }
        i++;
    }
    if (i >= n || n - i > 6) {
        return false;
    }
    long v = 0;
    for (; i < n; i++) {
        if (!isdigit((unsigned char) s[i])) {
            return false;
        }
        v = v * 10 + (s[i] - '0');
    }
    *out = neg ? -v : v;
    return true;
}

// "a,b,c" of integers in a range: signed lists (|v| in 1..hi, no zero) or plain (lo..hi).
static bool check_int_list(const char *v, size_t n, long lo, long hi, bool allow_sign)
{
    size_t i = 0;
    int items = 0;
    while (i <= n) {
        size_t s = i;
        while (i < n && v[i] != ',') {
            i++;
        }
        long x;
        bool had_sign = false;
        if (!read_int(v + s, i - s, &x, &had_sign) || ++items > RRULE_LIST_MAX) {
            return false;
        }
        if (allow_sign) {
            long a = x < 0 ? -x : x;
            if (a < 1 || a > hi) {
                return false;
            }
        } else if (had_sign || x < lo || x > hi) {
            return false;
        }
        if (i == n) {
            break;
        }
        i++;  // ','
    }
    return true;
}

static bool is_day_code(const char *s)
{
    static const char *const codes[7] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
    for (int d = 0; d < 7; d++) {
        if (strncasecmp(s, codes[d], 2) == 0) {
            return true;
        }
    }
    return false;
}

static int day_index(const char *s)
{
    static const char *const codes[7] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
    for (int d = 0; d < 7; d++) {
        if (strncasecmp(s, codes[d], 2) == 0) {
            return d;
        }
    }
    return 1;
}

// "MO,2TU,-1FR": a day, optionally with an ordinal (1..53, signed). Sets *has_ordinal.
static bool check_byday(const char *v, size_t n, bool *has_ordinal)
{
    size_t i = 0;
    int items = 0;
    *has_ordinal = false;
    while (i <= n) {
        size_t s = i;
        while (i < n && v[i] != ',') {
            i++;
        }
        size_t len = i - s;
        if (++items > 14 || len < 2) {
            return false;
        }
        const char *item = v + s;
        if (!is_day_code(item + len - 2)) {
            return false;
        }
        if (len > 2) {
            long x;
            if (!read_int(item, len - 2, &x, NULL)) {
                return false;
            }
            long a = x < 0 ? -x : x;
            if (a < 1 || a > 53) {
                return false;
            }
            *has_ordinal = true;
        }
        if (i == n) {
            break;
        }
        i++;
    }
    return true;
}

// Checks a rule and writes the text libical gets. False: the rule is not taken.
static bool sanitize(const char *rule, size_t len, bool all_day, rule_text_t *out)
{
    unsigned seen = 0;
    int freq = -1;  // 0 DAILY, 1 WEEKLY, 2 MONTHLY, 3 YEARLY
    bool ordinal = false;
    long interval = 1;
    int wkst = 1;  // 0 SU .. 6 SA; the default week starts on Monday
    out->len = 0;
    out->text[0] = '\0';

    size_t i = 0;
    while (i < len) {
        size_t s = i;
        while (i < len && rule[i] != ';') {
            i++;
        }
        size_t part_len = i - s;
        if (i < len) {
            i++;  // ';'
        }
        if (part_len == 0) {
            continue;
        }
        const char *part = rule + s;
        const char *eq = memchr(part, '=', part_len);
        if (!eq || eq == part || eq == part + part_len - 1) {
            return false;
        }
        const char *key = part;
        size_t key_len = (size_t) (eq - part);
        const char *val = eq + 1;
        size_t val_len = part_len - key_len - 1;
        unsigned bit = 0;
        bool ok = true;

        if (key_is(key, key_len, "FREQ")) {
            bit = K_FREQ;
            if (val_len == 5 && strncasecmp(val, "DAILY", 5) == 0) {
                freq = 0;
            } else if (val_len == 6 && strncasecmp(val, "WEEKLY", 6) == 0) {
                freq = 1;
            } else if (val_len == 7 && strncasecmp(val, "MONTHLY", 7) == 0) {
                freq = 2;
            } else if (val_len == 6 && strncasecmp(val, "YEARLY", 6) == 0) {
                freq = 3;
            } else {
                return false;  // HOURLY and below, or nonsense
            }
            ok = !(seen & bit) && text_add(out, "FREQ", val, val_len);
        } else if (key_is(key, key_len, "INTERVAL")) {
            bit = K_INTERVAL;
            long x;
            ok = !(seen & bit) && read_int(val, val_len, &x, NULL) && x >= 1 &&
                 x <= RRULE_INTERVAL_MAX && text_add(out, "INTERVAL", val, val_len);
            interval = x;
        } else if (key_is(key, key_len, "COUNT")) {
            bit = K_COUNT;
            long x;
            ok = !(seen & bit) && read_int(val, val_len, &x, NULL) && x >= 1 &&
                 x <= RRULE_COUNT_MAX && text_add(out, "COUNT", val, val_len);
        } else if (key_is(key, key_len, "UNTIL")) {
            bit = K_UNTIL;  // read by the caller, not passed on
            ok = !(seen & bit) && val_len >= 8 && val_len <= 24;
        } else if (key_is(key, key_len, "WKST")) {
            bit = K_WKST;
            ok = !(seen & bit) && val_len == 2 && is_day_code(val) && text_add(out, "WKST", val, 2);
            wkst = day_index(val);
        } else if (key_is(key, key_len, "BYDAY")) {
            bit = K_BYDAY;
            ok = !(seen & bit) && check_byday(val, val_len, &ordinal) &&
                 text_add(out, "BYDAY", val, val_len);
        } else if (key_is(key, key_len, "BYMONTHDAY")) {
            bit = K_BYMONTHDAY;
            ok = !(seen & bit) && check_int_list(val, val_len, 1, 31, true) &&
                 text_add(out, "BYMONTHDAY", val, val_len);
        } else if (key_is(key, key_len, "BYMONTH")) {
            bit = K_BYMONTH;
            ok = !(seen & bit) && check_int_list(val, val_len, 1, 12, false) &&
                 text_add(out, "BYMONTH", val, val_len);
        } else if (key_is(key, key_len, "BYYEARDAY")) {
            bit = K_BYYEARDAY;
            ok = !(seen & bit) && check_int_list(val, val_len, 1, 366, true) &&
                 text_add(out, "BYYEARDAY", val, val_len);
        } else if (key_is(key, key_len, "BYWEEKNO")) {
            bit = K_BYWEEKNO;
            ok = !(seen & bit) && check_int_list(val, val_len, 1, 53, true) &&
                 text_add(out, "BYWEEKNO", val, val_len);
        } else if (key_is(key, key_len, "BYSETPOS")) {
            bit = K_BYSETPOS;
            ok = !(seen & bit) && check_int_list(val, val_len, 1, 366, true) &&
                 text_add(out, "BYSETPOS", val, val_len);
        } else if (key_is(key, key_len, "BYHOUR")) {
            bit = K_BYHOUR;
            ok = !(seen & bit) && check_int_list(val, val_len, 0, 23, false) &&
                 text_add(out, "BYHOUR", val, val_len);
        } else if (key_is(key, key_len, "BYMINUTE")) {
            bit = K_BYMINUTE;
            ok = !(seen & bit) && check_int_list(val, val_len, 0, 59, false) &&
                 text_add(out, "BYMINUTE", val, val_len);
        } else if (key_is(key, key_len, "BYSECOND")) {
            bit = K_BYSECOND;
            ok = !(seen & bit) && check_int_list(val, val_len, 0, 59, false) &&
                 text_add(out, "BYSECOND", val, val_len);
        } else {
            return false;  // RSCALE, SKIP, X-..., anything else
        }
        if (!ok) {
            return false;
        }
        seen |= bit;
    }

    // the combinations RFC 5545 §3.3.10 allows
    if (!(seen & K_FREQ)) {
        return false;
    }
    if ((seen & K_BYWEEKNO) && freq != 3) {
        return false;
    }
    if ((seen & K_BYYEARDAY) && freq != 3) {
        return false;
    }
    if ((seen & K_BYMONTHDAY) && freq == 1) {
        return false;
    }
    // Yearly rules the implementations read differently - the standard leaves the month (BYMONTHDAY
    // without BYMONTH: libical takes DTSTART's month, python-dateutil every month) and the days
    // (BYWEEKNO without BYDAY: one day or the whole week) to "derived from DTSTART". A frame that
    // shows the wrong days is worse than one that leaves the event out, so these are not taken.
    // (Found by running random rules through both: every MONTHLY, WEEKLY and DAILY rule and every
    // other YEARLY one agreed.)
    if (freq == 3 && (seen & K_BYMONTHDAY) && !(seen & K_BYMONTH)) {
        return false;
    }
    if (freq == 3 && (seen & K_BYWEEKNO)) {
        // the week number of a day at the turn of the year (week 52, 53, 1, -1) is read
        // differently, too
        return false;
    }
    if (freq == 1 && interval > 1 && (seen & K_BYDAY) && wkst >= 2) {
        // "every second week, on these days" counts weeks from WKST: libical drops DTSTART's own
        // instance and shifts the weeks for WKST=TU..SA (python-dateutil does not); MO and SU, what
        // calendar apps write, agree
        return false;
    }
    if (ordinal && ((freq != 2 && freq != 3) || (seen & K_BYWEEKNO))) {
        return false;
    }
    if ((seen & K_BYSETPOS) && !(seen & K_ANY_BY)) {
        return false;
    }
    if (all_day && (seen & (K_BYHOUR | K_BYMINUTE | K_BYSECOND))) {
        return false;
    }
    return true;
}

static struct icaltimetype to_ical(const rrule_wall_t *w, bool all_day)
{
    struct icaltimetype t = icaltime_null_time();
    t.year = w->year;
    t.month = w->month;
    t.day = w->day;
    t.hour = all_day ? 0 : w->hour;
    t.minute = all_day ? 0 : w->minute;
    t.second = all_day ? 0 : w->second;
    t.is_date = all_day ? 1 : 0;
    t.zone = NULL;  // wall clock: floating
    return t;
}

static int wall_cmp(const rrule_wall_t *a, const rrule_wall_t *b)
{
    if (a->year != b->year) {
        return a->year < b->year ? -1 : 1;
    }
    if (a->month != b->month) {
        return a->month < b->month ? -1 : 1;
    }
    if (a->day != b->day) {
        return a->day < b->day ? -1 : 1;
    }
    if (a->hour != b->hour) {
        return a->hour < b->hour ? -1 : 1;
    }
    if (a->minute != b->minute) {
        return a->minute < b->minute ? -1 : 1;
    }
    if (a->second != b->second) {
        return a->second < b->second ? -1 : 1;
    }
    return 0;
}

#ifdef ESP_PLATFORM
// libical keeps global state (error number, scratch buffers): one caller at a time.
static SemaphoreHandle_t s_lock;
static portMUX_TYPE s_lock_init = portMUX_INITIALIZER_UNLOCKED;

static void lock(void)
{
    if (!s_lock) {
        SemaphoreHandle_t m = xSemaphoreCreateMutex();
        portENTER_CRITICAL(&s_lock_init);
        if (!s_lock) {
            s_lock = m;
            m = NULL;
        }
        portEXIT_CRITICAL(&s_lock_init);
        if (m) {
            vSemaphoreDelete(m);
        }
    }
    if (s_lock) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }
}

static void unlock(void)
{
    if (s_lock) {
        xSemaphoreGive(s_lock);
    }
}
#else
static void lock(void) {}
static void unlock(void) {}
#endif

int calendar_rrule_expand(const char *rule, size_t len, const rrule_wall_t *dtstart, bool all_day,
                          const rrule_wall_t *from, const rrule_wall_t *to, rrule_wall_t *out,
                          int max_out)
{
    if (!rule || !dtstart || !from || !to || !out || max_out <= 0) {
        return -1;
    }
    rule_text_t text;
    if (!sanitize(rule, len, all_day, &text)) {
        return -1;
    }
    if (wall_cmp(from, to) >= 0) {
        return 0;
    }

    int result = -1;
    lock();
    icalerror_clear_errno();
    icallimit_set(ICAL_LIMIT_RECURRENCE_SEARCH, RRULE_SEARCH_LIMIT);

    struct icalrecurrencetype *recur = icalrecurrencetype_new_from_string(text.text);
    icalrecur_iterator *it = NULL;
    if (recur && icalerrno == ICAL_NO_ERROR) {
        it = icalrecur_iterator_new(recur, to_ical(dtstart, all_day));
    }
    if (it && icalerrno == ICAL_NO_ERROR) {
        bool counted = recur->count > 0;  // COUNT: the first instance must be the first of DTSTART
        bool jumped = false;
        if (!counted && wall_cmp(from, dtstart) > 0) {
            jumped = icalrecur_iterator_set_start(it, to_ical(from, all_day));
            if (!jumped) {
                icalerror_clear_errno();
            }
        }
        // a rule that could not be jumped to the window is walked from DTSTART like one with COUNT
        bool walk = counted || !jumped;
        int n = 0;
        int steps = 0;
        bool reached = !walk || wall_cmp(from, dtstart) <= 0;
        result = 0;
        for (;;) {
            struct icaltimetype t = icalrecur_iterator_next(it);
            if (icaltime_is_null_time(t) || icalerrno != ICAL_NO_ERROR) {
                break;
            }
            rrule_wall_t w;
            w.year = (int16_t) t.year;
            w.month = (int8_t) t.month;
            w.day = (int8_t) t.day;
            w.hour = (int8_t) (all_day ? 0 : t.hour);
            w.minute = (int8_t) (all_day ? 0 : t.minute);
            w.second = (int8_t) (all_day ? 0 : t.second);
            if (wall_cmp(&w, to) >= 0) {
                reached = true;
                break;
            }
            if (wall_cmp(&w, from) >= 0) {
                reached = true;
                if (n >= max_out) {
                    break;
                }
                out[n++] = w;
            }
            if (++steps > RRULE_MAX_STEPS + max_out) {
                break;
            }
        }
        if (icalerrno != ICAL_NO_ERROR) {
            result = -1;
        } else if (!reached && steps > RRULE_MAX_STEPS) {
            ESP_LOGW(TAG, "A rule with COUNT starts too long before the window - event left out");
            result = -1;
        } else {
            result = n;
        }
    }

    if (it) {
        icalrecur_iterator_free(it);
    }
    if (recur) {
        icalrecurrencetype_unref(recur);
    }
    icalmemory_free_ring();
    icalerror_clear_errno();
    unlock();
    return result;
}

#endif  // FEATURE_AGENDA_RRULE
