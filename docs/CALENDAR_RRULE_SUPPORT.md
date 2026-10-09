# Calendar RRULE support (Agenda Mode)

> **Build option:** compiled in only with `python build.py --with agenda`; without it the firmware is the upstream firmware (see [FEATURES.md](FEATURES.md)).

Which recurring-event (`RRULE`) values this fork's ICS parser (`main/calendar_ics.c`,
`parse_rrule()`/`expand_series()`) understands, what it does with the exceptions of a series (`EXDATE`,
`RDATE`, `RECURRENCE-ID`, `STATUS`), and why the rest is rejected. An event whose `RRULE` isn't fully
supported - or whose `EXDATE`/`RDATE` can't be read - is **skipped entirely** (fail-closed) rather than
shown once as if it weren't recurring: showing a wrong/misleading occurrence was judged worse than
showing nothing for that event.

This intentionally covers only a useful subset of RFC 5545, not the full spec - see
[Not supported](#not-supported-and-why) for the reasoning behind each gap.

## Supported

| `RRULE` part | Support | Notes |
|---|---|---|
| `FREQ=DAILY` | ✅ Full | |
| `FREQ=WEEKLY` | ✅ Full | |
| `INTERVAL=N` | ✅ Full | "every N days/weeks" |
| `COUNT=N` | ✅ Full | Stops after N instances of the rule - instances that an `EXDATE` removes count too (RFC 5545) |
| `UNTIL=<date>` | ✅ Full | Inclusive end bound - an occurrence starting after this is excluded. Accepts a bare date (`20260101`) or a date-time, with or without a trailing `Z` |
| `BYDAY=<single day>` | ✅ Conditional | Only when it names the **same weekday `DTSTART` already falls on** (e.g. `DTSTART` is a Wednesday, `BYDAY=WE`) - see below for why. The weekday is the one in the zone `DTSTART` is written in: UTC for a time with a `Z` |
| `WKST=<day>` | ✅ Ignored (harmlessly) | Parsed but has no effect - see below for why that's safe |

A plain `FREQ=WEEKLY` with no `BYDAY` at all also works and always has - it's the presence of a
*single* `BYDAY` that used to be rejected until this was fixed.

### The exceptions of a series

These are properties of the `VEVENT`, not parts of the `RRULE`, and are applied to every supported
series (and to a single event that carries them):

| Property | Effect |
|---|---|
| `EXDATE` | The named instance is not shown. Any number of `EXDATE` lines and comma-separated lists, any line length. A date-time matches by instant (so `DTSTART;TZID=...` and an `EXDATE` in UTC match when they are the same moment); a bare date (`VALUE=DATE`) removes the instance of that day. An `EXDATE` that can't be read (a `PERIOD`, a malformed date) drops the whole event |
| `RDATE` | An extra instance (a date-time or a date), as long as the series. An `RDATE` that the rule makes anyway is one instance; an `EXDATE` removes an `RDATE` too. An event with `RDATE` but no `RRULE` is the `DTSTART` instance plus the `RDATE`s. `VALUE=PERIOD` can't be read and drops the whole event |
| `RECURRENCE-ID` | A `VEVENT` with the same `UID` and a `RECURRENCE-ID` replaces the one instance it names: that instance of the series is not shown, and the `VEVENT` is shown as the ordinary event it is (it carries its own `DTSTART`, `SUMMARY`). It may come before or after the series in the feed. An exception is one instance, whatever else it says: if it carries a copy of the series' `RRULE`/`EXDATE`, those are ignored. `RECURRENCE-ID;RANGE=THISANDFUTURE` ("this and all later instances are changed") would need the changes applied to the series from there on, which this parser can't do: that instance and every later one are left out, as is the changed event itself |
| `STATUS:CANCELLED` | The event is not shown - a called-off single event, a whole series, or (as an exception with a `RECURRENCE-ID`) one instance |
| `DURATION` | The length of an event that has no `DTEND` (`PT1H30M`, `P1DT2H`, `P1W`; an all-day event ends at a local midnight). `DTEND` wins if both are given; a negative or malformed value is ignored (the event keeps its default length: none, or one day if it is an all-day event) |

Only the exceptions that can touch the window are kept (up to 128 `EXDATE` and 64 `RDATE` values per
series, up to 512 `RECURRENCE-ID`s per feed - more are ignored with a warning in the log).

Before this existed the lines were not read at all: an `EXDATE`d instance was still shown, a moved instance
showed up twice (at its old time from the series, at its new one as its own event) and a called-off one
stayed on the screen.

### Why a single `BYDAY` is safe, but only when it matches `DTSTART`

Real calendar apps (Google Calendar, Outlook, Apple Calendar) almost always emit
`FREQ=WEEKLY;BYDAY=<day>` for a plain "repeat weekly" event, even one with no special pattern -
this is standard, not an edge case. When that single `BYDAY` names the same weekday `DTSTART`
already falls on, it's semantically identical to plain `FREQ=WEEKLY` (which already worked) - the
generated occurrences are exactly the same either way. If `BYDAY` names a *different* weekday than
`DTSTART`, that describes a genuinely different pattern (the parser can't know which one the
calendar author actually intended), so it still fails closed like any unsupported rule.

### Why `WKST` can just be ignored

`WKST` (week-start-day) only changes which occurrences are valid for patterns this parser doesn't
support anyway - `BYSETPOS`, `BYWEEKNO`, or multiple `BYDAY` values combined with `INTERVAL>1`. With
at most one `BYDAY` value (the only case ever accepted), `WKST` has no effect on the actual result,
so rejecting the whole rule over it would have been overly strict. (Confirmed live 2026-09-16: a
real calendar export's plain weekly Wednesday event, `FREQ=WEEKLY;WKST=MO;BYDAY=WE`, was being
dropped by `WKST` alone, even after `BYDAY` itself was accepted.)

## Time zones and daylight saving time

A `DTSTART`/`DTEND` with no trailing `Z` (a "floating" time, or one qualified with `TZID=...`) is
interpreted as the **device's own configured local timezone** (via `mktime()`) rather than actually
resolving the named IANA timezone from `TZID`. This matches how every other wake/schedule
computation in this firmware already works, and is accurate as long as the calendar's own
timezone matches the device's - which is true for the overwhelming majority of personal calendars,
but would be wrong for an event deliberately created in a different timezone than the device sits
in.

**A series repeats on the wall clock.** For such a local `DTSTART` the n-th instance is `DTSTART`'s time
of day on the n-th period's calendar day, so "every day at 09:00" stays at 09:00 when the clocks change.
(Until this was fixed the n-th instance was `DTSTART` plus n x 86400 s: a series that began in winter
showed up an hour late in summer, and one that began in summer an hour early in winter - for the whole
half year in between. The demo package's "Midnight snack check" appeared at 01:00 in summer. All the host
tests run with `TZ=UTC0`, where the two are the same thing, which is why it went unnoticed; the tests of
`CalendarIcsLocalTime` run in CET/CEST.) An event's length is kept: 90 minutes stay 90 minutes on the day
the clocks change, a multi-day all-day event keeps its number of days (its end is a local midnight, not
`start + n x 86400 s`). A time that does not exist on the day the clocks go forward (02:30 on that day)
or that occurs twice when they go back still gives exactly one instance that day; which of the two
instants `mktime()` picks is up to the C library.

A `DTSTART` in UTC (`...Z`) repeats at the **same instants** all year, as the standard says: on the wall
clock it moves by an hour.

What is still **not** done is resolving a foreign `TZID` (`America/New_York` is read as the device's time
zone); that needs the `VTIMEZONE` of the feed or a time zone database.

## The list limit

At most `ICS_MAX_EVENTS` (48) events come back for a window. When more overlap it, the ones that
**start first** are kept (the agenda shows what comes first); until this was fixed it was the first 48 in
the order of the file, whichever time they were at.

## Not supported (and why)

| `RRULE` part | Why not | Could it be added later? |
|---|---|---|
| `FREQ=MONTHLY` | Months have variable length (28-31 days) - the expander steps in whole days (`period_days`), which works for DAILY/WEEKLY. A real calendar-arithmetic expansion (increment the month, re-normalize) would be needed | Yes, but needs a separate expansion path, not a small tweak |
| `FREQ=YEARLY` | Same problem, worse (leap years: 365 vs. 366 days) | Yes - combined with `BYMONTHDAY`+`BYMONTH` this is the classic birthday/anniversary pattern, and was in fact the most common rejected rule found in real-world testing. Meaningfully more work than the `WKST`/`BYDAY`/`UNTIL` fixes, since it's a new expansion strategy rather than a new accepted parameter |
| `FREQ=HOURLY`/`MINUTELY`/`SECONDLY` | Not meaningful for a display that wakes at most every few minutes | No - out of scope for this project regardless of implementation cost |
| Multiple `BYDAY` values (e.g. `BYDAY=MO,WE,FR`) | A genuinely different pattern (multiple weekdays per week) - this parser's model represents "one occurrence every N periods," not "occurrences on several specific weekdays within each period" | Possible in principle, but a real architecture change (would need to generate several candidate weekdays per period and merge them) |
| `BYDAY` with an ordinal prefix (e.g. `BYDAY=1MO`, `BYDAY=-1FR` - "1st Monday", "last Friday") | Different meaning entirely ("Nth weekday of the period," used with `BYSETPOS` or `FREQ=MONTHLY`/`YEARLY`) - the current single-letter-code parser doesn't recognize the numeric prefix and fails closed on the malformed-looking value | Only meaningful together with `BYSETPOS`/`MONTHLY`/`YEARLY` support |
| `BYMONTHDAY`, `BYMONTH`, `BYWEEKNO`, `BYYEARDAY`, `BYHOUR`, `BYMINUTE`, `BYSECOND` | Only meaningful combined with `MONTHLY`/`YEARLY` (see above), or a time-of-day set the model has no room for | Same as `FREQ=YEARLY` above |
| `BYSETPOS` (e.g. "the 2nd Tuesday of the month") | Needs the full monthly/yearly occurrence set generated first, then picked by position - a materially different algorithm | Possible, but the most complex of the unsupported list; not seen in real-world testing so far |
| `RECURRENCE-ID;RANGE=THISANDFUTURE` | The changes would have to be applied to the series from the named instance on; the instances from there on are left out instead | Possible, with the same machinery a time zone database would need |
| `RDATE;VALUE=PERIOD`, `EXDATE` of a period | Not readable; the whole event is dropped (fail-closed) | Rare in practice |
| A foreign `TZID` (not the device's zone) | Read as the device's own time zone, see above | Needs the `VTIMEZONE` of the feed or a time zone database |
| Any unrecognized/malformed component | Fails closed - a value this parser doesn't understand at all could silently mean something that changes which occurrences are valid | N/A by design - the whole point of failing closed |

## Where to look in code

- `ics_rrule_t` / `parse_rrule()` (`main/calendar_ics.c`) - what's parsed from the raw `RRULE`
  value and why each rejected component is rejected.
- `expand_series()` (`main/calendar_ics.c`) - how accepted rules are turned into actual occurrence
  timestamps within a requested window (wall-clock arithmetic for local `DTSTART`s, fixed instants for
  UTC ones), minus the exceptions, plus the `RDATE`s.
- `finalize_vevent()` (`main/calendar_ics.c`) - the `BYDAY`-vs-`DTSTART`-weekday cross-check that
  only becomes possible once `DTSTART` is known (RRULE components can appear in any order within a
  `VEVENT` block per RFC 5545, so `parse_rrule()` alone can't validate this at parse time), the
  `STATUS` check and the reading of `EXDATE`/`RDATE`.
- `collect_overrides()` / `instance_hidden()` (`main/calendar_ics.c`) - the `RECURRENCE-ID`s of the whole
  feed, read in a first pass, and how they hide instances of a series.
- `host_tests/test_calendar_ics.cpp` - one test per behavior documented above, including a
  regression test built from a real-world `.ics` export; the `CalendarIcsLocalTime` tests run in a zone
  with daylight saving time.
