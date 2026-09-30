#include "vtodo.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "image_processor.h"

#define VTODO_LINE_MAX 600  // one unfolded content line (a long DESCRIPTION is cut, not an error)

// Undoes RFC 5545 line folding in place (a line starting with a space or tab continues the
// previous one) and turns every line break into a single '\n'. Returns the new length.
static size_t unfold(char *text, size_t len)
{
    size_t src = 0, dst = 0;
    while (src < len) {
        while (src < len && text[src] != '\n' && text[src] != '\r') {
            text[dst++] = text[src++];
        }
        if (src >= len) {
            break;
        }
        // one line break (CRLF, LF or CR); a following space or tab folds the next line onto this
        size_t after = src;
        if (text[after] == '\r' && after + 1 < len && text[after + 1] == '\n') {
            after += 2;
        } else {
            after += 1;
        }
        if (after < len && (text[after] == ' ' || text[after] == '\t')) {
            src = after + 1;  // dropped: the break and the fold's first whitespace
        } else {
            text[dst++] = '\n';
            src = after;
        }
    }
    text[dst] = '\0';
    return dst;
}

// Days since 1970-01-01 of a civil date (Howard Hinnant's algorithm).
static long days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned) (y - era * 400);
    unsigned doy = (unsigned) ((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long) doe - 719468;
}

static bool digits(const char *s, int n)
{
    for (int i = 0; i < n; i++) {
        if (!isdigit((unsigned char) s[i])) {
            return false;
        }
    }
    return true;
}

static int number(const char *s, int n)
{
    int v = 0;
    for (int i = 0; i < n; i++) {
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

// DUE value ("20261005", "20261005T120000Z", "20261005T120000") -> "YYYY-MM-DD". A UTC time is
// shown as the local date of the frame; anything else keeps the date it was written with.
static bool due_date_of(const char *value, char out[11])
{
    size_t n = strlen(value);
    if (n < 8 || !digits(value, 8)) {
        return false;
    }
    int y = number(value, 4), mo = number(value + 4, 2), d = number(value + 6, 2);
    if (mo < 1 || mo > 12 || d < 1 || d > 31) {
        return false;
    }
    bool utc_time = n >= 16 && value[8] == 'T' && digits(value + 9, 6) && value[15] == 'Z';
    if (utc_time) {
        long seconds = days_from_civil(y, mo, d) * 86400L + number(value + 9, 2) * 3600L +
                       number(value + 11, 2) * 60L + number(value + 13, 2);
        time_t t = (time_t) seconds;
        struct tm local;
        if (localtime_r(&t, &local) != NULL) {
            y = local.tm_year + 1900;
            mo = local.tm_mon + 1;
            d = local.tm_mday;
        }
    }
    // (the modulo keeps every field within its width, for the compiler's format check)
    snprintf(out, 11, "%04u-%02u-%02u", (unsigned) y % 10000, (unsigned) mo % 100,
             (unsigned) d % 100);
    return true;
}

// RFC 5545 TEXT: \n \N -> space (the column shows one line), \, \; \\ -> the character.
static void unescape_text(const char *in, char *out, size_t out_len)
{
    size_t o = 0;
    for (size_t i = 0; in[i] != '\0' && o + 1 < out_len; i++) {
        if (in[i] == '\\' && in[i + 1] != '\0') {
            i++;
            out[o++] = (in[i] == 'n' || in[i] == 'N') ? ' ' : in[i];
        } else {
            out[o++] = in[i];
        }
    }
    out[o] = '\0';
}

static char priority_letter(int ical_priority)
{
    if (ical_priority >= 1 && ical_priority <= 2) {
        return 'A';
    }
    if (ical_priority >= 3 && ical_priority <= 4) {
        return 'B';
    }
    if (ical_priority == 5) {
        return 'C';
    }
    if (ical_priority >= 6 && ical_priority <= 9) {
        return 'D';
    }
    return 0;
}

// The list keeps the TODO_MAX_ITEMS best items: due first (none last), then priority (none
// last), then arrival. `seq` is the arrival number.
typedef struct {
    todo_item_t item;
    long seq;
} ranked_t;

static int rank_compare(const ranked_t *a, const ranked_t *b)
{
    bool a_due = a->item.due_date[0] != '\0', b_due = b->item.due_date[0] != '\0';
    if (a_due != b_due) {
        return a_due ? -1 : 1;
    }
    if (a_due) {
        int by_date = strcmp(a->item.due_date, b->item.due_date);
        if (by_date != 0) {
            return by_date;
        }
    }
    int pa = a->item.priority ? a->item.priority : 'Z' + 1;
    int pb = b->item.priority ? b->item.priority : 'Z' + 1;
    if (pa != pb) {
        return pa - pb;
    }
    return a->seq < b->seq ? -1 : (a->seq > b->seq ? 1 : 0);
}

static void insert_ranked(ranked_t *list, int *count, const ranked_t *candidate)
{
    int pos = *count;
    while (pos > 0 && rank_compare(candidate, &list[pos - 1]) < 0) {
        pos--;
    }
    if (pos >= TODO_MAX_ITEMS) {
        return;  // worse than everything kept
    }
    int last = (*count < TODO_MAX_ITEMS) ? *count : TODO_MAX_ITEMS - 1;
    for (int i = last; i > pos; i--) {
        list[i] = list[i - 1];
    }
    list[pos] = *candidate;
    if (*count < TODO_MAX_ITEMS) {
        (*count)++;
    }
}

// One property line "NAME;PARAMS:value" -> name (upper-cased, up to the first ';' or ':') and the
// value after the first ':'. False for a line without a colon.
static bool split_property(char *line, char *name, size_t name_len, char **value)
{
    char *colon = strchr(line, ':');
    if (!colon) {
        return false;
    }
    size_t n = 0;
    while (line + n < colon && line[n] != ';' && n + 1 < name_len) {
        name[n] = (char) toupper((unsigned char) line[n]);
        n++;
    }
    name[n] = '\0';
    *value = colon + 1;
    return true;
}

esp_err_t vtodo_parse(char *ics, size_t len, todo_list_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (!ics) {
        return ESP_ERR_INVALID_ARG;
    }
    len = unfold(ics, len);

    ranked_t *ranked = calloc(TODO_MAX_ITEMS, sizeof(*ranked));
    if (!ranked) {
        return ESP_ERR_NO_MEM;
    }
    int kept = 0;
    long seq = 0;

    bool in_todo = false;
    int alarm_depth = 0;  // a VALARM inside the to-do has properties of its own
    ranked_t current;
    memset(&current, 0, sizeof(current));
    bool done = false;

    char *p = ics;
    char *end = ics + len;
    while (p < end) {
        char *nl = memchr(p, '\n', (size_t) (end - p));
        char *line_end = nl ? nl : end;
        if (nl) {
            *nl = '\0';
        } else {
            *end = '\0';  // the text has room for its terminator (unfold() wrote it)
        }
        char *line = p;
        p = nl ? nl + 1 : end;

        if (strncasecmp(line, "BEGIN:VTODO", 11) == 0) {
            in_todo = true;
            alarm_depth = 0;
            done = false;
            memset(&current, 0, sizeof(current));
            current.seq = seq++;
            continue;
        }
        if (!in_todo) {
            continue;
        }
        if (strncasecmp(line, "BEGIN:VALARM", 12) == 0) {
            alarm_depth++;
            continue;
        }
        if (strncasecmp(line, "END:VALARM", 10) == 0) {
            if (alarm_depth > 0) {
                alarm_depth--;
            }
            continue;
        }
        if (strncasecmp(line, "END:VTODO", 9) == 0) {
            in_todo = false;
            if (!done && current.item.text[0] != '\0') {
                insert_ranked(ranked, &kept, &current);
            }
            continue;
        }
        if (alarm_depth > 0 || (size_t) (line_end - line) == 0) {
            continue;
        }

        char name[24];
        char *value;
        if (!split_property(line, name, sizeof(name), &value)) {
            continue;
        }
        if (strcmp(name, "SUMMARY") == 0) {
            char text[VTODO_LINE_MAX];
            unescape_text(value, text, sizeof(text));
            image_processor_sanitize_ascii(text, current.item.text, sizeof(current.item.text));
        } else if (strcmp(name, "PRIORITY") == 0) {
            current.item.priority = priority_letter(atoi(value));
        } else if (strcmp(name, "DUE") == 0) {
            if (!due_date_of(value, current.item.due_date)) {
                current.item.due_date[0] = '\0';
            }
        } else if (strcmp(name, "STATUS") == 0) {
            done = done || strncasecmp(value, "COMPLETED", 9) == 0 ||
                   strncasecmp(value, "CANCELLED", 9) == 0;
        } else if (strcmp(name, "COMPLETED") == 0) {
            done = true;
        } else if (strcmp(name, "PERCENT-COMPLETE") == 0) {
            done = done || atoi(value) >= 100;
        }
    }

    out->count = kept;
    for (int i = 0; i < kept; i++) {
        out->items[i] = ranked[i].item;
    }
    free(ranked);
    return ESP_OK;
}
