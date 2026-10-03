#include "recipe_qr.h"

#include <string.h>

#ifdef ESP_PLATFORM
#include "qrcode.h"
#else
#include "qrcodegen.h"
#endif

static void set_module(recipe_qr_t *qr, int x, int y, bool dark)
{
    if (dark) {
        int index = y * qr->size + x;
        qr->bits[index / 8] |= (uint8_t) (0x80 >> (index % 8));
    }
}

bool recipe_qr_module(const recipe_qr_t *qr, int x, int y)
{
    if (!qr || x < 0 || y < 0 || x >= qr->size || y >= qr->size) {
        return false;
    }
    int index = y * qr->size + x;
    return (qr->bits[index / 8] & (0x80 >> (index % 8))) != 0;
}

#ifdef ESP_PLATFORM

// esp_qrcode hands the finished code to a callback, which copies it into the caller's structure.
static recipe_qr_t *target;

static void copy_code(esp_qrcode_handle_t code)
{
    int size = esp_qrcode_get_size(code);
    if (!target || size <= 0 || size > RECIPE_QR_MAX_SIZE) {
        return;
    }
    target->size = size;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            set_module(target, x, y, esp_qrcode_get_module(code, x, y));
        }
    }
}

bool recipe_qr_encode(const char *text, recipe_qr_t *out)
{
    if (!out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (!text || !text[0]) {
        return false;
    }
    esp_qrcode_config_t config = {
        .display_func = copy_code,
        .max_qrcode_version = RECIPE_QR_MAX_VERSION,
        .qrcode_ecc_level = ESP_QRCODE_ECC_LOW,
    };
    target = out;
    esp_err_t err = esp_qrcode_generate(&config, text);
    target = NULL;
    if (err != ESP_OK || out->size == 0) {
        memset(out, 0, sizeof(*out));
        return false;
    }
    return true;
}

#else

bool recipe_qr_encode(const char *text, recipe_qr_t *out)
{
    if (!out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (!text || !text[0]) {
        return false;
    }
    uint8_t code[qrcodegen_BUFFER_LEN_FOR_VERSION(RECIPE_QR_MAX_VERSION)];
    uint8_t temp[qrcodegen_BUFFER_LEN_FOR_VERSION(RECIPE_QR_MAX_VERSION)];
    if (!qrcodegen_encodeText(text, temp, code, qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN,
                              RECIPE_QR_MAX_VERSION, qrcodegen_Mask_AUTO, true)) {
        return false;
    }
    int size = qrcodegen_getSize(code);
    if (size <= 0 || size > RECIPE_QR_MAX_SIZE) {
        return false;
    }
    out->size = size;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            set_module(out, x, y, qrcodegen_getModule(code, x, y));
        }
    }
    return true;
}

#endif
