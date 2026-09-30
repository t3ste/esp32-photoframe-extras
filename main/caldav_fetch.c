#include "caldav_fetch.h"

#include <stdbool.h>
#include <stdlib.h>

#include "caldav.h"
#include "esp_log.h"
#include "http_fetch.h"

static const char *TAG = "caldav_fetch";

esp_err_t caldav_report_text(const char *url, int timeout_ms, size_t max_bytes,
                             const char *first_body, const char *second_body, char **out_text,
                             size_t *out_len)
{
    *out_text = NULL;
    *out_len = 0;
    char resolved[CALDAV_URL_MAX_LEN];
    const char *http_url = caldav_resolve_url(url, resolved, sizeof(resolved));
    if (!http_url) {
        ESP_LOGW(TAG, "The CalDAV address is too long");
        return ESP_ERR_INVALID_ARG;
    }
    for (int attempt = 0; attempt < 2; attempt++) {
        const char *body = attempt == 0 ? first_body : second_body;
        if (!body) {
            break;
        }
        char *answer = NULL;
        size_t len = 0;
        bool truncated = false;
        int status = 0;
        esp_err_t err = http_fetch_report(http_url, timeout_ms, max_bytes, body, &answer, &len,
                                          &truncated, &status);
        if (err == ESP_OK) {
            if (truncated) {
                ESP_LOGW(TAG, "CalDAV answer cut at %u bytes - using what was captured",
                         (unsigned) max_bytes);
            }
            *out_text = answer;
            *out_len = caldav_extract_calendar_data(answer, len);
            return ESP_OK;
        }
        bool body_refused = (status == 400 || status == 415 || status == 422 || status == 501);
        if (!body_refused || attempt == 1 || !second_body) {
            ESP_LOGW(TAG, "CalDAV query failed: %s", esp_err_to_name(err));
            return err;
        }
        ESP_LOGW(TAG,
                 "The server did not take the query (HTTP %d) - asking again in a simpler form",
                 status);
    }
    return ESP_FAIL;
}
