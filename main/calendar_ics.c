#include "calendar_ics.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "esp_log.h"
#include "http_fetch.h"
#include "image_processor.h"

static const char *TAG = "calendar_ics";

#define ICS_HTTP_TIMEOUT_MS 10000
// A real Google Calendar export (many years of history, recurring series,
// categories/attendees on every VEVENT) was found live to exceed the
// original 96KB cap and get silently truncated mid-parse - confirmed live
// at ~800KB and still growing over time (the user's own calendar). Google's
// "secret address" ICS export has no query parameter to limit it to a date
// range, so there's no way to ask for less data up front. This is
// necessarily a "raise the ceiling" mitigation, not a permanent fix - a
// truly unbounded-size-safe fix needs the parser to work incrementally on
// the HTTP response stream (RFC 5545 line-unfolding included) rather than
// buffering the whole body first, which is a real rewrite deliberately not
// attempted under time pressure here. The response buffer lives in PSRAM
// (http_fetch_get()), which this project has megabytes of headroom in, so
// there's no reason to be stingy with the cap in the meantime - only the
// fixed-size ics_event_list_t output (ICS_MAX_EVENTS entries) is actually
// bounded by anything else.
#define ICS_MAX_RESPONSE_BYTES (2 * 1024 * 1024)
#define ICS_LINE_MAX_LEN 600  // a folded SUMMARY can legitimately run long
// Bounds of the recurrence handling (see expand_series()): the EXDATE/RDATE values of one series
// that fall into the window, the RECURRENCE-ID exceptions of a whole feed that can matter for the
// window, and the instances of one series that are looked at. Instances before the window can reach
// into it, so exceptions are kept from this far back.
#define ICS_MAX_EXDATES 128
#define ICS_MAX_RDATES 64
#define ICS_MAX_OVERRIDES 512
#define ICS_MAX_CANDIDATES (ICS_MAX_EVENTS + ICS_MAX_RDATES)
#define ICS_EXCEPTION_LOOKBACK_S (40 * 86400)

// Un-folds ICS line-folding in place: a line that starts with a single
// space or tab is a continuation of the previous line (RFC 5545 §3.1) - so
// that continuation's leading whitespace plus the line break before it are
// removed, joining it onto the previous logical line. Unfolding only ever
// removes bytes, so this can safely compact the buffer in place. Every
// remaining (non-folded) line break becomes a single '\n', regardless of
// whether the input used bare LF or CRLF. Returns the new length
// (NUL-terminated at that point too).
static size_t ics_unfold(char *body, size_t len)
{
    size_t src = 0, dst = 0;
    while (src < len) {
        while (src < len && body[src] != '\n' && body[src] != '\r') {
            body[dst++] = body[src++];
        }
        if (src >= len) {
            break;
        }
        if (body[src] == '\r') {
            src++;
        }
        if (src < len && body[src] == '\n') {
            src++;
        }
        if (src < len && (body[src] == ' ' || body[src] == '\t')) {
            src++;  // fold continuation - swallow the break, keep appending
            continue;
        }
        body[dst++] = '\n';
    }
    body[dst] = '\0';
    return dst;
}

// Splits an unfolded property line "NAME[;PARAM=VAL;...]:VALUE" into the
// property name (e.g. "DTSTART") and the raw value after the first colon.
// Returns false if no colon is present (not a property line, or malformed).
static bool split_ics_property(const char *line, size_t line_len, const char **name,
                               size_t *name_len, const char **value, size_t *value_len)
{
    const char *colon = memchr(line, ':', line_len);
    if (!colon) {
        return false;
    }
    const char *semi = memchr(line, ';', (size_t) (colon - line));
    *name = line;
    *name_len = semi ? (size_t) (semi - line) : (size_t) (colon - line);
    *value = colon + 1;
    *value_len = line_len - (size_t) (colon - line) - 1;
    return true;
}

static bool name_is(const char *name, size_t name_len, const char *literal)
{
    size_t lit_len = strlen(literal);
    return name_len == lit_len && strncmp(name, literal, lit_len) == 0;
}

// Parses a "YYYYMMDD" or "YYYYMMDDTHHMMSS[Z]" ICS date-time value into a
// broken-down time. Doesn't validate the date/time fields are in-range
// (e.g. month 13) - the eventual time_t conversion just produces a
// nonsensical-but-non-crashing result for genuinely malformed input,
// matching this project's fail-soft parsing philosophy.
static bool parse_ics_datetime(const char *value, size_t value_len, struct tm *out_tm,
                               bool *out_all_day, bool *out_utc)
{
    if (value_len < 8) {
        return false;
    }
    for (int i = 0; i < 8; i++) {
        if (!isdigit((unsigned char) value[i])) {
            return false;
        }
    }
    memset(out_tm, 0, sizeof(*out_tm));
    // -1 (not the 0 the memset above leaves) so ics_datetime_to_time()'s
    // mktime() call determines DST itself for a local (non-UTC) event's
    // date instead of always assuming standard time - see the identical fix
    // in rtc_driver_pcf85063/pcf8563 for the full mechanics.
    out_tm->tm_isdst = -1;
    out_tm->tm_year = (value[0] - '0') * 1000 + (value[1] - '0') * 100 + (value[2] - '0') * 10 +
                      (value[3] - '0') - 1900;
    out_tm->tm_mon = (value[4] - '0') * 10 + (value[5] - '0') - 1;
    out_tm->tm_mday = (value[6] - '0') * 10 + (value[7] - '0');

    if (value_len == 8) {
        *out_all_day = true;
        *out_utc = false;
        return true;
    }
    *out_all_day = false;
    if (value_len < 15 || value[8] != 'T') {
        return false;
    }
    for (int i = 9; i < 15; i++) {
        if (!isdigit((unsigned char) value[i])) {
            return false;
        }
    }
    out_tm->tm_hour = (value[9] - '0') * 10 + (value[10] - '0');
    out_tm->tm_min = (value[11] - '0') * 10 + (value[12] - '0');
    out_tm->tm_sec = (value[13] - '0') * 10 + (value[14] - '0');
    *out_utc = (value_len >= 16 && value[15] == 'Z');
    return true;
}

// Portable UTC "timegm()" equivalent (Howard Hinnant's days-from-civil
// algorithm - pure integer arithmetic, no libc timezone dependency), used
// for a "Z"-suffixed (explicitly UTC) DTSTART/DTEND so its interpretation
// never depends on the host/device's own local TZ setting - unlike a bare
// or TZID-qualified timestamp, which deliberately does go through
// mktime() (see calendar_ics.h's doc comment on that approximation).
static time_t ics_timegm(const struct tm *tm)
{
    long y = tm->tm_year + 1900;
    int m = tm->tm_mon + 1;
    int d = tm->tm_mday;
    y -= (m <= 2) ? 1 : 0;
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned) (y - era * 400);
    unsigned doy = (153 * (unsigned) (m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned) d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long days = era * 146097 + (long) doe - 719468;  // days since 1970-01-01
    return (time_t) days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
}

static time_t ics_datetime_to_time(const struct tm *tm, bool utc)
{
    if (utc) {
        return ics_timegm(tm);
    }
    struct tm local = *tm;
    return mktime(&local);
}

// Decodes the small set of RFC 5545 §3.3.11 text escapes. Anything else is
// copied through as literal text - image_processor_sanitize_ascii()
// downstream will drop what it can't render anyway.
static void decode_ics_text(const char *in, size_t in_len, char *out, size_t out_len)
{
    size_t o = 0, i = 0;
    while (i < in_len && o + 1 < out_len) {
        if (in[i] == '\\' && i + 1 < in_len) {
            char next = in[i + 1];
            if (next == 'n' || next == 'N') {
                out[o++] = ' ';
                i += 2;
                continue;
            }
            if (next == ',' || next == ';' || next == '\\') {
                out[o++] = next;
                i += 2;
                continue;
            }
        }
        out[o++] = in[i++];
    }
    out[o] = '\0';
}

// RRULE-lite: FREQ=DAILY/WEEKLY only, optional INTERVAL (default 1), COUNT,
// UNTIL, WKST (ignored, see parse_rrule()), and a single-value BYDAY.
// Anything else in the rule (a multi-value BYDAY like "MO,WE,FR",
// BYMONTHDAY, BYSETPOS, an unrecognized FREQ, ...) makes `supported` false -
// the caller then skips the whole event rather than risk showing a wrong
// occurrence. (EXDATE, RDATE, RECURRENCE-ID and STATUS are properties of the
// VEVENT, not parts of the rule: see finalize_vevent().)
typedef struct {
    bool supported;
    bool weekly;   // false = daily
    int interval;  // >= 1
    bool has_count;
    int count;
    // 0=SU..6=SA, or -1 if the rule had no BYDAY at all. A single BYDAY
    // value is common (real calendar apps almost always emit one for a
    // "weekly" recurrence, even a plain single-weekday one) and safe to
    // accept ONLY once finalize_vevent() confirms it matches DTSTART's own
    // weekday - see its comment for why a mismatch still fails closed.
    int byday;
    // Inclusive end bound (RFC 5545: an occurrence starting after this
    // instant is excluded) - unlike the other unsupported components,
    // UNTIL only ever narrows the result (same idea as the already-supported
    // COUNT, just date-bounded instead of count-bounded), so it's always
    // safe to honor rather than reject the whole rule.
    bool has_until;
    time_t until;
} ics_rrule_t;

// Parses one RRULE value ("FREQ=DAILY;INTERVAL=2;COUNT=10"-style,
// ';'-separated "KEY=VALUE" components, order not significant per RFC
// 5545). Bails out (supported = false) the moment any component isn't one
// of the handful this project chose to support - see ics_rrule_t's own
// comment for the rationale.
static bool parse_rrule(const char *value, size_t value_len, ics_rrule_t *out)
{
    memset(out, 0, sizeof(*out));
    out->interval = 1;
    out->byday = -1;
    bool have_freq = false;

    size_t i = 0;
    while (i < value_len) {
        size_t part_start = i;
        while (i < value_len && value[i] != ';') {
            i++;
        }
        size_t part_len = i - part_start;
        if (i < value_len) {
            i++;  // skip ';'
        }
        if (part_len == 0) {
            continue;
        }

        const char *part = value + part_start;
        const char *eq = memchr(part, '=', part_len);
        if (!eq) {
            return false;  // malformed component - fail closed
        }
        size_t key_len = (size_t) (eq - part);
        const char *val_ptr = eq + 1;
        size_t val_len = part_len - key_len - 1;

        if (key_len == 4 && strncmp(part, "FREQ", 4) == 0) {
            have_freq = true;
            if (val_len == 5 && strncmp(val_ptr, "DAILY", 5) == 0) {
                out->weekly = false;
            } else if (val_len == 6 && strncmp(val_ptr, "WEEKLY", 6) == 0) {
                out->weekly = true;
            } else {
                return false;  // MONTHLY/YEARLY/HOURLY/... not supported
            }
        } else if (key_len == 8 && strncmp(part, "INTERVAL", 8) == 0) {
            char buf[16];
            size_t n = val_len < sizeof(buf) - 1 ? val_len : sizeof(buf) - 1;
            memcpy(buf, val_ptr, n);
            buf[n] = '\0';
            int iv = atoi(buf);
            out->interval = (iv < 1) ? 1 : iv;
        } else if (key_len == 5 && strncmp(part, "COUNT", 5) == 0) {
            char buf[16];
            size_t n = val_len < sizeof(buf) - 1 ? val_len : sizeof(buf) - 1;
            memcpy(buf, val_ptr, n);
            buf[n] = '\0';
            out->count = atoi(buf);
            out->has_count = true;
        } else if (key_len == 5 && strncmp(part, "UNTIL", 5) == 0) {
            struct tm until_tm;
            bool until_all_day, until_utc;
            if (!parse_ics_datetime(val_ptr, val_len, &until_tm, &until_all_day, &until_utc)) {
                return false;  // malformed UNTIL value - fail closed
            }
            out->until = ics_datetime_to_time(&until_tm, until_utc);
            out->has_until = true;
        } else if (key_len == 5 && strncmp(part, "BYDAY", 5) == 0) {
            // A single day value is common - real calendar apps almost
            // always emit BYDAY for a "weekly" recurrence, even a plain
            // single-weekday one - and safe to accept here; finalize_vevent()
            // still cross-checks it against DTSTART's own weekday before
            // actually trusting it (this function doesn't have DTSTART yet,
            // since RRULE can appear before it in the VEVENT block).
            // Multiple comma-separated values ("MO,WE,FR") describe a
            // genuinely different pattern this project's simple
            // weekly-with-interval model can't represent - still fails
            // closed, same as before.
            if (memchr(val_ptr, ',', val_len) != NULL) {
                return false;
            }
            static const char *const day_codes[7] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
            int day = -1;
            for (int d = 0; d < 7; d++) {
                if (val_len == 2 && strncmp(val_ptr, day_codes[d], 2) == 0) {
                    day = d;
                    break;
                }
            }
            if (day < 0) {
                return false;  // unrecognized value (e.g. "1MO" ordinal form) - fail closed
            }
            out->byday = day;
        } else if (key_len == 4 && strncmp(part, "WKST", 4) == 0) {
            // Week-start-day only affects which occurrences are valid for
            // patterns this project doesn't support anyway (BYSETPOS,
            // BYWEEKNO, or multiple BYDAY values combined with INTERVAL>1) -
            // with at most one BYDAY value, the only case ever accepted
            // above, WKST changes nothing about the actual result, so it's
            // safe to just ignore instead of rejecting the whole rule.
            // Confirmed live: a real calendar export's simple weekly
            // Wednesday event ("FREQ=WEEKLY;WKST=MO;BYDAY=WE") was being
            // dropped by this alone, even after BYDAY itself was accepted.
        } else {
            // EXDATE, BYMONTHDAY, BYSETPOS, ... - none of these are safe to
            // just ignore (they'd change which occurrences are actually
            // valid), so the whole rule is unsupported rather than silently
            // wrong.
            return false;
        }
    }

    out->supported = have_freq;
    return out->supported;
}

// A date or date-time of an EXDATE/RDATE value.
typedef struct {
    time_t t;
    bool all_day;  // a bare date (VALUE=DATE), not a date-time
} ics_stamp_t;

// What a VEVENT with a RECURRENCE-ID says about the series that has the same UID: the one instance
// it names is replaced (by this very event, which carries its own DTSTART/SUMMARY/STATUS and is
// shown as an ordinary event) or, with RANGE=THISANDFUTURE, that instance and every later one.
typedef struct {
    uint64_t uid;  // hash of the UID
    time_t rid;
    bool all_day;
    bool and_future;
} ics_override_t;

// Scratch space of one calendar_ics_parse() call (heap, so it does not weigh on the stack of
// whatever task calls us): the recurrence exceptions of the whole feed, and the dates of the
// series being expanded.
typedef struct {
    ics_override_t *overrides;
    int n_overrides;
    int cap_overrides;
    bool overrides_truncated;
    ics_stamp_t exdates[ICS_MAX_EXDATES];
    int n_exdates;
    ics_stamp_t rdates[ICS_MAX_RDATES];
    int n_rdates;
    time_t cand[ICS_MAX_CANDIDATES];
    int n_cand;
    int dropped;  // events that did not fit into ICS_MAX_EVENTS
} ics_ctx_t;

typedef struct {
    bool have_dtstart;
    struct tm dtstart_tm;
    bool dtstart_utc;
    bool all_day;

    bool have_dtend;
    struct tm dtend_tm;
    bool dtend_utc;
    bool have_duration;  // DURATION instead of DTEND
    int64_t duration_s;

    char raw_summary[ICS_SUMMARY_MAX_LEN];
    bool have_summary;

    bool has_rrule;
    ics_rrule_t rrule;

    bool has_exdate;
    bool has_rdate;
    bool cancelled;  // STATUS:CANCELLED
    bool has_uid;
    uint64_t uid;
    bool has_recurrence_id;      // this VEVENT is one instance of a series (an exception)
    bool recurrence_and_future;  // RECURRENCE-ID;RANGE=THISANDFUTURE

    // The lines of this VEVENT in the (unfolded) feed: EXDATE and RDATE are read from there when
    // the series is expanded, so that any number of them (and any line length) works without a
    // table.
    const char *block_start;
    const char *block_end;
} ics_vevent_state_t;

static void reset_vevent_state(ics_vevent_state_t *st)
{
    memset(st, 0, sizeof(*st));
}

static bool time_overlaps_window(time_t start, time_t end, time_t window_start, time_t window_end)
{
    return start < window_end && end > window_start;
}

// Whole days since 1970-01-01 of a civil date (Howard Hinnant's algorithm, as in ics_timegm()).
static int64_t days_from_civil(int64_t y, int m, int d)
{
    y -= (m <= 2) ? 1 : 0;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void civil_from_days(int64_t z, int *y_out, int *m_out, int *d_out)
{
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t y = yoe + era * 400;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    int d = (int) (doy - (153 * mp + 2) / 5 + 1);
    int m = (int) (mp < 10 ? mp + 3 : mp - 9);
    *y_out = (int) (y + (m <= 2 ? 1 : 0));
    *m_out = m;
    *d_out = d;
}

// The instant of a wall-clock time on a civil day, in the device's local time zone. Going through
// the calendar day (and not adding 86400 s to an earlier instant) is what keeps "every day at
// 09:00" at 09:00 when the clocks change.
static time_t local_time_on_day(int64_t day, int hour, int min, int sec)
{
    int y, m, d;
    civil_from_days(day, &y, &m, &d);
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = y - 1900;
    tm.tm_mon = m - 1;
    tm.tm_mday = d;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    tm.tm_isdst = -1;
    return mktime(&tm);
}

static int64_t local_day_of(time_t t)
{
    struct tm tm;
    localtime_r(&t, &tm);
    return days_from_civil(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

static bool same_local_day(time_t a, time_t b)
{
    return local_day_of(a) == local_day_of(b);
}

static uint64_t hash_text(const char *s, size_t n)
{
    uint64_t h = 1469598103934665603ULL;  // FNV-1a
    for (size_t i = 0; i < n; i++) {
        h ^= (unsigned char) s[i];
        h *= 1099511628211ULL;
    }
    return h;
}

// Stores `summary` etc. as an event; when the list is full, the event that starts last is the one
// that goes (the agenda shows what comes first) - not whichever happened to come last in the
// file. Returns false if the new event was the one left out.
static bool add_event(ics_event_list_t *out, time_t start, time_t end, bool all_day,
                      const char *summary)
{
    int idx;
    if (out->count < ICS_MAX_EVENTS) {
        idx = out->count++;
    } else {
        idx = 0;
        for (int i = 1; i < out->count; i++) {
            if (out->events[i].start > out->events[idx].start) {
                idx = i;
            }
        }
        if (start >= out->events[idx].start) {
            return false;
        }
    }
    ics_event_t *e = &out->events[idx];
    e->start = start;
    e->end = end;
    e->all_day = all_day;
    strncpy(e->summary, summary && summary[0] != '\0' ? summary : "(untitled)",
            ICS_SUMMARY_MAX_LEN - 1);
    e->summary[ICS_SUMMARY_MAX_LEN - 1] = '\0';
    return true;
}

// Reads a DURATION value as RFC 5545 §3.3.6 writes it - [+]P[nW][nD][T[nH][nM][nS]] - into seconds
// (a week is 7 days, a day 86400 s). False for anything else: a negative or malformed duration is
// not used, the event keeps its default length.
static bool parse_ics_duration(const char *v, size_t n, int64_t *seconds)
{
    size_t i = 0;
    if (i < n && v[i] == '+') {
        i++;
    }
    if (i >= n || v[i] != 'P') {
        return false;
    }
    i++;
    int64_t total = 0;
    bool in_time = false, any = false;
    while (i < n) {
        if (v[i] == 'T') {
            in_time = true;
            i++;
            continue;
        }
        int64_t num = 0;
        size_t digits = 0;
        while (i < n && isdigit((unsigned char) v[i]) && digits < 9) {
            num = num * 10 + (v[i] - '0');
            i++;
            digits++;
        }
        if (digits == 0 || i >= n) {
            return false;
        }
        switch (v[i++]) {
        case 'W':
            total += num * 7 * 86400;
            break;
        case 'D':
            total += num * 86400;
            break;
        case 'H':
        case 'M':
        case 'S':
            if (!in_time) {
                return false;  // (months and the like are not a duration of the standard)
            }
            total += num * (v[i - 1] == 'H' ? 3600 : v[i - 1] == 'M' ? 60 : 1);
            break;
        default:
            return false;
        }
        any = true;
    }
    if (!any) {
        return false;
    }
    *seconds = total;
    return true;
}

// True if the property line carries the parameter `name` with the value `value` (both compared
// without regard to case), e.g. "RANGE" and "THISANDFUTURE" in
// "RECURRENCE-ID;RANGE=THISANDFUTURE:...".
static bool line_param_is(const char *line, size_t line_len, const char *name, const char *value)
{
    const char *colon = memchr(line, ':', line_len);
    size_t params_len = colon ? (size_t) (colon - line) : line_len;
    size_t name_len = strlen(name);
    size_t value_len = strlen(value);
    for (size_t i = 0; i < params_len; i++) {
        if (line[i] != ';') {
            continue;
        }
        size_t at = i + 1;
        if (at + name_len + 1 + value_len > params_len) {
            continue;
        }
        if (strncasecmp(line + at, name, name_len) != 0 || line[at + name_len] != '=') {
            continue;
        }
        if (strncasecmp(line + at + name_len + 1, value, value_len) != 0) {
            continue;
        }
        size_t after = at + name_len + 1 + value_len;
        if (after == params_len || line[after] == ';') {
            return true;
        }
    }
    return false;
}

// Reads the "a,b,c" of an EXDATE/RDATE value (dates or date-times) and keeps the ones in [lo, hi).
// Returns false if an item cannot be read - a PERIOD ("start/end"), a malformed date: what the
// property means is then unknown and the caller drops the whole event, as it does for an RRULE it
// does not understand.
static bool parse_stamp_list(const char *value, size_t value_len, ics_stamp_t *arr, int *n, int cap,
                             time_t lo, time_t hi, bool *truncated)
{
    size_t i = 0;
    while (i < value_len) {
        size_t s = i;
        while (i < value_len && value[i] != ',') {
            i++;
        }
        size_t item_len = i - s;
        if (i < value_len) {
            i++;  // skip ','
        }
        while (item_len > 0 && isspace((unsigned char) value[s + item_len - 1])) {
            item_len--;
        }
        if (item_len == 0) {
            continue;
        }
        if (memchr(value + s, '/', item_len) != NULL) {
            return false;
        }
        struct tm tm;
        bool all_day, utc;
        if (!parse_ics_datetime(value + s, item_len, &tm, &all_day, &utc)) {
            return false;
        }
        time_t t = ics_datetime_to_time(&tm, utc);
        if (t < lo || t >= hi) {
            continue;
        }
        if (*n >= cap) {
            *truncated = true;
            continue;
        }
        arr[*n].t = t;
        arr[*n].all_day = all_day;
        (*n)++;
    }
    return true;
}

// Collects the dates of every `prop` line (EXDATE or RDATE) of the VEVENT that fall into [lo, hi).
static bool collect_stamps(const ics_vevent_state_t *st, const char *prop, ics_stamp_t *arr, int *n,
                           int cap, time_t lo, time_t hi, bool *truncated)
{
    *n = 0;
    if (!st->block_start || !st->block_end) {
        return true;
    }
    const char *p = st->block_start;
    while (p < st->block_end) {
        const char *nl = memchr(p, '\n', (size_t) (st->block_end - p));
        const char *line_end = nl ? nl : st->block_end;
        const char *name, *value;
        size_t name_len, value_len;
        if (line_end > p &&
            split_ics_property(p, (size_t) (line_end - p), &name, &name_len, &value, &value_len) &&
            name_is(name, name_len, prop)) {
            if (!parse_stamp_list(value, value_len, arr, n, cap, lo, hi, truncated)) {
                return false;
            }
        }
        p = nl ? nl + 1 : st->block_end;
    }
    return true;
}

static void add_override(ics_ctx_t *ctx, uint64_t uid, time_t rid, bool all_day, bool and_future)
{
    if (ctx->n_overrides >= ICS_MAX_OVERRIDES) {
        ctx->overrides_truncated = true;
        return;
    }
    if (ctx->n_overrides >= ctx->cap_overrides) {
        int cap = ctx->cap_overrides ? ctx->cap_overrides * 2 : 32;
        if (cap > ICS_MAX_OVERRIDES) {
            cap = ICS_MAX_OVERRIDES;
        }
        ics_override_t *grown = realloc(ctx->overrides, (size_t) cap * sizeof(*grown));
        if (!grown) {
            ctx->overrides_truncated = true;
            return;
        }
        ctx->overrides = grown;
        ctx->cap_overrides = cap;
    }
    ics_override_t *o = &ctx->overrides[ctx->n_overrides++];
    o->uid = uid;
    o->rid = rid;
    o->all_day = all_day;
    o->and_future = and_future;
}

// First pass over a feed that has RECURRENCE-IDs: the exceptions can come before or after the
// series they belong to, so they are all read before any series is expanded. Only those that can
// matter for the window are kept - an exception is for one instance, and an instance that is not in
// the window is not shown anyway (RANGE=THISANDFUTURE hides everything after it, so those count as
// soon as they begin before the window ends).
static void collect_overrides(ics_ctx_t *ctx, const char *body, size_t body_len,
                              time_t window_start, time_t window_end)
{
    const char *pos = body;
    const char *body_end = body + body_len;
    bool in_event = false, in_alarm = false;
    bool has_uid = false, has_rid = false, rid_all_day = false, and_future = false;
    uint64_t uid = 0;
    time_t rid = 0;
    while (pos < body_end) {
        const char *nl = memchr(pos, '\n', (size_t) (body_end - pos));
        const char *line_end = nl ? nl : body_end;
        size_t line_len = (size_t) (line_end - pos);
        const char *line = pos;
        pos = nl ? nl + 1 : body_end;
        if (line_len >= 12 && strncmp(line, "BEGIN:VEVENT", 12) == 0) {
            in_event = true;
            in_alarm = false;
            has_uid = has_rid = rid_all_day = and_future = false;
            continue;
        }
        if (line_len >= 10 && strncmp(line, "END:VEVENT", 10) == 0) {
            if (in_event && has_uid && has_rid) {
                bool relevant = and_future ? rid < window_end
                                           : (rid >= window_start - ICS_EXCEPTION_LOOKBACK_S &&
                                              rid < window_end);
                if (relevant) {
                    add_override(ctx, uid, rid, rid_all_day, and_future);
                }
            }
            in_event = false;
            continue;
        }
        if (line_len >= 12 && strncmp(line, "BEGIN:VALARM", 12) == 0) {
            in_alarm = true;
            continue;
        }
        if (line_len >= 10 && strncmp(line, "END:VALARM", 10) == 0) {
            in_alarm = false;
            continue;
        }
        if (!in_event || in_alarm) {
            continue;
        }
        const char *name, *value;
        size_t name_len, value_len;
        if (!split_ics_property(line, line_len, &name, &name_len, &value, &value_len)) {
            continue;
        }
        if (name_is(name, name_len, "UID")) {
            uid = hash_text(value, value_len);
            has_uid = true;
        } else if (name_is(name, name_len, "RECURRENCE-ID")) {
            struct tm tm;
            bool utc;
            if (parse_ics_datetime(value, value_len, &tm, &rid_all_day, &utc)) {
                rid = ics_datetime_to_time(&tm, utc);
                has_rid = true;
                and_future = line_param_is(line, line_len, "RANGE", "THISANDFUTURE");
            }
        }
    }
    if (ctx->overrides_truncated) {
        ESP_LOGW(TAG, "More than %d recurrence exceptions in the window - some may not be applied",
                 ICS_MAX_OVERRIDES);
    }
}

// True if the instance of the series starting at `occ_start` is not to be shown: it is listed in an
// EXDATE, or another VEVENT of the same UID replaces it (RECURRENCE-ID).
static bool instance_hidden(const ics_ctx_t *ctx, const ics_vevent_state_t *st, time_t occ_start,
                            bool series_all_day)
{
    for (int i = 0; i < ctx->n_exdates; i++) {
        const ics_stamp_t *e = &ctx->exdates[i];
        if (e->t == occ_start ||
            (e->all_day && !series_all_day && same_local_day(e->t, occ_start))) {
            return true;
        }
    }
    if (st->has_uid) {
        for (int i = 0; i < ctx->n_overrides; i++) {
            const ics_override_t *o = &ctx->overrides[i];
            if (o->uid != st->uid) {
                continue;
            }
            if (o->and_future) {
                if (occ_start >= o->rid) {
                    return true;
                }
            } else if (o->rid == occ_start ||
                       (o->all_day && !series_all_day && same_local_day(o->rid, occ_start))) {
                return true;
            }
        }
    }
    return false;
}

// The occurrences of one series, as far as the expansion needs to know them.
typedef struct {
    time_t start;      // DTSTART
    time_t duration;   // seconds, timed events
    int64_t dur_days;  // whole days, all-day events
    bool all_day;
    bool utc;  // DTSTART in UTC: the instants are fixed, whatever the clocks do locally
    int64_t period_days;
    int64_t base_day;    // civil day of DTSTART (local series)
    int hour, min, sec;  // wall-clock time of DTSTART (local series)
} ics_series_t;

static time_t series_start(const ics_series_t *s, int64_t k)
{
    if (k == 0) {
        return s->start;
    }
    if (s->utc) {
        return s->start + (time_t) (k * s->period_days * 86400);
    }
    return local_time_on_day(s->base_day + k * s->period_days, s->hour, s->min, s->sec);
}

static time_t series_end(const ics_series_t *s, time_t occ_start)
{
    if (s->all_day) {
        return local_time_on_day(local_day_of(occ_start) + s->dur_days, 0, 0, 0);
    }
    return occ_start + s->duration;
}

static int compare_times(const void *a, const void *b)
{
    time_t ta = *(const time_t *) a;
    time_t tb = *(const time_t *) b;
    return (ta > tb) - (ta < tb);
}

static void add_candidate(ics_ctx_t *ctx, time_t start)
{
    if (ctx->n_cand < ICS_MAX_CANDIDATES) {
        ctx->cand[ctx->n_cand++] = start;
    }
}

// Expands a series (an RRULE this parser understands, or a single event that has EXDATE/RDATE) into
// whichever instances overlap [window_start, window_end), minus the excluded and replaced ones,
// plus the RDATEs.
//
// A DTSTART in local time (a bare time or one with a TZID - both are read as the device's own time
// zone, see calendar_ics.h) repeats on the wall clock: the n-th instance is DTSTART's time of day
// on the n-th period's civil day. Adding n * 86400 s would drift an hour when the clocks change
// (a series started in winter would show up an hour late in summer). A DTSTART in UTC repeats at
// fixed instants, as the standard says. The first instance is found by jumping to the right period
// rather than walking from DTSTART, which can be years in the past.
static void expand_series(ics_ctx_t *ctx, const ics_vevent_state_t *st, const ics_rrule_t *rule,
                          time_t start, time_t end, const char *summary, time_t window_start,
                          time_t window_end, ics_event_list_t *out)
{
    ics_series_t s;
    memset(&s, 0, sizeof(s));
    s.start = start;
    s.duration = end > start ? end - start : 0;
    s.all_day = st->all_day;
    s.utc = st->dtstart_utc && !st->all_day;
    s.period_days = (int64_t) (rule->weekly ? 7 : 1) * rule->interval;
    if (s.period_days <= 0) {
        s.period_days = 1;  // defensive - interval is already clamped >= 1 by parse_rrule()
    }
    if (s.all_day) {
        s.dur_days = ((int64_t) s.duration + 43200) / 86400;
        if (s.dur_days < 1) {
            s.dur_days = 1;
        }
    }
    s.base_day = days_from_civil(st->dtstart_tm.tm_year + 1900, st->dtstart_tm.tm_mon + 1,
                                 st->dtstart_tm.tm_mday);
    s.hour = st->dtstart_tm.tm_hour;
    s.min = st->dtstart_tm.tm_min;
    s.sec = st->dtstart_tm.tm_sec;

    // First period to look at. An instance that started before the window can still reach into it
    // (an in-progress multi-day event), so back up by as many periods as the event is long.
    int64_t k0, back;
    if (s.utc) {
        int64_t period_secs = s.period_days * 86400;
        int64_t diff = (int64_t) window_start - (int64_t) start;
        k0 = diff > 0 ? diff / period_secs : 0;
        back = (int64_t) s.duration / period_secs + 2;
    } else {
        int64_t diff = local_day_of(window_start) - s.base_day;
        k0 = diff > 0 ? diff / s.period_days : 0;
        int64_t span_days = s.all_day ? s.dur_days : (int64_t) s.duration / 86400 + 1;
        back = span_days / s.period_days + 2;
    }
    k0 = k0 > back ? k0 - back : 0;

    // Hard safety cap, sized to the window (a flat number once truncated a 30-day expansion): the
    // periods the window spans plus the backed-up ones, never more than 400 beyond those.
    int64_t window_days = ((int64_t) window_end - (int64_t) window_start) / 86400 + 2;
    if (window_days < 0) {
        window_days = 0;
    }
    int64_t max_iterations = window_days / s.period_days + back + 3;
    if (max_iterations > back + 400) {
        max_iterations = back + 400;
    }

    ctx->n_cand = 0;
    for (int64_t k = k0; k < k0 + max_iterations; k++) {
        if (rule->has_count && k >= rule->count) {
            break;  // COUNT counts the instances the rule makes, excluded ones included
        }
        time_t occ_start = series_start(&s, k);
        if (occ_start >= window_end) {
            break;  // start only increases with k - nothing further can matter
        }
        if (rule->has_until && occ_start > rule->until) {
            break;  // UNTIL is inclusive (RFC 5545) - past it, nothing further can matter either
        }
        if (!time_overlaps_window(occ_start, series_end(&s, occ_start), window_start, window_end)) {
            continue;
        }
        if (!instance_hidden(ctx, st, occ_start, s.all_day)) {
            add_candidate(ctx, occ_start);
        }
    }

    for (int i = 0; i < ctx->n_rdates; i++) {
        time_t occ_start = ctx->rdates[i].t;
        if (ctx->rdates[i].all_day && !s.all_day) {
            // a bare date on a timed series: DTSTART's time of day on that day
            occ_start = local_time_on_day(local_day_of(occ_start), s.hour, s.min, s.sec);
        }
        if (!time_overlaps_window(occ_start, series_end(&s, occ_start), window_start, window_end)) {
            continue;
        }
        if (!instance_hidden(ctx, st, occ_start, s.all_day)) {
            add_candidate(ctx, occ_start);
        }
    }

    // An RDATE can name an instance the rule makes as well: it is one instance.
    qsort(ctx->cand, (size_t) ctx->n_cand, sizeof(ctx->cand[0]), compare_times);
    for (int i = 0; i < ctx->n_cand; i++) {
        if (i > 0 && ctx->cand[i] == ctx->cand[i - 1]) {
            continue;
        }
        if (!add_event(out, ctx->cand[i], series_end(&s, ctx->cand[i]), s.all_day, summary)) {
            ctx->dropped++;
        }
    }
}

// Finalizes one VEVENT block (called at "END:VEVENT"): fills in missing
// DTEND per the documented defaults, then either expands a supported RRULE
// (see expand_series()) or includes the single event if it overlaps the
// requested window. A present-but-unsupported RRULE, or an EXDATE/RDATE that
// cannot be read, means the event is skipped entirely, fail-soft - showing it
// once as if non-recurring, or with an instance that is meant to be gone,
// would be actively misleading.
static void finalize_vevent(ics_ctx_t *ctx, const ics_vevent_state_t *st, time_t window_start,
                            time_t window_end, ics_event_list_t *out)
{
    if (!st->have_dtstart) {
        return;
    }
    if (st->cancelled) {
        return;  // STATUS:CANCELLED - a called-off event, or a whole series
    }
    if (st->recurrence_and_future) {
        // "this and all later instances are changed": the changes would have to be applied to the
        // series from there on, which this parser cannot do. The instances before it stay; the
        // changed ones are left out rather than shown wrongly.
        ESP_LOGW(TAG, "Skipping an event with RECURRENCE-ID;RANGE=THISANDFUTURE (not supported)");
        return;
    }
    time_t start = ics_datetime_to_time(&st->dtstart_tm, st->dtstart_utc);

    time_t end;
    if (st->have_dtend) {
        end = ics_datetime_to_time(&st->dtend_tm, st->dtend_utc);
    } else if (st->have_duration && st->all_day) {
        // whole days, to a local midnight (a day is not always 86400 s)
        int64_t days = (st->duration_s + 43200) / 86400;
        end = local_time_on_day(local_day_of(start) + (days < 1 ? 1 : days), 0, 0, 0);
    } else if (st->have_duration) {
        end = start + (time_t) st->duration_s;
    } else if (st->all_day) {
        // Spec is ambiguous when DTEND is absent: a timed event defaults to zero duration; an
        // all-day VALUE=DATE event's single DTSTART day means exactly one day (to the next local
        // midnight, which is not 86400 s later on the day the clocks change).
        end = local_time_on_day(local_day_of(start) + 1, 0, 0, 0);
    } else {
        end = start;
    }

    const char *summary = st->have_summary ? st->raw_summary : NULL;

    // An exception (RECURRENCE-ID) is one instance, whatever else it says: some producers copy the
    // series' RRULE/EXDATE into it, which must not make a series of the single instance.
    if (st->has_recurrence_id || (!st->has_rrule && !st->has_exdate && !st->has_rdate)) {
        if (!time_overlaps_window(start, end, window_start, window_end)) {
            return;
        }
        if (!add_event(out, start, end, st->all_day, summary)) {
            ctx->dropped++;
        }
        return;
    }

    // A series, or a single event that has an EXDATE/RDATE (one instance that can be excluded, plus
    // the RDATEs): the latter is a series of one.
    ics_rrule_t single;
    memset(&single, 0, sizeof(single));
    single.supported = true;
    single.interval = 1;
    single.byday = -1;
    single.has_count = true;
    single.count = 1;
    const ics_rrule_t *rule = &single;
    if (st->has_rrule) {
        if (!st->rrule.supported) {
            return;
        }
        if (st->rrule.byday >= 0) {
            // parse_rrule() accepted a single BYDAY value without knowing
            // FREQ/DTSTART yet (RRULE components can appear in any order
            // per RFC 5545) - only trust it now: BYDAY on a DAILY rule is a
            // genuinely different pattern ("every day, but only Mondays" is
            // not "every day") this project's model can't represent, and a
            // BYDAY that names a different weekday than DTSTART's own is
            // likewise something this model can't reproduce. Both fail
            // closed exactly like any other unsupported rule, rather than
            // silently showing the wrong days. DTSTART's weekday is the one
            // of the zone DTSTART is written in: UTC for a "Z" time.
            struct tm start_tm;
            if (st->dtstart_utc) {
                gmtime_r(&start, &start_tm);
            } else {
                localtime_r(&start, &start_tm);
            }
            if (!st->rrule.weekly || start_tm.tm_wday != st->rrule.byday) {
                return;
            }
        }
        rule = &st->rrule;
    }

    // Only exceptions that can touch the window are kept: an instance outside it is not shown
    // anyway.
    bool truncated = false;
    time_t lo = window_start - ICS_EXCEPTION_LOOKBACK_S;
    if (!collect_stamps(st, "EXDATE", ctx->exdates, &ctx->n_exdates, ICS_MAX_EXDATES, lo,
                        window_end + 2 * 86400, &truncated) ||
        !collect_stamps(st, "RDATE", ctx->rdates, &ctx->n_rdates, ICS_MAX_RDATES, lo, window_end,
                        &truncated)) {
        ESP_LOGW(TAG, "Skipping a repeating event: an EXDATE/RDATE could not be read");
        return;
    }
    if (truncated) {
        ESP_LOGW(TAG,
                 "An event has more than %d/%d EXDATE/RDATE values in the window - the rest "
                 "is ignored",
                 ICS_MAX_EXDATES, ICS_MAX_RDATES);
    }
    expand_series(ctx, st, rule, start, end, summary, window_start, window_end, out);
}

static int compare_events_by_start(const void *a, const void *b)
{
    const ics_event_t *ea = (const ics_event_t *) a;
    const ics_event_t *eb = (const ics_event_t *) b;
    if (ea->start < eb->start) {
        return -1;
    }
    if (ea->start > eb->start) {
        return 1;
    }
    return 0;
}

// True if `needle` occurs in the first `len` bytes of `buf` (which may hold NUL bytes: a feed is
// not trusted to be text, so strstr() would stop at the first one).
static bool contains_text(const char *buf, size_t len, const char *needle)
{
    size_t n = strlen(needle);
    const char *p = buf;
    const char *end = buf + len;
    while (p < end && (size_t) (end - p) >= n) {
        p = memchr(p, needle[0], (size_t) (end - p) - n + 1);
        if (!p) {
            return false;
        }
        if (memcmp(p, needle, n) == 0) {
            return true;
        }
        p++;
    }
    return false;
}

esp_err_t calendar_ics_parse(char *body, size_t body_len, time_t window_start, time_t window_end,
                             ics_event_list_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (!body || body_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    body_len = ics_unfold(body, body_len);

    ics_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return ESP_ERR_NO_MEM;
    }
    if (contains_text(body, body_len, "RECURRENCE-ID")) {
        collect_overrides(ctx, body, body_len, window_start, window_end);
    }

    bool in_event = false;
    // A VALARM is a sub-block *inside* VEVENT (RFC 5545 §3.6.6) that can
    // carry its own SUMMARY/DESCRIPTION/TRIGGER properties (e.g. a reminder
    // text distinct from the event's own title). Without tracking this
    // separately, a VALARM's SUMMARY line would overwrite the real event's
    // SUMMARY below, since both share the same property name and this
    // parser otherwise only distinguishes "inside VEVENT" from "outside".
    bool in_alarm = false;
    ics_vevent_state_t st;
    reset_vevent_state(&st);

    const char *pos = body;
    const char *body_end = body + body_len;
    while (pos < body_end) {
        const char *nl = memchr(pos, '\n', (size_t) (body_end - pos));
        const char *line_end = nl ? nl : body_end;
        size_t line_len = (size_t) (line_end - pos);
        if (line_len > ICS_LINE_MAX_LEN) {
            line_len = ICS_LINE_MAX_LEN;  // tolerate, just don't overrun scratch buffers below
        }
        const char *line = pos;
        pos = nl ? nl + 1 : body_end;

        if (line_len == 0) {
            continue;
        }

        if (line_len >= 12 && strncmp(line, "BEGIN:VEVENT", 12) == 0) {
            in_event = true;
            in_alarm = false;
            reset_vevent_state(&st);
            st.block_start = line;
            continue;
        }
        if (line_len >= 10 && strncmp(line, "END:VEVENT", 10) == 0) {
            if (in_event) {
                st.block_end = line;
                finalize_vevent(ctx, &st, window_start, window_end, out);
            }
            in_event = false;
            in_alarm = false;
            continue;
        }
        if (line_len >= 12 && strncmp(line, "BEGIN:VALARM", 12) == 0) {
            in_alarm = true;
            continue;
        }
        if (line_len >= 10 && strncmp(line, "END:VALARM", 10) == 0) {
            in_alarm = false;
            continue;
        }
        if (!in_event || in_alarm) {
            continue;  // VCALENDAR/VTIMEZONE header noise, or a VALARM's own properties - ignored
        }

        const char *name, *value;
        size_t name_len, value_len;
        if (!split_ics_property(line, line_len, &name, &name_len, &value, &value_len)) {
            continue;
        }

        if (name_is(name, name_len, "DTSTART")) {
            if (parse_ics_datetime(value, value_len, &st.dtstart_tm, &st.all_day,
                                   &st.dtstart_utc)) {
                st.have_dtstart = true;
            }
        } else if (name_is(name, name_len, "DTEND")) {
            bool all_day_unused;
            if (parse_ics_datetime(value, value_len, &st.dtend_tm, &all_day_unused,
                                   &st.dtend_utc)) {
                st.have_dtend = true;
            }
        } else if (name_is(name, name_len, "DURATION")) {
            st.have_duration = parse_ics_duration(value, value_len, &st.duration_s);
        } else if (name_is(name, name_len, "SUMMARY")) {
            char raw[ICS_LINE_MAX_LEN];
            size_t copy_len = (value_len < sizeof(raw) - 1) ? value_len : sizeof(raw) - 1;
            memcpy(raw, value, copy_len);
            raw[copy_len] = '\0';

            char unescaped[ICS_LINE_MAX_LEN];
            decode_ics_text(raw, copy_len, unescaped, sizeof(unescaped));

            char ascii[ICS_SUMMARY_MAX_LEN];
            image_processor_sanitize_ascii(unescaped, ascii, sizeof(ascii));

            strncpy(st.raw_summary, ascii, ICS_SUMMARY_MAX_LEN - 1);
            st.raw_summary[ICS_SUMMARY_MAX_LEN - 1] = '\0';
            st.have_summary = true;
        } else if (name_is(name, name_len, "RRULE")) {
            st.has_rrule = true;
            parse_rrule(value, value_len, &st.rrule);  // rrule.supported tells finalize_vevent()
        } else if (name_is(name, name_len, "EXDATE")) {
            st.has_exdate = true;
        } else if (name_is(name, name_len, "RDATE")) {
            st.has_rdate = true;
        } else if (name_is(name, name_len, "STATUS")) {
            st.cancelled = value_len >= 9 && strncasecmp(value, "CANCELLED", 9) == 0;
        } else if (name_is(name, name_len, "UID")) {
            st.uid = hash_text(value, value_len);
            st.has_uid = true;
        } else if (name_is(name, name_len, "RECURRENCE-ID")) {
            st.has_recurrence_id = true;
            st.recurrence_and_future = line_param_is(line, line_len, "RANGE", "THISANDFUTURE");
        }
    }

    if (ctx->dropped > 0) {
        ESP_LOGW(TAG, "%d event(s) beyond the first %d of the window were left out", ctx->dropped,
                 ICS_MAX_EVENTS);
    }
    free(ctx->overrides);
    free(ctx);

    if (out->count > 1) {
        qsort(out->events, (size_t) out->count, sizeof(out->events[0]), compare_events_by_start);
    }

    return ESP_OK;  // an empty (no matching events) feed is not an error
}

// Reads the entire contents of `path` into a freshly malloc'd, NUL-terminated
// buffer. Returns NULL (and logs nothing - a missing cache file on the very
// first fetch, or after it was never written, is an expected, silent case)
// on any failure.
static char *read_whole_file(const char *path, size_t *out_len)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) {
        fclose(fp);
        return NULL;
    }
    char *buf = malloc((size_t) size + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    size_t read = fread(buf, 1, (size_t) size, fp);
    fclose(fp);
    buf[read] = '\0';
    if (out_len) {
        *out_len = read;
    }
    return buf;
}

esp_err_t calendar_ics_fetch(const char *url, int timeout_ms, time_t window_start,
                             time_t window_end, const char *cache_path, const char *etag_in,
                             char *etag_out, size_t etag_out_len, ics_event_list_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (etag_out && etag_out_len > 0) {
        etag_out[0] = '\0';
    }
    if (!url || url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    char *body = NULL;
    size_t body_len = 0;
    bool truncated = false;
    bool not_modified = false;
    esp_err_t err = http_fetch_get_conditional(
        url, timeout_ms > 0 ? timeout_ms : ICS_HTTP_TIMEOUT_MS, ICS_MAX_RESPONSE_BYTES, etag_in,
        &body, &body_len, &truncated, etag_out, etag_out_len, &not_modified, NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Calendar fetch failed: %s", esp_err_to_name(err));
        return err;
    }

    if (not_modified) {
        // Server didn't necessarily repeat the ETag on a 304 - keep sending
        // the one that got us this 304 in the first place.
        if (etag_out && etag_out_len > 0 && etag_out[0] == '\0' && etag_in) {
            strncpy(etag_out, etag_in, etag_out_len - 1);
            etag_out[etag_out_len - 1] = '\0';
        }
        if (!cache_path) {
            ESP_LOGW(TAG,
                     "Calendar 304 Not Modified but no cache configured - treating as failure");
            return ESP_FAIL;
        }
        body = read_whole_file(cache_path, &body_len);
        if (!body) {
            ESP_LOGW(TAG, "Calendar 304 Not Modified but no cached copy available");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Calendar feed unchanged (304) - reusing cached copy");
    } else {
        if (truncated) {
            ESP_LOGW(TAG, "Calendar response truncated at %d bytes - parsing what was captured",
                     ICS_MAX_RESPONSE_BYTES);
        }
        if (cache_path) {
            FILE *fp = fopen(cache_path, "wb");
            if (fp) {
                fwrite(body, 1, body_len, fp);
                fclose(fp);
            } else {
                ESP_LOGW(TAG, "Could not write Calendar cache file");
            }
        }
    }

    err = calendar_ics_parse(body, body_len, window_start, window_end, out);
    free(body);
    return err;
}

esp_err_t calendar_ics_fetch_once(const char *url, int timeout_ms, const char *cache_path)
{
    if (!url || url[0] == '\0' || !cache_path) {
        return ESP_ERR_INVALID_ARG;
    }

    char *body = NULL;
    size_t body_len = 0;
    bool truncated = false;
    esp_err_t err = http_fetch_get(url, timeout_ms > 0 ? timeout_ms : ICS_HTTP_TIMEOUT_MS,
                                   ICS_MAX_RESPONSE_BYTES, &body, &body_len, &truncated, NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "One-shot Calendar fetch failed: %s", esp_err_to_name(err));
        return err;
    }
    if (truncated) {
        ESP_LOGW(TAG,
                 "One-shot Calendar response truncated at %d bytes - caching what was captured",
                 ICS_MAX_RESPONSE_BYTES);
    }

    FILE *fp = fopen(cache_path, "wb");
    if (!fp) {
        ESP_LOGW(TAG, "Could not write Calendar cache file %s", cache_path);
        free(body);
        return ESP_FAIL;
    }
    fwrite(body, 1, body_len, fp);
    fclose(fp);
    free(body);
    return ESP_OK;
}

esp_err_t calendar_ics_read_cache(const char *cache_path, time_t window_start, time_t window_end,
                                  ics_event_list_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (!cache_path) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t body_len = 0;
    char *body = read_whole_file(cache_path, &body_len);
    if (!body) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = calendar_ics_parse(body, body_len, window_start, window_end, out);
    free(body);
    return err;
}

esp_err_t calendar_ics_write_expanded_cache(const char *path, const ics_event_list_t *list)
{
    if (!path || !list) {
        return ESP_ERR_INVALID_ARG;
    }
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        ESP_LOGW(TAG, "Could not write expanded ICS cache file %s", path);
        return ESP_FAIL;
    }
    for (int i = 0; i < list->count; i++) {
        const ics_event_t *ev = &list->events[i];
        char summary[ICS_SUMMARY_MAX_LEN];
        strncpy(summary, ev->summary, sizeof(summary) - 1);
        summary[sizeof(summary) - 1] = '\0';
        for (char *p = summary; *p != '\0'; p++) {
            if (*p == '\t' || *p == '\n' || *p == '\r') {
                *p = ' ';
            }
        }
        fprintf(fp, "%lld\t%lld\t%d\t%s\n", (long long) ev->start, (long long) ev->end,
                ev->all_day ? 1 : 0, summary);
    }
    fclose(fp);
    return ESP_OK;
}

esp_err_t calendar_ics_read_expanded_cache(const char *path, ics_event_list_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (!path) {
        return ESP_ERR_INVALID_ARG;
    }
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return ESP_ERR_NOT_FOUND;
    }

    char line[64 + ICS_SUMMARY_MAX_LEN];
    while (out->count < ICS_MAX_EVENTS && fgets(line, sizeof(line), fp)) {
        char *tab1 = strchr(line, '\t');
        char *tab2 = tab1 ? strchr(tab1 + 1, '\t') : NULL;
        char *tab3 = tab2 ? strchr(tab2 + 1, '\t') : NULL;
        if (!tab3) {
            continue;  // malformed/corrupted line - skip it, fail-soft
        }
        *tab1 = '\0';
        *tab2 = '\0';
        *tab3 = '\0';
        char *summary_start = tab3 + 1;
        size_t slen = strlen(summary_start);
        while (slen > 0 && (summary_start[slen - 1] == '\n' || summary_start[slen - 1] == '\r')) {
            summary_start[--slen] = '\0';
        }

        ics_event_t *ev = &out->events[out->count++];
        memset(ev, 0, sizeof(*ev));
        ev->start = (time_t) strtoll(line, NULL, 10);
        ev->end = (time_t) strtoll(tab1 + 1, NULL, 10);
        ev->all_day = (strtol(tab2 + 1, NULL, 10) != 0);
        strncpy(ev->summary, summary_start, sizeof(ev->summary) - 1);
        ev->summary[sizeof(ev->summary) - 1] = '\0';
    }
    fclose(fp);
    return ESP_OK;
}

bool calendar_ics_has_upcoming_event(const ics_event_list_t *list, time_t now)
{
    if (!list) {
        return false;
    }
    for (int i = 0; i < list->count; i++) {
        if (list->events[i].end > now) {
            return true;
        }
    }
    return false;
}
