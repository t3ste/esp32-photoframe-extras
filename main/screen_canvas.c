#include "screen_canvas.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "feature_config.h"
#include "fonts.h"
#if FEATURE_GLYPHS
#include "glyph_extras.h"
#endif

#define FONT_CELL_W 17
#define FONT_CELL_H 24

int canvas_unit(const canvas_t *canvas)
{
    int smaller = canvas->width < canvas->height ? canvas->width : canvas->height;
    int unit = smaller / 40;
    return unit < 1 ? 1 : unit;
}

int canvas_text_scale(const canvas_t *canvas, int steps)
{
    int unit = canvas_unit(canvas);
    int body = (unit + 6) / 12;  // 12 px unit (800x480) -> 1, 30 px -> 3
    if (body < 1) {
        body = 1;
    }
    return body * (steps < 1 ? 1 : steps);
}

static void put_pixel(canvas_t *canvas, int x, int y, canvas_color_t c)
{
    if (x < 0 || y < 0 || x >= canvas->width || y >= canvas->height) {
        return;
    }
    uint8_t *p = canvas->rgb + ((size_t) y * (size_t) canvas->width + (size_t) x) * 3;
    p[0] = c.r;
    p[1] = c.g;
    p[2] = c.b;
}

void canvas_rotate_cw(const canvas_t *src, canvas_t *dst)
{
    if (!src || !dst || !src->rgb || !dst->rgb || dst->width != src->height ||
        dst->height != src->width) {
        return;
    }
    for (int out_y = 0; out_y < dst->height; out_y++) {
        uint8_t *row = dst->rgb + (size_t) out_y * dst->width * 3;
        for (int out_x = 0; out_x < dst->width; out_x++) {
            const uint8_t *pixel =
                src->rgb + ((size_t) (src->height - 1 - out_x) * src->width + (size_t) out_y) * 3;
            row[out_x * 3] = pixel[0];
            row[out_x * 3 + 1] = pixel[1];
            row[out_x * 3 + 2] = pixel[2];
        }
    }
}

void canvas_fill(canvas_t *canvas, canvas_color_t color)
{
    canvas_rect(canvas, 0, 0, canvas->width, canvas->height, color);
}

void canvas_rect(canvas_t *canvas, int x, int y, int w, int h, canvas_color_t color)
{
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > canvas->width ? canvas->width : x + w;
    int y1 = y + h > canvas->height ? canvas->height : y + h;
    for (int row = y0; row < y1; row++) {
        uint8_t *p = canvas->rgb + ((size_t) row * (size_t) canvas->width + (size_t) x0) * 3;
        for (int col = x0; col < x1; col++) {
            *p++ = color.r;
            *p++ = color.g;
            *p++ = color.b;
        }
    }
}

void canvas_frame(canvas_t *canvas, int x, int y, int w, int h, int thickness, canvas_color_t color)
{
    if (thickness * 2 > w || thickness * 2 > h) {
        canvas_rect(canvas, x, y, w, h, color);
        return;
    }
    canvas_rect(canvas, x, y, w, thickness, color);
    canvas_rect(canvas, x, y + h - thickness, w, thickness, color);
    canvas_rect(canvas, x, y + thickness, thickness, h - 2 * thickness, color);
    canvas_rect(canvas, x + w - thickness, y + thickness, thickness, h - 2 * thickness, color);
}

void canvas_disc(canvas_t *canvas, int cx, int cy, int radius, canvas_color_t color)
{
    if (radius <= 0) {
        return;
    }
    long r2 = (long) radius * radius;
    for (int dy = -radius; dy <= radius; dy++) {
        int y = cy + dy;
        if (y < 0 || y >= canvas->height) {
            continue;
        }
        // half width of the chord at this row
        int half = (int) sqrtf((float) (r2 - (long) dy * dy));
        canvas_rect(canvas, cx - half, y, 2 * half + 1, 1, color);
    }
}

void canvas_ring_sector(canvas_t *canvas, int cx, int cy, int outer_radius, int inner_radius,
                        float start_deg, float end_deg, canvas_color_t color)
{
    if (outer_radius <= 0 || inner_radius >= outer_radius) {
        return;
    }
    if (inner_radius < 0) {
        inner_radius = 0;
    }
    float span = end_deg - start_deg;
    if (span <= 0.0f) {
        return;
    }
    bool full = span >= 360.0f;
    // normalise the start into [0, 360)
    float start = fmodf(start_deg, 360.0f);
    if (start < 0.0f) {
        start += 360.0f;
    }
    long outer2 = (long) outer_radius * outer_radius;
    long inner2 = (long) inner_radius * inner_radius;
    for (int dy = -outer_radius; dy <= outer_radius; dy++) {
        int y = cy + dy;
        if (y < 0 || y >= canvas->height) {
            continue;
        }
        for (int dx = -outer_radius; dx <= outer_radius; dx++) {
            int x = cx + dx;
            if (x < 0 || x >= canvas->width) {
                continue;
            }
            long d2 = (long) dx * dx + (long) dy * dy;
            if (d2 > outer2 || d2 < inner2) {
                continue;
            }
            if (!full) {
                // clockwise angle from 12 o'clock
                float angle = atan2f((float) dx, (float) -dy) * (180.0f / (float) M_PI);
                if (angle < 0.0f) {
                    angle += 360.0f;
                }
                float rel = angle - start;
                if (rel < 0.0f) {
                    rel += 360.0f;
                }
                if (rel >= span) {
                    continue;
                }
            }
            put_pixel(canvas, x, y, color);
        }
    }
}

void canvas_pill(canvas_t *canvas, int x, int y, int w, int h, canvas_color_t color)
{
    int r = h / 2;
    if (w <= h) {
        canvas_disc(canvas, x + w / 2, y + h / 2, w / 2, color);
        return;
    }
    canvas_rect(canvas, x + r, y, w - 2 * r, h, color);
    canvas_disc(canvas, x + r, y + r, r, color);
    canvas_disc(canvas, x + w - r - 1, y + r, r, color);
}

void canvas_triangle_down(canvas_t *canvas, int tip_x, int tip_y, int size, canvas_color_t color)
{
    for (int row = 0; row < size; row++) {
        // row 0 is the widest (top), the last row is the tip
        int half = (size - 1 - row) * (size / 2 + 1) / size;
        canvas_rect(canvas, tip_x - half, tip_y - (size - 1) + row, 2 * half + 1, 1, color);
    }
}

void canvas_line(canvas_t *canvas, float x0, float y0, float x1, float y1, int radius,
                 canvas_color_t color)
{
    if (radius < 1) {
        radius = 1;
    }
    float length = hypotf(x1 - x0, y1 - y0);
    int steps = (int) ceilf(length / ((float) radius * 0.5f + 0.5f));
    if (steps < 1) {
        steps = 1;
    }
    for (int i = 0; i <= steps; i++) {
        float f = (float) i / (float) steps;
        canvas_disc(canvas, (int) lroundf(x0 + (x1 - x0) * f), (int) lroundf(y0 + (y1 - y0) * f),
                    radius, color);
    }
}

void canvas_sparkline(canvas_t *canvas, int x, int y, int w, int h, const float *values, int count,
                      canvas_color_t end_color)
{
    if (count < 2 || w < 4 || h < 4) {
        return;
    }
    int u = canvas_unit(canvas);
    int radius = u / 8 < 1 ? 1 : u / 8;
    float low = values[0], high = values[0];
    for (int i = 1; i < count; i++) {
        low = values[i] < low ? values[i] : low;
        high = values[i] > high ? values[i] : high;
    }
    float span = high - low;
    int inset = 2 * radius + 2;  // room for the end disc (radius 2r + 1) inside the box
    float px = 0, py = 0;
    for (int i = 0; i < count; i++) {
        float fx = (float) i / (float) (count - 1);
        float fy = span > 0.0f ? (values[i] - low) / span : 0.5f;
        float cx = (float) (x + inset) + fx * (float) (w - 2 * inset);
        float cy = (float) (y + h - inset) - fy * (float) (h - 2 * inset);
        if (i > 0) {
            canvas_line(canvas, px, py, cx, cy, radius, CANVAS_BLACK);
        }
        px = cx;
        py = cy;
    }
    canvas_disc(canvas, (int) lroundf(px), (int) lroundf(py), radius * 2 + 1, end_color);
}

void canvas_arrow(canvas_t *canvas, int cx, int cy, int size, bool up, canvas_color_t color)
{
    if (size < 3) {
        size = 3;
    }
    for (int row = 0; row < size; row++) {
        int half = up ? row / 2 : (size - 1 - row) / 2;
        canvas_rect(canvas, cx - half, cy - size / 2 + row, 2 * half + 1, 1, color);
    }
}

int canvas_text_width(const char *text, int scale)
{
    return (int) strlen(text) * FONT_CELL_W * scale;
}

int canvas_text_height(int scale)
{
    return FONT_CELL_H * scale;
}

// The bitmap rows (3 bytes each) of a byte of text, or NULL for a byte without a glyph. `scratch`
// holds a composed glyph.
static const uint8_t *glyph_rows(uint8_t c, uint8_t scratch[FONT_CELL_H * 3])
{
#if FEATURE_GLYPHS
    if (glyph_extras_is_code(c) && glyph_extras_compose(c, Font24.table, scratch)) {
        return scratch;
    }
#else
    (void) scratch;
#endif
    if (c < ' ' || c > 0x7E) {
        return NULL;
    }
    return &Font24.table[(size_t) (c - ' ') * FONT_CELL_H * 3];
}

int canvas_text(canvas_t *canvas, int x, int y, const char *text, int scale, canvas_color_t color)
{
    if (scale < 1) {
        scale = 1;
    }
    uint8_t scratch[FONT_CELL_H * 3];
    for (const unsigned char *p = (const unsigned char *) text; *p != '\0'; p++) {
        const uint8_t *rows = glyph_rows(*p, scratch);
        if (rows) {
            for (int row = 0; row < FONT_CELL_H; row++) {
                for (int col = 0; col < FONT_CELL_W; col++) {
                    if (rows[row * 3 + col / 8] & (0x80 >> (col % 8))) {
                        canvas_rect(canvas, x + col * scale, y + row * scale, scale, scale, color);
                    }
                }
            }
        }
        x += FONT_CELL_W * scale;
    }
    return x;
}

void canvas_text_centered(canvas_t *canvas, int center_x, int y, const char *text, int scale,
                          canvas_color_t color)
{
    canvas_text(canvas, center_x - canvas_text_width(text, scale) / 2, y, text, scale, color);
}

void canvas_text_right(canvas_t *canvas, int right_x, int y, const char *text, int scale,
                       canvas_color_t color)
{
    canvas_text(canvas, right_x - canvas_text_width(text, scale), y, text, scale, color);
}

void canvas_text_fit(const char *text, int max_width, int scale, char *out, size_t out_len)
{
    if (out_len == 0) {
        return;
    }
    int cell = FONT_CELL_W * (scale < 1 ? 1 : scale);
    size_t fit = max_width > 0 ? (size_t) (max_width / cell) : 0;
    size_t len = strlen(text);
    if (len <= fit) {
        size_t n = len < out_len - 1 ? len : out_len - 1;
        memcpy(out, text, n);
        out[n] = '\0';
        return;
    }
    if (fit == 0) {
        out[0] = '\0';
        return;
    }
    size_t n = fit - 1 < out_len - 2 ? fit - 1 : out_len - 2;
    memcpy(out, text, n);
    out[n] = '~';
    out[n + 1] = '\0';
}

void canvas_text_from_utf8(const char *utf8, char *out, size_t out_len)
{
    if (out_len == 0) {
        return;
    }
    size_t o = 0;
    const unsigned char *p = (const unsigned char *) utf8;
    while (*p != '\0' && o + 1 < out_len) {
        unsigned char b0 = *p;
        if (b0 < 0x80) {
            out[o++] = (char) b0;
            p++;
            continue;
        }
#if FEATURE_GLYPHS
        if (glyph_extras_is_code(b0)) {  // already display text
            out[o++] = (char) b0;
            p++;
            continue;
        }
#endif
        uint32_t cp;
        int extra;
        if ((b0 & 0xE0) == 0xC0) {
            cp = b0 & 0x1F;
            extra = 1;
        } else if ((b0 & 0xF0) == 0xE0) {
            cp = b0 & 0x0F;
            extra = 2;
        } else if ((b0 & 0xF8) == 0xF0) {
            cp = b0 & 0x07;
            extra = 3;
        } else {
            p++;  // a stray continuation byte
            continue;
        }
        bool valid = true;
        for (int i = 0; i < extra; i++) {
            if ((p[1 + i] & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            cp = (cp << 6) | (p[1 + i] & 0x3F);
        }
        if (!valid) {
            p++;
            continue;
        }
        p += 1 + extra;
#if FEATURE_GLYPHS
        uint8_t code = glyph_extras_code_for_codepoint(cp);
        if (code != 0) {
            out[o++] = (char) code;
            continue;
        }
#else
        const char *digraph = NULL;
        switch (cp) {
        case 0x00E4:
            digraph = "ae";
            break;
        case 0x00F6:
            digraph = "oe";
            break;
        case 0x00FC:
            digraph = "ue";
            break;
        case 0x00C4:
            digraph = "Ae";
            break;
        case 0x00D6:
            digraph = "Oe";
            break;
        case 0x00DC:
            digraph = "Ue";
            break;
        case 0x00DF:
            digraph = "ss";
            break;
        default:
            break;
        }
        if (digraph) {
            for (const char *d = digraph; *d != '\0' && o + 1 < out_len; d++) {
                out[o++] = *d;
            }
        }
#endif
    }
    out[o] = '\0';
}

int canvas_text_fit_scale(const char *text, int max_width, int max_scale)
{
    for (int scale = max_scale; scale > 1; scale--) {
        if (canvas_text_width(text, scale) <= max_width) {
            return scale;
        }
    }
    return 1;
}

int canvas_text_wrap(const char *text, int max_width, int scale, char lines[][CANVAS_WRAP_LINE_MAX],
                     int max_lines)
{
    int cell = FONT_CELL_W * (scale < 1 ? 1 : scale);
    size_t per_line = max_width > 0 ? (size_t) (max_width / cell) : 1;
    if (per_line < 1) {
        per_line = 1;
    }
    if (per_line > CANVAS_WRAP_LINE_MAX - 1) {
        per_line = CANVAS_WRAP_LINE_MAX - 1;
    }
    int count = 0;
    const char *p = text;
    while (*p != '\0' && count < max_lines) {
        while (*p == ' ') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        size_t remaining = strlen(p);
        size_t take;
        if (remaining <= per_line) {
            take = remaining;
        } else {
            // the last space that keeps the line within the width; else cut inside a long word
            size_t space = per_line;
            while (space > 0 && p[space] != ' ') {
                space--;
            }
            take = space > 0 ? space : per_line;
        }
        memcpy(lines[count], p, take);
        lines[count][take] = '\0';
        // no trailing space on a line
        while (take > 0 && lines[count][take - 1] == ' ') {
            lines[count][--take] = '\0';
        }
        p += take;
        while (*p == ' ') {
            p++;
        }
        count++;
    }
    if (*p != '\0' && count > 0) {
        // text is left over: mark the last line as cut
        size_t len = strlen(lines[count - 1]);
        if (len >= per_line) {
            len = per_line - 1;
        }
        lines[count - 1][len] = '~';
        lines[count - 1][len + 1] = '\0';
    }
    return count;
}

int canvas_note_lines(const char *const *sources, int source_count, const char *stamp_long,
                      const char *stamp_short, int max_width, int scale,
                      char lines[][CANVAS_WRAP_LINE_MAX])
{
    bool has_stamp = (stamp_long && stamp_long[0]) || (stamp_short && stamp_short[0]);
    const char *stamps[2] = {stamp_long, stamp_short};
    // the stamp for a line of its own: the long one if there is one
    const char *own_stamp = (stamp_long && stamp_long[0]) ? stamp_long : stamp_short;
    int usable_sources = 0;
    for (int i = 0; i < source_count; i++) {
        usable_sources += (sources && sources[i] && sources[i][0]) ? 1 : 0;
    }

    if (usable_sources == 0) {
        if (!has_stamp) {
            return 0;
        }
        // the stamp alone: the long one if it fits, else the short one, else cut
        const char *pick = own_stamp;
        if (canvas_text_width(own_stamp, scale) > max_width && stamp_short && stamp_short[0]) {
            pick = stamp_short;
        }
        canvas_text_fit(pick, max_width, scale, lines[0], CANVAS_WRAP_LINE_MAX);
        return 1;
    }

    if (has_stamp) {
        for (int i = 0; i < source_count; i++) {
            if (!sources[i] || !sources[i][0]) {
                continue;
            }
            for (int k = 0; k < 2; k++) {
                if (!stamps[k] || !stamps[k][0]) {
                    continue;
                }
                char joined[2 * CANVAS_WRAP_LINE_MAX + 4];
                snprintf(joined, sizeof(joined), "%s - %s", sources[i], stamps[k]);
                if (strlen(joined) < CANVAS_WRAP_LINE_MAX &&
                    canvas_text_width(joined, scale) <= max_width) {
                    memcpy(lines[0], joined, strlen(joined) + 1);
                    return 1;
                }
            }
        }
    }

    // not on one line: the source (the first wording that fits a line, else the last one wrapped)
    int count = 0;
    const char *fallback = NULL;
    for (int i = 0; i < source_count && count == 0; i++) {
        if (!sources[i] || !sources[i][0]) {
            continue;
        }
        fallback = sources[i];
        if (canvas_text_width(sources[i], scale) <= max_width) {
            canvas_text_fit(sources[i], max_width, scale, lines[0], CANVAS_WRAP_LINE_MAX);
            count = 1;
        }
    }
    if (count == 0) {
        count = canvas_text_wrap(fallback, max_width, scale, lines, 2);
    }
    if (has_stamp && count < CANVAS_NOTE_LINES_MAX) {
        canvas_text_fit(own_stamp, max_width, scale, lines[count], CANVAS_WRAP_LINE_MAX);
        count++;
    }
    return count;
}

int canvas_text_first_fit(canvas_t *canvas, int x, int y, int max_width, int scale,
                          canvas_color_t color, const char *const *texts, int count)
{
    for (int i = 0; texts && i < count; i++) {
        if (texts[i] && texts[i][0] && canvas_text_width(texts[i], scale) <= max_width) {
            canvas_text(canvas, x, y, texts[i], scale, color);
            return i;
        }
    }
    return -1;
}

int canvas_header_label_width(const char *label, bool late, int scale)
{
    return canvas_text_width(label, scale) + (late ? canvas_text_width(" !", scale) : 0);
}

void canvas_header_label(canvas_t *canvas, int right_x, int y, int scale, const char *label,
                         bool late, canvas_color_t color)
{
    if (late) {
        canvas_text(canvas, right_x - canvas_text_width("!", scale), y, "!", scale, CANVAS_YELLOW);
        right_x -= canvas_text_width(" !", scale);
    }
    canvas_text_right(canvas, right_x, y, label, scale, color);
}
