#include "screen_digits.h"

#include <math.h>
#include <string.h>

#define PI_F 3.14159265f

// A pen: strokes of a given radius inside the box of one glyph. Coordinates of the glyph shapes are
// fractions of the box (0..1 both ways, y down) of the stroke centre line; the pen keeps the
// stroke inside the box by insetting the centre line by the radius.
typedef struct {
    canvas_t *canvas;
    float x0, y0;  // top left of the glyph box
    float w, h;    // size of the area of the centre line
    float radius;  // stroke radius in pixels
    canvas_color_t color;
} pen_t;

static float map_x(const pen_t *pen, float nx)
{
    return pen->x0 + pen->radius + nx * pen->w;
}

static float map_y(const pen_t *pen, float ny)
{
    return pen->y0 + pen->radius + ny * pen->h;
}

static void stamp(const pen_t *pen, float x, float y)
{
    canvas_disc(pen->canvas, (int) lroundf(x), (int) lroundf(y), (int) pen->radius, pen->color);
}

// A straight stroke between two points given in glyph fractions.
static void seg(const pen_t *pen, float nx0, float ny0, float nx1, float ny1)
{
    float x0 = map_x(pen, nx0), y0 = map_y(pen, ny0);
    float x1 = map_x(pen, nx1), y1 = map_y(pen, ny1);
    float length = hypotf(x1 - x0, y1 - y0);
    float step = pen->radius * 0.5f < 1.0f ? 1.0f : pen->radius * 0.5f;
    int steps = (int) ceilf(length / step);
    if (steps < 1) {
        steps = 1;
    }
    for (int i = 0; i <= steps; i++) {
        float f = (float) i / (float) steps;
        stamp(pen, x0 + (x1 - x0) * f, y0 + (y1 - y0) * f);
    }
}

// An arc of an ellipse: centre and radii in glyph fractions, angles in degrees, 0 = right,
// 90 = down (so increasing angles run clockwise on the screen).
static void arc(const pen_t *pen, float ncx, float ncy, float nrx, float nry, float deg0,
                float deg1)
{
    float rx = nrx * pen->w, ry = nry * pen->h;
    float cx = map_x(pen, ncx), cy = map_y(pen, ncy);
    float longest = rx > ry ? rx : ry;
    float sweep = fabsf(deg1 - deg0) * PI_F / 180.0f;
    float step = pen->radius * 0.5f < 1.0f ? 1.0f : pen->radius * 0.5f;
    int steps = (int) ceilf(sweep * longest / step);
    if (steps < 8) {
        steps = 8;
    }
    for (int i = 0; i <= steps; i++) {
        float a = (deg0 + (deg1 - deg0) * (float) i / (float) steps) * PI_F / 180.0f;
        stamp(pen, cx + rx * cosf(a), cy + ry * sinf(a));
    }
}

// Proportions, all relative to the glyph height.
// The stroke is drawn with discs of `radius` pixels, so it is 2 * radius + 1 pixels thick.
static int radius_of(int height)
{
    int radius = height / 18;
    return radius < 1 ? 1 : radius;
}

static int stroke_of(int height)
{
    return 2 * radius_of(height) + 1;
}

static int digit_width(int height)
{
    return height * 56 / 100;
}

static int gap_of(int height)
{
    return stroke_of(height) * 3 / 2;
}

static int degree_width(int height)
{
    return height * 40 / 100;
}

static int dot_width(int height)
{
    return stroke_of(height);
}

// Width of one glyph, 0 for a character that is skipped.
static int glyph_width(char c, int height)
{
    if ((c >= '0' && c <= '9') || c == '-' || c == '+') {
        return digit_width(height);
    }
    if (c == '.' || c == ':') {
        return dot_width(height);
    }
    if (c == '*') {
        return degree_width(height);
    }
    if (c == ' ') {
        return digit_width(height) / 2;
    }
    return 0;
}

int canvas_big_width(const char *text, int height)
{
    int width = 0;
    bool any = false;
    for (const char *p = text; p && *p; p++) {
        int w = glyph_width(*p, height);
        if (w == 0) {
            continue;
        }
        width += w + gap_of(height);
        any = true;
    }
    return any ? width - gap_of(height) : 0;
}

static void draw_glyph(canvas_t *canvas, int x, int y, char c, int height, canvas_color_t color)
{
    int stroke = stroke_of(height);
    int width = glyph_width(c, height);
    pen_t pen = {canvas,
                 (float) x,
                 (float) y,
                 (float) (width - stroke),
                 (float) (height - stroke),
                 (float) radius_of(height),
                 color};
    switch (c) {
    case '0':
        arc(&pen, 0.5f, 0.5f, 0.5f, 0.5f, 0, 360);
        break;
    case '1':
        seg(&pen, 0.5f, 0.0f, 0.5f, 1.0f);
        seg(&pen, 0.5f, 0.0f, 0.1f, 0.22f);
        break;
    case '2':
        arc(&pen, 0.5f, 0.27f, 0.5f, 0.27f, 200, 380);
        seg(&pen, 0.97f, 0.36f, 0.0f, 1.0f);
        seg(&pen, 0.0f, 1.0f, 1.0f, 1.0f);
        break;
    case '3':
        arc(&pen, 0.48f, 0.26f, 0.44f, 0.26f, 205, 450);
        arc(&pen, 0.5f, 0.75f, 0.5f, 0.25f, 270, 515);
        break;
    case '4':
        seg(&pen, 0.72f, 1.0f, 0.72f, 0.0f);
        seg(&pen, 0.72f, 0.0f, 0.0f, 0.68f);
        seg(&pen, 0.0f, 0.68f, 1.0f, 0.68f);
        break;
    case '5':
        seg(&pen, 0.92f, 0.0f, 0.12f, 0.0f);
        seg(&pen, 0.12f, 0.0f, 0.09f, 0.56f);
        arc(&pen, 0.5f, 0.72f, 0.5f, 0.28f, 215, 505);
        break;
    case '6':
        arc(&pen, 0.95f, 0.68f, 0.95f, 0.66f, 180, 270);
        arc(&pen, 0.5f, 0.68f, 0.5f, 0.32f, 0, 360);
        break;
    case '7':
        seg(&pen, 0.0f, 0.0f, 1.0f, 0.0f);
        seg(&pen, 1.0f, 0.0f, 0.32f, 1.0f);
        break;
    case '8':
        arc(&pen, 0.5f, 0.25f, 0.42f, 0.25f, 0, 360);
        arc(&pen, 0.5f, 0.745f, 0.5f, 0.255f, 0, 360);
        break;
    case '9':
        arc(&pen, 0.5f, 0.32f, 0.5f, 0.32f, 0, 360);
        arc(&pen, 0.05f, 0.32f, 0.95f, 0.66f, 0, 90);
        break;
    case '-':
        seg(&pen, 0.05f, 0.5f, 0.95f, 0.5f);
        break;
    case '+':
        seg(&pen, 0.05f, 0.5f, 0.95f, 0.5f);
        seg(&pen, 0.5f, 0.28f, 0.5f, 0.72f);
        break;
    case '.':
        seg(&pen, 0.0f, 1.0f, 0.0f, 1.0f);
        break;
    case ':':
        seg(&pen, 0.0f, 0.3f, 0.0f, 0.3f);
        seg(&pen, 0.0f, 0.75f, 0.0f, 0.75f);
        break;
    case '*': {
        // a thinner ring at the top
        int thin_radius = radius_of(height) * 4 / 5 < 1 ? 1 : radius_of(height) * 4 / 5;
        int thin = 2 * thin_radius + 1;
        pen_t ring = {canvas,
                      (float) x,
                      (float) y,
                      (float) (width - thin),
                      (float) (width - thin),
                      (float) thin_radius,
                      color};
        arc(&ring, 0.5f, 0.5f, 0.5f, 0.5f, 0, 360);
        break;
    }
    default:
        break;
    }
}

int canvas_big_text(canvas_t *canvas, int x, int y, const char *text, int height,
                    canvas_color_t color)
{
    int cursor = x;
    bool first = true;
    for (const char *p = text; p && *p; p++) {
        int w = glyph_width(*p, height);
        if (w == 0) {
            continue;
        }
        if (!first) {
            cursor += gap_of(height);
        }
        first = false;
        draw_glyph(canvas, cursor, y, *p, height, color);
        cursor += w;
    }
    return cursor;
}

void canvas_big_text_centered(canvas_t *canvas, int center_x, int y, const char *text, int height,
                              canvas_color_t color)
{
    canvas_big_text(canvas, center_x - canvas_big_width(text, height) / 2, y, text, height, color);
}

int canvas_big_fit_height(const char *text, int max_width, int max_height)
{
    int height = max_height;
    while (height > 1 && canvas_big_width(text, height) > max_width) {
        height--;
    }
    return height < 1 ? 1 : height;
}
