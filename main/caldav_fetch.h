#ifndef CALDAV_FETCH_H
#define CALDAV_FETCH_H

#include <stddef.h>

#include "esp_err.h"

/**
 * @file caldav_fetch.h
 * @brief The network side of the CalDAV options (build option `caldav`): one REPORT, the
 * iCalendar text of its answer.
 */

/**
 * @brief Sends a CalDAV REPORT to a `caldav(s)://` address (the login inside it is handled as
 * described in source_auth.h) and returns the iCalendar text the multistatus answer carries.
 *
 * If the server refuses `first_body` as such (HTTP 400, 415, 422 or 501 - it does not know a
 * filter or `expand` in it), `second_body` is sent instead, when there is one. Any other failure,
 * and above all a refused login (401/403), ends it at once.
 *
 * @param url The `caldav://` or `caldavs://` address.
 * @param timeout_ms Per attempt (the caller passes its own default).
 * @param max_bytes Cap of the answer (a longer one is cut; what was captured is used).
 * @param first_body The request body to try first (calendar-query XML).
 * @param second_body The fallback request body, or NULL.
 * @param out_text Receives the collected iCalendar text (free with free()).
 * @param out_len Receives its length (0 if the answer held no calendar-data).
 */
esp_err_t caldav_report_text(const char *url, int timeout_ms, size_t max_bytes,
                             const char *first_body, const char *second_body, char **out_text,
                             size_t *out_len);

#endif
