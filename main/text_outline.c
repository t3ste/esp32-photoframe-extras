#include "text_outline.h"

#include <string.h>

static const char *const STYLE_NAMES[TEXT_STYLE_COUNT] = {"bar", "white", "black"};

const char *text_style_name(text_style_t style)
{
    if ((int) style < 0 || (int) style >= TEXT_STYLE_COUNT) {
        return STYLE_NAMES[TEXT_STYLE_BAR];
    }
    return STYLE_NAMES[style];
}

int text_style_from_name(const char *name)
{
    if (!name) {
        return -1;
    }
    for (int i = 0; i < TEXT_STYLE_COUNT; i++) {
        if (strcmp(name, STYLE_NAMES[i]) == 0) {
            return i;
        }
    }
    return -1;
}

text_style_t text_style_clamp(int value)
{
    return (value >= 0 && value < TEXT_STYLE_COUNT) ? (text_style_t) value : TEXT_STYLE_BAR;
}

int text_border_clamp(int px)
{
    if (px < TEXT_BORDER_MIN) {
        return TEXT_BORDER_MIN;
    }
    return px > TEXT_BORDER_MAX ? TEXT_BORDER_MAX : px;
}

bool text_style_ink_white(text_style_t style)
{
    return style == TEXT_STYLE_WHITE;
}

int text_border_offsets(int border_px, int8_t offsets[][2], int max)
{
    int r = text_border_clamp(border_px);
    int count = 0;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if ((dx == 0 && dy == 0) || dx * dx + dy * dy > r * r + 1 || count >= max) {
                continue;
            }
            offsets[count][0] = (int8_t) dx;
            offsets[count][1] = (int8_t) dy;
            count++;
        }
    }
    return count;
}
