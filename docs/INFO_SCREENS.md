# Information screens

> **Build option:** compiled in only with `python build.py --with info-screens` (needs `agenda` and `glyphs`; the build pulls
> them in); without it the firmware is the upstream firmware (see [FEATURES.md](FEATURES.md)). On its own this option adds no
> new page - it is the base that pages such as the [chore wheel](CHORE_WHEEL.md) (`--with chore-wheel`) are built on.

The Agenda draws a page of ToDo items and calendar events whenever its schedule fires (Settings -> Agenda -> Schedule). With
this option the frame can also draw **other full-screen pages** on that same schedule: every time the schedule would draw the
Agenda, the next page of the rotation is drawn instead. The Agenda itself is one member of the rotation.

A picture of every page, and of the Agenda it takes turns with: [SCREENSHOTS.md](SCREENSHOTS.md).

## Settings

Settings -> Agenda -> **Information screens**: tick the pages that take part. The Agenda is ticked by default, so a firmware
with this option behaves as before until a page is added.

- One ticked page: every run of the schedule draws that page. Tick only "Agenda" for the plain Agenda.
- Several ticked pages: they take turns in a fixed order (Agenda first, then the others in the order of the list). With a
  schedule of `0 */12 *` and the Agenda plus the chore wheel ticked, the frame shows the Agenda at 00:00 and the chore wheel at 12:00.
- Nothing ticked: the Agenda (a rotation cannot be empty).
- With `--with schedule-pages` each schedule can draw pages of its own, with priorities and a minimum time between two displays: [SCHEDULE_PAGES.md](SCHEDULE_PAGES.md).
- The schedule is active as soon as a page other than the Agenda is ticked, even if neither ToDo nor a calendar is switched on.
  An Agenda without ToDo and calendars has nothing to show and is skipped in the rotation.

Over the API: `GET /api/config` reports `info_screens` (the ticked pages, by name: `agenda`, `chore-wheel`, `weather`, `fact`, `finance`, `fuel`, `markets`) and
`info_screens_available` (the pages this firmware contains); `PATCH /api/config` accepts `info_screens` as a list of names
(unknown names are ignored).

## The pages

| Page | Build option | Details |
| --- | --- | --- |
| Agenda | `agenda` | [CALENDAR_RRULE_SUPPORT.md](CALENDAR_RRULE_SUPPORT.md) |
| Chore wheel | `chore-wheel` | [CHORE_WHEEL.md](CHORE_WHEEL.md) |
| Weather | `weather-screen` | [WEATHER_SCREEN.md](WEATHER_SCREEN.md) |
| Fact of the day | `fact-of-the-day` | [FACT_OF_THE_DAY.md](FACT_OF_THE_DAY.md) |
| Exchange rates | `finance-snapshot` | [FINANCE_SNAPSHOT.md](FINANCE_SNAPSHOT.md) |
| Fuel prices | `fuel-prices` | [FUEL_PRICES.md](FUEL_PRICES.md) |
| Markets (stocks, ETFs, crypto) | `market-quotes` | [MARKET_QUOTES.md](MARKET_QUOTES.md) |

The drawing toolkit that the pages share also has big stroke digits (`main/screen_digits.c`) for numbers that should fill a
quarter of the panel.

## Language

The pages use the language of the on-display text - the overlay language setting (Settings -> Overlays -> Overlay language:
English or German), the same the Agenda follows; a build without the `overlays` option stays English. German month and weekday
names come with real umlauts thanks to the `glyphs` option.

## How it works

- **Rotation:** a counter kept in the settings memory (so it goes on after a sleep) selects the next page among the ticked
  ones. It only moves when there is more than one page in the rotation.
- **Drawing:** every page is a plain function that draws into an RGB canvas the size of the panel (`main/screen_canvas.c`: text in
  the frame's 17x24 font at 1x-4x, rectangles, discs, ring sectors, pills), which is written like the Agenda's picture and shown
  through the normal display path. The functions know nothing about the hardware, so they are tested on a PC and can be looked at
  as pictures for every board size (`host_tests/render_screens.cpp`).
- **Sizes:** the layout follows the panel: 800x480, 960x540, 480x800, 1200x1600 and 1872x1404 were checked, landscape and portrait.
- **Memory:** the canvas (width x height x 3 bytes) is allocated in PSRAM for the moment of drawing and freed after it.

## Adding a page

A page is a `screen_<name>.c` with a render function, one entry in `info_screens.h` (the ids are bits of a stored mask - do
not renumber) and `info_screens.c`, its own build option that requires `info-screens`, and its settings. See
`main/screen_chore_wheel.c` for a complete one.
`main/screen_weather.c` shows the pattern for a page with data from the internet: the drawing takes a plain data struct with a status
(ok / no network / no answer / ...), the fetch lives in `info_screens.c`, and both are tested on the PC (`host_tests/test_screens.cpp`,
`test_finance.cpp`, `test_fuel.cpp`, `test_market.cpp`) with real recorded answers as fixtures.

## Notes on the pages

Pages whose data come from the internet (weather, exchange rates, fuel prices, markets) end with a small note that says **when the data were fetched**, in the frame's local time:
`Updated 30 Sep 14:35` (English) or `Stand 30.09. 14:35` (German) on the weather page; on the exchange-rate, fuel and markets pages the note shares the line of the
source - `Yahoo Finance, Twelve Data - 30 Sep 14:35` - and takes a second line only where the panel is too narrow for both. The markets page shows the time of its newest fetch,
so a page drawn from the kept prices (blue) tells how old they are. A frame whose clock was never set draws no note: a wrong time is worse than none.

The pages with a line chart (exchange rates, markets) name the **day of the newest data point** in the header - `Close 30 Sep` (`Schluss 30.09.`) for prices, `Rates 30 Sep`
(`Kurse 30.09.`) for the ECB's rates - so that it cannot be taken for today's date, and put a small yellow **`!`** behind it when that day is **older than the last trading day**.
They write **how long the lines run**, in calendar days from the first to the last point (`41 d`, German `41 T`), once after the heading when all lines run the same time
(`MARKETS  41 d`), and under each chart the **change over the whole line** (`+6.9%`) next to the change of the last day.

## Limits

- The pages only redraw when the schedule fires; there is no live clock.
- One turn of the schedule draws one page, and it stays until the next turn. Drawing takes the panel about 20 s plus the fetch of a page with data from the
  internet, and a colour panel should not be refreshed more often than its maker allows (Waveshare: not more often than every 3 minutes) - so the schedule
  wants an interval of 3 minutes or more; with `*/3 * *` and six pages each page is on the panel for 3 minutes out of 18.
- Pages with data from the internet (weather, exchange rates, fuel prices, markets) fetch it when they are drawn - one request each (the markets page one per symbol) - and show a message that says why
  when the frame has no network on that wake or the service does not answer; the next turn of the rotation tries again. Nothing is cached between turns, except that the markets page keeps its last good
  answers on the storage and shows them (in blue) when it cannot fetch new ones.
