#include "screen_fact.h"

#include <stdio.h>
#include <string.h>

#define FACT_MAX_LINES 16
#define FACT_MAX_TEXT_SCALE_STEPS 4  // the fact is drawn up to four times the body text size

// The date for the header: "Wed 30 Sep" / "Mi 30. Sep", as display text.
static void header_date(const info_now_t *now, char *out, size_t out_len)
{
    char weekday[16], month[24], month_short[8];
    canvas_text_from_utf8(info_weekday_short(now->wday, now->german), weekday, sizeof(weekday));
    canvas_text_from_utf8(info_month_name(now->month, now->german), month, sizeof(month));
    snprintf(month_short, sizeof(month_short), "%.3s", month);
    snprintf(out, out_len, "%s %d%s %s", weekday, now->day, now->german ? "." : "", month_short);
}

void fact_screen_render(canvas_t *canvas, const info_now_t *now, const fact_t *fact)
{
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    int line = canvas_text_height(s);
    int width = canvas->width;
    int height = canvas->height;
    canvas_fill(canvas, CANVAS_WHITE);

    // the header band
    int band_h = line + 2 * u;
    canvas_rect(canvas, 0, 0, width, band_h, CANVAS_RED);
    char heading[CANVAS_WRAP_LINE_MAX], date[CANVAS_WRAP_LINE_MAX], fitted[CANVAS_WRAP_LINE_MAX];
    canvas_text_from_utf8(now->german ? "FAKT DES TAGES" : "FACT OF THE DAY", heading,
                          sizeof(heading));
    header_date(now, date, sizeof(date));
    int date_w = canvas_text_width(date, s);
    canvas_text_fit(heading, width - 5 * u - date_w, s, fitted, sizeof(fitted));
    canvas_text(canvas, 2 * u, u, fitted, s, CANVAS_WHITE);
    canvas_text_right(canvas, width - 2 * u, u, date, s, CANVAS_WHITE);

    int y = band_h + u;

    // the topic
    if (fact->title[0] != '\0') {
        char title[FACT_TITLE_MAX * 2];
        canvas_text_from_utf8(fact->title, title, sizeof(title));
        char cut[CANVAS_WRAP_LINE_MAX];
        canvas_text_fit(title, width - 8 * u, s, cut, sizeof(cut));
        int pill_h = line + u;
        int pill_w = canvas_text_width(cut, s) + 2 * u;
        canvas_pill(canvas, 2 * u, y, pill_w, pill_h, CANVAS_BLUE);
        canvas_text(canvas, 3 * u, y + u / 2, cut, s, CANVAS_WHITE);
        y += pill_h + u;
    }

    // the question box at the bottom
    int box_x = 2 * u, box_w = width - 4 * u;
    int question_h = 0;
    char question_lines[2][CANVAS_WRAP_LINE_MAX];
    int question_count = 0;
    char label[CANVAS_WRAP_LINE_MAX];
    if (fact->question[0] != '\0') {
        char question[FACT_QUESTION_MAX * 2];
        canvas_text_from_utf8(fact->question, question, sizeof(question));
        question_count = canvas_text_wrap(question, box_w - 4 * u, s, question_lines, 2);
        canvas_text_from_utf8(now->german ? "\xC3\x9C"
                                            "BERLEGE MAL:"
                                          : "THINK ABOUT IT:",
                              label, sizeof(label));
        question_h = (1 + question_count) * line + 3 * u;
    }
    int body_bottom = height - 2 * u - (question_h > 0 ? question_h + u : 0);

    // the fact: the biggest text that fits between the topic and the question
    char text[FACT_TEXT_MAX * 2];
    canvas_text_from_utf8(fact->text, text, sizeof(text));
    int body_w = width - 6 * u;
    int body_h = body_bottom - y;
    char lines[FACT_MAX_LINES][CANVAS_WRAP_LINE_MAX];
    int count = 0;
    int scale = s;  // only used once a size fitted at least one line
    for (int steps = FACT_MAX_TEXT_SCALE_STEPS; steps >= 1; steps--) {
        int candidate = s * steps;
        int candidate_line = canvas_text_height(candidate);
        int room = body_h / (candidate_line + candidate_line / 4);
        if (room < 1) {
            continue;
        }
        int wanted = canvas_text_wrap(text, body_w, candidate, lines,
                                      room < FACT_MAX_LINES ? room : FACT_MAX_LINES);
        // does the whole text fit? (wrapping ends the last line with ~ if it did not)
        bool cut =
            wanted > 0 && lines[wanted - 1][strlen(lines[wanted - 1]) - 1] == '~' && steps > 1;
        count = wanted;
        scale = candidate;
        if (!cut) {
            break;
        }
    }
    if (count > 0) {
        int line_h = canvas_text_height(scale);
        int pitch = line_h + line_h / 4;
        int block_h = count * pitch - line_h / 4;
        int top = y + (body_h - block_h) / 2;
        for (int i = 0; i < count; i++) {
            canvas_text(canvas, 3 * u, top + i * pitch, lines[i], scale, CANVAS_BLACK);
        }
    }

    // the question
    if (question_h > 0) {
        int box_y = height - 2 * u - question_h;
        canvas_rect(canvas, box_x, box_y, box_w, question_h, CANVAS_YELLOW);
        canvas_frame(canvas, box_x, box_y, box_w, question_h, 1 + u / 12, CANVAS_BLACK);
        canvas_text(canvas, box_x + 2 * u, box_y + u, label, s, CANVAS_RED);
        for (int i = 0; i < question_count; i++) {
            canvas_text(canvas, box_x + 2 * u, box_y + u + (1 + i) * line + u / 2,
                        question_lines[i], s, CANVAS_BLACK);
        }
    }
}
