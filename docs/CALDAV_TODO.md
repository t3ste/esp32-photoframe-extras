# CalDAV task lists in the ToDo column

> **Build option:** compiled in only with `python build.py --with caldav-todo` (it pulls in `caldav`, `source-auth` and
> `agenda`); without it the firmware and its web UI are the upstream ones (see [FEATURES.md](FEATURES.md)).

The Agenda's ToDo column reads a todo.txt file. With this option the ToDo address may instead be a **CalDAV task list** of
your own server (Nextcloud Tasks, Baikal, Radicale, ...):

```
caldavs://user:password@cloud.example.org/remote.php/dav/calendars/user/tasks/
caldav://user:password@192.168.1.20:5232/user/tasks/               (plain http, see SOURCE_AUTH.md)
```

The address is written in the same field as the todo.txt URL (Settings -> Agenda -> ToDo); how the login goes into it, which
characters to encode and why plain `http://` needs a tick are described in [SOURCE_AUTH.md](SOURCE_AUTH.md) and
[CALDAV.md](CALDAV.md) (the address of a task list has the same shape as that of a calendar).

## What the column shows

The to-dos (`VTODO`) of the list, like the lines of a todo.txt:

| CalDAV | In the column |
| --- | --- |
| `SUMMARY` | the text (umlauts are written as ae/oe/ue, like all text the frame draws) |
| `PRIORITY` 1-9 (1 is the highest) | the priority chip: 1-2 -> **A**, 3-4 -> **B**, 5 -> **C**, 6-9 -> **D**; none or 0: no chip |
| `DUE` | the due date and its colour (overdue / today / later). A date is taken as it is; a UTC time (`...Z`) becomes the **frame's** local date; a time with a zone or none keeps the date it was written with |
| finished (`STATUS` COMPLETED or CANCELLED, a `COMPLETED` time, `PERCENT-COMPLETE` 100) | left out, like the `x ` lines of a todo.txt |
| repeating (`RRULE`) | listed **once**, as it stands - it is not expanded |

Order: the ones due first (soonest first), then those without a due date; among equals the higher priority first, then the
order the server sent them. At most 24 to-dos are shown, like with a todo.txt.

## How it is fetched

One CalDAV `REPORT` for the to-dos of the list, at every agenda wake, without a conditional request or a saved copy (the answer
has neither an `ETag` nor a copy that could be reused). The frame asks for the **open** to-dos only - to-dos with a `COMPLETED`
time are filtered out by the server. A server that does not know that filter (it answers 400, 415, 422 or 501) is asked
again for all to-dos, and the finished ones are dropped on the frame. A refused login (401/403) or any other error ends the
fetch at once: the column is left out of that agenda cycle, as with a todo.txt that cannot be fetched.

## Not built

- Several lists at once (one address per column, as with the todo.txt).
- The finished to-dos struck through, a colour per list, other orders (by list, by priority) - the column draws what todo.txt
  items have; the CalDAV items are mapped onto those.
- Writing anything back (ticking a to-do off): the frame only reads.
- Sub-tasks (`RELATED-TO`), repeats (`RRULE`) expanded, categories as tags.
