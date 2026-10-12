#define SD_READ_RETRY_NO_MACRO
#include "sd_read_retry.h"

#include <stddef.h>

size_t sd_read_with_retry(const sd_read_ops_t *ops, void *ctx, void *buf, size_t size, size_t count,
                          int *retried)
{
    if (retried) {
        *retried = 0;
    }
    long start = ops->tell(ctx);
    size_t got = ops->read(buf, size, count, ctx);
    if (got == count || !ops->error(ctx)) {
        return got;  // all of it, or a plain end of file
    }

    if (ops->on_failure) {
        ops->on_failure(ctx, size * count, size * got);
    }
    if (start < 0) {
        return got;  // no way back to where the read began
    }
    ops->clear(ctx);
    if (ops->pause) {
        ops->pause(ctx);
    }
    if (ops->seek(ctx, start) != 0) {
        return got;
    }
    if (retried) {
        *retried = 1;
    }
    got = ops->read(buf, size, count, ctx);
    if (ops->on_retry_result) {
        ops->on_retry_result(ctx, got == count || !ops->error(ctx));
    }
    return got;
}

#ifdef ESP_PLATFORM
#include <errno.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sd_read";

static size_t stdio_read(void *buf, size_t size, size_t count, void *ctx)
{
    return fread(buf, size, count, (FILE *) ctx);
}

static int stdio_error(void *ctx)
{
    return ferror((FILE *) ctx);
}

static void stdio_clear(void *ctx)
{
    clearerr((FILE *) ctx);
}

static long stdio_tell(void *ctx)
{
    return ftell((FILE *) ctx);
}

static int stdio_seek(void *ctx, long pos)
{
    return fseek((FILE *) ctx, pos, SEEK_SET);
}

static void log_failure(void *ctx, size_t wanted, size_t got)
{
    (void) ctx;
    ESP_LOGW(TAG,
             "SD read failed after %u of %u bytes (errno %d); internal heap %u free, lowest %u, "
             "largest DMA-capable block %u - trying once more",
             (unsigned) got, (unsigned) wanted, errno,
             (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
}

static void pause_a_moment(void *ctx)
{
    (void) ctx;
    vTaskDelay(pdMS_TO_TICKS(30));
}

static void log_retry_result(void *ctx, int recovered)
{
    (void) ctx;
    if (recovered) {
        ESP_LOGI(TAG, "The second try read it");
    } else {
        ESP_LOGE(TAG, "The second try failed too (errno %d)", errno);
    }
}

size_t fork_fread(void *buf, size_t size, size_t count, FILE *fp)
{
    static const sd_read_ops_t ops = {
        .read = stdio_read,
        .error = stdio_error,
        .clear = stdio_clear,
        .tell = stdio_tell,
        .seek = stdio_seek,
        .on_failure = log_failure,
        .pause = pause_a_moment,
        .on_retry_result = log_retry_result,
    };
    return sd_read_with_retry(&ops, fp, buf, size, count, NULL);
}
#endif  // ESP_PLATFORM
