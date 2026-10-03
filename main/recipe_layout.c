#include "recipe_layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------------------------
// Design values: pixels of an 800x480 panel (480x800 for the tall layout); scaled to the canvas
// ---------------------------------------------------------------------------------------------

#define ELLIPSIS ((char) 0x85)  // the ellipsis in the font's code

typedef struct {
    int dw, dh;         // the design canvas
    int width, height;  // the real one
    int u100;           // the uniform scale in percent: min(width / dw, height / dh)
    int scale;          // the font scale: that, rounded, at least 1
} geo_t;

static int sx(const geo_t *g, int v)
{
    return (v * g->width + g->dw / 2) / g->dw;
}

// A size that should keep its proportions (the picture, the code, margins, gaps): the uniform
// scale, so a 4:3 panel does not stretch them.
static int su(const geo_t *g, int v)
{
    int r = (v * g->u100 + 50) / 100;
    return r < 1 && v > 0 ? 1 : r;
}

// What belongs to the fonts moves in whole font scales
static int sf(const geo_t *g, int v)
{
    return v * g->scale;
}

// The ladder of sizes: ingredients and preparation, biggest first (a short recipe is shown in big
// letters). The preparation never goes under 12 pixels (at the font scale), which is the smallest
// that reads on the panel.
static const struct {
    int ing, body;
} STEPS[] = {{24, 22}, {22, 20}, {20, 19}, {19, 18}, {18, 17}, {17, 16},
             {16, 15}, {15, 14}, {14, 13}, {13, 12}, {12, 12}};
#define STEP_COUNT ((int) (sizeof(STEPS) / sizeof(STEPS[0])))

static const int TITLE_STEPS[] = {30, 24, 19};
#define TITLE_STEP_COUNT ((int) (sizeof(TITLE_STEPS) / sizeof(TITLE_STEPS[0])))

// ---------------------------------------------------------------------------------------------
// Words
// ---------------------------------------------------------------------------------------------

static int text_w(const recipe_font_t *font, int scale, const char *text, size_t len)
{
    return recipe_font_text_width(font, text, len, scale);
}

// The next line of text[pos..end) that fits `width`: it breaks at a space, and rather outside a
// bracket than inside ("(750 ml)" stays together if it can); a word wider than the line is broken
// between characters. Returns the length of the line (without the space it breaks at); `*next` is
// where the following line starts (after the space).
static size_t next_line(const recipe_font_t *font, int scale, const char *text, size_t pos,
                        size_t end, int width, size_t *next)
{
    int used = 0;
    int depth = 0;
    size_t last_space = (size_t) -1;        // the last space outside a bracket
    size_t last_inner_space = (size_t) -1;  // the last space inside one
    for (size_t i = pos; i < end; i++) {
        int advance = recipe_font_char_width(font, (uint8_t) text[i]) * scale;
        if (text[i] == ' ') {
            if (depth == 0) {
                last_space = i;
            } else {
                last_inner_space = i;
            }
        } else if (text[i] == '(') {
            depth++;
        } else if (text[i] == ')' && depth > 0) {
            depth--;
        }
        if (used + advance > width && i > pos) {
            if (text[i] == ' ' && depth == 0) {
                *next = i + 1;
                return i - pos;
            }
            if (last_space != (size_t) -1 && last_space > pos) {
                *next = last_space + 1;
                return last_space - pos;
            }
            if (last_inner_space != (size_t) -1 && last_inner_space > pos) {
                *next = last_inner_space + 1;
                return last_inner_space - pos;
            }
            *next = i;  // one word wider than the line
            return i - pos;
        }
        used += advance;
    }
    *next = end;
    return end - pos;
}

// Cuts a line so that it and an ellipsis fit `width`, drops the dangling punctuation, and marks it.
static void end_with_ellipsis(recipe_line_t *line, const recipe_font_t *font, int scale,
                              const char *text)
{
    int width = line->width;
    int dots = recipe_font_char_width(font, (uint8_t) ELLIPSIS) * scale;
    while (line->len > 0 && (text_w(font, scale, text + line->start, line->len) + dots > width ||
                             strchr(" .,;:-", text[line->start + line->len - 1]))) {
        line->len--;
    }
    line->flags |= RECIPE_LINE_ELLIPSIS;
}

// ---------------------------------------------------------------------------------------------
// The texts that depend on the language
// ---------------------------------------------------------------------------------------------

static const char *warning_text(unsigned flag, bool german)
{
    switch (flag) {
    case RECIPE_WARN_LAST_RECIPE:
        return german ? "Letztes Rezept" : "Last recipe";
    case RECIPE_WARN_RELAXED:
        return german ? "Filter gelockert" : "Filters relaxed";
    case RECIPE_WARN_NO_IMAGE:
        return german ? "Kein Bild" : "No image";
    case RECIPE_WARN_NO_NETWORK:
        return "Offline";  // the same word in both languages
    default:
        return "";
    }
}

static const char *text_cut_warning(bool german)
{
    return german ? "Text gek\xFCrzt" : "Text cut";  // gekuerzt, with the umlaut of the code
}

// ---------------------------------------------------------------------------------------------
// The layout
// ---------------------------------------------------------------------------------------------

static void trim_to_width(char *text, const recipe_font_t *font, int scale, int width)
{
    size_t len = strlen(text);
    if (text_w(font, scale, text, len) <= width) {
        return;
    }
    int dots = recipe_font_char_width(font, (uint8_t) ELLIPSIS) * scale;
    while (len > 0 && text_w(font, scale, text, len) + dots > width) {
        len--;
    }
    while (len > 0 && text[len - 1] == ' ') {
        len--;
    }
    text[len] = ELLIPSIS;
    text[len + 1] = '\0';
}

static bool try_layout(const recipe_t *r, const recipe_layout_input_t *in, const geo_t *g,
                       bool landscape, int step, recipe_layout_t *out)
{
    memset(out, 0, sizeof(*out));
    out->width = g->width;
    out->height = g->height;
    out->landscape = landscape;
    out->scale = g->scale;
    int scale = g->scale;
    out->ing_px = STEPS[step].ing * scale;
    out->body_px = STEPS[step].body * scale;
    out->ing_line_h = (STEPS[step].ing + 4) * scale;
    out->body_line_h = (STEPS[step].body + 3) * scale;
    out->meta_font = recipe_font_find(15, false);
    out->head_font = recipe_font_find(20, true);
    out->ing_font = recipe_font_find(STEPS[step].ing, false);
    out->body_font = recipe_font_find(STEPS[step].body, false);
    out->small_font = recipe_font_find(12, false);
    out->warn_font = recipe_font_find(12, true);
    out->ing_label = r->german ? "ZUTATEN" : "INGREDIENTS";
    out->body_label = r->german ? "ZUBEREITUNG" : "PREPARATION";
    if (!out->meta_font || !out->head_font || !out->ing_font || !out->body_font ||
        !out->small_font || !out->warn_font) {
        return false;
    }
    int margin_l = landscape ? sx(g, 30) : sx(g, 24);
    int margin_r = landscape ? sx(g, 18) : sx(g, 20);
    int bottom = g->height - su(g, 10);

    // ---- the corner of the warnings: the title keeps clear of the widest warning there can be
    unsigned flags[] = {RECIPE_WARN_LAST_RECIPE, RECIPE_WARN_RELAXED, RECIPE_WARN_NO_IMAGE,
                        RECIPE_WARN_NO_NETWORK};
    int zone = 0;
    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
        char probe[48];
        snprintf(probe, sizeof(probe), "! %s", warning_text(flags[i], r->german));
        int w = text_w(out->warn_font, scale, probe, strlen(probe));
        if (w > zone) {
            zone = w;
        }
    }
    {
        char probe[48];
        snprintf(probe, sizeof(probe), "! %s", text_cut_warning(r->german));
        int w = text_w(out->warn_font, scale, probe, strlen(probe));
        if (w > zone) {
            zone = w;
        }
    }
    out->warn_right = g->width - margin_r;

    // ---- the title: the biggest size that fits one line, else two lines of the smallest
    size_t title_len = strlen(r->title);
    int title_width = landscape ? g->width - margin_l - margin_r - zone - sx(g, 8)
                                : g->width - margin_l - margin_r;
    out->title_font = recipe_font_find(TITLE_STEPS[TITLE_STEP_COUNT - 1], false);
    for (int i = 0; i < TITLE_STEP_COUNT; i++) {
        const recipe_font_t *font = recipe_font_find(TITLE_STEPS[i], false);
        if (font && text_w(font, scale, r->title, title_len) <= title_width) {
            out->title_font = font;
            break;
        }
    }
    {
        size_t pos = 0;
        while (pos < title_len && out->title_count < 2) {
            size_t next;
            size_t len =
                next_line(out->title_font, scale, r->title, pos, title_len, title_width, &next);
            recipe_line_t *line = &out->title[out->title_count++];
            line->start = (uint16_t) pos;
            line->len = (uint16_t) len;
            line->width = (int16_t) title_width;
            pos = next;
            while (pos < title_len && r->title[pos] == ' ') {
                pos++;
            }
            if (out->title_count == 2 && pos < title_len) {
                end_with_ellipsis(line, out->title_font, scale, r->title);
            }
        }
        if (out->title_count == 0) {
            out->title_count = 1;
        }
    }

    // ---- the header's rows
    int title_line_h = (out->title_font->px + 2) * scale;
    int title_block = title_line_h * out->title_count;
    if (title_block < sf(g, 32)) {
        title_block = sf(g, 32);
    }
    int title_y;
    if (landscape) {
        title_y = sf(g, 2);
        out->warn_y[0] = sf(g, 8);
    } else {
        // the source (left) and the warnings (right) have two rows of their own above the title
        out->warn_y[0] = sf(g, 6);
        title_y = sf(g, 6) + sf(g, 28);
    }
    out->warn_y[1] = out->warn_y[0] + sf(g, 13);
    for (int i = 0; i < out->title_count; i++) {
        out->title[i].x = (int16_t) margin_l;
        out->title[i].y = (int16_t) (title_y + i * title_line_h);
    }
    out->meta_x = margin_l;
    out->meta_y = title_y + title_block;
    {
        const char *category = r->category[0] ? r->category : (r->german ? "Rezept" : "Recipe");
        if (r->time[0]) {
            snprintf(out->meta, sizeof(out->meta), "%s | %s", category, r->time);
        } else {
            snprintf(out->meta, sizeof(out->meta), "%s", category);
        }
        // a long path of categories ends with an ellipsis rather than leave the page
        trim_to_width(out->meta, out->meta_font, scale, g->width - margin_l - margin_r);
    }
    snprintf(out->source, sizeof(out->source), "%s: %s", r->german ? "Quelle" : "Source",
             r->source);
    out->source_right = g->width - margin_r;
    if (landscape) {
        out->source_y = out->meta_y + sf(g, 4);
        int meta_end = margin_l + text_w(out->meta_font, scale, out->meta, strlen(out->meta));
        trim_to_width(out->source, out->small_font, scale,
                      out->source_right - meta_end - sx(g, 12));
    } else {
        out->source_y = out->warn_y[0];
        trim_to_width(out->source, out->small_font, scale,
                      g->width - margin_l - margin_r - zone - sx(g, 12));
    }
    out->rule_y = out->meta_y + sf(g, 24);
    out->rule_h = sf(g, 2);
    int heading_y = out->rule_y + sf(g, 6);
    int content_y = heading_y + sf(g, 26);

    // ---- the columns, the picture and the code
    int ing_x = margin_l;
    int left_w;
    int body_x;
    int image_w = su(g, 176), image_h = su(g, 118);
    if (in->show_image) {
        out->image.w = image_w;
        out->image.h = image_h;
        out->image.x = g->width - margin_r - image_w;
        out->image.y = heading_y;
        out->image_placeholder = !in->have_photo;
        out->mat = su(g, 3);
    }
    if (in->qr && in->qr->size > 0) {
        int module = su(g, 68) / in->qr->size;
        if (module < 1) {
            module = 1;
        }
        out->qr_module_px = module;
        int side = module * (in->qr->size + 2);  // with a module of quiet zone all round
        out->qr.w = out->qr.h = side;
        out->qr.x = g->width - margin_r - side;
        out->qr.y = bottom - side;
    }
    if (landscape) {
        left_w = (int) ((long) (g->width - sx(g, 48)) * 295 / 1000);
        body_x = ing_x + left_w + sx(g, 6);
    } else {
        left_w = in->show_image ? out->image.x - sx(g, 8) - ing_x : g->width - margin_l - margin_r;
        body_x = margin_l;
    }
    out->ing_head_x = ing_x;
    out->ing_head_y = heading_y;

    // ---- the ingredients
    int bullet = sf(g, 8);
    out->bullet_size = bullet;
    int indent = sf(g, 14);
    int y = content_y;
    for (int i = 0; i < r->ingredient_count; i++) {
        const char *text = r->ingredients[i];
        size_t len = strlen(text);
        if (len == 0) {
            continue;
        }
        // how many lines this one needs
        recipe_line_t lines[8];
        int count = 0;
        size_t pos = 0;
        while (pos < len && count < 8) {
            size_t next;
            size_t n = next_line(out->ing_font, scale, text, pos, len, left_w - indent, &next);
            lines[count].x = (int16_t) (ing_x + indent);
            lines[count].width = (int16_t) (left_w - indent);
            lines[count].start = (uint16_t) pos;
            lines[count].len = (uint16_t) n;
            lines[count].ingredient = (uint8_t) i;
            lines[count].flags = count == 0 ? RECIPE_LINE_BULLET : 0;
            count++;
            pos = next;
        }
        if (y + count * out->ing_line_h > bottom ||
            out->ing_count + count > RECIPE_LAYOUT_ING_LINES) {
            out->ing_cut = true;
            if (out->ing_count > 0) {
                end_with_ellipsis(&out->ing[out->ing_count - 1], out->ing_font, scale,
                                  r->ingredients[out->ing[out->ing_count - 1].ingredient]);
            }
            break;
        }
        for (int k = 0; k < count; k++) {
            lines[k].y = (int16_t) (y + k * out->ing_line_h);
            out->ing[out->ing_count++] = lines[k];
        }
        y += count * out->ing_line_h + sf(g, 4);
    }
    int ing_end = y;

    // ---- the preparation
    int body_y;
    if (landscape) {
        out->body_head_x = body_x;
        out->body_head_y = heading_y;
        body_y = content_y;
    } else {
        int below = ing_end;
        if (in->show_image && out->image.y + out->image.h + su(g, 8) > below) {
            below = out->image.y + out->image.h + su(g, 8);
        }
        out->body_head_x = margin_l;
        out->body_head_y = below + sf(g, 4);
        body_y = out->body_head_y + sf(g, 26);
    }
    int right_edge = g->width - margin_r;
    const char *text = r->text;
    size_t text_len = strlen(text);
    size_t pos = 0;
    y = body_y;
    while (pos < text_len && !out->body_cut) {
        // one paragraph: up to the next line break
        const char *nl = memchr(text + pos, '\n', text_len - pos);
        size_t end = nl ? (size_t) (nl - text) : text_len;
        while (pos < end) {
            while (pos < end && text[pos] == ' ') {
                pos++;
            }
            if (pos >= end) {
                break;
            }
            // the width of the line at this height: the picture and the code are in the way
            int width = right_edge - body_x;
            if (in->show_image && landscape && y < out->image.y + out->image.h + su(g, 4)) {
                width = out->image.x - body_x - sx(g, 8);
            }
            if (out->qr.w > 0 && y + out->body_line_h > out->qr.y - su(g, 4)) {
                int narrow = out->qr.x - body_x - sx(g, 10);
                if (narrow < width) {
                    width = narrow;
                }
            }
            if (y + out->body_line_h > bottom || out->body_count >= RECIPE_LAYOUT_BODY_LINES) {
                out->body_cut = true;
                if (out->body_count > 0) {
                    end_with_ellipsis(&out->body[out->body_count - 1], out->body_font, scale, text);
                }
                break;
            }
            size_t next;
            size_t len = next_line(out->body_font, scale, text, pos, end, width, &next);
            recipe_line_t *line = &out->body[out->body_count++];
            line->x = (int16_t) body_x;
            line->y = (int16_t) y;
            line->width = (int16_t) width;
            line->start = (uint16_t) pos;
            line->len = (uint16_t) len;
            y += out->body_line_h;
            pos = next;
        }
        if (out->body_cut) {
            break;
        }
        pos = nl ? end + 1 : text_len;
        y += sf(g, 5);
    }
    if (r->text_cut && !out->body_cut && out->body_count > 0) {
        out->body_cut = true;  // the source had more than the recipe holds
        end_with_ellipsis(&out->body[out->body_count - 1], out->body_font, scale, text);
    }
    out->cut = out->ing_cut || out->body_cut;

    // ---- the warnings in the corner: cut text first, then what the input asks for; two lines
    if (out->cut) {
        snprintf(out->warn[out->warn_count++], sizeof(out->warn[0]), "! %s",
                 text_cut_warning(r->german));
    }
    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]) && out->warn_count < 2; i++) {
        if (in->warnings & flags[i]) {
            snprintf(out->warn[out->warn_count++], sizeof(out->warn[0]), "! %s",
                     warning_text(flags[i], r->german));
        }
    }
    return !out->cut;
}

bool recipe_layout_build(const recipe_t *recipe, const recipe_layout_input_t *input, int width,
                         int height, bool landscape, recipe_layout_t *out)
{
    if (out) {
        memset(out, 0, sizeof(*out));  // whatever happens below, the callers get a defined layout
    }
    if (!recipe || !input || !out || width < 100 || height < 100) {
        return false;
    }
    geo_t g;
    g.dw = landscape ? 800 : 480;
    g.dh = landscape ? 480 : 800;
    g.width = width;
    g.height = height;
    int ux = width * 100 / g.dw;
    int uy = height * 100 / g.dh;
    g.u100 = ux < uy ? ux : uy;
    g.scale = (g.u100 + 50) / 100;
    if (g.scale < 1) {
        g.scale = 1;
    }
    for (int step = 0; step < STEP_COUNT; step++) {
        if (try_layout(recipe, input, &g, landscape, step, out)) {
            return true;
        }
    }
    return false;  // `out` holds the smallest size, with the cut it needed
}

// ---------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------

static void draw_text(canvas_t *canvas, const recipe_font_t *font, int scale, int x, int y,
                      const char *text, size_t len, canvas_color_t color, bool ellipsis)
{
    int end = recipe_font_draw(canvas, font, scale, x, y, text, len, color);
    if (ellipsis) {
        char dots = ELLIPSIS;
        recipe_font_draw(canvas, font, scale, end, y, &dots, 1, color);
    }
}

void recipe_layout_draw(canvas_t *canvas, const recipe_t *recipe, const recipe_layout_t *l,
                        const recipe_qr_t *qr, const uint8_t *photo, int photo_w, int photo_h,
                        bool grayscale)
{
    if (!canvas || !recipe || !l) {
        return;
    }
    canvas_fill(canvas, CANVAS_WHITE);
    if (l->scale < 1 || !l->title_font || !l->meta_font || !l->head_font || !l->ing_font ||
        !l->body_font || !l->small_font || !l->warn_font) {
        return;  // a layout that was not built (a panel under 100 pixels): a white page
    }
    int scale = l->scale;

    // the header
    for (int i = 0; i < l->title_count; i++) {
        draw_text(canvas, l->title_font, scale, l->title[i].x, l->title[i].y,
                  recipe->title + l->title[i].start, l->title[i].len, CANVAS_RED,
                  (l->title[i].flags & RECIPE_LINE_ELLIPSIS) != 0);
    }
    draw_text(canvas, l->meta_font, scale, l->meta_x, l->meta_y, l->meta, strlen(l->meta),
              CANVAS_BLUE, false);
    {
        int w = recipe_font_text_width(l->small_font, l->source, strlen(l->source), scale);
        int x = l->landscape ? l->source_right - w : l->meta_x;
        draw_text(canvas, l->small_font, scale, x, l->source_y, l->source, strlen(l->source),
                  CANVAS_BLACK, false);
    }
    for (int i = 0; i < l->warn_count; i++) {
        int w = recipe_font_text_width(l->warn_font, l->warn[i], strlen(l->warn[i]), scale);
        draw_text(canvas, l->warn_font, scale, l->warn_right - w, l->warn_y[i], l->warn[i],
                  strlen(l->warn[i]), CANVAS_RED, false);
    }
    canvas_rect(canvas, 0, l->rule_y, canvas->width, l->rule_h, CANVAS_YELLOW);

    // the headings
    draw_text(canvas, l->head_font, scale, l->ing_head_x, l->ing_head_y, l->ing_label,
              strlen(l->ing_label), CANVAS_BLACK, false);
    draw_text(canvas, l->head_font, scale, l->body_head_x, l->body_head_y, l->body_label,
              strlen(l->body_label), CANVAS_BLACK, false);

    // the ingredients
    for (int i = 0; i < l->ing_count; i++) {
        const recipe_line_t *line = &l->ing[i];
        const char *text = recipe->ingredients[line->ingredient];
        if (line->flags & RECIPE_LINE_BULLET) {
            int center =
                line->y + l->ing_font->ascent * scale - (l->ing_font->px * scale * 35) / 100;
            canvas_rect(canvas, line->x - (l->bullet_size + scale * 6), center - l->bullet_size / 2,
                        l->bullet_size, l->bullet_size, CANVAS_RED);
        }
        draw_text(canvas, l->ing_font, scale, line->x, line->y, text + line->start, line->len,
                  CANVAS_BLACK, (line->flags & RECIPE_LINE_ELLIPSIS) != 0);
    }

    // the preparation
    for (int i = 0; i < l->body_count; i++) {
        const recipe_line_t *line = &l->body[i];
        draw_text(canvas, l->body_font, scale, line->x, line->y, recipe->text + line->start,
                  line->len, CANVAS_BLACK, (line->flags & RECIPE_LINE_ELLIPSIS) != 0);
    }

    // the picture
    if (l->image.w > 0) {
        int t = l->mat > 2 ? l->mat / 3 : 1;
        canvas_frame(canvas, l->image.x, l->image.y, l->image.w, l->image.h, t, CANVAS_BLACK);
        int inset = t + l->mat;
        int ix = l->image.x + inset, iy = l->image.y + inset;
        int iw = l->image.w - 2 * inset, ih = l->image.h - 2 * inset;
        if (photo && !l->image_placeholder) {
            recipe_draw_photo(canvas, ix, iy, iw, ih, photo, photo_w, photo_h, grayscale);
        } else {
            const char *note = recipe->german ? "Kein Bild" : "No image";
            int w = recipe_font_text_width(l->small_font, note, strlen(note), scale);
            draw_text(canvas, l->small_font, scale, ix + (iw - w) / 2,
                      iy + (ih - l->small_font->px * scale) / 2, note, strlen(note), CANVAS_BLACK,
                      false);
        }
    }

    // the QR code, on white
    if (l->qr.w > 0 && qr && qr->size > 0) {
        canvas_rect(canvas, l->qr.x, l->qr.y, l->qr.w, l->qr.h, CANVAS_WHITE);
        for (int y = 0; y < qr->size; y++) {
            for (int x = 0; x < qr->size; x++) {
                if (recipe_qr_module(qr, x, y)) {
                    canvas_rect(canvas, l->qr.x + (x + 1) * l->qr_module_px,
                                l->qr.y + (y + 1) * l->qr_module_px, l->qr_module_px,
                                l->qr_module_px, CANVAS_BLACK);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------
// The picture
// ---------------------------------------------------------------------------------------------

typedef struct {
    int r, g, b;
} rgb_t;

static const rgb_t PALETTE[6] = {{0, 0, 0},     {255, 255, 255}, {255, 0, 0},
                                 {255, 255, 0}, {0, 255, 0},     {0, 0, 255}};

static int clamp255(int v)
{
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}

void recipe_draw_photo(canvas_t *canvas, int x, int y, int w, int h, const uint8_t *rgb, int src_w,
                       int src_h, bool grayscale)
{
    if (!canvas || !rgb || w < 1 || h < 1 || src_w < 1 || src_h < 1) {
        return;
    }
    // the area of the source that fills the box: cropped at the middle, in 16.16 fixed point
    long scale_x = ((long) src_w << 16) / w;
    long scale_y = ((long) src_h << 16) / h;
    long step = scale_x < scale_y ? scale_x : scale_y;  // source pixels per box pixel (16.16)
    long crop_w = step * w;
    long crop_h = step * h;
    long origin_x = (((long) src_w << 16) - crop_w) / 2;
    long origin_y = (((long) src_h << 16) - crop_h) / 2;

    // the error of the pixels to come: this row and the next, three channels (Floyd-Steinberg)
    int *err = calloc((size_t) (w + 2) * 2 * 3, sizeof(int));
    if (!err) {
        return;
    }
    int *cur = err;
    int *nxt = err + (size_t) (w + 2) * 3;
    for (int row = 0; row < h; row++) {
        long sy0 = origin_y + step * row;
        long sy1 = sy0 + step;
        int y0 = (int) (sy0 >> 16);
        int y1 = (int) ((sy1 - 1) >> 16);
        if (y1 >= src_h) {
            y1 = src_h - 1;
        }
        for (int col = 0; col < w; col++) {
            long sx0 = origin_x + step * col;
            long sx1 = sx0 + step;
            int x0 = (int) (sx0 >> 16);
            int x1 = (int) ((sx1 - 1) >> 16);
            if (x1 >= src_w) {
                x1 = src_w - 1;
            }
            // the average of the source pixels this one covers
            long sum_r = 0, sum_g = 0, sum_b = 0;
            int n = 0;
            for (int yy = y0; yy <= y1; yy++) {
                for (int xx = x0; xx <= x1; xx++) {
                    const uint8_t *p = rgb + ((size_t) yy * src_w + xx) * 3;
                    sum_r += p[0];
                    sum_g += p[1];
                    sum_b += p[2];
                    n++;
                }
            }
            if (n == 0) {
                n = 1;
            }
            int avg_r = (int) (sum_r / n), avg_g = (int) (sum_g / n), avg_b = (int) (sum_b / n);
            rgb_t out;
            if (grayscale) {
                int luma = clamp255((avg_r * 299 + avg_g * 587 + avg_b * 114) / 1000 +
                                    cur[(col + 1) * 3 + 0]);
                int value = ((luma + 8) / 17) * 17;  // sixteen grays: 0 17 34 ... 255
                out.r = out.g = out.b = value;
                int e = luma - value;
                cur[(col + 2) * 3 + 0] += e * 7 / 16;
                nxt[(col + 0) * 3 + 0] += e * 3 / 16;
                nxt[(col + 1) * 3 + 0] += e * 5 / 16;
                nxt[(col + 2) * 3 + 0] += e * 1 / 16;
                canvas_rect(canvas, x + col, y + row, 1, 1,
                            (canvas_color_t){(uint8_t) out.r, (uint8_t) out.g, (uint8_t) out.b});
                continue;
            }
            int r = clamp255(avg_r + cur[(col + 1) * 3 + 0]);
            int gr = clamp255(avg_g + cur[(col + 1) * 3 + 1]);
            int b = clamp255(avg_b + cur[(col + 1) * 3 + 2]);
            int best = 0;
            long best_d = -1;
            for (int k = 0; k < 6; k++) {
                long dr = r - PALETTE[k].r, dg = gr - PALETTE[k].g, db = b - PALETTE[k].b;
                long d = dr * dr + dg * dg + db * db;
                if (best_d < 0 || d < best_d) {
                    best_d = d;
                    best = k;
                }
            }
            out = PALETTE[best];
            int er = r - out.r, eg = gr - out.g, eb = b - out.b;
            int *targets[4] = {&cur[(col + 2) * 3], &nxt[(col + 0) * 3], &nxt[(col + 1) * 3],
                               &nxt[(col + 2) * 3]};
            static const int weight[4] = {7, 3, 5, 1};
            for (int t = 0; t < 4; t++) {
                targets[t][0] += er * weight[t] / 16;
                targets[t][1] += eg * weight[t] / 16;
                targets[t][2] += eb * weight[t] / 16;
            }
            canvas_rect(canvas, x + col, y + row, 1, 1,
                        (canvas_color_t){(uint8_t) out.r, (uint8_t) out.g, (uint8_t) out.b});
        }
        // the next row becomes the current one
        int *swap = cur;
        cur = nxt;
        nxt = swap;
        memset(nxt, 0, (size_t) (w + 2) * 3 * sizeof(int));
    }
    free(err);
}
