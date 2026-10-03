#include "recipe_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "config_manager.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "http_fetch.h"
#include "jpeg_decoder.h"

static const char *TAG = "recipe_service";

#define USER_AGENT "Mozilla/5.0 (compatible; esp32-photoframe recipes)"
#define HTTP_TIMEOUT_MS 15000

// ---------------------------------------------------------------------------------------------
// The environment of the engine
// ---------------------------------------------------------------------------------------------

// A GET that never logs its address (the search words and TheMealDB's key are in it). A body is
// returned only for a 200 answer.
static char *service_get(void *ctx, const char *url, size_t max_bytes, size_t *len, int *status)
{
    (void) ctx;
    char *body = NULL;
    size_t body_len = 0;
    int http_status = 0;
    esp_err_t err = http_fetch_get_once(url, HTTP_TIMEOUT_MS, max_bytes, &body, &body_len,
                                        &http_status, USER_AGENT);
    if (status) {
        *status = http_status;
    }
    if (err != ESP_OK || http_status != 200 || !body) {
        free(body);
        return NULL;
    }
    if (len) {
        *len = body_len;
    }
    return body;
}

// A JPEG into RGB888 in the PSRAM, scaled down by whole halves while it stays as big as the box.
static uint8_t *service_decode(void *ctx, const uint8_t *jpeg, size_t len, int box_w, int box_h,
                               int *w, int *h)
{
    (void) ctx;
    esp_jpeg_image_cfg_t cfg = {.indata = (uint8_t *) jpeg,
                                .indata_size = len,
                                .out_format = JPEG_IMAGE_FORMAT_RGB888,
                                .out_scale = JPEG_IMAGE_SCALE_0};
    esp_jpeg_image_output_t info;
    if (esp_jpeg_get_image_info(&cfg, &info) != ESP_OK || info.width < 1 || info.height < 1) {
        return NULL;
    }
    int shift = 0;
    int sw = info.width, sh = info.height;
    while (shift < 3 && sw / 2 >= box_w && sh / 2 >= box_h) {
        sw /= 2;
        sh /= 2;
        shift++;
    }
    static const esp_jpeg_image_scale_t SCALES[4] = {JPEG_IMAGE_SCALE_0, JPEG_IMAGE_SCALE_1_2,
                                                     JPEG_IMAGE_SCALE_1_4, JPEG_IMAGE_SCALE_1_8};
    cfg.out_scale = SCALES[shift];
    if (esp_jpeg_get_image_info(&cfg, &info) != ESP_OK) {
        return NULL;
    }
    uint8_t *rgb = heap_caps_malloc(info.output_len, MALLOC_CAP_SPIRAM);
    if (!rgb) {
        ESP_LOGW(TAG, "No memory for a %dx%d picture", info.width, info.height);
        return NULL;
    }
    cfg.outbuf = rgb;
    cfg.outbuf_size = info.output_len;
    if (esp_jpeg_decode(&cfg, &info) != ESP_OK) {
        heap_caps_free(rgb);
        return NULL;
    }
    *w = info.width;
    *h = info.height;
    return rgb;
}

static unsigned service_random(void *ctx)
{
    (void) ctx;
    return esp_random();
}

static unsigned long service_now_ms(void *ctx)
{
    (void) ctx;
    return (unsigned long) (esp_timer_get_time() / 1000);
}

// ---- the last recipe

#define LAST_MAGIC 0x31504352u  // "RCP1"

typedef struct {
    uint32_t magic;
    uint32_t size;  // sizeof(recipe_t): a file of another build is not read
} last_header_t;

static bool service_load_last(void *ctx, recipe_t *recipe, uint8_t **jpeg, size_t *jpeg_len)
{
    (void) ctx;
    FILE *file = fopen(RECIPE_LAST_PATH, "rb");
    if (!file) {
        return false;
    }
    last_header_t header;
    bool ok = fread(&header, sizeof(header), 1, file) == 1 && header.magic == LAST_MAGIC &&
              header.size == sizeof(recipe_t) && fread(recipe, sizeof(*recipe), 1, file) == 1;
    fclose(file);
    if (!ok) {
        return false;
    }
    // the text of a file is not trusted to end where it should
    recipe->title[sizeof(recipe->title) - 1] = '\0';
    recipe->text[sizeof(recipe->text) - 1] = '\0';
    recipe->url[sizeof(recipe->url) - 1] = '\0';
    recipe->image_url[sizeof(recipe->image_url) - 1] = '\0';
    if (recipe->ingredient_count < 0 || recipe->ingredient_count > RECIPE_INGREDIENTS_MAX) {
        recipe->ingredient_count = 0;
    }
    *jpeg = NULL;
    *jpeg_len = 0;
    FILE *photo = fopen(RECIPE_PHOTO_PATH, "rb");
    if (photo) {
        fseek(photo, 0, SEEK_END);
        long size = ftell(photo);
        fseek(photo, 0, SEEK_SET);
        if (size > 100 && size < 600 * 1024) {
            uint8_t *data = heap_caps_malloc((size_t) size, MALLOC_CAP_SPIRAM);
            if (data && fread(data, 1, (size_t) size, photo) == (size_t) size) {
                *jpeg = data;
                *jpeg_len = (size_t) size;
            } else {
                heap_caps_free(data);
            }
        }
        fclose(photo);
    }
    return true;
}

static void service_save_last(void *ctx, const recipe_t *recipe, const uint8_t *jpeg,
                              size_t jpeg_len)
{
    (void) ctx;
    FILE *file = fopen(RECIPE_LAST_PATH, "wb");
    if (!file) {
        return;
    }
    last_header_t header = {LAST_MAGIC, sizeof(recipe_t)};
    fwrite(&header, sizeof(header), 1, file);
    fwrite(recipe, sizeof(*recipe), 1, file);
    fclose(file);
    if (jpeg && jpeg_len > 0) {
        FILE *photo = fopen(RECIPE_PHOTO_PATH, "wb");
        if (photo) {
            fwrite(jpeg, 1, jpeg_len, photo);
            fclose(photo);
        }
    } else {
        remove(RECIPE_PHOTO_PATH);  // a recipe without a picture: no old picture stays with it
    }
}

// ---- the list of recipes shown lately

static void read_history(char *history, size_t cap)
{
    history[0] = '\0';
    FILE *file = fopen(RECIPE_SEEN_PATH, "rb");
    if (!file) {
        return;
    }
    size_t n = fread(history, 1, cap - 1, file);
    fclose(file);
    history[n] = '\0';
}

static void write_history(const char *history)
{
    FILE *file = fopen(RECIPE_SEEN_PATH, "wb");
    if (!file) {
        return;
    }
    fwrite(history, 1, strlen(history), file);
    fclose(file);
}

// ---------------------------------------------------------------------------------------------

recipe_result_t recipe_service_load(recipe_t *recipe, recipe_outcome_t *out,
                                    const recipe_options_t *options, bool wifi_connected, int width,
                                    int height, bool landscape)
{
    char history[RECIPE_HISTORY_CAP];
    char before[RECIPE_HISTORY_CAP];
    read_history(history, sizeof(history));
    snprintf(before, sizeof(before), "%s", history);

    recipe_env_t env = {
        .get = service_get,
        .decode = service_decode,
        .random = service_random,
        .now_ms = service_now_ms,
        .load_last = service_load_last,
        .save_last = service_save_last,
        .ctx = NULL,
        .mealdb_key = config_manager_get_recipe_mealdb_key(),
    };
    recipe_canvas_t canvas = {width, height, landscape};
    memset(out, 0, sizeof(*out));
    out->recipe = recipe;
    recipe_result_t result =
        recipe_engine_run(options, &canvas, &env, wifi_connected, history, sizeof(history), out);
    if (strcmp(history, before) != 0) {
        write_history(history);
    }
    ESP_LOGI(TAG, "Recipe: %s (%d tr%s, filters %s, %d request%s)",
             result == RECIPE_RESULT_NEW ? "a new one"
                                         : (result == RECIPE_RESULT_LAST ? "the last one" : "none"),
             out->tries, out->tries == 1 ? "y" : "ies", out->stage ? "relaxed" : "as set",
             out->requests, out->requests == 1 ? "" : "s");
    return result;
}
