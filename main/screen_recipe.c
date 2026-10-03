#include "screen_recipe.h"

#include <stdlib.h>
#include <string.h>

#include "recipe_layout.h"
#include "recipe_qr.h"

// "No recipe" in the firmware's own font, like the other pages say that they have nothing to show.
static void message(canvas_t *canvas, const info_now_t *now, recipe_screen_status_t status)
{
    static const char *const en[][2] = {
        {"", ""},
        {"No network", "A recipe arrives once WiFi is up"},
        {"No recipe could be loaded", "Trying again next time"},
    };
    static const char *const de[][2] = {
        {"", ""},
        {"Kein Netz", "Ein Rezept kommt, sobald WLAN da ist"},
        {"Es konnte kein Rezept geladen werden",
         "Neuer Versuch beim n\xC3\xA4"
         "chsten Mal"},
    };
    int index = (int) status;
    if (index < RECIPE_SCREEN_NO_NETWORK || index > RECIPE_SCREEN_NO_RECIPE) {
        index = RECIPE_SCREEN_NO_RECIPE;
    }
    int u = canvas_unit(canvas);
    int s = canvas_text_scale(canvas, 1);
    char title[CANVAS_WRAP_LINE_MAX], raw[CANVAS_WRAP_LINE_MAX];
    canvas_text_from_utf8(now->german ? de[index][0] : en[index][0], raw, sizeof(raw));
    canvas_text_fit(raw, canvas->width - 4 * u, s, title, sizeof(title));
    char detail_text[CANVAS_WRAP_LINE_MAX], lines[3][CANVAS_WRAP_LINE_MAX];
    canvas_text_from_utf8(now->german ? de[index][1] : en[index][1], detail_text,
                          sizeof(detail_text));
    int line_count =
        detail_text[0] ? canvas_text_wrap(detail_text, canvas->width - 4 * u, s, lines, 3) : 0;

    canvas_fill(canvas, CANVAS_WHITE);
    int line_h = canvas_text_height(s);
    int block_h = line_h + (line_count > 0 ? u + line_count * line_h : 0);
    int y = canvas->height / 2 - block_h / 2;
    canvas_text_centered(canvas, canvas->width / 2, y, title, s, CANVAS_BLACK);
    for (int i = 0; i < line_count; i++) {
        canvas_text_centered(canvas, canvas->width / 2, y + line_h + u + i * line_h, lines[i], s,
                             CANVAS_BLACK);
    }
}

void recipe_screen_render(canvas_t *canvas, bool landscape, const info_now_t *now,
                          const recipe_screen_data_t *data)
{
    if (data->status != RECIPE_SCREEN_OK || !data->recipe) {
        message(canvas, now,
                data->status == RECIPE_SCREEN_OK ? RECIPE_SCREEN_NO_RECIPE : data->status);
        return;
    }
    recipe_qr_t *qr = NULL;
    if (data->options.qr && data->recipe->url[0]) {
        qr = malloc(sizeof(*qr));
        if (qr && !recipe_qr_encode(data->recipe->url, qr)) {
            free(qr);
            qr = NULL;
        }
    }
    recipe_layout_input_t input = {0};
    input.show_image = data->options.image;
    input.have_photo = data->photo != NULL;
    input.qr = qr;
    input.warnings = data->warnings;
    recipe_layout_t *layout = malloc(sizeof(*layout));
    if (!layout) {
        free(qr);
        message(canvas, now, RECIPE_SCREEN_NO_RECIPE);
        return;
    }
    recipe_layout_build(data->recipe, &input, canvas->width, canvas->height, landscape, layout);
    recipe_layout_draw(canvas, data->recipe, layout, qr, data->photo, data->photo_w, data->photo_h,
                       data->grayscale);
    free(layout);
    free(qr);
}
