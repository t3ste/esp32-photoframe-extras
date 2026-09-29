#ifndef CALDAV_H
#define CALDAV_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

/**
 * @file caldav.h
 * @brief CalDAV calendar queries (build option `caldav`).
 *
 * A calendar address written as `caldavs://user:password@host/dav/user/calendar/` (`caldav://`
 * for plain http) is not downloaded as a whole ICS file: the frame asks the server for the events
 * of the coming days with one `REPORT` request (RFC 4791 calendar-query, time-range) and lets
 * the server expand repeating events (`expand`), so only what is shown is transferred and rules
 * the on-device parser does not know (monthly, yearly, exceptions) work.
 *
 * Pure string handling with no ESP-IDF dependency, so the host tests link it; the request itself
 * is made by calendar_ics.c through http_fetch_report().
 */

// Room for the REPORT request body of caldav_build_report_body().
#define CALDAV_REPORT_BODY_MAX 1024

/** @brief True for a `caldav://` or `caldavs://` address (case-insensitive). */
bool caldav_is_url(const char *url);

/**
 * @brief Turns a `caldavs://` address into `https://` and a `caldav://` one into `http://`.
 *
 * @return `url` itself if it is neither, `buf` if it was rewritten, NULL if `buf` is too small.
 */
const char *caldav_resolve_url(const char *url, char *buf, size_t buf_len);

/**
 * @brief Writes the calendar-query REPORT body for the events between `start` and `end`.
 *
 * @param expand Ask the server to expand repeating events into single events inside the range.
 * @return Length of the body, or -1 if `buf` is too small.
 */
int caldav_build_report_body(char *buf, size_t buf_len, time_t start, time_t end, bool expand);

/**
 * @brief Collects the iCalendar text of a multistatus response, in place.
 *
 * Every `calendar-data` element's text (entities decoded, CDATA taken as is) is moved to the
 * front of `xml`, separated by newlines, and the buffer is NUL-terminated. The result is what
 * calendar_ics_parse() takes: several VCALENDAR objects one after the other.
 *
 * @param xml Response body (modified).
 * @param len Its length.
 * @return Length of the collected text (0 if the response holds no calendar-data).
 */
size_t caldav_extract_calendar_data(char *xml, size_t len);

#endif
