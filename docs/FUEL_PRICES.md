# Fuel prices

> **Build option:** compiled in only with `python build.py --with fuel-prices` (needs `info-screens` and `overlays`; the build pulls them in
> together with what they need). Without it the firmware is the upstream firmware (see [FEATURES.md](FEATURES.md)).

A page for the [information screens](INFO_SCREENS.md) with the **cheapest petrol stations around a place**: the cheapest first, each with
its brand, street and town, the distance and the price in big digits with the third decimal small, like on the pump. **Germany only** - the
prices come from the Markttransparenzstelle fuer Kraftstoffe through the free [Tankerkoenig](https://creativecommons.tankerkoenig.de) API.

## Settings

Settings -> Agenda -> Information screens: tick **Fuel prices**, then

| Field | Meaning |
| --- | --- |
| API key | your personal key from creativecommons.tankerkoenig.de (free, after a registration there). It is **write-only**: the frame never returns it, it is not in a normal settings export (only in the export that includes credentials), and a button removes it |
| Fuel | Super E5, Super E10 or Diesel |
| Radius | 1 to 25 km around the place |
| Stations | how many stations to show, 1 to 5 |
| Hide closed | leave out stations that are closed right now (on by default) |

The **place** is the one of the weather (Settings -> Overlays -> Weather location: a name, which the frame looks up once, or latitude and
longitude); nothing is entered twice. The frame must be online when the page is drawn: it asks the service once, at that moment.

## What the page shows

- A yellow header with the fuel type and the radius.
- One row per station, the cheapest first (green bar and a green number for the cheapest): brand (or name), street and town, the distance in
  kilometres, and the price in euros per litre.
- The attribution the licence asks for - `tankerkoenig.de, CC BY 4.0` - and the time the prices were fetched, at the bottom.

A station without a price is never shown as 0. If there is nothing to show the page says why: no key, no place, no network on this wake, the
service did not answer, the service refused the request (with its own words, for instance for a wrong key), or no open station with a price in the
radius.

## Notes

- The answer is read one station at a time (a dense town can send 100 KB), and an answer that was cut off still gives the stations before the cut.
- The key is checked for letters, digits and dashes before it goes into the request; so are the coordinates.
- Limits of the service (rate, terms) are those of tankerkoenig.de; with the schedule of the Agenda at every few hours the frame is far below
  what they ask for.
