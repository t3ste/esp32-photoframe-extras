#ifndef VTODO_H
#define VTODO_H

#include <stddef.h>

#include "esp_err.h"
#include "todo.h"

/**
 * @file vtodo.h
 * @brief iCalendar to-dos (VTODO) as items of the Agenda's ToDo column (build option
 * `caldav-todo`).
 *
 * The ToDo column reads a todo.txt file; a CalDAV task list hands out VTODO components instead.
 * This turns them into the same todo_item_t, so the column and its colours work unchanged:
 *
 * - `SUMMARY` is the text (ASCII-sanitized like the rest of the display text).
 * - `PRIORITY` 1-9 (1 = highest) becomes the priority chip: 1-2 -> A, 3-4 -> B, 5 -> C, 6-9 -> D;
 *   0 or none: no chip.
 * - `DUE` (a date, a UTC time - shown as the frame's local date - or a floating/zoned time, whose
 *   date is taken as written) becomes the due date.
 * - A to-do that is done (`STATUS` COMPLETED/CANCELLED, a `COMPLETED` time, `PERCENT-COMPLETE` 100)
 *   is left out, like a "x " line of a todo.txt.
 * - A repeating to-do (`RRULE`) is listed once, as it stands - it is not expanded.
 *
 * At most TODO_MAX_ITEMS are kept: the ones due first (undated last), then by priority, then in the
 * order they came. Pure string handling, so the host tests link it.
 */

/**
 * @brief Parses the VTODO components of iCalendar text (several VCALENDAR objects may follow each
 * other, as a CalDAV multistatus collects them).
 *
 * @param ics The text; it is modified (line folding is undone in place). It must be NUL-terminated,
 * with room for the terminator after `len` bytes.
 * @param len Its length.
 * @param out Receives the items (count = 0 if there are none - not an error).
 */
esp_err_t vtodo_parse(char *ics, size_t len, todo_list_t *out);

#endif
