#include "caldav.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

bool caldav_is_url(const char *url)
{
    return url &&
           (strncasecmp(url, "caldavs://", 10) == 0 || strncasecmp(url, "caldav://", 9) == 0);
}

const char *caldav_resolve_url(const char *url, char *buf, size_t buf_len)
{
    const char *scheme = NULL;
    size_t prefix = 0;
    if (url && strncasecmp(url, "caldavs://", 10) == 0) {
        scheme = "https://";
        prefix = 10;
    } else if (url && strncasecmp(url, "caldav://", 9) == 0) {
        scheme = "http://";
        prefix = 9;
    }
    if (!scheme) {
        return url;
    }
    int n = snprintf(buf, buf_len, "%s%s", scheme, url + prefix);
    return (n < 0 || (size_t) n >= buf_len) ? NULL : buf;
}

// 20260930T000000Z
static void format_utc(time_t t, char out[17])
{
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(out, 17, "%Y%m%dT%H%M%SZ", &tm);
}

int caldav_build_report_body(char *buf, size_t buf_len, time_t start, time_t end, bool expand)
{
    char s[17], e[17];
    format_utc(start, s);
    format_utc(end, e);
    char data[96] = "";
    if (expand) {
        snprintf(data, sizeof(data), "<c:expand start=\"%s\" end=\"%s\"/>", s, e);
    }
    int n = snprintf(buf, buf_len,
                     "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                     "<c:calendar-query xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
                     "<d:prop><c:calendar-data>%s</c:calendar-data></d:prop>"
                     "<c:filter><c:comp-filter name=\"VCALENDAR\">"
                     "<c:comp-filter name=\"VEVENT\"><c:time-range start=\"%s\" end=\"%s\"/>"
                     "</c:comp-filter></c:comp-filter></c:filter>"
                     "</c:calendar-query>",
                     data, s, e);
    return (n < 0 || (size_t) n >= buf_len) ? -1 : n;
}

int caldav_build_todo_report_body(char *buf, size_t buf_len, bool only_open)
{
    int n = snprintf(
        buf, buf_len,
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<c:calendar-query xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
        "<d:prop><c:calendar-data/></d:prop>"
        "<c:filter><c:comp-filter name=\"VCALENDAR\">"
        "<c:comp-filter name=\"VTODO\">%s</c:comp-filter>"
        "</c:comp-filter></c:filter>"
        "</c:calendar-query>",
        only_open ? "<c:prop-filter name=\"COMPLETED\"><c:is-not-defined/></c:prop-filter>" : "");
    return (n < 0 || (size_t) n >= buf_len) ? -1 : n;
}

// Encodes a code point as UTF-8 at out; returns the number of bytes (0 for an invalid one).
static size_t utf8_encode(unsigned long cp, char *out)
{
    if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        return 0;
    }
    if (cp < 0x80) {
        out[0] = (char) cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char) (0xC0 | (cp >> 6));
        out[1] = (char) (0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char) (0xE0 | (cp >> 12));
        out[1] = (char) (0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char) (0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char) (0xF0 | (cp >> 18));
    out[1] = (char) (0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char) (0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char) (0x80 | (cp & 0x3F));
    return 4;
}

// Decodes the entity at s (which points at '&', end is the end of the input): writes the
// decoded bytes to out and returns how many input bytes the entity took; 0 if it is not one
// (the '&' is then kept as it is).
static size_t decode_entity(const char *s, const char *end, char *out, size_t *out_n)
{
    static const struct {
        const char *name;
        char ch;
    } named[] = {{"&lt;", '<'}, {"&gt;", '>'}, {"&amp;", '&'}, {"&quot;", '"'}, {"&apos;", '\''}};
    for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); i++) {
        size_t n = strlen(named[i].name);
        if ((size_t) (end - s) >= n && memcmp(s, named[i].name, n) == 0) {
            out[0] = named[i].ch;
            *out_n = 1;
            return n;
        }
    }
    if ((size_t) (end - s) >= 4 && s[1] == '#') {
        const char *p = s + 2;
        int base = 10;
        if (*p == 'x' || *p == 'X') {
            base = 16;
            p++;
        }
        unsigned long cp = 0;
        const char *digits = p;
        while (p < end && *p != ';' && p - s < 12) {
            int d;
            if (*p >= '0' && *p <= '9') {
                d = *p - '0';
            } else if (base == 16 && *p >= 'a' && *p <= 'f') {
                d = *p - 'a' + 10;
            } else if (base == 16 && *p >= 'A' && *p <= 'F') {
                d = *p - 'A' + 10;
            } else {
                return 0;
            }
            cp = cp * (unsigned long) base + (unsigned long) d;
            p++;
        }
        if (p >= end || *p != ';' || p == digits) {
            return 0;
        }
        size_t n = utf8_encode(cp, out);
        if (n == 0) {
            return 0;
        }
        *out_n = n;
        return (size_t) (p - s) + 1;
    }
    return 0;
}

// If the tag starting at p (which points at '<') is a start tag whose local name (the part after
// a namespace prefix) is `name`, returns a pointer just after its '>'; a self-closing tag or any
// other tag gives NULL. With want_close, the same for an end tag `</...name>`.
static const char *match_tag(const char *p, const char *end, const char *name, bool want_close)
{
    const char *q = p + 1;
    if (want_close) {
        if (q >= end || *q != '/') {
            return NULL;
        }
        q++;
    } else if (q < end && (*q == '/' || *q == '?' || *q == '!')) {
        return NULL;
    }
    const char *tag_end = memchr(q, '>', (size_t) (end - q));
    if (!tag_end) {
        return NULL;
    }
    // name of the tag: up to whitespace, '/' or '>'
    const char *name_end = q;
    while (name_end < tag_end && *name_end != ' ' && *name_end != '\t' && *name_end != '\r' &&
           *name_end != '\n' && *name_end != '/') {
        name_end++;
    }
    const char *colon = NULL;
    for (const char *c = q; c < name_end; c++) {
        if (*c == ':') {
            colon = c;
        }
    }
    const char *local = colon ? colon + 1 : q;
    size_t local_len = (size_t) (name_end - local);
    if (local_len != strlen(name) || strncmp(local, name, local_len) != 0) {
        return NULL;
    }
    if (!want_close && tag_end > q && tag_end[-1] == '/') {
        return NULL;  // <c:calendar-data/> holds nothing
    }
    return tag_end + 1;
}

size_t caldav_extract_calendar_data(char *xml, size_t len)
{
    if (!xml) {
        return 0;
    }
    const char *end = xml + len;
    const char *r = xml;
    char *w = xml;
    bool first = true;

    while (r < end) {
        const char *lt = memchr(r, '<', (size_t) (end - r));
        if (!lt) {
            break;
        }
        const char *open_end = match_tag(lt, end, "calendar-data", false);
        if (!open_end) {
            r = lt + 1;
            continue;
        }
        r = open_end;
        // Each element is preceded by its start tag, so the separator never overtakes the reader.
        if (!first) {
            *w++ = '\n';
        }
        first = false;

        // The element's text, up to its end tag.
        while (r < end) {
            if (*r == '<') {
                if (end - r >= 9 && memcmp(r, "<![CDATA[", 9) == 0) {
                    r += 9;
                    while (r < end && !(end - r >= 3 && memcmp(r, "]]>", 3) == 0)) {
                        *w++ = *r++;
                    }
                    if (r < end) {
                        r += 3;
                    }
                    continue;
                }
                const char *close_end = match_tag(r, end, "calendar-data", true);
                if (close_end) {
                    r = close_end;
                    break;
                }
                // some other markup inside the element: not part of the text
                const char *gt = memchr(r, '>', (size_t) (end - r));
                r = gt ? gt + 1 : end;
                continue;
            }
            if (*r == '&') {
                char dec[4];
                size_t n = 0;
                size_t used = decode_entity(r, end, dec, &n);
                if (used > 0) {
                    memcpy(w, dec, n);
                    w += n;
                    r += used;
                    continue;
                }
            }
            *w++ = *r++;
        }
    }
    *w = '\0';
    return (size_t) (w - xml);
}
