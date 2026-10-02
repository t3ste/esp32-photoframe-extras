# Demo package

> Board: `waveshare_photopainter_73` only, full-feature firmware only, for now.

A ready-to-import example configuration that shows what the [optional features](FEATURES.md) can do, built
entirely from invented, public example data - no private information of any kind. The files live in
[`examples/waveshare_photopainter_73/`](../examples/waveshare_photopainter_73/): two importable configurations,
five example calendars, an example todo.txt, three Calendar color profiles and five generated placeholder photos.

## Try it

1. Flash or update to a release build (it already has every feature this board supports).
2. Web UI → **Settings → Maintenance → Config Backup → Export Config** first, to keep your own settings (tick
   "Include credentials and URLs" for a backup you can fully restore from later - importing overwrites your
   current settings, though never your WiFi credentials or device name, which the example file never touches).
3. **Import Config**, choosing one of the two example files:
   - `demo-config-url.json` - a random photo from a public source every 10 minutes; weather shows on the Agenda
     calendar's day dividers.
   - `demo-config-storage.json` - photos from the frame's own album (upload the five generated sample photos, or
     your own) with a one-line weather overlay, plus the low-battery and climate badges.
4. Either way, a 7-day Agenda calendar (five example calendars, one with 8 events on Monday alone) renders every
   15 minutes. Calendars C, D and E are downloaded only once, when their URL is saved: if the import ran without a
   working connection, press **Refresh now** next to each of them in **Settings -> Agenda**.
5. Optional: import one of the three color profiles from `color_profiles/` into a slot of **Settings -> Agenda ->
   Calendar color profiles** and pick it as the active one - the configuration does not touch the profiles.

The two profiles exist because the firmware can't draw an overlay on a URL-fetched photo (that mode streams the
image straight to the display and never creates a file to draw on) - so the URL profile puts the weather on the
calendar instead, and the storage profile puts it on the photo.

## The configuration, briefly

Both files set only the `config` section (never `processing`, `palette` or `albums`, which would overwrite your
own calibration). What's in them:

- **Time zone and weather**: Paris (`CET-1CEST,M3.5.0,M10.5.0/3`), with the coordinates set explicitly alongside
  the location name, so an existing saved location can't silently outrank it.
- **Photo rotation**: every 10 minutes; the URL profile fetches from [cataas.com](https://cataas.com/) (a public,
  keyless "random cat photo" API - free to swap for any URL you like), the storage profile rotates the frame's
  own album.
- **Agenda**: calendar A-B refresh automatically; C-E are fetched once (on save/refresh) and stay valid for about
  a month. The 7-day grid layout is used, which needs the ToDo column switched off - it is.
- **ToDo**: the URL is saved but the switch is off, so the grid stays 7 days; turn "Show ToDo list" on in the
  Agenda tab to see it (the layout then falls back to the list view).
- **Chimes**: on, with quiet hours 22:00-07:00; the rotation beep itself is off (a beep every 10 minutes would be
  irritating).
- **Climate**: logging on, badge on for the storage profile.
- **The alarm clock is left alone** - the import neither arms one nor clears a schedule you already have. Try it
  separately: [ALARMCLOCK_USER_GUIDE.md](ALARMCLOCK_USER_GUIDE.md).

## Example content

**Calendars A-E** (`examples/waveshare_photopainter_73/calendars/`): every event repeats weekly (so the calendars
never go stale) with a different color per source. Calendar A ("Family") has 8 events on Monday alone plus one on
each other day; B ("Work") adds a 3-day multi-day event; C ("Sport"), D ("School") and E ("Household") are
smaller. All names are invented.

**todo.txt**: a handful of tasks in the standard [todo.txt](https://github.com/todotxt/todo.txt) format -
priorities, `+project`/`@context` tags, a mix of overdue, upcoming and undated due dates. Since the dates are
fixed (not relative to "today"), they need occasional manual refreshing to keep the "one overdue, several
upcoming" mix realistic.

**Sample photos** (`examples/waveshare_photopainter_73/photos/`): five generated 800×480 gradients, for the
storage profile - no camera, no licence question. Regenerate with `python scripts/generate_demo_photos.py`.

**Color profiles** (`examples/waveshare_photopainter_73/color_profiles/`): three exports of the Agenda's Calendar
color profiles (the same file `profile-editor.html` and the Settings -> Agenda -> Calendar color profiles
**Export** button produce), differing in their marking color (white, yellow, blue). Import one into a profile slot
(1-3) to see the Calendar view in it - they hold color choices only. How profile 1 looks:
[SCREENSHOTS.md](SCREENSHOTS.md#a-colour-profile).

## What it can't show

WiFi, Telegram, and any other personal setting are not part of example data and are left untouched by the import.
Yearly/monthly calendar events (birthdays, holidays) aren't supported by the calendar parser
([details](CALENDAR_RRULE_SUPPORT.md)), so the example calendars only use weekly recurring events, and a static
ToDo file can't show "due today" matching the date you actually try it.

## Status

The example files are served from this project's GitHub Pages site, each under its own address:
[demo-config-url.json](https://t3stier.github.io/esp32-photoframe-rebuild/examples/waveshare_photopainter_73/demo-config-url.json),
[demo-config-storage.json](https://t3stier.github.io/esp32-photoframe-rebuild/examples/waveshare_photopainter_73/demo-config-storage.json), `https://t3stier.github.io/esp32-photoframe-rebuild/examples/waveshare_photopainter_73/todo.txt`,
`https://t3stier.github.io/esp32-photoframe-rebuild/examples/waveshare_photopainter_73/calendars/calendar-a.ics` and so on (the folder address itself has no page, so there is nothing to open there); every deploy of the
site copies the whole `examples/` tree and checks that the URLs the two configs use answer `200`. GitHub's CDN can
keep serving a cached `404` for up to ten minutes after a file first appears, so an import made right after a
deploy may need a **Refresh now** for calendars C-E. See [MAINTAINING.md](MAINTAINING.md) for how it is deployed
and checked.
