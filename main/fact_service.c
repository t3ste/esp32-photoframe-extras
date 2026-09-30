#include "fact_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "fact_service";

#define FACT_PACK_TMP_PATH FS_MOUNT_POINT "/.facts.tmp"

esp_err_t fact_service_load(char **out_text, size_t *out_len)
{
    *out_text = NULL;
    if (out_len) {
        *out_len = 0;
    }
    struct stat st;
    if (stat(FACT_PACK_PATH, &st) != 0) {
        return ESP_ERR_NOT_FOUND;
    }
    if (st.st_size < 0 || st.st_size > FACT_PACK_MAX_BYTES) {
        return ESP_ERR_INVALID_SIZE;
    }
    FILE *f = fopen(FACT_PACK_PATH, "rb");
    if (!f) {
        return ESP_ERR_NOT_FOUND;
    }
    char *text = malloc((size_t) st.st_size + 1);
    if (!text) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    size_t got = fread(text, 1, (size_t) st.st_size, f);
    fclose(f);
    text[got] = '\0';
    *out_text = text;
    if (out_len) {
        *out_len = got;
    }
    return ESP_OK;
}

esp_err_t fact_service_store(const char *data, size_t len)
{
    if (len > FACT_PACK_MAX_BYTES) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (len == 0) {
        unlink(FACT_PACK_PATH);
        return ESP_OK;
    }
    FILE *f = fopen(FACT_PACK_TMP_PATH, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Cannot write the fact pack");
        return ESP_FAIL;
    }
    size_t written = fwrite(data, 1, len, f);
    int closed = fclose(f);
    if (written != len || closed != 0) {
        unlink(FACT_PACK_TMP_PATH);
        return ESP_FAIL;
    }
    unlink(FACT_PACK_PATH);  // FAT cannot rename over an existing file
    if (rename(FACT_PACK_TMP_PATH, FACT_PACK_PATH) != 0) {
        unlink(FACT_PACK_TMP_PATH);
        return ESP_FAIL;
    }
    return ESP_OK;
}

// The fact of the day from the user's pack; false if there is none or nothing usable in it.
static bool pick_from_pack(long day_number, fact_t *out)
{
    char *text = NULL;
    if (fact_service_load(&text, NULL) != ESP_OK) {
        return false;
    }
    // 64 facts of about 430 bytes: in PSRAM, not on the task's stack
    fact_t *facts = heap_caps_malloc(sizeof(fact_t) * FACT_MAX, MALLOC_CAP_SPIRAM);
    bool picked = false;
    if (facts) {
        int skipped = 0;
        int count = fact_pack_parse(text, facts, FACT_MAX, &skipped);
        if (count > 0) {
            int index = fact_pick_index(day_number, count);
            *out = facts[index];
            picked = true;
            ESP_LOGI(TAG, "Fact %d of %d from the user's pack (%d line(s) skipped)", index + 1,
                     count, skipped);
        } else {
            ESP_LOGW(TAG, "The fact pack has no usable line - using the built-in facts");
        }
    }
    heap_caps_free(facts);
    free(text);
    return picked;
}

void fact_service_pick(bool german, long day_number, fact_t *out, bool *from_pack)
{
    bool user = pick_from_pack(day_number, out);
    if (from_pack) {
        *from_pack = user;
    }
    if (user) {
        return;
    }
    int index = fact_pick_index(day_number, fact_builtin_count());
    const fact_t *fact = fact_builtin(index < 0 ? 0 : index, german);
    memset(out, 0, sizeof(*out));
    if (fact) {
        *out = *fact;
    }
}
