# Weather screen

> **Build option:** compiled in only with `python build.py --with weather-screen` (needs `info-screens` and `overlays`; the build
> pulls them in). Without it the firmware is the upstream firmware (see [FEATURES.md](FEATURES.md)).

A full-screen weather page for the [information screens](INFO_SCREENS.md): today's weather as a big icon with a very large
temperature, and the next four days as rows. It is drawn on the schedule of the Agenda (Settings -> Agenda -> Schedule) whenever
its turn comes in the rotation.

## Settings

- Settings -> Agenda -> Information screens: tick **Weather**.
- The place, the weather service (Open-Meteo, wttr.in or yr.no) and the language come from the **Overlays** tab (Weather location,
  Overlay language) - the same settings the photo overlay and the Agenda's calendar use, so nothing is entered twice. The place can be
  a name (it is looked up once) or coordinates.
- The frame must be online when the page is drawn; the forecast is fetched at that moment (one request, no key).

## What is on the page

| Part | Content |
| --- | --- |
| Header | the place, `TODAY` (`HEUTE`) in red when the forecast starts today, and the date |
| Hero | a big icon, today's high in very large digits, today's low in blue under it, the condition in words |
| Rows | up to four further days: weekday, a small icon, the high in black and the low in blue |

The temperatures are rounded degrees Celsius. The icons are drawn from shapes at any size (a yellow sun, a white outlined cloud,
blue rain, black snow flakes, a yellow lightning bolt); the numbers are drawn with thick round strokes, so they stay smooth at any size.
The condition texts are whole words in English and German ("Light rain", "Leichter Regen") for every WMO weather code the
providers answer with; a code that is not in the list is shown as "Unknown".

If there is nothing to show, the page says why instead of showing an empty picture: no place set, no network on this wake, or the
weather service did not answer (the next turn tries again).

## Limits

- The forecast has the daily high, low and condition only - no current temperature, wind, humidity or precipitation chance (the frame's
  weather module does not request them). The big number is therefore *today's high*, labelled by the low under it.
- wttr.in gives three days, the rest is taken from Open-Meteo (as for the Agenda's grid); yr.no buckets days by UTC date.
- No feels-like, sunrise or sun-position data.
