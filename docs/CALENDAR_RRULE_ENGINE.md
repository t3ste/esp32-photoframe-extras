# Recurrence rules through libical (`agenda-rrule`)

> **Build option:** `python build.py --with agenda-rrule` (needs `agenda`; part of the `extras` bundle of the extended line). Without it the firmware keeps the Agenda's own
> expander, which takes daily and weekly rules only; with it that expander is not compiled any more and libical takes every rule - the matrix is in
> [CALENDAR_RRULE_SUPPORT.md](CALENDAR_RRULE_SUPPORT.md). With no build option at all the firmware is the upstream firmware (see [FEATURES.md](FEATURES.md)).

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

## What happened to the reader's own expander

The Agenda's own rule reader (`ics_rrule_t`, `parse_rrule()`, the DAILY/WEEKLY loop `gather_simple()` and the check that a single `BYDAY` names `DTSTART`'s weekday) is **compiled out in a build with
`agenda-rrule`**: every rule - the daily and weekly ones too - goes to libical, and a single event that has an `EXDATE` or `RDATE` is a series of one (`gather_single()`: its own instance, which an
`EXDATE` can take away). It was never a fallback at run time - a rule the engine refuses leaves the event out, it does not hand it to the old code - so taking it out changes no result. Before it went, the two were run
side by side (a debugging build that expanded every series with both and logged a difference as instants): 4068 series over 1176 windows on a PC and 110 series on the frame (the demo calendars, an invented feed of rules,
a feed that crosses both daylight saving changes, a 2 MB generated feed) agreed in every instance; and the engine build before and after the removal gave the same events in all of 1236 windows of the public and invented feeds.

The old code stays **in a build without the option**: that is the base project's `agenda`, which does not carry libical (and an `agenda` build that does not want 96 KB more flash). It is the code the tests of
`host_tests/test_calendar_ics.cpp` run on. It can only go if the base project takes libical in as well.

## Limits and cost

- **Flash:** +98,160 bytes (about 96 KB) on a Waveshare build (ESP32-S3), measured against the same build with `agenda` only.
- **RAM:** libical allocates while it works (the rule, the iterator) and the adapter frees all of it at the end of every call. Measured on the host with a counting `malloc` (64-bit pointers, so an overestimate for the ESP32) while a 2 MB
  feed with monthly and yearly rules was read: the engine adds about **16 KB** to the peak (1126 allocations, none larger), and nothing stays allocated. A TLS connection that follows finds the same internal heap as without the option.
- **Boards:** run on the Waveshare 7.3" (ESP32-S3); compiled and linked, not run, for the XIAO EE02 (ESP32-S3) and the M5Paper (ESP32).
- **One task at a time:** libical is built without threads (`ICAL_SYNC_MODE_NONE`), the adapter serialises its calls with a mutex.
- **Time** (ESP32-S3, a generated 2 MB feed of about 4000 events, a 30-day window): reading the file from storage takes 2.0 s, the parse 2.2 s when the rules are ones the simple expander takes too and 3.0 s for a feed full of
  monthly, yearly and `BYDAY`-list rules; with the flat cache written back the whole re-expansion takes 4.3 - 5.1 s (the simple expander alone: 4.5 s measured before the option existed). The re-expansion happens
  when a source changes and about once a month, not on every wake (see [CALENDAR_RRULE_SUPPORT.md](CALENDAR_RRULE_SUPPORT.md)). The 2 s that were proposed as a target for the whole thing cannot be met by anything that
  reads such a file: the read alone takes that long.
- **Candidates per series:** at most `ICS_MAX_CANDIDATES` (the list limit plus the `RDATE` room) instances of one series are kept, the earliest ones.

## How it is checked

- `host_tests/test_calendar_rrule.cpp`: the adapter against the instances python-dateutil makes (19 rules - leap day, `BYSETPOS`, `WKST`, far-past starts, all-day), 50 refused rules and the
  bounds. `host_tests/test_calendar_ics_rrule.cpp`: `calendar_ics.c` with the option on (`UNTIL`, `EXDATE`, `RDATE`, `RECURRENCE-ID`, durations, daylight saving time).
- A random-rule differential (thousands of rules, libical against dateutil) and an exhaustive `WKST` run found the three refused families above.
- A mutation fuzzer over `calendar_rrule_expand()` (random and broken rule texts in exact-size buffers, random starts and windows, ASan / UBSan / leak, the count, order and window of the result checked): 2.1 million rules over seven
  seeds, about a quarter accepted. It found one defect, a read of two bytes past the end of a rule that ends in `WKST=`, which is fixed and has a test.
- The 97 public calendars of python-recurring-ical-events as fixtures, ASan / UBSan / leak runs of the tests, a mutation fuzzer over the feed parser (64,000 rounds with the engine compiled in, rule fragments among the insertions).
- **The parallel comparison with the old expander** (a debugging build, gone again with the old expander, see above): 4068 series over 1176 windows on a PC and 110 series on the frame (Waveshare, ESP32-S3, libical compiled for the
  Xtensa), no difference. It is the check that libical, built for the Xtensa, gives what it gives on the host.
- After the removal: the engine build gives the same events as before in 1236 windows of the public and invented feeds, and the tests of a single event with an `EXDATE` or `RDATE` are new.

## Where to look in code

- `components/libical/` - the vendored library, `UPSTREAM.md` (tag, commit, checksum, update procedure via `vendor.sh`), `port/config.h` (our build configuration), licence texts. The
  component is empty without the option. libical is used under the **MPL-2.0** ([notice](third_party/LIBICAL-NOTICE.md)).
- `main/calendar_rrule.c` / `.h` - `calendar_rrule_expand()`: the checks, the iterator, the bounds.
- `main/calendar_ics.c` - with the option: `gather_with_engine()` (window, `UNTIL`, overlap, hidden instances) and `gather_single()`; without it: `parse_rrule()` and `gather_simple()` (the reader's own expander).
