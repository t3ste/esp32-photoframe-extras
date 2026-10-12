# Calendar priority, the order of the entries and the calendar week

> **Build option:** compiled in only with `python build.py --with agenda`; without it the firmware is the upstream
> firmware (see [FEATURES.md](FEATURES.md)). Web UI: **Settings -> Agenda -> Calendar**.

The Calendar view merges up to five calendars (A-E) into one list per day. Three settings decide what that list looks like
when there is more of it than the screen has room for.

| Setting | JSON key | Default | What it does |
| --- | --- | --- | --- |
| Calendar priority | `agenda_cal_priority` | `ABCDE` | The calendars from the most to the least important. `C,A,B,D,E`, `e > d > c > b > a` and `ABCDE` all work: each of the letters A-E must appear exactly once, case and separators do not matter. Anything else is refused with `400` and a message. |
| Order within a day | `agenda_cal_order` | `priority` | `priority`: the entries of calendar A first, then B, ... - each calendar by time. `time`: strictly by time, the priority only decides between entries that start together. |
| Show the calendar week | `agenda_cal_show_week` | `true` | The ISO week on today's divider (list) or cell (grid) and on every Monday's, e.g. `Mo 12. KW42` (`CW42` with the English overlay language). |

## Which entries are given up

A column of the list layout, or a cell of a 7-day grid, has a fixed number of rows. When the entries do not fit, the frame
used to cut the list at the bottom, so whatever happened to be last - often an entry of a calendar that matters - was only
counted in `+N more`. Now **the entries of the calendars of the lowest priority go first**:

- *List layout:* every day keeps its divider (the forecast sits there), and the remaining rows go to the entries by priority
  and, inside a priority, in time order - wherever in the window they are. Without room for even the dividers the list is cut
  at the bottom as before.
- *7-day grid:* the same rule inside each cell.
- What is given up is only counted in `+N more`.

The order is a total one: the all-day entries of several calendars all start at midnight, and `qsort` used to put them in an
arbitrary order that changed from one refresh to the next. Ties are decided by the end, the calendar and the position in the
feed (`main/agenda_order.c`).

## The rotation pattern in the list layout

The "shift" models (2-2-3, week / week, 3-4: **Settings -> Agenda -> Calendar -> Rotation pattern**) marked days only in the
7-day grid. The list layout (1-3 days, the default) shows the marking too: the header of a marked day takes the colour profile's
mark colour when the profile's `markColorsHeader` is on, and the block of the day's entries is filled with it when it is off -
the same rule as in the grid.

## How it is tested

`host_tests/test_agenda_order.cpp` covers the parsing of the priority, the ordering (including a property test that it is a
total order), the selection of the entries that stay, and the ISO week (known dates and a 60-year walk over the year
boundaries). The drawing itself is checked with a host render harness of the Agenda (the real `agenda_renderer.c` on a PC, made-up
calendars) and on a frame.
