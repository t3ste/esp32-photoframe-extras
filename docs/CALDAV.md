# CalDAV calendars

> **Build option:** compiled in only with `python build.py --with caldav` (it pulls in `source-auth` and `agenda`); without
> it the firmware and its web UI are the upstream ones (see [FEATURES.md](FEATURES.md)).

A calendar on your own server (Nextcloud, Baikal, Radicale, ...) can be entered as a **CalDAV address** in Calendar A-E:

```
caldavs://user:password@cloud.example.org/remote.php/dav/calendars/user/personal/
caldav://user:password@192.168.1.20:5232/user/personal/            (plain http, see below)
```

`caldavs://` is CalDAV over `https://`, `caldav://` over plain `http://` (like `webcals://` and `webcal://`). The login goes
into the address like described in [SOURCE_AUTH.md](SOURCE_AUTH.md) - special characters percent-encoded, and over plain
`http://` only with **Allow a login over plain http://** ticked.

## What is different from an ICS address

A normal calendar address is downloaded **as a whole** and read by the frame. For a CalDAV address the frame instead asks the
server one question - a CalDAV `REPORT` (RFC 4791 `calendar-query`) - "which events fall into the coming days?":

- **Only what is shown is transferred.** A calendar with years of history is no longer a download of megabytes (the frame
  reads at most 2 MB of an ICS file and cuts the rest off).
- **The server expands repeating events.** The frame's own reader understands daily and weekly repeats; a monthly or yearly
  event, `BYDAY` lists, or an exception ("not on the 3rd") were left out ([CALENDAR_RRULE_SUPPORT.md](CALENDAR_RRULE_SUPPORT.md)). The server sends every occurrence as a single event
  instead, so they all show up. (A server that does not take this - it answers 400, 415, 422 or 501 - is asked again without
  it, and the frame's own reader does what it can.)
- Calendars C-E are queried **once** (on save or **Refresh now**) for the time from last week to a year ahead.
- No conditional download (`ETag`) - the answer is small and fetched each time.

## Which address?

The address of the **calendar itself** (a "collection"), the one a CalDAV client such as Thunderbird or DAVx5 asks for - not
the server's start page and not the "calendar home". Typical shapes:

| Server | Calendar address |
| --- | --- |
| Nextcloud | `https://HOST/remote.php/dav/calendars/USER/CALENDAR/` |
| Baikal | `https://HOST/dav.php/calendars/USER/CALENDAR/` |
| Radicale | `http://HOST:5232/USER/CALENDAR/` |

Write `caldavs://` (or `caldav://`) instead of `https://` (`http://`). If your server also offers the calendar as an ICS link
(`...?export`), that link works as a normal address too - but it is the whole calendar.

## Limits

- Events (`VEVENT`) only; the ToDo list reads its own address (a todo.txt).
- No discovery: the address has to be the calendar, not the account.
- Basic and Digest login only. Google Calendar's CalDAV (OAuth) and iCloud (app-specific passwords, redirects) are not supported -
  use the calendar's ICS "secret address" for Google.
- A server that redirects (`301/302`) is refused while a login is set - use the final address.

## When it does not show up

The frame's log says what the server answered (it never prints the address or the login): `The server refused the login
(HTTP 401)` - wrong user or password (or a `@` that is not written as `%40`); `REPORT returned HTTP 404/405` - not the
address of a calendar; `Not sending a login over plain http://` - use `caldavs://` or tick the http option.
