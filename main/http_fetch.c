#include "http_fetch.h"

#include <stdlib.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "feature_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if FEATURE_SOURCE_AUTH
#include "config_manager.h"
#include "source_auth.h"
#endif

static const char *TAG = "http_fetch";

// Mirrors telegram_bot.c's TELEGRAM_HTTP_RETRY_COUNT/_DELAY_MS - transient
// TLS/network hiccups are common enough on ESP32 to warrant a couple of
// quick retries rather than giving up on the first blip.
#define HTTP_FETCH_RETRY_COUNT 3
#define HTTP_FETCH_RETRY_DELAY_MS 1500

// A CalDAV REPORT in place of the GET (http_fetch_report(), build option caldav).
typedef struct {
    const char *body;  // request body (XML)
    int status;        // out: the last HTTP status seen, 0 if the server never answered
} http_report_t;

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    size_t max_len;
    bool overflow;
    char *etag_out;  // NULL if the caller doesn't want the ETag captured
    size_t etag_out_len;
} http_body_buf_t;

static esp_err_t body_capture_handler(esp_http_client_event_t *evt)
{
    http_body_buf_t *ctx = (http_body_buf_t *) evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_HEADER) {
        if (ctx->etag_out && ctx->etag_out_len > 0 && strcasecmp(evt->header_key, "ETag") == 0) {
            strncpy(ctx->etag_out, evt->header_value, ctx->etag_out_len - 1);
            ctx->etag_out[ctx->etag_out_len - 1] = '\0';
        }
        return ESP_OK;
    }

    if (evt->event_id != HTTP_EVENT_ON_DATA) {
        return ESP_OK;
    }
#if FEATURE_SOURCE_AUTH
    // The body of the 401 that opens a Basic/Digest handshake is not the answer.
    if (esp_http_client_get_status_code(evt->client) == 401) {
        return ESP_OK;
    }
#endif
    if (ctx->overflow || evt->data_len <= 0) {
        return ESP_OK;
    }

    size_t need = ctx->len + (size_t) evt->data_len + 1;
    if (need > ctx->max_len) {
        // Cap reached - keep whatever was captured so far and stop growing;
        // not necessarily a hard failure for callers happy with a truncated
        // body (e.g. an RSS feed's trailing items).
        ctx->overflow = true;
        return ESP_OK;
    }
    if (need > ctx->cap) {
        size_t new_cap = ctx->cap ? ctx->cap * 2 : 4096;
        while (new_cap < need) {
            new_cap *= 2;
        }
        if (new_cap > ctx->max_len) {
            new_cap = ctx->max_len;
        }
        // PSRAM, not the default (internal-preferred) heap: this can grow up
        // to max_response_bytes (tens of KB for weather/headlines), and
        // internal SRAM exhaustion here was measured to break a subsequent
        // TLS handshake in the same wake cycle - see the fix in
        // display_manager.c's rotate_random() for the original incident.
        char *grown = heap_caps_realloc(ctx->buf, new_cap, MALLOC_CAP_SPIRAM);
        if (!grown) {
            ESP_LOGE(TAG, "Out of memory growing response buffer to %zu bytes", new_cap);
            ctx->overflow = true;
            return ESP_OK;
        }
        ctx->buf = grown;
        ctx->cap = new_cap;
    }

    memcpy(ctx->buf + ctx->len, evt->data, evt->data_len);
    ctx->len += evt->data_len;
    ctx->buf[ctx->len] = '\0';
    return ESP_OK;
}

// Shared retry/client-setup core behind both http_fetch_get() and
// http_fetch_get_conditional() - the two differ only in whether they send
// If-None-Match and whether a 304 is treated as success-with-no-body rather
// than a retry-worthy failure. `report` is NULL for a plain GET.
static esp_err_t do_http_fetch(const char *url, int timeout_ms, size_t max_response_bytes,
                               const char *if_none_match, char **out_body, size_t *out_len,
                               bool *out_truncated, char *out_etag, size_t out_etag_len,
                               bool *out_not_modified, const char *user_agent,
                               http_report_t *report)
{
#if !FEATURE_CALDAV
    (void) report;
#endif
    *out_body = NULL;
    if (out_len) {
        *out_len = 0;
    }
    if (out_truncated) {
        *out_truncated = false;
    }
    if (out_not_modified) {
        *out_not_modified = false;
    }
    if (out_etag && out_etag_len > 0) {
        out_etag[0] = '\0';
    }

    esp_err_t last_err = ESP_FAIL;

#if FEATURE_SOURCE_AUTH
    // A login in the URL (https://user:password@host/...) is taken out of it here and handed to
    // the HTTP client separately (see source_auth.h) - the client would neither decode the
    // percent-escapes nor keep the password out of its error log.
    char clean_url[SOURCE_AUTH_URL_MAX_LEN];
    char login_user[SOURCE_AUTH_USER_MAX_LEN];
    char login_pass[SOURCE_AUTH_PASS_MAX_LEN];
    bool has_login = false;
    switch (source_auth_split_url(url, clean_url, sizeof(clean_url), login_user, sizeof(login_user),
                                  login_pass, sizeof(login_pass))) {
    case SOURCE_AUTH_SPLIT:
        if (!source_auth_is_https(clean_url) && !config_manager_get_source_auth_allow_http()) {
            ESP_LOGE(TAG,
                     "Not sending a login over plain http:// - use https://, or allow it in the "
                     "settings");
            return ESP_ERR_NOT_ALLOWED;
        }
        url = clean_url;
        has_login = true;
        break;
    case SOURCE_AUTH_INVALID:
        ESP_LOGE(TAG,
                 "The login in the URL is not valid (a part is too long, or a special character "
                 "is not percent-encoded, e.g. @ as %%40)");
        return ESP_ERR_INVALID_ARG;
    default:
        break;
    }
#endif

    for (int attempt = 1; attempt <= HTTP_FETCH_RETRY_COUNT; attempt++) {
        if (attempt > 1) {
            ESP_LOGW(TAG, "Retrying GET (%d/%d) after %d ms...", attempt, HTTP_FETCH_RETRY_COUNT,
                     HTTP_FETCH_RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(HTTP_FETCH_RETRY_DELAY_MS));
        }

        if (out_etag && out_etag_len > 0) {
            out_etag[0] = '\0';  // don't carry a partial value across retries
        }
        http_body_buf_t ctx = {
            .max_len = max_response_bytes, .etag_out = out_etag, .etag_out_len = out_etag_len};

        esp_http_client_config_t config = {
            .url = url,
            .timeout_ms = timeout_ms,
            .event_handler = body_capture_handler,
            .user_data = &ctx,
            .buffer_size = 2048,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .user_agent = user_agent,
            // This project's lwIP config resolves only one address per
            // hostname (CONFIG_LWIP_DNS_MAX_HOST_IP=1) with no
            // "happy eyeballs" fallback between address families - if that
            // one address happens to be IPv6 and the route to it is
            // broken/slow for this network (a real-world case found live:
            // a Google Calendar ICS fetch failed with ESP_ERR_HTTP_CONNECT
            // on every attempt despite the identical URL working fine from
            // a browser/curl, which do have automatic dual-stack fallback),
            // the fetch fails outright with no retry via IPv4. Forcing IPv4
            // here sidesteps that whole class of failure; IPv4 reachability
            // is universal for every host this shared helper talks to.
            .addr_type = HTTP_ADDR_TYPE_INET,
        };
#if FEATURE_SOURCE_AUTH
        if (has_login) {
            config.username = login_user;
            config.password = login_pass;
            // https: send Basic right away (one request). http (only when the setting allows
            // it): wait for the server's challenge instead, so a Digest server never sees the
            // password itself. A Digest challenge is answered either way.
            config.auth_type =
                source_auth_is_https(url) ? HTTP_AUTH_TYPE_BASIC : HTTP_AUTH_TYPE_NONE;
            // One answer to a challenge, then give up: a wrong password must not be tried ten
            // times.
            config.max_authorization_retries = 1;
            // Never carry the login to a host a redirect points at.
            config.disable_auto_redirect = true;
        }
#endif

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "Failed to init HTTP client for GET");
            free(ctx.buf);
            last_err = ESP_FAIL;
            continue;
        }

        if (if_none_match && if_none_match[0] != '\0') {
            esp_http_client_set_header(client, "If-None-Match", if_none_match);
        }
#if FEATURE_CALDAV
        if (report) {
            esp_http_client_set_method(client, HTTP_METHOD_REPORT);
            esp_http_client_set_header(client, "Content-Type", "application/xml; charset=utf-8");
            esp_http_client_set_header(client, "Depth", "1");
            esp_http_client_set_post_field(client, report->body, (int) strlen(report->body));
        }
#endif

        esp_err_t err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);
#if FEATURE_CALDAV
        if (report) {
            report->status = status;
        }
#endif

#if FEATURE_SOURCE_AUTH
        // Retrying cannot help here, and a wrong password must not be hammered at the server.
        if (has_login && (status == 401 || status == 403)) {
            ESP_LOGE(TAG,
                     "The server refused the login (HTTP %d) - check the user name and password",
                     status);
            free(ctx.buf);
            return ESP_FAIL;
        }
        if (has_login && status >= 300 && status < 400) {
            ESP_LOGE(TAG,
                     "The server redirects (HTTP %d) - not followed while a login is set, "
                     "use the final address",
                     status);
            free(ctx.buf);
            return ESP_FAIL;
        }
#endif
#if FEATURE_CALDAV
        // A 4xx to a REPORT will not change on a retry; the caller decides what to do about it
        // (calendar_ics.c asks again without `expand`).
        if (report && status >= 400 && status < 500) {
            ESP_LOGE(TAG, "REPORT returned HTTP %d", status);
            free(ctx.buf);
            return ESP_FAIL;
        }
#endif

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "GET failed: %s", esp_err_to_name(err));
            free(ctx.buf);
            last_err = err;
            continue;
        }

        if (status == 304) {
            ESP_LOGI(TAG, "GET returned HTTP 304 Not Modified");
            free(ctx.buf);
            if (out_not_modified) {
                *out_not_modified = true;
            }
            return ESP_OK;
        }

#if FEATURE_CALDAV
        // A CalDAV answer is 207 Multi-Status.
        if (report && status == 207 && ctx.buf) {
            status = 200;
        }
#endif

        if (status != 200 || !ctx.buf) {
            ESP_LOGE(TAG, "GET returned HTTP %d", status);
            free(ctx.buf);
            last_err = ESP_FAIL;
            continue;
        }

        *out_body = ctx.buf;
        if (out_len) {
            *out_len = ctx.len;
        }
        if (out_truncated) {
            *out_truncated = ctx.overflow;
        }
        return ESP_OK;
    }

    return last_err;
}

esp_err_t http_fetch_get(const char *url, int timeout_ms, size_t max_response_bytes,
                         char **out_body, size_t *out_len, bool *out_truncated,
                         const char *user_agent)
{
    return do_http_fetch(url, timeout_ms, max_response_bytes, NULL, out_body, out_len,
                         out_truncated, NULL, 0, NULL, user_agent, NULL);
}

esp_err_t http_fetch_get_conditional(const char *url, int timeout_ms, size_t max_response_bytes,
                                     const char *if_none_match, char **out_body, size_t *out_len,
                                     bool *out_truncated, char *out_etag, size_t out_etag_len,
                                     bool *out_not_modified, const char *user_agent)
{
    return do_http_fetch(url, timeout_ms, max_response_bytes, if_none_match, out_body, out_len,
                         out_truncated, out_etag, out_etag_len, out_not_modified, user_agent, NULL);
}

#if FEATURE_MARKET_QUOTES || FEATURE_ARTWORKS || FEATURE_ROUTE_TIME || FEATURE_RECIPES
esp_err_t http_fetch_get_once(const char *url, int timeout_ms, size_t max_response_bytes,
                              char **out_body, size_t *out_len, int *out_status,
                              const char *user_agent)
{
    *out_body = NULL;
    *out_status = 0;
    if (out_len) {
        *out_len = 0;
    }
    esp_err_t last_err = ESP_FAIL;
    for (int attempt = 1; attempt <= 2; attempt++) {
        if (attempt > 1) {
            vTaskDelay(pdMS_TO_TICKS(HTTP_FETCH_RETRY_DELAY_MS));
        }
        http_body_buf_t ctx = {.max_len = max_response_bytes};
        esp_http_client_config_t config = {
            .url = url,
            .timeout_ms = timeout_ms,
            .event_handler = body_capture_handler,
            .user_data = &ctx,
            .buffer_size = 2048,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .user_agent = user_agent,
            .addr_type = HTTP_ADDR_TYPE_INET,  // see do_http_fetch()
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "Failed to init HTTP client for GET");
            free(ctx.buf);
            continue;
        }
        esp_err_t err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);
        // A 401 without a WWW-Authenticate header (what a wrong API key gets) makes the client
        // return ESP_ERR_NOT_SUPPORTED although the server answered: an HTTP error status is an
        // answer, whatever the client says about the request (seen on a frame with a wrong key).
        bool answered = err == ESP_OK || status >= 400;
        if (!answered) {
            ESP_LOGE(TAG, "GET failed: %s", esp_err_to_name(err));
            free(ctx.buf);
            last_err = err;
            continue;  // the server did not answer: one more try
        }
        // the server answered: whatever it said stays its answer
        *out_status = status;
        *out_body = ctx.buf;
        if (out_len) {
            *out_len = ctx.len;
        }
        return ESP_OK;
    }
    return last_err;
}
#endif

#if FEATURE_CALDAV
esp_err_t http_fetch_report(const char *url, int timeout_ms, size_t max_response_bytes,
                            const char *request_body, char **out_body, size_t *out_len,
                            bool *out_truncated, int *out_status)
{
    http_report_t report = {.body = request_body, .status = 0};
    esp_err_t err = do_http_fetch(url, timeout_ms, max_response_bytes, NULL, out_body, out_len,
                                  out_truncated, NULL, 0, NULL, NULL, &report);
    if (out_status) {
        *out_status = report.status;
    }
    return err;
}
#endif
