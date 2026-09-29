// Link-satisfying stub: headlines.c references http_fetch_get() from
// headlines_fetch() (the network-calling wrapper), but the headlines host
// test only exercises headlines_extract() (the pure parsing logic) and
// never actually calls headlines_fetch() - this stub exists purely so the
// test binary links, not to be meaningfully invoked.
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "esp_err.h"

const char *esp_err_to_name(esp_err_t code)
{
    return code == ESP_OK ? "ESP_OK" : "ESP_ERR";
}

esp_err_t http_fetch_get(const char *url, int timeout_ms, size_t max_response_bytes,
                         char **out_body, size_t *out_len, bool *out_truncated,
                         const char *user_agent)
{
    (void) url;
    (void) timeout_ms;
    (void) max_response_bytes;
    (void) user_agent;
    *out_body = NULL;
    if (out_len) {
        *out_len = 0;
    }
    if (out_truncated) {
        *out_truncated = false;
    }
    return ESP_FAIL;
}

// Link-satisfying stub for todo.c/calendar_ics.c's conditional-GET (ETag)
// caching - same rationale as http_fetch_get() above, never actually invoked
// by the parser-only host tests.
esp_err_t http_fetch_get_conditional(const char *url, int timeout_ms, size_t max_response_bytes,
                                     const char *if_none_match, char **out_body, size_t *out_len,
                                     bool *out_truncated, char *out_etag, size_t out_etag_len,
                                     bool *out_not_modified, const char *user_agent)
{
    (void) url;
    (void) timeout_ms;
    (void) max_response_bytes;
    (void) if_none_match;
    (void) user_agent;
    *out_body = NULL;
    if (out_len) {
        *out_len = 0;
    }
    if (out_truncated) {
        *out_truncated = false;
    }
    if (out_etag && out_etag_len > 0) {
        out_etag[0] = '\0';
    }
    if (out_not_modified) {
        *out_not_modified = false;
    }
    return ESP_FAIL;
}

// Scriptable stand-in for http_fetch_report() (caldav feature): records what calendar_ics.c
// asked for and answers with the next canned status; a 207 returns fake_report.response as the
// body, anything else fails with that status, like the real function does for a 4xx.
#include <stdlib.h>
#include <string.h>

#include "fake_http_fetch.h"

fake_report_t fake_report;

void fake_report_reset(void)
{
    memset(&fake_report, 0, sizeof(fake_report));
}

esp_err_t http_fetch_report(const char *url, int timeout_ms, size_t max_response_bytes,
                            const char *request_body, char **out_body, size_t *out_len,
                            bool *out_truncated, int *out_status)
{
    (void) timeout_ms;
    (void) max_response_bytes;
    int idx =
        fake_report.calls < FAKE_REPORT_MAX_CALLS ? fake_report.calls : FAKE_REPORT_MAX_CALLS - 1;
    fake_report.calls++;
    snprintf(fake_report.url[idx], sizeof(fake_report.url[idx]), "%s", url);
    snprintf(fake_report.body[idx], sizeof(fake_report.body[idx]), "%s", request_body);
    int status = fake_report.status[idx] ? fake_report.status[idx] : 207;
    if (out_status) {
        *out_status = status;
    }
    *out_body = NULL;
    if (out_len) {
        *out_len = 0;
    }
    if (out_truncated) {
        *out_truncated = false;
    }
    if (status != 207) {
        return ESP_FAIL;
    }
    size_t n = strlen(fake_report.response);
    *out_body = (char *) malloc(n + 1);
    memcpy(*out_body, fake_report.response, n + 1);
    if (out_len) {
        *out_len = n;
    }
    return ESP_OK;
}
