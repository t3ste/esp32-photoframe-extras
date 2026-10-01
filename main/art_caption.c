#include "art_caption.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// ---------------------------------------------------------------------------------------------
// Folding tables (ASCII text, indexed by code point)

// U+00C0 .. U+00FF. NULL = kept as it is (the letters the font draws), "" = dropped.
static const char *const LATIN1[64] = {
    "A", "A", "A", "A", NULL, "A", "AE", "C", "E", "E", "E", "E", "I",  "I", "I",  "I",
    "D", "N", "O", "O", "O",  "O", NULL, "x", "O", "U", "U", "U", NULL, "Y", "Th", NULL,
    "a", "a", "a", "a", NULL, "a", "ae", "c", "e", "e", "e", "e", "i",  "i", "i",  "i",
    "d", "n", "o", "o", "o",  "o", NULL, "",  "o", "u", "u", "u", NULL, "y", "th", "y",
};

// U+0100 .. U+017F
static const char *const LATIN_EXT_A[128] = {
    "A", "a", "A",  "a",  "A", "a", "C", "c", "C", "c", "C", "c", "C", "c", "D", "d",
    "D", "d", "E",  "e",  "E", "e", "E", "e", "E", "e", "E", "e", "G", "g", "G", "g",
    "G", "g", "G",  "g",  "H", "h", "H", "h", "I", "i", "I", "i", "I", "i", "I", "i",
    "I", "i", "IJ", "ij", "J", "j", "K", "k", "k", "L", "l", "L", "l", "L", "l", "L",
    "l", "L", "l",  "N",  "n", "N", "n", "N", "n", "n", "N", "n", "O", "o", "O", "o",
    "O", "o", "OE", "oe", "R", "r", "R", "r", "R", "r", "S", "s", "S", "s", "S", "s",
    "S", "s", "T",  "t",  "T", "t", "T", "t", "U", "u", "U", "u", "U", "u", "U", "u",
    "U", "u", "U",  "u",  "W", "w", "Y", "y", "Y", "Z", "z", "Z", "z", "Z", "z", "s",
};

// ---------------------------------------------------------------------------------------------
// UTF-8

// Decodes one character; returns the bytes used (1 for a byte that is not valid UTF-8).
static int utf8_next(const unsigned char *s, uint32_t *cp)
{
    if (s[0] < 0x80) {
        *cp = s[0];
        return 1;
    }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        *cp = ((uint32_t) (s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *cp = ((uint32_t) (s[0] & 0x0F) << 12) | ((uint32_t) (s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return 3;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 &&
        (s[3] & 0xC0) == 0x80) {
        *cp = ((uint32_t) (s[0] & 0x07) << 18) | ((uint32_t) (s[1] & 0x3F) << 12) |
              ((uint32_t) (s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        return 4;
    }
    *cp = 0xFFFD;
    return 1;
}

// The characters the font draws as themselves (see the glyphs option)
static bool is_font_glyph(uint32_t cp)
{
    switch (cp) {
    case 0xE4:    // a with two dots
    case 0xF6:    // o with two dots
    case 0xFC:    // u with two dots
    case 0xC4:    // A with two dots
    case 0xD6:    // O with two dots
    case 0xDC:    // U with two dots
    case 0xDF:    // sharp s
    case 0xB0:    // degree
    case 0x20AC:  // euro
        return true;
    default:
        return false;
    }
}

// What a character is folded to: its own UTF-8 bytes (a font glyph), ASCII text, or "" (dropped).
static const char *fold_one(uint32_t cp, const char *own_bytes, bool *is_own)
{
    *is_own = false;
    if (cp >= 0x20 && cp < 0x7F) {
        *is_own = true;
        return own_bytes;
    }
    if (is_font_glyph(cp)) {
        *is_own = true;
        return own_bytes;
    }
    if (cp >= 0xC0 && cp <= 0xFF) {
        return LATIN1[cp - 0xC0];
    }
    if (cp >= 0x100 && cp <= 0x17F) {
        return LATIN_EXT_A[cp - 0x100];
    }
    switch (cp) {
    case 0x09:
    case 0x0A:
    case 0x0D:
    case 0xA0:    // no-break space
    case 0x2009:  // thin space
    case 0x202F:
        return " ";
    case 0x2010:  // hyphen
    case 0x2011:
    case 0x2012:  // dashes
    case 0x2013:
    case 0x2014:
    case 0x2212:  // minus
        return "-";
    case 0x2018:  // curly quotes
    case 0x2019:
    case 0x201A:
    case 0x2032:
        return "'";
    case 0x201C:
    case 0x201D:
    case 0x201E:
    case 0xAB:
    case 0xBB:
        return "\"";
    case 0x2026:  // ellipsis
        return "...";
    default:
        return "";
    }
}

size_t art_caption_fold(const char *utf8, char *out, size_t out_len)
{
    if (!out || out_len == 0) {
        return 0;
    }
    out[0] = '\0';
    if (!utf8) {
        return 0;
    }
    size_t used = 0;
    bool last_blank = true;  // no blank at the start
    const unsigned char *p = (const unsigned char *) utf8;
    while (*p) {
        uint32_t cp;
        int step = utf8_next(p, &cp);
        char own[5] = {0};
        for (int i = 0; i < step && i < 4; i++) {
            own[i] = (char) p[i];
        }
        bool is_own;
        const char *text = fold_one(cp, own, &is_own);
        if (!text) {  // NULL in the tables: a letter the font has
            text = own;
        }
        size_t len = strlen(text);
        bool blank = (len == 1 && text[0] == ' ');
        if (blank && last_blank) {
            p += step;
            continue;
        }
        if (len > 0) {
            if (used + len + 1 > out_len) {
                break;
            }
            memcpy(out + used, text, len);
            used += len;
            out[used] = '\0';
            last_blank = blank;
        }
        p += step;
    }
    while (used > 0 && out[used - 1] == ' ') {  // no blank at the end
        out[--used] = '\0';
    }
    return used;
}

int art_caption_char_count(const char *utf8)
{
    int count = 0;
    if (!utf8) {
        return 0;
    }
    for (const unsigned char *p = (const unsigned char *) utf8; *p; p++) {
        if ((*p & 0xC0) != 0x80) {
            count++;
        }
    }
    return count;
}

// The bytes of the first `chars` characters of the text
static size_t prefix_bytes(const char *utf8, int chars)
{
    size_t i = 0;
    int seen = 0;
    while (utf8[i]) {
        if (((unsigned char) utf8[i] & 0xC0) != 0x80) {
            if (seen == chars) {
                break;
            }
            seen++;
        }
        i++;
    }
    return i;
}

// Appends `text` (cut at `max_bytes` bytes of the buffer) to `out`.
static void append(char *out, size_t out_len, const char *text)
{
    size_t used = strlen(out);
    size_t room = out_len - used - 1;
    size_t len = strlen(text);
    if (len > room) {
        len = room;
    }
    memcpy(out + used, text, len);
    out[used + len] = '\0';
}

// artist " - " title " (" year ")" - parts that are empty are left out
static void build(const char *artist, const char *title, const char *year, char *out,
                  size_t out_len)
{
    out[0] = '\0';
    if (artist[0]) {
        append(out, out_len, artist);
    }
    if (title[0]) {
        if (artist[0]) {
            append(out, out_len, " - ");
        }
        append(out, out_len, title);
    }
    if (year[0]) {
        if (artist[0] || title[0]) {
            append(out, out_len, " (");
            append(out, out_len, year);
            append(out, out_len, ")");
        } else {
            append(out, out_len, year);
        }
    }
}

static void copy_out(const char *text, char *out, size_t out_len)
{
    size_t len = strlen(text);
    if (len >= out_len) {
        len = out_len - 1;
    }
    memcpy(out, text, len);
    out[len] = '\0';
}

void art_caption_compose(const char *artist, const char *title, const char *year, int max_chars,
                         char *out, size_t out_len)
{
    if (!out || out_len == 0) {
        return;
    }
    out[0] = '\0';
    if (max_chars < 8) {
        max_chars = 8;
    }
    char a[128], t[192], y[32], full[512];
    art_caption_fold(artist ? artist : "", a, sizeof(a));
    art_caption_fold(title ? title : "", t, sizeof(t));
    art_caption_fold(year ? year : "", y, sizeof(y));
    if (art_caption_char_count(y) > 12) {  // a long date text is of no use in one line
        y[0] = '\0';
    }
    bool has_digit = false;
    for (const char *c = y; *c; c++) {
        has_digit = has_digit || (*c >= '0' && *c <= '9');
    }
    if (!has_digit) {  // "n.d." and the like say nothing
        y[0] = '\0';
    }

    build(a, t, y, full, sizeof(full));
    if (art_caption_char_count(full) <= max_chars) {
        copy_out(full, out, out_len);
        return;
    }

    // Shorten the title: what the artist, the separator and the year take stays
    int la = art_caption_char_count(a);
    int lt = art_caption_char_count(t);
    int ly = art_caption_char_count(y);
    if (lt > 0) {
        int separator = (la > 0) ? 3 : 0;       // " - "
        int year_part = (ly > 0) ? ly + 3 : 0;  // " (year)"
        int budget = max_chars - la - separator - year_part;
        if (budget >= 8) {
            char cut[192];
            size_t keep = prefix_bytes(t, budget - 1);
            memcpy(cut, t, keep);
            cut[keep] = '\0';
            strcat(cut, "~");
            build(a, cut, y, full, sizeof(full));
            if (art_caption_char_count(full) <= max_chars) {
                copy_out(full, out, out_len);
                return;
            }
        }
    }

    // Cut the whole text
    size_t keep = prefix_bytes(full, max_chars - 1);
    if (keep + 2 > out_len) {
        keep = out_len > 2 ? out_len - 2 : 0;
    }
    memcpy(out, full, keep);
    out[keep] = '\0';
    strcat(out, "~");
}
