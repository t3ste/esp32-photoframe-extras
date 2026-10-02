# Demo package: Waveshare PhotoPainter, full firmware

Overview: [docs/DEMO_PACKAGE.md](../../docs/DEMO_PACKAGE.md). A ready-to-import example configuration for the full-feature
release firmware of the `waveshare_photopainter_73` board, built entirely from invented, public example data - no
private information of any kind. It shows most of what the optional features do without needing a Telegram bot, a
Home Assistant instance, or a real calendar.

## What is in here

| File | What |
| --- | --- |
| `demo-config-url.json` | Importable configuration: photos from a public random-image source, forecast on the calendar's day dividers |
| `demo-config-storage.json` | Same, but photos come from the frame's own album and carry a one-line weather overlay and the low-battery badge |
| `calendars/calendar-a.ics` .. `calendar-e.ics` | Five example calendars (A-E), all fixed **weekly** recurring events so they never go stale |
| `todo.txt` | An example [todo.txt](https://github.com/todotxt/todo.txt) list, in the format the Agenda's ToDo column reads |
| `photos/` | Five generated placeholder photos (no camera, no licence question) for the storage profile |
| `color_profiles/` | Three ready-made Calendar color profiles (exports from a device, differing in their marking color), to import into the Agenda tab's profile slots 1-3 |

## How to try it

1. Flash or update to a release build (the release firmware already has every feature this board supports).
2. Web UI -> **Settings -> Maintenance -> Config Backup -> Export Config** first, to keep your own settings (tick
   "Include credentials and URLs" for a complete backup you can fully restore later).
3. **Import Config**, choose `demo-config-url.json` (or `demo-config-storage.json` - see below).
4. For the storage profile only: **Gallery** (the tab next to Settings, not inside it), upload the five files from
   `photos/` (or any photos of your own) - uploading without picking a different album lands them in "Default",
   which is enabled from the factory. If you upload into a different/new album instead, switch its own toggle in
   the Gallery on, or storage rotation has nothing enabled to rotate through.
5. Calendars C, D and E are downloaded **once**, when their URL is saved, and kept on the frame for about a
   month. If the import ran while the files were not reachable (no WiFi yet, or the site was not deployed), press
   **Refresh now** next to each of them in **Settings -> Agenda** - importing the same URL again does not fetch
   again. Calendars A and B and the ToDo list are fetched by the frame on its own every Agenda cycle, so those
   catch up without any action.
6. Optional: **Settings -> Agenda -> Calendar color profiles**, **Import** one of the files in `color_profiles/`
   into a slot (1-3) and pick it as the active one - the configuration itself does not touch the profiles.
7. Watch the frame: a new photo every 10 minutes, the 7-day calendar every 15 minutes.

To go back to your own settings, re-import the backup from step 2.

## The two profiles

The firmware cannot draw the weather overlay on a URL-fetched photo (that mode streams the picture straight to the
display and never creates a file to draw on - see [docs/OVERLAYS.md](../../docs/OVERLAYS.md)), so there are two
configurations instead of one:

- **URL** (`demo-config-url.json`): `image_url` fetches a random photo from [cataas.com](https://cataas.com/)
  (a public "cat as a service" API, a new photo on every request, no key needed) every 10 minutes. The weather
  instead shows on the Agenda's day dividers.
- **Storage** (`demo-config-storage.json`): photos come from the frame's own album (upload the five files in
  `photos/`, or your own), with a one-line weather overlay, the low-battery badge and the climate badge switched on.

Both keep `image_url` set (so switching rotation mode back to `url` needs no re-typing) and share the calendars,
timezone and weather location.

## What it demonstrates and what it deliberately does not

- **Timezone and weather**: Paris (`CET-1CEST,M3.5.0,M10.5.0/3`; latitude/longitude set explicitly so an old
  location on the frame cannot silently win over the name).
- **Agenda / Calendar**: the 7-day grid (`grid_a` layout - this needs the ToDo column switched off, which it is)
  filled from calendars A ("Family", 8 events on Monday alone) through E ("Household"); a multi-day event on
  Calendar B; every event repeats weekly so the demo never goes stale, and the day dividers carry the Paris
  forecast.
- **ToDo**: the todo.txt URL is saved but the column stays **off**, so the layout keeps the 7-day grid - switch
  "Show ToDo list" on in the Agenda tab to see it (the layout then falls back to the list view, by design).
- **Chimes**: on with a quiet-hours window (22:00-07:00); rotation itself is muted (a beep every 10 minutes would be
  irritating), low-battery/OTA/critical-error chimes stay on.
- **Climate**: logging is on (fills the Climate History tab) on a board with the sensor.
- **The alarm clock is intentionally left alone** by this configuration - it neither sets a schedule nor clears
  one you already have. See [docs/ALARMCLOCK_USER_GUIDE.md](../../docs/ALARMCLOCK_USER_GUIDE.md) if you want to
  try it.
- **What cannot be demonstrated with example data at all**: Telegram (a bot token and chat ID are inherently
  personal), Home Assistant, and anything that needs your own WiFi, device password or API keys - none of that is
  in these files, and importing them will not touch your WiFi credentials or device name.
- **RRULE limits**: the calendar parser only understands `DAILY`/`WEEKLY` recurrence (see
  [docs/CALENDAR_RRULE_SUPPORT.md](../../docs/CALENDAR_RRULE_SUPPORT.md)), so a yearly event like a birthday
  cannot be shown by a static example file; the ToDo's `due:` dates are fixed, so "due today" will not line up
  with the date you actually try this.

## Hosting

These files are served from this project's GitHub Pages site alongside the web flasher, each under its own address:
[demo-config-url.json](https://t3stier.github.io/esp32-photoframe-rebuild/examples/waveshare_photopainter_73/demo-config-url.json),
[demo-config-storage.json](https://t3stier.github.io/esp32-photoframe-rebuild/examples/waveshare_photopainter_73/demo-config-storage.json) and the others in the table above (the folder address
`https://t3stier.github.io/esp32-photoframe-rebuild/examples/waveshare_photopainter_73/` itself has no page) - the URLs inside the two JSON files point there. The site's `deploy-pages` job copies this whole `examples/` tree on every deploy and then
checks that every URL the two configs use answers `200` (see [docs/MAINTAINING.md](../../docs/MAINTAINING.md)
section 10). GitHub's CDN may keep serving a cached `404` for a URL for up to ten minutes after the file first
appears - if an import fails right after a deploy, wait a little and press **Refresh now** (step 5 above).
Testing locally: point a device at a temporary local HTTP server instead, or edit the URLs in the JSON files before
importing (the frame logs a failed download and fails soft - see
[docs/CALENDAR_RRULE_SUPPORT.md](../../docs/CALENDAR_RRULE_SUPPORT.md)).

## Regenerating the sample photos

```sh
python scripts/generate_demo_photos.py
```
