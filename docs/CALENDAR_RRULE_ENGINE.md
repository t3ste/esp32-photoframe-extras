# Recurrence rules through libical (`agenda-rrule`)

> **Build option:** `python build.py --with agenda-rrule` (needs `agenda`; part of the `extras` bundle of the extended line). Without it the firmware keeps the Agenda's own
> expander, which takes daily and weekly rules only - see [CALENDAR_RRULE_SUPPORT.md](CALENDAR_RRULE_SUPPORT.md). With no build option at all the firmware is the upstream
> firmware (see [FEATURES.md](FEATURES.md)).

The Agenda's own reader leaves a **monthly or yearly event out** ("the second Monday of the month", "the last Friday", a birthday), as it does a `BYDAY` list, `BYSETPOS` and the like. With
this option a recurrence rule goes to the recurrence iterator of [libical](https://github.com/libical/libical) (v4.0.6, vendored unmodified in `components/libical`), which knows RFC 5545's
rules. What the iterator makes of the rule are wall-clock dates; [`main/calendar_ics.c`](../main/calendar_ics.c) turns them into instants as it always did.

## What an event can say now

| Rule | Example |
|---|---|
| `FREQ=DAILY`, `WEEKLY`, `MONTHLY`, `YEARLY` with `INTERVAL` (1..1000), `COUNT` (1..100000) or `UNTIL` | every other month |
| `BYDAY` list, also with an ordinal in a monthly or yearly rule | `FREQ=MONTHLY;BYDAY=2MO` (the second Monday), `BYDAY=-1FR` (the last Friday), `FREQ=WEEKLY;BYDAY=MO,WE,FR` |
| `BYMONTHDAY` (negative counts from the end), `BYMONTH`, `BYYEARDAY` | `FREQ=YEARLY;BYMONTH=10;BYMONTHDAY=12` (a birthday; the date that does not exist in a year, 29 February, is skipped) |
| `BYSETPOS` | `FREQ=MONTHLY;BYDAY=MO,TU,WE,TH,FR;BYSETPOS=-1` (the last weekday of the month) |
| `BYHOUR`, `BYMINUTE`, `BYSECOND` (not for an all-day event) | |
| `WKST` | |

`EXDATE`, `RDATE`, `RECURRENCE-ID`, `STATUS:CANCELLED`, `DURATION`, the time zone handling, the all-day events and the list limit are exactly what
[CALENDAR_RRULE_SUPPORT.md](CALENDAR_RRULE_SUPPORT.md) says; the iterator only decides *which days* a rule hits.

## What is refused (the event is left out, as before)

A rule is checked in [`main/calendar_rrule.c`](../main/calendar_rrule.c) before libical sees it: libical's internal assertions are a firmware abort, so a rule that failed the checks never gets there.

| Refused | Why |
|---|---|
| `FREQ=HOURLY`, `MINUTELY`, `SECONDLY` | A display that wakes every few minutes has no use for them |
| `RSCALE`, `SKIP` (RFC 7529), unknown or repeated parts | Not meaningful here / a rule this firmware cannot judge |
| A part outside its range, a combination the standard does not allow (`BYWEEKNO` outside a yearly rule, an ordinal `BYDAY` in a weekly rule, `BYMONTHDAY` in a weekly rule, `BYSETPOS` without another `BY` part, ...) | Would be an assertion in libical |
| `FREQ=YEARLY` with `BYMONTHDAY` but no `BYMONTH` | libical takes `DTSTART`'s month, python-dateutil all twelve - two readings of the same rule |
| `FREQ=YEARLY` with `BYWEEKNO` | The two disagree about the weeks that cross a year's end |
| `FREQ=WEEKLY` with `INTERVAL` > 1, a `BYDAY` list and a `WKST` of TU..SA | libical leaves out the instance of `DTSTART` itself |
| A rule with `COUNT` that begins more than 5000 instances before the window | The iterator would have to count them all; the event is left out and the log says so (a rule without `COUNT` jumps to the window) |
| A rule that never matches ("30 February"), a search that takes more than 4000 steps | Bounded on purpose |

The three "the two disagree" lines come from running thousands of random rules through libical and python-dateutil (see below): every `DAILY`, `WEEKLY` and `MONTHLY` rule and every other
`YEARLY` rule gave the same instances; these three families did not, and a wrong date on a wall is worse than no date.

## Limits and cost

- **Flash:** +98,160 bytes (about 96 KB) on a Waveshare build (ESP32-S3), measured against the same build with `agenda` only.
- **RAM:** libical allocates while it works (the rule, the iterator); the adapter frees the iterator and the ring buffer libical keeps its strings in at the end of every call.
- **One task at a time:** libical is built without threads (`ICAL_SYNC_MODE_NONE`), the adapter serialises its calls with a mutex.
- **Time:** reading a 2.4 MB feed of 30 days on an ESP32-S3 takes 4.3 - 4.8 s with the option and about 4.5 s without it; the time is not in the parse (on the host the same file takes ~8 ms), it is
  dominated by something else on the device, most likely reading the file from storage.
- **Candidates per series:** at most `ICS_MAX_CANDIDATES` (the list limit plus the `RDATE` room) instances of one series are kept, the earliest ones.

## How it is checked

- `host_tests/test_calendar_rrule.cpp`: the adapter against the instances python-dateutil makes (19 rules - leap day, `BYSETPOS`, `WKST`, far-past starts, all-day), 50 refused rules and the
  bounds. `host_tests/test_calendar_ics_rrule.cpp`: `calendar_ics.c` with the option on (`UNTIL`, `EXDATE`, `RDATE`, `RECURRENCE-ID`, durations, daylight saving time).
- A random-rule differential (thousands of rules, libical against dateutil) and an exhaustive `WKST` run found the three refused families above.
- The 97 public calendars of python-recurring-ical-events as fixtures, ASan / UBSan / leak runs of the tests, a mutation fuzzer over the feed parser.
- **The parallel comparison** (`-DICS_RRULE_COMPARE`, never in a release build): `calendar_ics.c` expands every series with libical *and*, for a rule its own expander understands too (daily,
  weekly, a single `BYDAY`), with that one; a difference is logged as instants (never an event's text) and the end of a parse says `rrule compare: N series agree, M differ`. On the host it
  compared 4068 series over 1176 windows of the invented and the public feeds without a difference; it is also the check that libical, compiled for the Xtensa, gives what it gives on the host.

## Where to look in code

- `components/libical/` - the vendored library, `UPSTREAM.md` (tag, commit, checksum, update procedure via `vendor.sh`), `port/config.h` (our build configuration), licence texts. The
  component is empty without the option. libical is used under the **MPL-2.0** ([notice](third_party/LIBICAL-NOTICE.md)).
- `main/calendar_rrule.c` / `.h` - `calendar_rrule_expand()`: the checks, the iterator, the bounds.
- `main/calendar_ics.c` - `gather_with_engine()` (window, `UNTIL`, overlap, hidden instances) and `compare_with_simple()` (debug only), both under `FEATURE_AGENDA_RRULE`.
