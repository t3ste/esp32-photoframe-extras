#ifndef RENDER_SCREENS_EXTRA_H
#define RENDER_SCREENS_EXTRA_H

// The render cases of the info screens: a name and a function that draws that screen with sample
// data. Shared by the render harness (pictures) and the screen tests (checks).

#include <vector>

extern "C" {
#include "info_screens_core.h"
#include "screen_canvas.h"
#include "screen_chore_wheel.h"
}

struct RenderCase {
    const char *name;
    void (*draw)(canvas_t *canvas);
};

inline void draw_chore_wheel_english(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, false, &now);
    chore_config_t config;
    chore_config_parse(
        "Anna, Ben, Clara, Dave",
        "Take out the bins, Vacuum the living room, Water the plants, Wash the dishes", &config);
    chore_wheel_render(canvas, &now, &config);
}

inline void draw_chore_wheel_german(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 10, 7, true, &now);
    chore_config_t config;
    chore_config_parse("Anna, Bj\xC3\xB6rn, Cl\xC3\xA4ra, Dave, Eva",
                       "M\xC3\xBCll rausbringen, Wohnzimmer saugen, Blumen gie\xC3\x9F"
                       "en, "
                       "Geschirr sp\xC3\xBCle"
                       "n, Bad putzen, Einkaufen",
                       &config);
    chore_wheel_render(canvas, &now, &config);
}

inline void draw_chore_wheel_small(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 1, 1, false, &now);
    chore_config_t config;
    chore_config_parse("Sam, Alex", "Dishes, Trash", &config);
    chore_wheel_render(canvas, &now, &config);
}

inline void draw_chore_wheel_empty(canvas_t *canvas)
{
    info_now_t now;
    info_now_from_date(2026, 9, 30, true, &now);
    chore_config_t config;
    chore_config_parse("", "", &config);
    chore_wheel_render(canvas, &now, &config);
}

inline const std::vector<RenderCase> &render_cases()
{
    static const std::vector<RenderCase> cases = {
        {"chore-wheel-en", draw_chore_wheel_english},
        {"chore-wheel-de", draw_chore_wheel_german},
        {"chore-wheel-small", draw_chore_wheel_small},
        {"chore-wheel-empty", draw_chore_wheel_empty},
    };
    return cases;
}

#endif
