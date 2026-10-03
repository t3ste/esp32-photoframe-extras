#include "recipe_text.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

// ---------------------------------------------------------------------------------------------
// UTF-8 to the one-byte code of the fonts (Windows-1252)
// ---------------------------------------------------------------------------------------------

// The code points of Windows-1252 outside Latin-1: the bytes 0x80-0x9F.
static int cp1252_byte(uint32_t cp)
{
    switch (cp) {
    case 0x20AC:
        return 0x80;  // euro
    case 0x201A:
        return 0x82;  // low single quote
    case 0x0192:
        return 0x83;
    case 0x201E:
        return 0x84;  // low double quote (German opening quote)
    case 0x2026:
        return 0x85;  // ellipsis
    case 0x2020:
        return 0x86;
    case 0x2021:
        return 0x87;
    case 0x02C6:
        return 0x88;
    case 0x2030:
        return 0x89;
    case 0x0160:
        return 0x8A;
    case 0x2039:
        return 0x8B;
    case 0x0152:
        return 0x8C;
    case 0x017D:
        return 0x8E;
    case 0x2018:
        return 0x91;
    case 0x2019:
        return 0x92;
    case 0x201C:
        return 0x93;
    case 0x201D:
        return 0x94;
    case 0x2022:
        return 0x95;  // bullet
    case 0x2013:
        return 0x96;  // en dash
    case 0x2014:
        return 0x97;  // em dash
    case 0x02DC:
        return 0x98;
    case 0x2122:
        return 0x99;
    case 0x0161:
        return 0x9A;
    case 0x203A:
        return 0x9B;
    case 0x0153:
        return 0x9C;
    case 0x017E:
        return 0x9E;
    case 0x0178:
        return 0x9F;
    default:
        return -1;
    }
}

// Characters that are not in the font but have a plain form: written as that text; NULL = none.
static const char *ascii_form(uint32_t cp)
{
    switch (cp) {
    case 0x2010:
    case 0x2011:
    case 0x2012:
    case 0x2212:
    case 0x2043:
        return "-";
    case 0x2044:
    case 0x2215:
        return "/";
    case 0x2032:
        return "'";
    case 0x2033:
        return "\"";
    case 0x2153:
        return "1/3";
    case 0x2154:
        return "2/3";
    case 0x2155:
        return "1/5";
    case 0x2156:
        return "2/5";
    case 0x2157:
        return "3/5";
    case 0x2158:
        return "4/5";
    case 0x2159:
        return "1/6";
    case 0x215A:
        return "5/6";
    case 0x215B:
        return "1/8";
    case 0x215C:
        return "3/8";
    case 0x215D:
        return "5/8";
    case 0x215E:
        return "7/8";
    case 0x2002:
    case 0x2003:
    case 0x2004:
    case 0x2005:
    case 0x2006:
    case 0x2007:
    case 0x2008:
    case 0x2009:
    case 0x200A:
    case 0x202F:
    case 0x205F:
    case 0x3000:
        return " ";
    case 0x2024:
        return ".";
    case 0x00B4:
        return "'";  // the acute accent is in Latin-1 but reads as an apostrophe here
    default:
        return NULL;
    }
}

// Characters that vanish without a trace: soft hyphen, zero-width marks, the byte order mark.
static bool is_invisible(uint32_t cp)
{
    return cp == 0x00AD || (cp >= 0x200B && cp <= 0x200F) || cp == 0x2060 || cp == 0xFEFF ||
           (cp >= 0x202A && cp <= 0x202E) || (cp >= 0xFE00 && cp <= 0xFE0F);
}

// Decodes one UTF-8 sequence; returns its length (an invalid byte counts as one and gives 0xFFFD).
static size_t utf8_next(const unsigned char *s, uint32_t *cp)
{
    if (s[0] < 0x80) {
        *cp = s[0];
        return 1;
    }
    size_t need = 0;
    uint32_t value = 0;
    if ((s[0] & 0xE0) == 0xC0) {
        need = 1;
        value = s[0] & 0x1F;
    } else if ((s[0] & 0xF0) == 0xE0) {
        need = 2;
        value = s[0] & 0x0F;
    } else if ((s[0] & 0xF8) == 0xF0) {
        need = 3;
        value = s[0] & 0x07;
    } else {
        *cp = 0xFFFD;
        return 1;
    }
    for (size_t i = 1; i <= need; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            *cp = 0xFFFD;
            return i;  // the sequence broke off: skip what was there
        }
        value = (value << 6) | (s[i] & 0x3F);
    }
    *cp = value;
    return need + 1;
}

// ---------------------------------------------------------------------------------------------
// HTML: entities and tags
// ---------------------------------------------------------------------------------------------

typedef struct {
    const char *name;
    uint32_t cp;
} entity_t;

static const entity_t ENTITIES[] = {
    {"amp", '&'},       {"lt", '<'},        {"gt", '>'},        {"quot", '"'},
    {"apos", '\''},     {"nbsp", 0x00A0},   {"auml", 0x00E4},   {"ouml", 0x00F6},
    {"uuml", 0x00FC},   {"Auml", 0x00C4},   {"Ouml", 0x00D6},   {"Uuml", 0x00DC},
    {"szlig", 0x00DF},  {"eacute", 0x00E9}, {"egrave", 0x00E8}, {"agrave", 0x00E0},
    {"acirc", 0x00E2},  {"ecirc", 0x00EA},  {"ccedil", 0x00E7}, {"deg", 0x00B0},
    {"frac12", 0x00BD}, {"frac14", 0x00BC}, {"frac34", 0x00BE}, {"ndash", 0x2013},
    {"mdash", 0x2014},  {"hellip", 0x2026}, {"rsquo", 0x2019},  {"lsquo", 0x2018},
    {"ldquo", 0x201C},  {"rdquo", 0x201D},  {"bdquo", 0x201E},  {"euro", 0x20AC},
    {"times", 0x00D7},  {"middot", 0x00B7}, {"shy", 0x00AD},
};

// `s` points at '&'. If an entity follows, stores its code point and returns its length.
static size_t entity_at(const char *s, uint32_t *cp)
{
    // the ';' within a dozen characters - not past the end of the text
    const char *end = NULL;
    for (size_t i = 1; i < 12 && s[i]; i++) {
        if (s[i] == ';') {
            end = s + i;
            break;
        }
    }
    if (!end || end - s < 3) {
        return 0;
    }
    size_t len = (size_t) (end - s) - 1;  // the name between '&' and ';'
    const char *name = s + 1;
    if (name[0] == '#') {
        uint32_t value = 0;
        if (len >= 2 && (name[1] == 'x' || name[1] == 'X')) {
            for (size_t i = 2; i < len; i++) {
                if (!isxdigit((unsigned char) name[i])) {
                    return 0;
                }
                value =
                    value * 16 + (uint32_t) (isdigit((unsigned char) name[i])
                                                 ? name[i] - '0'
                                                 : (tolower((unsigned char) name[i]) - 'a' + 10));
            }
        } else {
            for (size_t i = 1; i < len; i++) {
                if (!isdigit((unsigned char) name[i])) {
                    return 0;
                }
                value = value * 10 + (uint32_t) (name[i] - '0');
            }
        }
        if (len < 2 || value == 0 || value > 0x10FFFF) {
            return 0;
        }
        *cp = value;
        return (size_t) (end - s) + 1;
    }
    for (size_t i = 0; i < sizeof(ENTITIES) / sizeof(ENTITIES[0]); i++) {
        if (strlen(ENTITIES[i].name) == len && strncmp(ENTITIES[i].name, name, len) == 0) {
            *cp = ENTITIES[i].cp;
            return (size_t) (end - s) + 1;
        }
    }
    return 0;
}

// `s` points at '<'. If a tag follows (a letter, '/' or '!' after it, closed by '>'), returns its
// length and says whether it ends a line (<br>, <p>, </p>, <div>, <li>, <h1>...).
static size_t tag_at(const char *s, bool *breaks_line)
{
    char next = s[1];
    if (!(isalpha((unsigned char) next) || next == '/' || next == '!')) {
        return 0;
    }
    const char *end = strchr(s, '>');
    if (!end) {
        return 0;
    }
    const char *name = s + 1;
    if (*name == '/') {
        name++;
    }
    char word[8] = {0};
    size_t n = 0;
    while (n < sizeof(word) - 1 && isalnum((unsigned char) name[n])) {
        word[n] = (char) tolower((unsigned char) name[n]);
        n++;
    }
    *breaks_line = strcmp(word, "br") == 0 || strcmp(word, "p") == 0 || strcmp(word, "div") == 0 ||
                   strcmp(word, "li") == 0 || strcmp(word, "tr") == 0 ||
                   (word[0] == 'h' && isdigit((unsigned char) word[1]) && word[2] == '\0');
    return (size_t) (end - s) + 1;
}

// ---------------------------------------------------------------------------------------------
// recipe_text_clean
// ---------------------------------------------------------------------------------------------

typedef struct {
    char *out;
    size_t cap;
    size_t len;
    bool cut;
} sink_t;

static void put(sink_t *sink, char c)
{
    if (sink->len + 1 < sink->cap) {
        sink->out[sink->len++] = c;
    } else {
        sink->cut = true;
    }
}

static void put_cp(sink_t *sink, uint32_t cp)
{
    if (cp == '\r') {
        return;  // a "\r\n" is one break: the '\n' counts
    }
    if (cp == '\n') {
        put(sink, '\n');
        return;
    }
    if (cp == '\t' || cp == 0x00A0 || cp == ' ') {
        put(sink, ' ');
        return;
    }
    if (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp < 0xA0)) {
        return;  // control characters (the C1 range is not Windows-1252's: those bytes come only
                 // from the cases of cp1252_byte())
    }
    if (is_invisible(cp) || cp == 0xFFFD) {
        return;
    }
    if (cp < 0x80) {
        put(sink, (char) cp);
        return;
    }
    const char *plain = ascii_form(cp);
    if (plain) {
        for (; *plain; plain++) {
            put(sink, *plain);
        }
        return;
    }
    if (cp >= 0xA0 && cp <= 0xFF) {
        put(sink, (char) cp);
        return;
    }
    int byte = cp1252_byte(cp);
    if (byte >= 0) {
        put(sink, (char) byte);
    }
    // anything else (other scripts, emoji, arrows ...) is not in the font: dropped
}

size_t recipe_text_clean(const char *utf8, char *out, size_t out_cap, bool *out_cut)
{
    if (out_cut) {
        *out_cut = false;
    }
    if (!out || out_cap == 0) {
        return 0;
    }
    out[0] = '\0';
    if (!utf8) {
        return 0;
    }
    // pass 1: decode, remove markup, map characters; white space is tidied in pass 2
    sink_t sink = {.out = out, .cap = out_cap, .len = 0, .cut = false};
    const unsigned char *p = (const unsigned char *) utf8;
    while (*p) {
        if (*p == '<') {
            bool breaks = false;
            size_t tag = tag_at((const char *) p, &breaks);
            if (tag) {
                if (breaks) {
                    put(&sink, '\n');
                }
                p += tag;
                continue;
            }
        }
        if (*p == '&') {
            uint32_t entity_cp = 0;
            size_t entity = entity_at((const char *) p, &entity_cp);
            if (entity) {
                put_cp(&sink, entity_cp);
                p += entity;
                continue;
            }
        }
        uint32_t cp;
        p += utf8_next(p, &cp);
        put_cp(&sink, cp);
    }
    out[sink.len] = '\0';

    // pass 2, in place: one space between words (none before a comma or semicolon: "Bund , nach
    // Geschmack" is how a source writes it), no space around a line break, no blank lines, no white
    // space at either end
    size_t w = 0;
    bool pending_space = false;
    bool pending_break = false;
    for (size_t r = 0; r < sink.len; r++) {
        char c = out[r];
        if (c == ' ') {
            pending_space = true;
        } else if (c == '\n') {
            pending_break = true;
        } else {
            if (w > 0) {
                if (pending_break) {
                    out[w++] = '\n';
                } else if (pending_space && c != ',' && c != ';') {
                    out[w++] = ' ';
                }
            }
            pending_space = false;
            pending_break = false;
            out[w++] = c;
        }
    }
    out[w] = '\0';
    if (out_cut) {
        *out_cut = sink.cut;
    }
    return w;
}

// ---------------------------------------------------------------------------------------------
// Steps and paragraphs
// ---------------------------------------------------------------------------------------------

// A line that is only a marker of a step: a number (with a dot, bracket or colon), "step 3", or
// signs alone. `number` gets the number (0 if the line has none).
static bool is_step_marker(const char *line, size_t len, int *number)
{
    size_t i = 0;
    while (i < len && line[i] == ' ') {
        i++;
    }
    if (i + 4 <= len && strncasecmp(line + i, "step", 4) == 0) {
        i += 4;
        while (i < len && line[i] == ' ') {
            i++;
        }
    }
    int value = 0;
    bool digits = false;
    while (i < len && isdigit((unsigned char) line[i])) {
        value = value * 10 + (line[i] - '0');
        digits = true;
        i++;
    }
    bool signs = false;
    for (; i < len; i++) {
        unsigned char c = (unsigned char) line[i];
        if (c == ' ' || c == '.' || c == ')' || c == ':' || c == '-' || c == '*' || c == 0x95 ||
            c == 0x96 || c == 0x97) {
            signs = true;
            continue;
        }
        return false;  // a letter or other character: it is text
    }
    (void) signs;
    if (!digits) {
        // only signs: a marker if there is something (a bare bullet), not an empty line
        bool any = false;
        for (size_t k = 0; k < len; k++) {
            if (line[k] != ' ') {
                any = true;
            }
        }
        if (!any) {
            return false;
        }
    }
    *number = digits ? value : 0;
    return true;
}

size_t recipe_text_merge_steps(const char *text, char *out, size_t out_cap)
{
    if (!out || out_cap == 0) {
        return 0;
    }
    out[0] = '\0';
    if (!text) {
        return 0;
    }
    sink_t sink = {.out = out, .cap = out_cap, .len = 0, .cut = false};
    int counter = 0;
    int pending = -1;  // the number of a marker whose text has not come yet
    const char *line = text;
    bool first = true;
    while (*line) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t) (end - line) : strlen(line);
        int number = 0;
        if (is_step_marker(line, len, &number)) {
            counter++;
            pending = number > 0 ? number : counter;
            if (number > 0) {
                counter = number;
            }
        } else if (len > 0) {
            if (!first) {
                put(&sink, '\n');
            }
            first = false;
            if (pending >= 0) {
                char label[16];
                int n = snprintf(label, sizeof(label), "%d. ", pending);
                for (int k = 0; k < n; k++) {
                    put(&sink, label[k]);
                }
                pending = -1;
            }
            for (size_t k = 0; k < len; k++) {
                put(&sink, line[k]);
            }
        }
        if (!end) {
            break;
        }
        line = end + 1;
    }
    out[sink.len] = '\0';
    return sink.len;
}

static bool is_upper_or_digit(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == 0xC4 || c == 0xD6 ||
           c == 0xDC || (c >= 0xC0 && c <= 0xDE && c != 0xD7);
}

// Words that end in a dot without ending a sentence (compared in lower case, without the dot).
static const char *const ABBREVIATIONS[] = {
    "ca",     "z",   "b",   "bzw", "evtl", "ggf", "ggfs", "usw", "etc",  "min", "std",
    "sek",    "nr",  "vgl", "ml",  "dl",   "cl",  "tl",   "el",  "kg",   "g",   "pck",
    "pkt",    "bsp", "d",   "h",   "u",    "a",   "o",    "tsp", "tbsp", "oz",  "lb",
    "approx", "e",   "i",   "vs",  "st",   "dr",  "inkl", "ca",  "max",  "n",   "ev",
};

// `dot` points at a '.', '!' or '?' that is followed by a space: does a sentence end here?
static bool sentence_ends(const char *start, const char *dot)
{
    if (*dot != '.') {
        return true;  // ! and ? end a sentence
    }
    // the word before the dot
    const char *word_end = dot;
    const char *word_start = word_end;
    while (word_start > start && word_start[-1] != ' ' && word_start[-1] != '\n' &&
           word_start[-1] != '(') {
        word_start--;
    }
    size_t len = (size_t) (word_end - word_start);
    if (len == 0) {
        return false;
    }
    bool numeric = true;
    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char) word_start[i])) {
            numeric = false;
        }
    }
    if (numeric) {
        return false;  // "Die 2. Haelfte", "Schritt 3."
    }
    if (len == 1 && isalpha((unsigned char) word_start[0])) {
        return false;  // an initial or a letter of "z. B."
    }
    char lower[12];
    if (len < sizeof(lower)) {
        for (size_t i = 0; i < len; i++) {
            lower[i] = (char) tolower((unsigned char) word_start[i]);
        }
        lower[len] = '\0';
        for (size_t i = 0; i < sizeof(ABBREVIATIONS) / sizeof(ABBREVIATIONS[0]); i++) {
            if (strcmp(lower, ABBREVIATIONS[i]) == 0) {
                return false;
            }
        }
    }
    return true;
}

size_t recipe_text_paragraphs(const char *text, char *out, size_t out_cap)
{
    if (!out || out_cap == 0) {
        return 0;
    }
    out[0] = '\0';
    if (!text) {
        return 0;
    }
    sink_t sink = {.out = out, .cap = out_cap, .len = 0, .cut = false};
    const char *line = text;
    bool first_paragraph = true;
    while (*line) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t) (end - line) : strlen(line);
        if (len > 0) {
            // the sentences of this line, grouped by two
            const char *sentence_start = line;
            int in_group = 0;
            for (size_t i = 0; i < len; i++) {
                bool last = i + 1 == len;
                const char *here = line + i;
                bool boundary = false;
                if (last) {
                    boundary = true;
                } else if ((*here == '.' || *here == '!' || *here == '?') && line[i + 1] == ' ' &&
                           is_upper_or_digit((unsigned char) line[i + 2 < len ? i + 2 : i + 1]) &&
                           sentence_ends(line, here)) {
                    boundary = true;
                }
                if (!boundary) {
                    continue;
                }
                in_group++;
                bool group_full = in_group == 2 || last;
                if (!group_full) {
                    continue;
                }
                size_t piece = (size_t) (line + i + 1 - sentence_start);
                if (!first_paragraph) {
                    put(&sink, '\n');
                }
                first_paragraph = false;
                for (size_t k = 0; k < piece; k++) {
                    put(&sink, sentence_start[k]);
                }
                sentence_start = line + i + 1;
                while (sentence_start < line + len && *sentence_start == ' ') {
                    sentence_start++;
                    i++;
                }
                in_group = 0;
            }
        }
        if (!end) {
            break;
        }
        line = end + 1;
    }
    out[sink.len] = '\0';
    return sink.len;
}

// ---------------------------------------------------------------------------------------------
// Times and amounts
// ---------------------------------------------------------------------------------------------

void recipe_text_format_minutes(int minutes, bool german, char *out, size_t out_cap)
{
    if (!out || out_cap == 0) {
        return;
    }
    out[0] = '\0';
    if (minutes <= 0) {
        return;
    }
    int hours = minutes / 60;
    int rest = minutes % 60;
    if (hours > 0 && rest > 0) {
        snprintf(out, out_cap, german ? "%d Std. %d Min." : "%d h %d min", hours, rest);
    } else if (hours > 0) {
        snprintf(out, out_cap, german ? "%d Std." : "%d h", hours);
    } else {
        snprintf(out, out_cap, german ? "%d Min." : "%d min", rest);
    }
}

void recipe_text_format_amount(double value, bool german, char *out, size_t out_cap)
{
    if (!out || out_cap == 0) {
        return;
    }
    out[0] = '\0';
    if (!(value > 0)) {
        return;
    }
    char text[32];
    snprintf(text, sizeof(text), "%.2f", value);
    // trim trailing zeros and a trailing point
    size_t len = strlen(text);
    while (len > 0 && text[len - 1] == '0') {
        text[--len] = '\0';
    }
    if (len > 0 && text[len - 1] == '.') {
        text[--len] = '\0';
    }
    if (len == 0 || strcmp(text, "0") == 0) {
        return;  // it rounded to nothing
    }
    if (german) {
        for (size_t i = 0; i < len; i++) {
            if (text[i] == '.') {
                text[i] = ',';
            }
        }
    }
    snprintf(out, out_cap, "%s", text);
}
