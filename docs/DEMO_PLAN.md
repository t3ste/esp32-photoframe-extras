# Plan: demo package for the `waveshare_photopainter_73` (full firmware)

Status: **plan only - nothing of it is built** (written 2026-09-28, no code was changed for it). Audience: the
maintainer, and whoever implements it later (read [MAINTAINING.md](MAINTAINING.md) first).

## 1. Goal

A user who has flashed the release firmware on a `waveshare_photopainter_73` can **import one example
configuration** in the Web UI (Settings -> Maintenance -> Config Backup -> Import Config) and immediately see the
firmware do everything it can do, with **representative example data and no private data**:

- a rotating photo from the internet, on a 10-minute schedule,
- a 7-day calendar (Agenda) every 15 minutes, filled from five example calendars A-E,
- a ToDo source that is stored but switched off,
- weather and time zone of Paris,
- an alarm every hour, chimes, climate readout.

Auxiliary files (calendars, ToDo file, optional sample photos, a short README) are hosted publicly so that the
imported configuration can point to them. Only this one board and its full-feature firmware are covered for now
(the maintainer has no other hardware to test); other boards can follow the same pattern later.

Non-goals: no firmware change, no new feature, no Telegram demo with a real bot, nothing that needs a user account.

## 2. Findings that change the original idea

Everything below was checked in the source or against the live services on 2026-09-28.

| Idea | Finding | Consequence |
| --- | --- | --- |
| `image_url` = `https://loremflickr.com/800/480` | The service now answers every non-browser request with **HTTP 401 and a JavaScript "Bot check" page** (89 KB HTML; tried with the frame's own User-Agent, over http and https, every path variant). The frame gets no picture. | Unusable. Replace the source (see below). |
| Any random-photo service | `picsum.photos` returns **progressive JPEGs** and redirects to a second host. The firmware's JPEG decoder (`esp_jpeg`/tjpgd) supports baseline only (the Telegram code documents the same limitation), and a pinned TLS issuer of the first host would not validate the second. `placedog.net` and `placebear.com` answered three requests each with the *same* picture. | Not suitable. |
| Working source | `cataas.com/cat?width=800&height=480`: a **different baseline JPEG, 800 x 480, on every request**, `Content-Length` set, no redirect over https; `http://` answers 301 -> https. | Recommended source. Cats only - it is a demo, and the URL is one line the user can change. |
| HTTPS `image_url` | Saving an https `image_url` makes the frame **pin the server's issuer certificate during the import request** (`main/cert_pin.c`); a failure there rejects that field. Only the *issuer* (intermediate) is pinned, and Let's Encrypt (cataas' issuer) may sign a renewed certificate with a different intermediate, so a pin can silently break later. `sdkconfig.defaults` has `CONFIG_ESP_TLS_INSECURE` + `SKIP_SERVER_CERT_VERIFY`: **without a pin the frame does not verify the server**. | Recommended value is the **`http://` URL**: no pin to break, the 301 to https is followed (to be confirmed on a device, test T3). The https form is the fallback. |
| Weather overlay on the URL-rotation photo | **Not possible.** URL mode streams the download row by row to the panel and never produces a processed image file (`fetch_stream_display` in `main/utils.c`); `overlay_manager_apply()` is only called for stored images (Storage rotation, gallery "Display Image", Telegram photos). [OVERLAYS.md](OVERLAYS.md) says so as well. | Two profiles (section 3). In the URL profile the weather appears where it *can*: the forecast on the Agenda's day dividers. |
| Agenda every 15 min next to a photo every 10 min | Agenda is **not an overlay**: a matching wake uses the whole display for ToDo/Calendar. On a tick where both fire, **agenda wins** and the photo skips its slot; the **alarm wins over both** (deep-sleep path; when the frame stays awake the alarm blocks for its ring time and the others follow). | Timeline in section 6. |
| 7-day calendar + ToDo "off but linked" | The 7-day grid **only takes effect while the ToDo column is off** (falls back to the list layout otherwise). This is what the maintainer asked for anyway. | Fits; note it in the README (turning the ToDo on switches the layout). |
| Calendar C/D/E | They are fetched **once** when their URL is saved or changed (or on "Refresh now"), the raw file is stored on the frame, and recurring events are **re-expanded from that stored file every 30 days on their own**. At most 48 events per expansion window. A/B are fetched (conditional GET) on every agenda wake. | Weekly recurring events stay alive on C-E indefinitely; keep each of them at <= 10 weekly series. |
| RRULE support | Only `FREQ=DAILY/WEEKLY`, `INTERVAL`, `COUNT`, `UNTIL`, and a single `BYDAY` equal to `DTSTART`'s weekday. `MONTHLY`, `YEARLY`, several `BYDAY`s, `EXDATE` are skipped silently ([CALENDAR_RRULE_SUPPORT.md](CALENDAR_RRULE_SUPPORT.md)). Times without `Z` are read in the frame's own time zone. | Example events use weekly rules with floating times: they never go stale and show the same wall-clock times in Paris. Birthdays/holidays (yearly) cannot be shown by a static file. |
| Import of URLs | `PATCH /api/config` accepts the (write-only) calendar and ToDo URLs; the Web UI's exporter merely leaves them out unless "Include credentials" is ticked. The importer sends the file's `config` as it is. The extra calendars C/D/E download **during** the import. | The example file can carry all six URLs. The import needs the frame online. |
| Weather location | A cached geocode (`weather_lat`/`weather_lon` from an earlier location) is indistinguishable from manual coordinates and **wins over the location name** (`resolve_lat_lon()` in `main/weather.c`). | The file must set `weather_lat`, `weather_lon` **and** the name, otherwise a user's old location still shows. |
| Other importable data | The importer applies `config`, `processing`, `palette`; `albums` are ignored in this fork's importer even though the confirmation dialog mentions them. | Leave `processing` and `palette` **out**: importing them would overwrite the user's palette calibration. |
| Import failure mode | With `FORK_FIXES` each field is applied on its own, so one failing field (e.g. a pin failure) does not discard the rest - but the request still answers 400 and the Web UI says "Failed to import config". | README: "if it says failed, check the message, most fields were applied". |

## 3. Proposed package

**Two profiles**, because the wish "URL photos *and* weather overlay" is not achievable without a firmware change:

| Profile | File | Photos | Weather |
| --- | --- | --- | --- |
| **URL** (what was asked for) | `demo-config-url.json` | `rotation_mode: url`, cataas photo every 10 min | forecast on the Agenda's day dividers (7 days); no overlay |
| **Storage** | `demo-config-storage.json` | `rotation_mode: storage` (photos from the frame's own albums), same 10-min schedule | **one-line weather overlay** on every photo, plus the low-battery badge and the climate badge |

Both profiles share calendars, ToDo link, Paris, schedules, alarm and chimes. Storage needs a few photos in an
album: the user's own, or the optional generated sample photos (section 5.3). The `image_url` stays stored in the
storage profile so switching back is one click.

A firmware change that draws overlays on URL-mode photos would remove the need for two profiles; it is large (the
photo is never held as a file or a full RGB buffer in that path) and out of scope here.

**Layout in the repository** (new, tracked; the name `examples/` is a proposal):

```
examples/waveshare_photopainter_73/
  README.md                    user guide: what it does, how to import, timeline, how to undo, what cannot be shown
  demo-config-url.json         importable, section 4
  demo-config-storage.json     importable, differs only in a handful of keys
  calendars/calendar-a.ics ... calendar-e.ics
  todo.txt
  photos/                      optional generated sample photos (section 5.3)
```

**Hosting.** The imported file contains absolute URLs, so the files must be served from a stable public address.

| Option | For | Against |
| --- | --- | --- |
| **GitHub Pages of the fork** (`https://t3stier.github.io/esp32-photoframe-rebuild/examples/waveshare_photopainter_73/...`) - **recommended** | Same origin as the web flasher; versioned with the repository, reviewed and tested in CI; needs no extra account; stable URL; served with `Content-Length` and ETag | Goes live only with the next `deploy-pages` run (`build.yml` needs a copy step for `examples/` into the site); the mirror has no Pages |
| GitHub Gists (the maintainer's suggestion) | Instant edits; one file per gist | One gist per file and per account, the account name appears in every URL, not versioned with the code, cannot be tested in CI, `raw` URLs pin a revision or follow "latest" unpredictably |
| `raw.githubusercontent.com/<fork>/main/examples/...` | Live right after a push; no build step | `text/plain`, ties the URLs to the branch name and the host's cache (5 min), mirror and fork differ |

Recommendation: Pages as the primary address; `raw.githubusercontent.com` is the fallback if Pages is down (the file
can simply be edited). Gists only if the maintainer prefers to edit calendars without commits.

## 4. The configuration in detail

The file has one top-level key, `config`. (`processing`, `palette` and `albums` are deliberately absent.)
`<BASE>` = the Pages address above. Values are the URL profile; the storage profile's differences are marked S.

| Key | Value | Why |
| --- | --- | --- |
| `timezone` | `CET-1CEST,M3.5.0,M10.5.0/3` | Paris (Central Europe with DST); all schedules and event times follow it |
| `display_orientation` | `landscape` | The frame's native format |
| `auto_rotate` | `true` | |
| `rotate_cron` | `["*/10 * *"]` | Photo every 10 minutes (`minute hour weekday`) |
| `rotation_mode` | `url`; S: `storage` | See section 3 |
| `image_url` | `http://cataas.com/cat?width=800&height=480` | Section 2; ASCII, < 256 chars |
| `save_downloaded_images` | `false` | Otherwise every URL photo is saved: 144 files per day fill the SD card |
| `sd_rotation_mode` | `random` | Storage profile: random album order |
| `weather_location_name` | `Paris` | Shown/used for geocoding fallback |
| `weather_lat`, `weather_lon` | `"48.8566"`, `"2.3522"` (strings) | Set explicitly, section 2 |
| `weather_provider` | `open-meteo` | Free, no key (others: `wttr.in`, `yr.no`) |
| `overlay_language` | `en` | Weekday/condition wording (`de` also exists) |
| `weather_overlay_enabled` | `false`; S: `true` | One-line overlay (storage only) |
| `weather_multiline_enabled` | `false` | One line, as asked |
| `overlay_epdgz_enabled` | S: `true` | Albums normally hold pre-rendered EPDGZ files; without this they get no overlay |
| `headlines_overlay_enabled` | `false` | Stored but off, like the ToDo (and, like the weather overlay, only drawn on stored photos) |
| `headlines_rss_url`, `headlines_count` | a public news feed URL, `3` | Prefilled so the user only has to switch it on (while headlines are on, the weather falls back to one line) |
| `low_battery_overlay_enabled`, `low_battery_overlay_threshold` | S: `true`, `16` | Corner badge, storage only |
| `agenda_todo_enabled` | `false` | Section 2: keeps the 7-day grid |
| `agenda_todo_url` | `<BASE>/todo.txt` | Stored, not fetched while off |
| `agenda_cal_enabled` | `true` | |
| `agenda_cal_url`, `agenda_cal_name` | `<BASE>/calendars/calendar-a.ics`, `Family` | Calendar A (blue), fetched every agenda wake |
| `agenda_cal_url2`, `agenda_cal_name2` | `.../calendar-b.ics`, `Work` | Calendar B (green) |
| `agenda_cal_c_enabled`, `_url`, `_name` | `true`, `.../calendar-c.ics`, `Sport` | C-E: one download during the import, own colour each (active colour profile) |
| `agenda_cal_d_enabled`, `_url`, `_name` | `true`, `.../calendar-d.ics`, `School` | |
| `agenda_cal_e_enabled`, `_url`, `_name` | `true`, `.../calendar-e.ics`, `Household` | |
| `agenda_cal_layout_mode` | `grid_a` | 7-day grid, today gets full width (`grid_b`: double height) |
| `agenda_cal_days` | `3` | Used by the list layout when the ToDo is switched on |
| `agenda_cal_time_display_mode` | `range` | `08:00-09:30` |
| `agenda_cal_multiday_mode` | `repeat_numbered` | Shows the multi-day example as `2/3:` |
| `agenda_cal_weather_enabled` | `true` | Forecast on the day dividers (up to 7 days in the grid) - the weather of the URL profile |
| `agenda_cal_weather_right_aligned` | `false` | |
| `agenda_shift_model` | `none` | The custody-style rotation pattern needs a start date and would only confuse |
| `agenda_cron` | `["*/15 * *"]` | Calendar every 15 minutes |
| `alarm_cron` | `["0 * *"]` | Every full hour (see D4) |
| `alarm_ring_duration_sec` | `10` | Short: an hourly 60 s alarm on a desk is a nuisance |
| `alarm_volume`, `alarm_ramp_sec`, `alarm_tune` | `50`, `0`, `0` | Moderate, no ramp, default melody |
| `chime_speaker_mode` | `battery_and_mains` | Beep feedback on |
| `chime_volume` | `40` | |
| `chime_quiet_enabled`, `chime_quiet_start`, `chime_quiet_end` | `true`, `"22:00"`, `"07:00"` | Shows the quiet-hours feature (the alarm ignores it) |
| `chime_event_rotation_enabled` | `false` | A beep every 10 minutes would be unbearable |
| `chime_event_low_battery_enabled`, `chime_event_ota_success_enabled`, `chime_event_critical_error_enabled` | `true` | Rare events |
| `chime_event_agenda_due_enabled`, `chime_event_wifi_reprovision_enabled`, `chime_event_telegram_photo_enabled` | `false` | Noisy / not applicable |
| `climate_room_type` | `living_room` | Classifies the SHTC3 reading |
| `climate_temp_unit` | `celsius` | |
| `climate_logging_enabled` | `true` | Fills the Climate History tab |
| `climate_agenda_header_enabled` | `true` | Temperature/humidity in the Agenda header |
| `climate_overlay_enabled` | `false`; S: `true` | Badge on stored photos |

**Deliberately absent** (never put into the file): `wifi_ssid`, `wifi_password`, `static_ip`/`dns_server`/other
network keys, `device_name` (keeps the user's own), `http_password`, `https_enabled`, every `telegram_*` key,
`access_token`, `http_header_key/value`, `ha_url`/`ha_enabled`, `openai_api_key`, `google_api_key`, `ntp_server`,
`ota_check_enabled`, and the sections `processing`, `palette`, `albums`. **Telegram cannot be demonstrated with
example data** (a bot token and a chat ID are personal); [TELEGRAM.md](TELEGRAM.md) explains the setup, and the
README says so.

Features whose settings are not part of this list (offline hotspot, error banner, HTTPS, WiFi resilience, face crop,
display history, battery history, OTA channel) stay at the user's own values; a later pass can add the harmless ones
(`error_overlay_enabled`, `display_history` toggles).

## 5. Example content

### 5.1 Calendars A-E

Common rules (so they never go stale and are parsed): plain ASCII text; one `VEVENT` per fixed weekly event with
`RRULE:FREQ=WEEKLY;BYDAY=<weekday of DTSTART>`; `DTSTART` in early January 2026 (a Monday-aligned start) with
**floating local times** (no `Z`, no `TZID`); a unique `UID` each; titles <= 30 characters (the parser keeps 160,
the grid cells are small); every file well under the 48-event window limit.

| Cal | Name (<= 24 chars) | Colour | Content (all weekly) | Events per week |
| --- | --- | --- | --- | --- |
| A | Family | blue | **Monday: 8 events** (07:30 to 21:00, one all-day, one long title, one two-hour range, one at midnight edge); **Tuesday-Sunday: one fixed, different event each** | 14 |
| B | Work | green | Monday-Friday one event each (stand-up, review, planning ...), one **multi-day** event Saturday-Monday (3 days, exercises `repeat_numbered`) | 6 |
| C | Sport | own | Tuesday and Thursday evening, Saturday morning | 3 |
| D | School | own | Monday-Friday: one 08:00-15:30 block; Wednesday: earlier end; Friday: short day | 5 |
| E | Household | own | Wednesday all-day "Recycling bin", Saturday all-day "Shopping", Sunday 18:00 "Dinner at Sam's" | 3 |

So Monday shows 8 events of A plus about two of B and one of D (roughly 11 on the busiest day, the one the grid has
to abbreviate), every other day 2-5. Budget: A/B are read for the visible window only; C-E expand 30 days: at most
`7 x 5 = 35` occurrences for the largest source, below the 48 limit. Names such as "Sam" are invented; no real
places, people or numbers.

### 5.2 ToDo file

`todo.txt` in the standard format the frame reads: <= 24 lines (`TODO_MAX_ITEMS`), no completed (`x `) lines, priorities
`(A)`-`(D)` to show the four colours, a few `+project` and `@context` tags, a few `due:` dates. A static file cannot
be relative to "today": use one clearly overdue date, several far-future dates and undated tasks, and say in the
README that "due today" cannot be shown by a fixed file. The ToDo stays **off** in the configuration; the README tells the
user to switch it on to see it (the layout then drops to the list).

### 5.3 Optional sample photos (storage profile)

Five 800 x 480 pictures **generated by a script** (`scripts/generate_demo_photos.py`: gradients, a sun disc, hills,
colour bars) so licence and privacy questions cannot arise; committed under `examples/.../photos/`. The README says:
upload them in the gallery (the Web UI converts them in the browser) or use your own photos. Skippable: the
storage profile also works with any album.

## 6. What the user sees (URL profile)

Cron rules are evaluated in Paris time. On a tick where several are due: alarm > agenda > photo.

| Minute | Event |
| --- | --- |
| :00 | alarm rings (10 s); photo and agenda of that tick wait or are skipped |
| :10, :20, :40, :50 | new random photo |
| :15, :45 | Agenda (7-day grid, Paris forecast, five calendars) |
| :30 | Agenda wins over the photo of the same tick |

A picture change on this panel takes about 30 seconds; with this cadence the demo is meant for a frame on USB power.
The Agenda stays on screen until the next photo (5-10 minutes).

## 7. Limits to state honestly in the README

- Telegram, WiFi, device password, Home Assistant, API keys: not part of a demo file (personal by nature).
- The random photo comes from a third-party service that may change or disappear; one line (`image_url`) to change.
- Yearly and monthly events (birthdays, holidays) are not supported by the calendar parser, so the example
  calendars cannot show them; ToDo "due today" cannot be shown by a static file.
- Importing overwrites the user's current settings: **export a backup first** (Maintenance -> Export Config, tick
  "Include credentials and URLs" for a complete one) and re-import it to go back. The demo file leaves WiFi and the
  device name alone.
- With an hourly alarm the frame will ring at night: the README shows how to clear the schedule (Alarm Clock tab).

## 8. Verification plan

**Automated (in CI, no device):**

1. `scripts/test_example_config.py` (Python unit test, runs with the other tooling tests): the files parse; the only
   top-level key is `config`; every key appears as a literal in `main/utils.c` (so a renamed firmware key fails the
   test); enum values and ranges are valid (cron rules parse with 3 fields, quiet hours `HH:MM`, colours, layout
   names); a deny-list of keys is absent (network, credentials, Telegram, `processing`, `palette`); all URLs start with
   the documented base or the image host and are <= 255 characters; a privacy scan (private IPv4 ranges, e-mail
   addresses, `token=`/`secret`/`key=` in URLs, Windows/user paths).
2. `host_tests/test_example_calendars.cpp` (GoogleTest, linked with the firmware's own `calendar_ics.c` and
   `todo.c`): every example file is parsed with `calendar_ics_parse()` for every day of a two-year range in the
   `Europe/Paris` zone; assertions: calendar A has exactly 8 events on Mondays and 1 on the other weekdays, no rule
   is dropped by the fail-closed RRULE check (the counts prove it), C-E stay below 48 events in a 30-day window,
   `todo.txt` parses with `todo_parse()` to <= 24 items with no completed line.
3. The site build test: after `deploy-pages` the six URLs answer 200 with `Content-Length` (a `curl` step or a
   post-deploy check in the workflow).

**On the device (needs the maintainer's permission; the released v218.0.2 firmware already contains everything, so no
flash is necessary):** back up the current configuration first (Export Config with credentials; keep the file
outside the repository, it holds private data) and restore it afterwards.

| Test | Check |
| --- | --- |
| T1 | Import of the URL profile answers OK; `GET /api/config` returns the values (presence check only, no secret values echoed) |
| T2 | `POST /api/rotate`: a cataas picture appears; `last_fetch_error` empty; the progressive-JPEG concern does not apply (baseline) |
| T3 | Redirect handling of `http://cataas.com/...` -> https without a pin; then the https form with a pin, and what a pin does after the intermediate changes (expect failure) |
| T4 | Calendar sources: `agenda_cal_c/d/e_configured` become true after the import; A/B load on the first agenda wake; Monday looks as designed (grid abbreviates, no overflow into other cells) |
| T5 | Paris forecast on the dividers, not the user's old location (set the location before importing to prove the coordinates win) |
| T6 | Timeline of section 6 over one hour (the :00 alarm ring, the :30 collision) |
| T7 | Storage profile: with photos in an album, one-line weather overlay, low-battery and climate badges |
| T8 | Restore the backup; nothing of the demo remains that was not in the backup |

## 9. Steps (in order) and decisions

Decisions for the maintainer (defaults follow the maintainer's own wishes; deviations are marked):

| # | Question | Recommendation |
| --- | --- | --- |
| D1 | Overlay and URL mode exclude each other | Two profiles (section 3); alternative: URL profile only |
| D2 | Hosting | Pages of the fork, `raw.githubusercontent.com` as fallback; gists only if wanted |
| D3 | Photo source | `http://cataas.com/cat?width=800&height=480` (test T3 decides http vs https) |
| D4 | Alarm | Literal `0 * *` as asked, 10 s; alternatives: `5 * *` (does not collide with the :00 photo/agenda tick) or `0 8-20 *` (no night ringing) |
| D5 | Language | English; German is one key (`overlay_language: de`) |
| D6 | Generated sample photos | Yes, cheap and licence-free |
| D7 | Folder name | `examples/waveshare_photopainter_73/` |
| D8 | Other boards | Later; keys of absent hardware (alarm, chimes, climate on boards without them) look ignored by the build, to be confirmed |

Steps:

1. Decide D1-D7.
2. Author the calendars and `todo.txt` (hand-written; keep them small enough to review).
3. Author both configuration files and the README.
4. Write the two automated tests (section 8); wire the Python one into CI's feature-tooling job and the C++ one into
   `host_tests/CMakeLists.txt`.
5. Add the copy of `examples/` into the site in `build.yml`'s `deploy-pages` (and the post-deploy URL check);
   a workflow-file change must be pushed as `t3stier` (MAINTAINING.md section 3).
6. Optional: sample-photo generator; a "Try the demo configuration" link on the landing page (fenced with
   `#if FORK_SITE`, the page is part of the device bundle) and a link from README/FEATURES.
7. Live tests T1-T8 with the maintainer's permission; photograph the result for the README if wanted.
8. CHANGELOG entry, docs links, release with the next version (maintainer's decision).

Estimated effort: authoring 2-3 hours, tests 2 hours, CI/deploy 1 hour, live tests about one hour of device time.

## 10. Privacy checklist for the package

Invented names only (`Sam`, `Alex`), no real places except the city name `Paris` and its public coordinates, no
personal calendar or account, no LAN address, no token; third-party services used are public and keyless
(Open-Meteo, cataas, a public news feed) and named in the README with a note that their terms apply; the test
scans of section 8 enforce it.
