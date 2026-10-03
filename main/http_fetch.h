#ifndef HTTP_FETCH_H
#define HTTP_FETCH_H

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "feature_config.h"

/**
 * @brief Performs a GET request (with a couple of retries on transient
 * network/TLS failure) against a plain public HTTPS endpoint (verified via
 * ESP-IDF's built-in public CA bundle - no custom cert pinning), returning
 * the response body.
 *
 * Shared by weather.c and headlines.c, whose fetch needs are identical in
 * shape to telegram_bot.c's telegram_http_get() but target non-secret URLs
 * (Open-Meteo, a user-configured RSS feed) so no token redaction is needed.
 *
 * @param url URL to GET.
 * @param timeout_ms Per-attempt HTTP timeout.
 * @param max_response_bytes Response body cap; a response larger than this
 * is truncated (whatever was captured up to the cap is still returned) - the
 * caller decides whether a truncated body is still useful (e.g. an RSS feed
 * with truncated trailing items is still fine to parse).
 * @param out_body Response body, NUL-terminated (caller frees with free()).
 * NULL on total failure (no data at all captured).
 * @param out_len Response body length in bytes (excluding the NUL), or NULL
 * if not needed.
 * @param out_truncated Set to true if the response was cut off at
 * max_response_bytes, false otherwise. May be NULL if not needed.
 * @param user_agent Custom User-Agent header value, or NULL for ESP-IDF's
 * default. Some free APIs (e.g. MET Norway's yr.no) require a real,
 * identifying User-Agent and reject/throttle requests without one.
 */
esp_err_t http_fetch_get(const char *url, int timeout_ms, size_t max_response_bytes,
                         char **out_body, size_t *out_len, bool *out_truncated,
                         const char *user_agent);

/**
 * @brief Like http_fetch_get(), but as an HTTP conditional GET: sends
 * `if_none_match` (if non-empty) as the request's If-None-Match header, and
 * reports back the response's ETag (if any) plus whether the server replied
 * 304 Not Modified. Shared infrastructure for any fetch worth re-validating
 * instead of always re-downloading in full - e.g. todo.c/calendar_ics.c's
 * ToDo/Calendar sources, mirroring the same pattern utils.c's rotation image
 * download already uses.
 *
 * @param if_none_match Previously-stored ETag to send back, or NULL/empty to
 * always fetch unconditionally (first-ever fetch, or caching disabled).
 * @param out_etag Buffer for the response's ETag header, or NULL if not
 * needed. Left as an empty string if the response carried no ETag.
 * @param out_etag_len Size of out_etag.
 * @param out_not_modified Set true if the server replied 304 (in which case
 * *out_body is NULL - the caller is expected to reuse whatever body it
 * cached from the previous 200 response). May be NULL if not needed.
 * @param user_agent See http_fetch_get().
 */
esp_err_t http_fetch_get_conditional(const char *url, int timeout_ms, size_t max_response_bytes,
                                     const char *if_none_match, char **out_body, size_t *out_len,
                                     bool *out_truncated, char *out_etag, size_t out_etag_len,
                                     bool *out_not_modified, const char *user_agent);

#if FEATURE_MARKET_QUOTES || FEATURE_ARTWORKS || FEATURE_ROUTE_TIME || FEATURE_RECIPES
/**
 * @brief A GET for APIs with a request quota (build options market-quotes, artworks, route-time,
 * recipes): unlike http_fetch_get() it asks again only if the server did not answer at all (a retry
 * of an answer would spend the quota again, or hammer a server that said "too many requests"), and
 * it returns the answer of any HTTP status - with its body, which is where such APIs write what
 * went wrong. The URL may hold an API key: it is never logged.
 *
 * @param out_body The body (NUL-terminated, caller frees with free()), NULL if the answer had none.
 * @param out_status The HTTP status of the answer (0 if the server never answered).
 * @return ESP_OK if the server answered (whatever the status), an error if it did not.
 * The other parameters are those of http_fetch_get().
 */
esp_err_t http_fetch_get_once(const char *url, int timeout_ms, size_t max_response_bytes,
                              char **out_body, size_t *out_len, int *out_status,
                              const char *user_agent);
#endif

#if FEATURE_CALDAV
/**
 * @brief Sends a CalDAV REPORT (Depth: 1, an XML body) and returns the multistatus answer
 * (207, or 200) - build option caldav. A login in the URL is handled as in http_fetch_get().
 * Any 4xx answer is returned at once, without the retries a GET gets.
 *
 * @param request_body The calendar-query XML (see caldav_build_report_body()).
 * @param out_status The last HTTP status the server answered with (0 if none), also on failure.
 * The other parameters are those of http_fetch_get().
 */
esp_err_t http_fetch_report(const char *url, int timeout_ms, size_t max_response_bytes,
                            const char *request_body, char **out_body, size_t *out_len,
                            bool *out_truncated, int *out_status);
#endif

#endif
