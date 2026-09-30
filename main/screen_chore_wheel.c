#include "screen_chore_wheel.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void chore_config_parse(const char *members_text, const char *tasks_text, chore_config_t *out)
{
    memset(out, 0, sizeof(*out));
    out->member_count =
        info_parse_list(members_text, &out->members[0][0], CHORE_NAME_MAX, CHORE_MAX_MEMBERS);
    out->task_count =
        info_parse_list(tasks_text, &out->tasks[0][0], CHORE_NAME_MAX, CHORE_MAX_TASKS);
}

int chore_assignee(int task, int iso_week, int member_count)
{
    if (member_count <= 0) {
        return -1;
    }
    int index = (task + iso_week) % member_count;
    return index < 0 ? index + member_count : index;
}

canvas_color_t chore_member_color(int member)
{
    static const canvas_color_t colors[CHORE_MAX_MEMBERS] = {
        {255, 0, 0}, {0, 0, 255}, {0, 255, 0}, {255, 255, 0}, {0, 0, 0}};
    return colors[((member % CHORE_MAX_MEMBERS) + CHORE_MAX_MEMBERS) % CHORE_MAX_MEMBERS];
}

// White text on the dark colours, black on yellow and green.
static canvas_color_t text_on(canvas_color_t fill)
{
    bool light = fill.r + fill.g + fill.b > 300;
    return light ? CANVAS_BLACK : CANVAS_WHITE;
}

// Display text of a name typed as UTF-8.
static void display_name(const char *utf8, char *out, size_t out_len)
{
    canvas_text_from_utf8(utf8, out, out_len);
}

static void draw_message(canvas_t *canvas, const info_now_t *now)
{
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    char line1[CANVAS_WRAP_LINE_MAX], line2[CANVAS_WRAP_LINE_MAX];
    canvas_text_from_utf8(now->german ? "Keine Aufgaben eingerichtet" : "No chores set up", line1,
                          sizeof(line1));
    canvas_text_from_utf8(now->german ? "Einstellungen > Info-Screens" : "Settings > Info screens",
                          line2, sizeof(line2));
    canvas_fill(canvas, CANVAS_WHITE);
    canvas_text_centered(canvas, canvas->width / 2, canvas->height / 2 - canvas_text_height(s),
                         line1, s, CANVAS_BLACK);
    canvas_text_centered(canvas, canvas->width / 2, canvas->height / 2 + u, line2, s, CANVAS_BLACK);
}

// A pill with a name in it, at (x, y); returns its width.
static int draw_name_pill(canvas_t *canvas, int x, int y, const char *name, int scale,
                          canvas_color_t fill, int max_width)
{
    int u = canvas_unit(canvas);
    int height = canvas_text_height(scale) + u / 2;
    char text[CANVAS_WRAP_LINE_MAX];
    canvas_text_fit(name, max_width - u * 2, scale, text, sizeof(text));
    int width = canvas_text_width(text, scale) + u * 2;
    canvas_pill(canvas, x, y, width, height, fill);
    canvas_text(canvas, x + u, y + u / 4, text, scale, text_on(fill));
    return width;
}

static void draw_wheel(canvas_t *canvas, int cx, int cy, int outer, int s, const info_now_t *now,
                       const chore_config_t *config)
{
    int u = canvas_unit(canvas);
    int n = config->member_count;
    int inner = outer * 55 / 100;
    float slice = 360.0f / (float) n;
    // Turn the wheel so that the member of the first chore is centred at the top.
    int top_member = chore_assignee(0, now->iso_week, n);
    float rotation = -((float) top_member + 0.5f) * slice;

    for (int m = 0; m < n; m++) {
        float start = (float) m * slice + rotation;
        canvas_ring_sector(canvas, cx, cy, outer, inner, start, start + slice,
                           chore_member_color(m));
    }
    // thin white gaps between the sectors
    if (n > 1) {
        for (int m = 0; m < n; m++) {
            float a = ((float) m * slice + rotation) * 3.14159265f / 180.0f;
            int steps = outer - inner;
            for (int i = 0; i <= steps; i++) {
                int px = cx + (int) ((float) (inner + i) * sinf(a));
                int py = cy - (int) ((float) (inner + i) * cosf(a));
                canvas_rect(canvas, px - u / 6, py - u / 6, u / 3 + 1, u / 3 + 1, CANVAS_WHITE);
            }
        }
    }
    // the initials of the members in their sectors
    for (int m = 0; m < n; m++) {
        float mid = ((float) m + 0.5f) * slice + rotation;
        float rad = mid * 3.14159265f / 180.0f;
        int radius = (outer + inner) / 2;
        char name[CANVAS_WRAP_LINE_MAX], initials[3];
        display_name(config->members[m], name, sizeof(name));
        initials[0] = name[0];
        initials[1] = '\0';
        int text_scale = outer >= 150 ? s * 2 : s;
        int lx = cx + (int) ((float) radius * sinf(rad));
        int ly = cy - (int) ((float) radius * cosf(rad));
        canvas_text_centered(canvas, lx, ly - canvas_text_height(text_scale) / 2, initials,
                             text_scale, text_on(chore_member_color(m)));
    }
    // the pointer
    canvas_triangle_down(canvas, cx, cy - outer + u / 2, u * 2, CANVAS_BLACK);

    // the week in the middle
    char week[16], label[24];
    snprintf(week, sizeof(week), "%d", now->iso_week);
    canvas_text_from_utf8(now->german ? "WOCHE" : "WEEK", label, sizeof(label));
    int big = canvas_text_fit_scale(week, inner * 3 / 2, s * 3);
    int label_scale = s;
    int block = canvas_text_height(big) + canvas_text_height(label_scale) + u / 2;
    int top = cy - block / 2;
    canvas_text_centered(canvas, cx, top, week, big, CANVAS_BLACK);
    canvas_text_centered(canvas, cx, top + canvas_text_height(big) + u / 2, label, label_scale,
                         CANVAS_BLACK);
}

static void draw_card(canvas_t *canvas, int x, int y, int w, int h, int s, int task,
                      const info_now_t *now, const chore_config_t *config)
{
    int u = canvas_unit(canvas);
    int line = canvas_text_height(s);
    int pad = u / 2 + 1;
    int member = chore_assignee(task, now->iso_week, config->member_count);
    int next_member = chore_assignee(task, now->iso_week + 1, config->member_count);
    canvas_color_t color = chore_member_color(member);
    char name[CANVAS_WRAP_LINE_MAX], who[CANVAS_WRAP_LINE_MAX], next[CANVAS_WRAP_LINE_MAX];
    display_name(config->tasks[task], name, sizeof(name));
    display_name(config->members[member], who, sizeof(who));
    display_name(config->members[next_member], next, sizeof(next));

    canvas_rect(canvas, x, y, u, h, color);  // the bar in the colour of the member
    canvas_frame(canvas, x, y, w, h, 1 + u / 12, CANVAS_BLACK);  // a hairline around the card

    // the number
    int badge_r = line * 6 / 10;
    int bx = x + u + pad + badge_r;
    bool two_lines = h >= 2 * line + line / 2 + 2 * pad + u;
    bool three_lines = h >= 3 * line + 3 * pad + u;
    // beside the name in a one-line card, at the top of a taller one
    int by = two_lines ? y + pad + badge_r : y + h / 2;
    char number[16];
    snprintf(number, sizeof(number), "%d", task + 1);
    canvas_disc(canvas, bx, by, badge_r, CANVAS_BLACK);
    canvas_text_centered(canvas, bx, by - line / 2, number, s, CANVAS_WHITE);

    int text_x = bx + badge_r + pad;
    int text_w = x + w - pad - text_x;
    char fitted[CANVAS_WRAP_LINE_MAX];

    if (two_lines) {
        canvas_text_fit(name, text_w, s, fitted, sizeof(fitted));
        canvas_text(canvas, text_x, y + pad, fitted, s, CANVAS_BLACK);
        int pill_y = y + pad + line + pad / 2;
        int pill_w = draw_name_pill(canvas, text_x, pill_y, who, s, color, text_w);
        if (three_lines) {
            char line3[2 * CANVAS_WRAP_LINE_MAX + 2];  // a label and a name, never cut here
            char label[24];
            canvas_text_from_utf8(now->german ? "N\xC3\x84"
                                                "CHSTE WOCHE:"
                                              : "NEXT WEEK:",
                                  label, sizeof(label));
            snprintf(line3, sizeof(line3), "%s %s", label, next);
            canvas_text_fit(line3, text_w, s, fitted, sizeof(fitted));
            (void) pill_w;
            canvas_text(canvas, text_x, pill_y + line + u / 2 + pad / 2, fitted, s, CANVAS_BLACK);
        }
    } else {
        // one line: the name to the left, the member's pill to the right
        int pill_h = line + u / 2;
        int pill_w_max = text_w / 2;
        char pill_text[CANVAS_WRAP_LINE_MAX];
        canvas_text_fit(who, pill_w_max - 2 * u, s, pill_text, sizeof(pill_text));
        int pill_w = canvas_text_width(pill_text, s) + 2 * u;
        int pill_x = x + w - pad - pill_w;
        canvas_text_fit(name, pill_x - text_x - pad, s, fitted, sizeof(fitted));
        canvas_text(canvas, text_x, y + (h - line) / 2, fitted, s, CANVAS_BLACK);
        canvas_pill(canvas, pill_x, y + (h - pill_h) / 2, pill_w, pill_h, color);
        canvas_text(canvas, pill_x + u, y + (h - pill_h) / 2 + u / 4, pill_text, s, text_on(color));
    }
}

void chore_wheel_render(canvas_t *canvas, const info_now_t *now, const chore_config_t *config)
{
    if (config->member_count < 1 || config->task_count < 1) {
        draw_message(canvas, now);
        return;
    }
    int u = canvas_unit(canvas);
    canvas_fill(canvas, CANVAS_WHITE);
    bool landscape = canvas->width >= canvas->height;

    // Text scale of the cards. On a wide panel the cards should hold CARD_CHARS characters and the
    // wheel keeps at least 60 % of the room its height allows; if that does not work out, the text
    // gets smaller until it does.
    const int card_chars = 18;
    int s = canvas_text_scale(canvas, 1);
    int wheel_cx, wheel_cy, outer;
    int cards_x, cards_y, cards_w, cards_h;
    if (landscape) {
        int outer_max = canvas->height / 2 - 3 * u;
        for (;; s--) {
            int cards_needed = card_chars * 17 * s + 7 * u;
            outer = (canvas->width - cards_needed - 6 * u) / 2;
            if (outer > outer_max) {
                outer = outer_max;
            }
            if (outer * 10 >= outer_max * 6 || s <= 1) {
                break;
            }
        }
        wheel_cx = 2 * u + outer;
        wheel_cy = canvas->height / 2 + u;
        cards_x = wheel_cx + outer + 2 * u;
        cards_y = 2 * u;
        cards_w = canvas->width - cards_x - 2 * u;
        cards_h = canvas->height - 4 * u;
    } else {
        int wheel_area = canvas->height * 42 / 100;
        outer = (canvas->width < wheel_area ? canvas->width : wheel_area) / 2 - 3 * u;
        wheel_cx = canvas->width / 2;
        wheel_cy = wheel_area / 2 + u;
        cards_x = 2 * u;
        cards_y = wheel_area + u;
        cards_w = canvas->width - 4 * u;
        cards_h = canvas->height - cards_y - 2 * u;
    }

    draw_wheel(canvas, wheel_cx, wheel_cy, outer, s, now, config);

    // the heading above the cards
    char heading[CANVAS_WRAP_LINE_MAX], text[CANVAS_WRAP_LINE_MAX];
    snprintf(text, sizeof(text), "%s %d-W%02d", now->german ? "DIESE WOCHE" : "THIS WEEK",
             now->iso_year, now->iso_week);
    {
        char raw[CANVAS_WRAP_LINE_MAX];
        canvas_text_from_utf8(text, raw, sizeof(raw));
        canvas_text_fit(raw, cards_w, s, heading, sizeof(heading));
    }
    int heading_h = canvas_text_height(s);
    if (landscape) {
        canvas_text(canvas, cards_x, cards_y, heading, s, CANVAS_BLACK);
    } else {
        canvas_text_centered(canvas, canvas->width / 2, cards_y, heading, s, CANVAS_BLACK);
    }
    int list_y = cards_y + heading_h + u;
    int list_h = cards_y + cards_h - list_y;
    int gap = u / 2 + 1;
    int card_h = (list_h - gap * (config->task_count - 1)) / config->task_count;
    for (int t = 0; t < config->task_count; t++) {
        draw_card(canvas, cards_x, list_y + t * (card_h + gap), cards_w, card_h, s, t, now, config);
    }
}
