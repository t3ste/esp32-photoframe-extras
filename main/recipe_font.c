#include "recipe_font.h"

const recipe_font_t *recipe_font_find(int px, bool bold)
{
    for (int i = 0; i < RECIPE_FONT_COUNT; i++) {
        if (RECIPE_FONTS[i].px == px && RECIPE_FONTS[i].bold == bold) {
            return &RECIPE_FONTS[i];
        }
    }
    return NULL;
}

int recipe_font_line_height(const recipe_font_t *font)
{
    return font ? font->ascent + font->descent : 0;
}

static const recipe_glyph_t *glyph_of(const recipe_font_t *font, uint8_t code)
{
    if (!font || code < RECIPE_FONT_FIRST) {
        return NULL;
    }
    return &font->glyphs[code - RECIPE_FONT_FIRST];
}

int recipe_font_char_width(const recipe_font_t *font, uint8_t code)
{
    const recipe_glyph_t *glyph = glyph_of(font, code);
    return glyph ? glyph->advance : 0;
}

int recipe_font_text_width(const recipe_font_t *font, const char *text, size_t len, int scale)
{
    if (scale < 1) {
        scale = 1;
    }
    int width = 0;
    for (size_t i = 0; text && i < len; i++) {
        width += recipe_font_char_width(font, (uint8_t) text[i]);
    }
    return width * scale;
}

int recipe_font_draw(canvas_t *canvas, const recipe_font_t *font, int scale, int x, int y,
                     const char *text, size_t len, canvas_color_t color)
{
    if (scale < 1) {
        scale = 1;
    }
    int baseline = y + font->ascent * scale;
    for (size_t i = 0; text && i < len; i++) {
        const recipe_glyph_t *glyph = glyph_of(font, (uint8_t) text[i]);
        if (!glyph) {
            continue;
        }
        if (glyph->width > 0) {
            int stride = (glyph->width + 7) / 8;
            const uint8_t *rows = font->bits + glyph->offset;
            int gx = x + glyph->left * scale;
            int gy = baseline - glyph->top * scale;
            for (int row = 0; row < glyph->height; row++) {
                int run = -1;  // the start of a run of set pixels in this row
                for (int col = 0; col <= glyph->width; col++) {
                    bool set = col < glyph->width &&
                               (rows[row * stride + col / 8] & (0x80 >> (col % 8))) != 0;
                    if (set && run < 0) {
                        run = col;
                    } else if (!set && run >= 0) {
                        canvas_rect(canvas, gx + run * scale, gy + row * scale, (col - run) * scale,
                                    scale, color);
                        run = -1;
                    }
                }
            }
        }
        x += glyph->advance * scale;
    }
    return x;
}
