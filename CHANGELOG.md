# Changelog

All notable changes to this project are documented here. See [docs/FEATURES.md](docs/FEATURES.md) for the full
list of optional features and [docs/MAINTAINING.md](docs/MAINTAINING.md) for how they're built and verified;
this file covers what changed and when.

This is a rebuild of [aitjcize/esp32-photoframe](https://github.com/aitjcize/esp32-photoframe) (starting from
`v2.18.0-27`), run as a fork of it: the canonical repository is the GitHub fork
[t3stier/esp32-photoframe-rebuild](https://github.com/t3stier/esp32-photoframe-rebuild), and
[t3ste/esp32-photoframe-rebuild](https://github.com/t3ste/esp32-photoframe-rebuild) is a mirror of it (the first
two releases were published there). With no build option chosen, this firmware *is* the upstream firmware.

**Versions** are `v<upstream>.<minor>.<patch>`: the first number is the upstream version without its dot
(upstream 2.18 → `218`) and only changes once this repository has taken over everything of that upstream
version; the last two numbers count this repository's own releases (`v218.0.0`, `v218.0.1`, ...; when upstream
has a 2.19 and it is merged in: `v219.0.0`, then `v219.0.1`, ...).

## [Unreleased]

### Fixed

- **Repeating events in the Agenda calendar were an hour off for half the year** (`agenda` option). A daily or weekly series was expanded by adding whole multiples of 24 hours to its first date, so after the clocks
  changed it drifted an hour: a series that began in winter showed up an hour late in summer (09:00 as 10:00), one that began in summer an hour early in winter - and so did the example calendars of the demo package
  (the weekly "Midnight snack check" stood at 01:00 in summer). A series now repeats on the wall clock (the same time of day on every period's calendar day), keeps its length across the switch and an all-day event
  ends at the next local midnight; a series in UTC (`...Z`) stays at its fixed instants, as the standard says. All the host tests had run with `TZ=UTC0`, where the two are the same thing; the new tests run in CET/CEST.
- **Exceptions of a series were not read** (`agenda` option): `EXDATE` (an instance that is left out, "not this Wednesday"), `RDATE` (an extra instance), `RECURRENCE-ID` (an instance that was moved or called off - it
  was shown twice, at the old and at the new time) and `STATUS:CANCELLED` (a called-off event stayed on the screen) were ignored, so an event could show up that was gone. They are applied now; what can't be read
  (an `EXDATE`/`RDATE` that is a period, a malformed date) drops the event rather than showing it wrongly, and `RECURRENCE-ID;RANGE=THISANDFUTURE` ("this and all later instances are changed", which is not
  supported) leaves those instances out. `docs/CALENDAR_RRULE_SUPPORT.md` claimed that `EXDATE` was rejected - it never was.
- **An event with `DURATION` instead of `DTEND` had no length** (`agenda` option): it ended where it began, so one that was already going on when the window opened was not found. `DURATION` is read now (`PT1H30M`, `P1DT2H`, `P1W`, ...).
- **A full calendar list kept the wrong events** (`agenda` option): when more than 48 events overlapped the window, the first 48 of the file were kept, whichever time they were at; it is the 48 that start first now.
- A weekly series with `BYDAY` whose `DTSTART` is in UTC (`...Z`) is judged by the weekday in UTC (it was the local weekday, so a series at 23:30Z whose rule says the UTC weekday was dropped).

## [v219.0.1] - 2026-10-05

This release is `v219.0.0` with the fix below; **`v219.0.0` was withdrawn** (set back to a draft) the same day because of it.

### Fixed

- **A build with the Agenda crashed on its HTTPS requests** (`agenda` option, so the full builds; seen with the update check; found on a frame installed with the web flasher: the Web UI's update check ended in "Failed to check
  for updates: Failed to fetch" after a long wait and the browser saw `ERR_CONNECTION_RESET`; the crash record said `assert failed: heap_caps_free ... free() target pointer is outside heap areas`, task
  `ota_check_task`, in `tls_ca_cb_fix.c`). The file (added in v218.7.1-rc3 to stop every TLS connection from leaking 200-400 bytes) freed the buffers of the certificate that ESP-IDF's certificate bundle hands
  to mbedTLS. ESP-IDF 6.0.0, which the local builds use, allocates them - that was the leak. The CI's image follows the `release-v6.0` branch (`v6.0.3-489`), where the leak is fixed in ESP-IDF itself: the
  certificate now points into the bundle in flash and into the peer certificate, nothing of it is allocated, and freeing it asserts. So the tests passed on the local builds and every CI build crashed at its
  first verified connection: **v219.0.0, v219.0.0-rc1 and the release candidates v218.7.1-rc3 and -rc4**. The wrapper now only repacks a certificate whose buffers are allocations of the callback
  (`owns_its_buffers()`: `subject_raw` in RAM and name entries that are not the peer's own) and leaves the others alone, so it works with both ESP-IDF behaviours. Frames on v218.0.3 or older are not affected
  (they do not have the wrapper).

## [v219.0.0] - 2026-10-05

> **Withdrawn** (the release was set back to a draft a few hours after it was published): every full build crashed on its HTTPS requests, see [v219.0.1](#v21901---2026-10-05) above. Everything below is part of v219.0.1.

### Added

- **Recipe page** (`--with recipes`, [docs/RECIPES.md](docs/RECIPES.md); part of `extras`, needs `info-screens`): an information page with **one cooking recipe and its picture** - the title in red,
  the category and time in blue, the ingredients on the left with bullets, the preparation on the right in paragraphs, the picture at the top right and an optional small QR code at the bottom
  right; in portrait the ingredients and the picture are on top and the preparation runs below over the full width. Sources: **Chefkoch** (the recipe of the day, classic / vegetarian / vegan, or a
  search with filters: words, category, country, type of meal, diet, property, time, rating, order; German) and **TheMealDB** (by category; English); the labels follow the source's language. The
  text uses the **largest size at which everything fits** (Noto Sans, SIL OFL, compiled in as bitmaps, 12-30 px, scaled up on big panels); the text flows narrower next to the picture and the code
  and never overlaps; a recipe without a picture or too long for the page is skipped. Errors: **three tries with exactly the filters that were set**, only then the filters are relaxed step by step
  (the page says so), and when nothing can be had at all the last recipe is shown again with a warning (none ever fetched: the page is skipped). Characters and step numbers of the sources are
  cleaned. The options are one NVS text plus TheMealDB's optional write-only key; Web UI card in the Agenda tab; `GET/PATCH /api/config` fields `recipe_*`. New: `recipe_font.c` and
  `scripts/gen_recipe_font.py`, `recipe_text.c`, `recipe_source.c`, `recipe_layout.c`, `recipe_qr.c`, `recipe_engine.c`, `recipe_service.c`, `screen_recipe.c` (everything but the glue is pure:
  182 host tests, also under AddressSanitizer / UBSan and a fuzzer, with invented sample answers; `host_tests/render_recipe.cpp` draws the page for all panels without a frame). The sources are unofficial or
  free interfaces (see the docs for their terms).
- **Travel time on the fuel page** (`--with route-time`, [docs/ROUTE_TIME.md](docs/ROUTE_TIME.md); part of `extras`, needs `fuel-prices`): the header of the fuel page shows how long the drive
  there and back between two addresses takes **right now**, with the traffic (`Hin 28 min  Rück 31 min`); a way that takes more than the usual time by a percentage (10) **and** a number of
  minutes (5) - both settable - is a **red block with a "!"**. Sources: TomTom first, HERE second (a free key of one or both; a refused key, no answer or no requests left
  continues with the next). The Web UI looks an address up (**Find**), lets you choose among the places the provider found, and takes both over only after the frame has
  calculated a **believable route both ways** between them (a time and a length above zero, not more than 12 hours, not shorter than the straight line, no absurd detour, not the same
  place twice); the **coordinates** are stored, nothing is looked up when the page is drawn, and a changed text drops the check. The usual times can be taken from the check (with or
  without traffic) or typed. The display shows only a name you choose; **the addresses are in an export only if it includes the credentials**; the keys are write-only and never
  logged, nor are the addresses. No old time is ever shown: without a time the header is as before. New: `route_time.c` (pure: requests, readers of both providers' answers,
  the believable-route and too-long rules; 34 host tests incl. cut-off and changed answers), `route_service.c` (the providers in order, the five-minute memory; 20 host tests with the
  HTTP layer faked), the header in `screen_fuel.c`, `POST /api/route/geocode` and `/api/route/check`; web: `utils/routeKey.js`. Run with a real TomTom key on one frame (the address look-up, the check of both ways and the fuel page taking the travel time), and a real HERE key works too (the maintainer's report);
  **not seen yet:** the red block on the panel. The test answers are assembled from the providers' documentation (the answers of an invalid key were recorded).
- **Pages per Agenda schedule** (`--with schedule-pages`, [docs/SCHEDULE_PAGES.md](docs/SCHEDULE_PAGES.md); part of `extras`, needs `info-screens`): each schedule of the Agenda can draw
  its own pages (chips under the schedule card; a rotation counter of its own), so that a schedule at 06:30 always shows the fuel page while the hourly one shows the Agenda. When schedules
  overlap, **the smaller number wins** (the order of the list is the priority, with up/down arrows), and no display replaces another within the **minimum time between two displays**
  (15 minutes, adjustable) or the schedule's own **hold time**; the **photo rotation has the lowest priority** and gives way too. A held-off fire does not wake the frame. **Optional:**
  a schedule with no page ticked behaves as before, and while no schedule has a page nothing of this is in force. Settings: `agenda_cron_pages`, `agenda_cron_hold`, `agenda_gap_min`.
  New: `sched_pick.c` (pure; 18 host tests, among them a comparison with a literal reference over random schedule sets and across the days the clock changes), `info_screens_next_for()`,
  `agenda_manager_rotation_seconds_until_next()`; web: `utils/schedulePages.js` (11 tests). Checked on a frame with four overlapping schedules: the three fires that had to draw drew the right
  pages, the two held-off ones neither drew nor woke the frame.
- **The markets, exchange-rate and fuel pages say which day their data are from, and the footer is one line.** The header names the trading day of the newest price (`Close 30 Sep`,
  `Schluss 30.09.`; the ECB's rates `Rates 30 Sep`) instead of a bare date that looked like today's, and a small yellow `!` behind it says that day is older than the last trading day
  (weekends counted, holidays not known). The source and the fetch time share one line (`Yahoo Finance, Twelve Data - 30 Sep 14:35`, `tankerkoenig.de, CC BY 4.0 - 30 Sep 14:35`), so the
  rows get a line more; a narrow panel keeps two lines. The time the charts cover is said once in the header (`MARKETS  41 d`) when all lines share it, and every line shows the change over
  its whole chart (`+6.9%`) next to the change of the last day. New pure helpers: `info_format_stamp_short()`, `info_format_day_label()`, `info_trading_days_behind()`,
  `info_span_labels()`, `canvas_note_lines()`, `canvas_text_first_fit()`, `canvas_header_label()`, `market_period_change_percent()`, `fx_period_change_percent()`.
  Checked on a frame (Waveshare PhotoPainter 7.3", 2026-10-02): the markets page took the newest day (the close of the same day) and both pages drew the one-line footer.
- **Artworks** (`--with artworks`, [docs/ARTWORKS.md](docs/ARTWORKS.md); part of `extras`): a rotation mode that shows a painting, a drawing or a print from a museum. Each
  rotation draws the kind of work first, then a random work of the Rijksmuseum, SMK (Denmark) or the Smithsonian American Art Museum whose record says public domain or
  CC0, loads the smallest picture that fits the panel (IIIF fit; baseline JPEG only), makes it display-ready, keeps it in the album `Art` and shows it with a small
  caption (white text, one-pixel black border, no bar, from a caption file next to the picture). The picture is put on the panel **whole (Fit, the default) or
  filling it (Cover)** by this mode's own setting, and pictures of the frame's orientation (Display Orientation) are **preferred**: a work of the other format is
  turned down - before loading if the record has its size (SMK), else from the picture's header - and another asked for, up to three times, the last one taken as it
  comes (switchable). No network or any failure: a picture of the album - **also when the album is switched off in the Gallery**, a not yet shown one of the right
  orientation if there is one. Before a
  new picture is kept and the free space is at or below 20 %, the oldest pictures this mode saved - only those with a caption file, only in that album - are deleted
  until 30 % is free (both settable); if that is not enough the picture is shown and not kept. Settings in the Auto Rotate tab and `/api/config` (the Smithsonian
  key is write-only). New: `art_select`/`art_caption`/`art_sources`/`art_store` (pure, host-tested against real answers of the three services, 79 tests, a byte-flip run
  under ASan/UBSan), `art_flow` (the device side), `image_processor_draw_caption_outlined()`; `http_fetch_get_once()` is built
  for it as well. Compiled for every board; run on one frame (Waveshare PhotoPainter 7.3", all three museums, one session) - a day of rotation, a full card and a lost network are not seen yet.
- **Pictures of the display** ([docs/SCREENSHOTS.md](docs/SCREENSHOTS.md), `docs/screens/`): the Agenda in its layouts (7-day grid A and B, ToDo and Calendar stacked and side by side) and in a colour profile, and every
  information page, with made-up sample data (no real names, places or calendars); also in the user guide to the extras, the page of each option, the README and the demo package. The information pages are drawn
  by the firmware's own drawing code through `host_tests/render_screens.cpp` (it existed; the page says how to draw them again), the Agenda pictures by the real `agenda_renderer.c` run on a PC with made-up
  sources by a small program that is not part of the repository.
- **Extended edition** ([docs/EXTENDED_EDITION.md](docs/EXTENDED_EDITION.md), [docs/EXTENDED_LINE.md](docs/EXTENDED_LINE.md)): this line of the firmware is published next to the base project - source in
  the branch `extras`, releases, update feed and web flasher in `t3ste/esp32-photoframe-extras`. New: a README notice with the warning about the update feed of self-built frames, a hardware test report
  issue template, the maintainer's guide for keeping it in step with `main`, and `SITE_REPO` (the demo site's base path and repository links follow the repository it is published from; unset = unchanged).
- **[docs/EXTRAS_USER_GUIDE.md](docs/EXTRAS_USER_GUIDE.md)**: a user guide to the newer options - what each does, where it is switched on in the Web UI, what to enter and what you need (keys, servers,
  a place), the notes on the information pages, how keys and passwords are kept, and a table of what to do when a page or calendar does not show up.
- **`extras` bundle** (`--with extras`): one name for every option added after the first fork release - `webcal`, `multi-upload`, `source-auth`, `caldav`, `caldav-todo`, `upload-dedup`,
  `glyphs`, `info-screens`, `chore-wheel`, `weather-screen`, `fact-of-the-day`, `finance-snapshot`, `fuel-prices`, `market-quotes` - with what they need (`agenda`, `overlays`). It is not an option of its own: the
  build, the web app and the listings only see the members. `--without <member>` trims it, `--without extras` takes all of them out of `--all-features`, and a member the board cannot build is skipped with
  a notice. Shown by `--list-features`; built by the compile matrix (`feature_matrix.py ... extras`) and CI ([docs/FEATURES.md](docs/FEATURES.md)).
- **`source-auth` build option** (`--with source-auth`, needs `agenda`): a calendar (A-E) or ToDo address may carry a login,
  `https://user:password@host/path` (special characters percent-encoded); the frame takes it out of the address and answers the server's
  401 with HTTP Basic or Digest. The address fields stay write-only and out of a normal config export; over plain `http://` the login is only
  sent when a new setting allows it, a refused login is not retried, and redirects are not followed with a login set
  ([docs/SOURCE_AUTH.md](docs/SOURCE_AUTH.md)).
- **`caldav` build option** (`--with caldav`, needs `source-auth`): a calendar address written `caldavs://user:password@host/path` (`caldav://` for
  plain http) is queried with a CalDAV `REPORT` instead of being downloaded whole - the server sends only the events of the coming days and
  expands repeating events itself, so monthly/yearly repeats and exceptions (which the on-device reader skips) show up, and a large calendar no
  longer runs into the 2 MB limit; a server that refuses the expand request is asked again without it
  ([docs/CALDAV.md](docs/CALDAV.md)).
- **`glyphs` build option** (`--with glyphs`): ä ö ü Ä Ö Ü ß ° € are drawn as themselves in the text the frame draws (overlay captions and headlines, the Agenda's ToDo and
  Calendar columns, Telegram captions) instead of being turned into ae/oe/ue/ss or dropped. The umlauts are the font's own letters with two dots, the sharp s, degree and euro signs are
  drawn in the same 17x24 cell; the text sanitizer keeps them as single bytes 0x80-0x88, so wrapping and centring are unchanged ([docs/GLYPHS.md](docs/GLYPHS.md)).
- **`caldav-todo` build option** (`--with caldav-todo`, needs `caldav`): a `caldavs://user:password@host/path` (or `caldav://`) address in the Agenda's ToDo
  field is read as a CalDAV task list - one `REPORT` for the open to-dos (a server that does not know that filter is asked for all and the finished ones
  are dropped on the frame). Priority 1-9 becomes the A-D chips, a due date the due colour (a UTC time as the frame's local date), finished and cancelled
  to-dos are left out, a repeating one is listed once; ordered by due date, then priority. The CalDAV query code is shared with `caldav`
  ([docs/CALDAV_TODO.md](docs/CALDAV_TODO.md)).
- **`info-screens` build option** (`--with info-screens`, needs `agenda` and `glyphs`): full-screen pages besides the Agenda that take turns with it on the Agenda's
  schedule - ticked in Settings -> Agenda -> Information screens; each run of the schedule draws the next ticked page (the Agenda is ticked by default, so nothing changes until a page is added),
  and the schedule also runs when only such a page is ticked. The pages are drawn by plain functions into an RGB canvas (text in the frame's font at 1x-4x, shapes), which is tested on a PC
  for every panel size ([docs/INFO_SCREENS.md](docs/INFO_SCREENS.md)).
- **`chore-wheel` build option** (`--with chore-wheel`, needs `info-screens`): a page for the household - a donut wheel with one coloured sector per member and one card per chore; chore `t` of ISO
  week `w` goes to member `(t + w) mod members`, so the chores move on every Monday. Up to 5 members and 6 chores, names typed in the Web UI (umlauts included), English and German
  ([docs/CHORE_WHEEL.md](docs/CHORE_WHEEL.md)).
- **`weather-screen` build option** (`--with weather-screen`, needs `info-screens` and `overlays`): a full-screen weather page in the Agenda's rotation - today's condition as a big icon (drawn
  from shapes: a yellow sun, outlined clouds, blue rain, a yellow bolt) with a very large temperature, the low under it, and the next four days as rows with weekday, icon, high and low. The numbers are
  drawn with thick round strokes at any size; the place, service and language are the ones of the Overlays tab; the condition texts are whole words in English and German for every WMO code
  ([docs/WEATHER_SCREEN.md](docs/WEATHER_SCREEN.md)).
- **`fact-of-the-day` build option** (`--with fact-of-the-day`, needs `info-screens`): a full-screen page with one fact a day - a red header with the date, the topic as a blue pill, the fact in the biggest text that fits and, if it has one,
  a yellow question box. 24 built-in facts (original wording, English and German with real umlauts), or your own list: a plain text file, one fact per line as `Topic|Fact|Question`, edited in the Web UI (Settings -> Agenda -> Information
  screens) and kept on the frame's storage (`GET`/`PUT /api/facts`); the fact is picked by the day, so it stays the same all day ([docs/FACT_OF_THE_DAY.md](docs/FACT_OF_THE_DAY.md)).
- **`finance-snapshot` build option** (`--with finance-snapshot`, needs `info-screens`): a full-screen exchange-rate page with the reference rates of the European Central Bank - up to four currencies (Settings ->
  Agenda -> Information screens, `USD, GBP, CHF, JPY` by default), each with the price of one euro in big digits, the change against the working day before as a green or red arrow, and a line chart of the last
  30 working days. One small request to the ECB's data portal (free, no key); checked against real answers of it ([docs/FINANCE_SNAPSHOT.md](docs/FINANCE_SNAPSHOT.md)).
- **`fuel-prices` build option** (`--with fuel-prices`, needs `info-screens` and `overlays`): a full-screen page with the cheapest petrol stations around the weather place - brand, street, distance and the price in big digits with the
  third decimal small like on the pump, the cheapest marked green; fuel type, radius (1-25 km), number of stations and "hide closed" are settings. Germany only (Tankerkoenig, CC BY 4.0, attribution and time of the retrieval
  on the page); the personal API key is write-only (never returned, only in the export that includes credentials, removable) and never logged. The answer is read one station at a time, so a big one or a cut-off one
  does no harm; parsed against the service's real answers ([docs/FUEL_PRICES.md](docs/FUEL_PRICES.md)).
- **`market-quotes` build option** (`--with market-quotes`, needs `info-screens`): a full-screen markets page with up to four symbols - stocks, ETFs, indices, futures, crypto and
  currency pairs, written the Yahoo way (`AAPL`, `EUNL.DE`, `^GDAXI`, `GC=F`, `BTC-EUR`, `EURUSD=X`) - each with the last price in big digits, the change against the day before
  and a line of the last 30 days. The prices come from a chain of three sources, tried in this order for each symbol: Yahoo Finance (no key, unofficial, can be switched off), Twelve Data
  (free key, 800 requests a day) and Alpha Vantage (free key, 25 requests a day); a source is left out when it has no key, does not serve that kind of symbol (symbols are translated to
  its notation) or its daily quota, which the frame counts, is used up. A source that refuses the key or is out of requests is not asked again in the same draw, and the answer of a source is
  never re-requested (a new `http_fetch_get_once()` retries only when the server did not answer at all; an HTTP error status - the 401 of a wrong key, which the ESP-IDF client reports as an error of the
  request - counts as an answer). The last good answers are kept on the storage, so a wake without network or a
  failed fetch shows the last known prices in blue. The two keys are write-only (never returned, only in the export that includes credentials, removable, never logged). Parsed against
  real answers of all three services ([docs/MARKET_QUOTES.md](docs/MARKET_QUOTES.md)). Shared with the fuel page: the object-by-object JSON scan (`json_scan.c`), and with the
  exchange-rate page: the sparkline (`canvas_sparkline()`).
- **`upload-dedup` build option** (`--with upload-dedup`): every album keeps a small index (`.dedup`) of the MD5 of its images, and an upload the
  album already has is refused (`409`, naming the file; the Web UI offers "Upload anyway") or stored with a warning - as a setting; compared by the file's
  bytes or by the decoded pixels (an EPDGZ inside its gzip wrapper, a PNG as RGB), so the same photo converted by two browsers still counts as one. The
  images that were there before can be indexed in the background (on save and at every start-up, or with "Index now"), and "Find duplicates" lists what an
  album has twice with a delete button per file; the batch upload skips duplicates unless told otherwise ([docs/UPLOAD_DEDUP.md](docs/UPLOAD_DEDUP.md)).
- **`multi-upload` build option** (`--with multi-upload`): the Web UI's upload takes a whole selection of files (up to 200) - photos are converted with
  the current settings one after the other (cover or fit, no editor), pre-rendered `.epdgz` files (for instance from `process-cli`) - and, if ticked,
  panel-sized PNGs - go up as they are; a queue shows progress and failures. The upload endpoint accepts an image without a thumbnail
  ([docs/MULTI_UPLOAD.md](docs/MULTI_UPLOAD.md)).
- **`webcal` build option** (`--with webcal`, needs `agenda`): a `webcal://` or `webcals://` subscription link - what calendar apps hand
  out for "subscribe" - is accepted for Calendars A-E and fetched over `https://`. Without the option such a link is fetched as is and fails.
- **`scripts/fetch_art.py`** (a PC helper, not firmware - no build option): fetches curated public-domain artworks from the public
  paperlesspaper art API, renders them for a chosen board with `process-cli` (cover/fit, the board's resolution, 16-level grey output for the
  grey panels) and writes an album folder for the SD card or uploads it to a frame, together with an `ATTRIBUTION.md`
  ([docs/ART_FETCH.md](docs/ART_FETCH.md)).
- **The demo package is online.** The site's deploy now serves `examples/` below
  `https://t3stier.github.io/esp32-photoframe-rebuild/examples/waveshare_photopainter_73/` (files only - the folder address itself
  has no page; the two configs are `.../demo-config-storage.json` and `.../demo-config-url.json`; the calendars and ToDo list
  the two importable demo configs point at - importing one used to leave calendars A-E and the ToDo list failing
  with HTTP 404 in the frame's log) and, as its last step, checks that every URL those configs use answers `200`.
  Three example Calendar color profiles (`color_profiles/`) join the package, and the landing page's "How it goes"
  list, `README.md` and `docs/FEATURES.md` link to it ([docs/DEMO_PACKAGE.md](docs/DEMO_PACKAGE.md)).
- The web flasher's pre-release entry (`manifest-prerelease.json`) now survives the site's next deploy: the site is
  rebuilt from scratch on every deploy, so the newest published pre-release (while it is newer than the stable
  release) is restored from its release like the stable one already was.

### Changed

- **Upstream v2.19.0 (`186ebaf`) is merged** (sixteen commits of 2026-10-05; the equality baseline of the proofs moved to it, the next release of this repository is `v219.0.0`). It brings:
  a **WiFi policy that never drops the credentials over an access point that is merely absent** (`wifi_retry_policy.c`, host-tested: only an AP that keeps *rejecting* the credentials ends in provisioning; a router that
  is still booting after a power cut, out of range or silent leaves the credentials alone and an interactive boot keeps reconnecting on its own - exactly the incident the `wifi-resilience` option was built
  for); the SD card rail is discharged on every boot and an unanswered SD init is retried (`board_hal`, `sdcard`); a **device password** documented (Authentication in [docs/API.md](docs/API.md)); the Web UI's
  config export carries `system_info` and `photoframe-process` can take the device settings and the panel size from an exported config file (`--device-config`); a retry of the web app's `npm install`; and
  five of the six fixes of our `fixes` option that we had sent upstream as a pull request (upstream took them, with changes of its own, and closed the request): the RTC no longer forces standard time
  during daylight saving, a config PATCH applies every valid field and reports the rejected ones, the settings POSTs are capped at 8 KiB and read completely, the enabled-album list has a lock, the OTA update
  check reads a chunked answer, follows only `https://` downloads and never races a running check; and upstream's own JPEG fix (a header walk that follows the decoder's rule, 64-bit size) replaced ours.
  Where upstream now has the same code, the `fixes` copy is gone (`main/jpeg_size_check.h` and its test, the guarded variants in `utils.c`, `ota_manager.c`, `album_manager.c`, `http_server.c`, the RTC drivers): with
  every option off the code is upstream's, as before (184 files, 0 differences). What stays under the options: everything that goes beyond upstream (the OTA channel and `http_fetch`-based release read, the
  recursive album delete, the DNS fallback, the Telegram, Agenda and the other features). The `wifi-resilience` option keeps its own parts (TX power cap on battery, a lower reconnect budget for Telegram power
  save, MIC-failure and 802.1X rejections counted as rejections) on top of upstream's policy; its *extended retry* and *reprovision when attempts run out* settings no longer had a case to act on - a failure that is
  not a rejection never reached them any more - and are removed (see *Removed* below). Conflicts: 17 files, all inside the guards of the options or in
  files this repository had edited by hand (`process-cli/cli.js` - `--device-config` next to `--board`/`--resolution`, `package.json`).
- **The Agenda tab of the Web UI is a list of sections** instead of one long page: *ToDo*, *Calendar*, *Extra ICS Calendars*, *Schedule*, *Information screens* and *Appearance and colors*. They start closed; the header of each says
  what is on (`Calendar on`, `Schedule 3 schedules`, `Information screens Fuel prices, Markets`), and the ones you opened stay open the next time (kept in the browser only). Settings that depend on another are
  **left out** while that one is off, instead of sitting there greyed out: the settings of an information page (its explanation, the members of the chore wheel, the symbols, *Use Yahoo Finance* and the keys of the
  markets page, the fuel key and the travel time card ...) show only while the page is ticked; the Calendar's display options only while the Calendar is on (its address fields stay, since it cannot be switched on
  without one), *Right-align forecast* only with the forecast on, the extra calendars only with the Calendar on; the layout choice for ToDo and Calendar is greyed out unless both are on. Nothing was removed and
  no setting changed its meaning. Only with `--with agenda`; a build without it has no Agenda tab.
- **The information pages with data from the internet say when the data were fetched, and the charts say how long they run.** Weather, exchange rates, fuel prices and markets end with a small
  note, `Updated 30 Sep 14:35` (`Stand 30.09. 14:35`) - the local time of the fetch; the markets page shows the time of its newest fetch, so a page drawn from the kept prices says how old they are.
  Without a clock that was ever set there is no note (a wrong time is worse than none). The fuel page's footer, which had the time only, has the same note now. The exchange-rate and markets
  rows show the span of the line under it (`41 d`, `41 T`: calendar days from the first to the last point). New pure helpers in `info_screens_core`: `info_days_between()`, `info_format_stamp()`,
  `info_format_span()`.
- Upstream `f5e3ec9` is merged (six commits of 2026-09-29, all about crash reports): **core dumps are captured in every build**, in the 56 KiB that were free below `ota_0` (the partition table
  generator puts the `coredump` partition there on every board layout); the last crash is kept as one record in the settings memory (reason, task, program counter, backtrace, firmware version, the ELF's
  SHA-256), reported by `GET /api/system-info` as `last_crash`, shown in the Maintenance tab (**Last crash**, with a copy button) and cleared by `POST /api/crash/clear`; the CI attaches each board's
  ELF to the release (`[board]-[version].elf`, for decoding a crash report - [docs/DEV.md](docs/DEV.md)). The `--debug` build option and `sdkconfig.defaults.debug` are gone (they did what is now always
  on), and so is the `fixes` option's extra `exc_cause`/`exc_vaddr` log line of the boot-time coredump summary: the new record names the exception and the faulting address itself. The CI uploads the ELF
  of the full builds only - the plain build would upload an artifact of the same name. A frame updated over OTA keeps its old partition table and so has no `coredump` partition: it records no crash
  (the firmware copes with that), a web-flasher install of this release does.
- Upstream `495e0b6` is merged (two commits, both in `main/main.c`): waking a frame with its CLEAR button no longer
  re-runs `board_hal_init()`/`display_manager_init()` (the second `spi_bus_initialize()` aborted, so the frame
  panicked and rebooted instead of clearing the screen - every ESP32-S3 board was affected; the climate reading
  taken on that wake is kept), and the boot-time coredump log now also prints the panic reason and keeps a dump it
  cannot summarise in flash instead of erasing it (debug builds only; the `fixes` option's extra `exc_cause`/
  `exc_vaddr` line is kept on top of it).
- `docs/FIXES.md` lists the fix areas it was missing (DNS fallback servers, the provisioning scan, the recursive album delete, the update check's pre-release comparison and state handling, the credential-free export, gallery paging and the upload format) and says which part of the update check is the fork's own.

### Removed

- **The `wifi-resilience` settings *Extended WiFi retry before reprovisioning* and *Reprovision when connection attempts run out*** are gone: Web UI switches, the API fields
  `wifi_extended_retry_enabled` / `wifi_reprovision_on_fail_enabled` (a config import that still carries them ignores them), the cold-boot retry loop in `main.c` and the cross-reboot attempt counter. Since upstream's
  v2.19.0 WiFi policy a failure that is not a rejection of the password keeps the credentials and is retried without limit, so only a real rejection can still end in provisioning - the settings had nothing
  left to act on. The option keeps the TX-power cap, the performance mode, the lower reconnect budget for Telegram power save and the MIC-failure / 802.1X rejection handling.

### Fixed

- **ToDo & Calendar had a redundant, misleading third on/off switch.** ToDo and Calendar each already have their own switch in
  their own section; a third one under Information screens, governing the same combined page for the schedule/rotation, could
  silently disagree with both (both on, this one off: the data neither section promises never shows; or the other way round,
  costing a wake that draws nothing). Removed; its state always comes from ToDo or Calendar being on, nowhere else - including
  the one case the tick could still affect after the previous entry's fix, a schedule or the shared rotation with nothing else
  ticked at all. `agendaScheduleDisabled` (the Schedule section's own enabled/disabled state) also only checked whether a
  schedule was ever given pages, the same imprecision the previous entry's `sched_pages_effective()` fixed on the firmware side
  - it now mirrors that precision, so the section reads as disabled whenever nothing would actually fire, not just when nothing
  was ever configured.
- **Three more `schedule-pages` issues found while answering the maintainer's own questions about it**: (1) the Recipe page could be ticked for a schedule in the Web UI but was silently dropped on save - a stray `0x7F` mask in `config_manager.c` was one bit short of the 8 information screens (Recipe is the 8th); (2) a schedule given only a page that was later unticked everywhere (or whose only page is ToDo & Calendar while neither is on) still woke the frame on its own schedule to find nothing to draw - it already fell back gracefully (the previous entry's fix, then the pre-existing "nothing configured" guard leaves the display alone), but the wake itself was wasted; `agenda_manager_is_enabled()` now checks whether a schedule's pages are actually in effect, not merely whether they were ever given; (3) a page that is on (ToDo & Calendar, or ticked under Information screens) but is not in any schedule's own list silently never gets drawn once every schedule has a list of its own - a warning under the Schedule section now names it. All three confirmed live (on a test frame): Recipe now stays assigned; a stale-only schedule produced no log entry at all at its fire time (before: a fallback message); the warning appeared the moment Recipe was ticked without being added to the schedule, named it, and cleared once unticked again.
- **A schedule kept drawing a page after it was switched off under Information screens, and the Schedule section's own hint text was wrong** (`schedule-pages`). The hint said the Agenda schedule "only applies while ToDo and/or Calendar is enabled" - true before `info-screens` existed, but no longer: a ticked information screen or a schedule with pages of its own already keeps it going on its own, confirmed live 2026-09-30, just never corrected in the text. Separately, a schedule's own page picks were independent of the Information screens tick list, so unticking a page there did not stop a schedule that had been given it from continuing to draw it - the two are now kept in step: an unticked page's chip in a schedule greys out (its tick is kept, not cleared, and comes back the moment the page is ticked again), and a schedule whose ticked pages have all become unavailable this way draws the shared rotation instead. The ToDo+Calendar page itself was also only ever labelled "Agenda" in these pickers, the same word the settings tab and the schedule section are called, which this also cleans up by labelling it "ToDo & Calendar" instead. 2 new host tests (`main/info_screens_core.c`).
- **The recipe page was hardened after a code audit** (static warnings, a review of every path that reads data from the internet, and a mutation fuzzer under
  ASan / UBSan / LeakSanitizer; details in [docs/RECIPES.md](docs/RECIPES.md)): the time (110 s) and the number of requests (30) are checked before **every** request, not
  only between the tries; an answer nested more than 32 levels is turned down before cJSON parses it (the build allows 1000, which needs ~64 KB of stack on the Xtensa);
  a picture address must be **https to a domain name** (no IP address, no home-network names, no login or odd port) and the sides of a JPEG are limited and checked
  against the size the decoder reports (the library multiplies in 32 bit: 40000 x 35792 pixels comes out as 72 704 bytes); the day page's list is looked for in every
  `ld+json` block; numbers from the answers are clamped instead of cast (an infinity was cast to int, undefined); a step number has at most four digits; an amount above
  100 kg is not shown; the text cleaning is linear (it was quadratic for a text with many `<` and no `>`); a layout that could not be built is defined and draws a white page;
  the last recipe read from its file is made safe (every text terminated) and is not written again when it is the same one (flash wear); when every recipe that fits the
  filters was shown lately one is shown again instead of relaxing the filters; the cleaned search text is whole UTF-8 and cleaning it twice changes nothing.
  The same weakness of the decoder library was in the **base's photo pipeline** (`image_processor.c`); it is fixed there too (next entry). 29 new host tests (the recipe suites have 182 now).
- **A JPEG with two frame headers could still make the photo pipeline write past its buffer** (`--with fixes`). Found by upstream's maintainer when reviewing the pull request that carried our
  JPEG fix there (upstream took a different fix and not ours): `esp_jpeg_get_image_info()` reports the size of the **first** SOF0 of a file, while the decoder (tjpgd's `jd_prepare`) takes the **last** one
  before SOS - so a file with a small first frame and a huge second one passed the size check of the entry below with a small buffer and was then decoded at the huge row stride, far past the allocation. The
  file reaches the frame unauthenticated (`/api/upload`, `/api/display-image`, the image URL, the Home Assistant push). The frame size is now also read with a header walk that follows the decoder's own rule
  (`main/jpeg_header.c`, the same file as upstream's, 7 host tests), and a file where the two disagree - or that has no usable header - is refused before anything is allocated. The 32-bit wrap of the entry
  below cannot be reached by a single header on this firmware (16-bit sides, and anything over twice the panel is decoded scaled down); that entry's check stays as a second line of defence.
- **A rejected WiFi change answered `200 success`** (`--with fixes`). Also found in that review: the config-PATCH change of the `fixes` option (every field applies on its own) let a WiFi network that could not be
  joined fall through without any error - the old credentials were kept and the request still said success; the Web UI and a config import never told you. It sets the error and its message again, as before
  the option. And the login-attempt limiter is reset only when a new device password was actually saved (it was also reset after a rejected one).
- **A photo that failed to read halfway was shown as a finished picture with the top white** (`--with fixes`). A BMP is stored bottom row first and read straight into the
  display buffer; after a one-off SD card I/O error in the middle of the file (`sdmmc_read_sectors_dma`, seen live on a frame) the decoder (`GUI_BMPfile.c`) logged it and stopped like a
  normal end, so every row above the last one read stayed white, the panel showed only the lower part of the photo, and the log said "Image displayed successfully" - the album
  rotations also recorded the photo as shown whatever the outcome. The decoder now reports the failure; both rotations (`display_manager.c`) record a photo as shown only when it was:
  the sequential one goes on with the next photo, the random one tries one other photo of the same pool, and both give up cleanly for that rotation (the next scheduled one tries
  again) if that fails too. PNG is not affected (libpng fails the whole decode). The normal path was checked live: full decode, panel update and "displayed successfully" as before; the failure
  branch itself could not be provoked on demand (a one-off SD glitch) and was reviewed by code.
- **A web page in a browser on the same network can no longer press buttons on the frame** (`--with fixes`). The API has no
  CORS headers, but a plain cross-site `POST` needs no permission, so any page you visited could send
  `POST /api/factory-reset` (wipes the settings) or `POST /api/config` to the frame, with no password set (the default). A
  request that carries an `Origin` header naming another host than the one it was sent to is now refused with `403`
  (`main/http_origin.h`, 10 host tests); curl, Home Assistant and the Web UI itself send no foreign `Origin` and are not
  affected (a Vite dev server proxy now drops it, `webapp/vite.config.js`). **Not covered: DNS rebinding** - set the device
  password (General -> Advanced network settings) if the frame is reachable from networks you do not control
  ([docs/API.md](docs/API.md#access-control)). Found in a second code audit of everything this repository changed.
- **The colour profile editor (`profile-editor.html`) ran script from an imported profile file.** The name of an imported
  profile and its colour values were written into the page as HTML, in the origin of the frame, so a shared profile file
  could call the whole API. Notes are plain text now, a colour must be a palette name or `#rrggbb` (what the frame accepts
  too), a profile name is cut, cleaned and cannot be `__proto__`, and what the browser kept from an earlier session is
  checked on load. Checked in Chrome with crafted files (markup and entities in name, file name, colours, mode, stored state).
- **Smaller hardening** (`--with fixes`): `POST /api/settings/processing` and `/palette` read at most 8 KiB, completely
  (they allocated whatever `Content-Length` said and read once); OTA follows only an `https://` download address and
  starts no update while a check is running; an `.epdgz` that unpacks to less than the panel needs is refused
  (it showed leftover memory; 4 host tests); the migration scripts no longer `eval` anything but a plain 0/1 expression;
  `exifreader` of the process-cli (HEIC/AVIF memory exhaustion) and the other vulnerable npm packages of the two
  Node projects are updated (`npm audit`: web app 0, process-cli 28, all in the Jest test chain); the workflows run
  read-only by default, the two third-party actions are pinned to a commit, the version of a build must be a plain
  name and the ref is read from the environment instead of being pasted into scripts.
- **A JPEG whose header lies about its size can no longer make the photo pipeline write past its buffer** (`--with fixes`). esp_jpeg works out the size of the decoded
  picture in 32 bit from the sides in the file's header: 40000 x 35792 pixels is 4 295 040 000 bytes, which wraps to 72 704, and the decoder's own test that its buffer is
  large enough uses the same wrapped number, so a buffer of 72 KB would be written with a picture of 4.3 GB (found in a code audit of the recipe page, whose pictures come from
  the internet; the photo pipeline decodes uploads and downloaded images the same way). `decode_jpg_buffer` now works the size out again in 64 bit
  (`main/jpeg_size_check.h`, 4 host tests - replaced by upstream's own, more complete fix in the v2.19.0 merge, see above) and refuses a header that cannot be read or does not add up, before anything is allocated; it also looks at the result of
  `esp_jpeg_get_image_info()`, which it used to ignore (a progressive JPEG left the size uninitialised). Photos of any real size are unaffected (a 108-megapixel photo is
  324 MB, no wrap).
- **Every TLS connection of a build with `agenda` leaked 200-400 bytes of internal heap** (found with a heap trace while testing the recipe page of the extended edition, which fetches three times a drawing). The `agenda` option switches on the cross-signed
  verification of ESP-IDF's certificate bundle (`CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_CROSS_SIGNED_VERIFY`, needed for Google Calendar); its callback builds a certificate for the trusted root out of separate
  `calloc()`s, and `mbedtls_x509_crt_free()` frees only the structure and the list nodes, so the name buffers were lost after every verification. A heap trace of one request showed exactly those allocations
  (`esp_crt_ca_cb_callback` -> `esp_crt_copy_asn1`); the free heap before each request then fell by a constant 200 B, with plain HTTP by nothing. New `main/tls_ca_cb_fix.c` (compiled with `agenda` when the
  option is on): `--wrap=mbedtls_ssl_conf_ca_cb` swaps the bundle's callback for one that calls it and packs the buffers it returned into one block that the certificate owns as its raw buffer, so mbedtls frees
  everything; a certificate that has a raw buffer is left alone, so a later ESP-IDF that frees its own is not affected. Checked on the frame: the trace shows no certificate allocation any more, the free heap
  stays level over 11 requests in a row, and the verification is the same as before for a normal host, Google Calendar (cross-signed) and the hosts that must be refused (self-signed, wrong host, untrusted root).
  A frame that sleeps was not affected (every wake starts afresh); one that never sleeps and fetches often lost memory until it restarted.
- **The Web UI of a build with only some of the options had parts that did nothing.** The Settings page fences a template part and the script part it uses separately, and four of them
  were fenced by the wrong option: the calendar switches of the Agenda tab sat under the alarm clock (a build with `agenda` but not `alarmclock` had Calendar and Extra calendar
  switches that moved but changed no setting), the temperature unit list of the climate option sat under the alarm clock too (empty with `climate` alone), the message line (`showSnackbar`)
  was only there with the offline hotspot although the error banner, the chimes and the duplicate search use it, and the error-overlay card with its test button showed in builds
  without `error-banner` and failed when pressed. Fixed. `scripts/migrate/web_bindings.py` now checks every feature set for this (see [docs/MAINTAINING.md](docs/MAINTAINING.md)); the
  full build and a build with no option were not affected, and the all-off proof is unchanged.
- **Markets: the chart could end a day early.** Yahoo's daily bars lag: a bar without a close is left out, and the bar of a day that has just ended may not be complete, so a frame that
  fetched at midnight could still show the close of the day before yesterday. The page now also takes the latest price Yahoo states for the symbol (`regularMarketPrice` at
  `regularMarketTime`) as the newest point when that is of a later day than the last bar, and the log says which day each chart ended on (`<symbol> from Yahoo Finance: 30 points, the newest of 2026-10-01`).
- **Host render harness: the `fact-long` case overran a buffer.** It copied a 63-character title into the 48-byte title of the fact with a plain `strcpy`, which wrote 16 bytes into the text field after it (hidden only
  because the text was written over it afterwards; a fortified build aborted). It truncates now. Only test code - the firmware's own title handling was not involved.
- **Agenda: with the frame awake (USB power) the next render came about 25 s too early, and a second one followed at once.** After a render the seconds to the next cron match come from the wall clock, but
  they were added to the time from before the render (the fetches and the panel's ~20 s refresh), so the next render fired right before the cron boundary and, one second later, again. The Agenda was drawn twice
  per turn - and with the `info-screens` option the rotation advanced twice, so every other page was on the panel for only a few seconds. Upstream fixed the same slip for the photo rotation; this is the same
  change for the Agenda. Checked on a frame with a 3-minute schedule and all pages: one render per turn, 179-181 s apart.
- **OTA: a frame that installed a release candidate is still offered the final release.** The version comparison
  stopped at `major.minor.patch`, so `v218.0.4-rc1` and `v218.0.4` compared equal; with the `fixes` option `-rc<n>`
  now sorts before the same version without a suffix (and `-rc2` before `-rc10`).
- **Gallery: listing a large album could hang indefinitely instead of just being slow.** `GET /api/images` used
  to walk an album's entire SD card directory in one HTTP request; an album with hundreds of source photos, each
  carrying a `.jpg` thumbnail and (with `facecrop`) a `.facecrop.json` sidecar, triples the real directory-entry
  count over the photo count alone (confirmed live: 450 photos, 1350 entries) - occasionally one of the many SD
  block reads that requires would fail outright (`allocate_dma_buf: not enough mem`, reproduced across two
  different SD cards, so not a worn-card issue), after which the request made no further progress at all. The
  endpoint now takes optional `offset`/`limit` paging, and the Web UI's gallery fetches bounded pages (60 images)
  instead of the whole album - "Load more" now fetches the next page instead of only revealing more of an
  already-fully-loaded list.
- **The Telegram bot token and chat ID were returned in plaintext by `GET /api/config` and so sat in plain view in their own Web UI fields**, pre-existing since the Telegram option was added
  (`3a55a45b`) - unlike the HTTP API password and the Agenda's calendar URLs, which this project already made write-only on purpose for exactly this reason; the chat ID was not even on the
  known list of fields this endpoint deliberately returns in plaintext (`docs/MAINTAINING.md`), so it was a plain oversight rather than a considered choice. Found from the maintainer's report of
  the fields suddenly being visible. Both are write-only now, the same pattern as `fuel_api_key`/`market_key_*`: `GET /api/config` reports only `telegram_bot_token_configured` and
  `telegram_chat_id_configured` (plus the existing combined `telegram_configured`), the Web UI's two fields start blank and are sent only when something new was typed (an empty value
  no longer clears a token by accident on an unrelated save), and each has its own **Remove** button (`telegram_bot_token_clear`/`telegram_chat_id_clear`). A full export with secrets still
  includes both, from the existing `/api/config/urls` endpoint, unchanged.
- **The Telegram card could show a stale, unrelated fetch error as if it were its own.** `last_fetch_error` is one slot shared by every image source (URL, Telegram, Home
  Assistant, ...); nothing cleared it when `rotation_mode` changed away from the mode that had set it, so a frame once run in URL mode and since switched to Telegram kept
  showing that old "Connection failed" under the Telegram card indefinitely - found live on a test frame together with the plaintext report above (the message named `ESP_ERR_HTTP_CONNECT`,
  which only the URL-mode fetch path ever produces, while this frame has been in Telegram mode). `apply_config_from_json` now clears it whenever an incoming `rotation_mode`
  actually differs from the one stored; checked live by switching the test frame's mode away and back, which cleared it immediately. Any error from the mode actually in use
  still shows exactly as before.

## [v218.0.3] - 2026-09-29

### Added

- The Agenda tab's Calendar color profiles (Settings → Agenda, slots 1-3) can now be **exported**, alongside the
  existing import/remove - `GET /api/agenda/color-profile?slot=N` downloads the raw stored profile, byte-identical
  to what `profile-editor.html` itself would export, so a profile can be backed up or moved to another device
  without needing the editor tool again.
- The debug log now reports how many Calendar A/B events were actually found within the render window on a
  successful fetch (`agenda_manager: Calendar A: N event(s) in window`) - a fetch that runs out of retries already
  logged a warning, but a *successful* fetch that legitimately (or unexpectedly) finds zero events looked
  identical to "nothing wrong" until now.

### Fixed

- **Agenda: an active Calendar source's name could disappear from the Calendar header.** The header used to show a
  source's name only when it had contributed at least one event *this* render cycle - a calendar that was fully
  configured but simply had nothing due in the current window (or hit a momentary fetch error) looked identical to
  one that had never been set up at all, with no way to tell the two apart (confirmed live: adding an event for the
  current week was the only way to make the name reappear). The header now shows every *active* source's name
  (enabled, and - for Calendar A/B - with a URL saved) regardless of whether it has events this cycle; likewise, an
  agenda render that has no events anywhere but does have at least one active source now still updates the display
  (showing all active names with an empty body) instead of silently leaving the previous, possibly stale, screen up.

### Changed

- Upstream `7ccabe0` (the image upload dithers with the preview's palette) is merged.
- The landing page inside the firmware is upstream's again: what belongs to the project's demo site only (fork links,
  pre-release channel, manifest names) is fenced with `#if FORK_SITE`, which only the demo site's build switches on.
  With every feature off the web bundle is byte-identical to upstream's again (it had silently stopped being so);
  `scripts/migrate/alloff_web.py` now checks that in CI.

## [v218.0.2] - 2026-09-28

Transition release: the project moves to [t3stier/esp32-photoframe-rebuild](https://github.com/t3stier/esp32-photoframe-rebuild).
It is published in the fork and once more in the former home, `t3ste/esp32-photoframe-rebuild`, so that frames
running v218.0.0 or v218.0.1 (which ask the former home for updates) receive it and from then on ask the fork.

### Changed

- **The frame's update feed, the web flasher (<https://t3stier.github.io/esp32-photoframe-rebuild/>) and all links
  point to the fork.** The CI bakes the feed into the firmware from the repository variable `OTA_REPO`, so the
  mirror's release builds point at the fork too. `t3ste/esp32-photoframe-rebuild` stays as a mirror.

### Fixed

- After installing an update over OTA the Updates tab kept offering the release that was just installed ("Update
  available: v218.0.1" while running v218.0.1) until the next check, because the saved "update available" state was
  restored unchecked at boot. It is only kept while the offered version is still newer than the running one.

## [v218.0.1] - 2026-09-28

First release with the full firmware; replaces v218.0.0, whose assets are the plain (upstream-equivalent) build.

### Changed

- **Releases carry the full firmware** (every optional feature the board's hardware supports) instead of the
  plain upstream build, and so do the web flasher and the frame's own update: whoever wants the plain firmware gets
  it from upstream. A frame running a release now updates itself to the next release without losing features
  (`esp32-photoframe-<board>.bin`). The plain build is still built by the CI (`-plain` file names) to prove that
  upstream's code compiles.
- The web flasher installs the firmware part by part instead of as one merged image, so **WiFi credentials and
  settings survive a flash** unless "Erase device" is ticked. (The merged image covers the settings partition, flashing
  it through the web flasher wiped every setting.) Writing the merged image with `esptool` at offset 0 still erases them.
- The `ota-channel` feature is only the stable / pre-release choice now; the "Alarm Clock firmware" option of the
  Updates tab (an old-fork variant that never existed as a release asset here) is gone.

### Fixed

- **Climate History did not load** on a device with a long log (up to 180 days, about 50,000 readings): building the
  whole log as formatted JSON took longer than the Web UI waits and more memory than the device has. The device now
  answers with at most 1000 evenly spaced readings (the newest included, in compact JSON), the chart says
  "N of M readings".
- The frame's update check could not read GitHub's chunked API answers ("Invalid content length") in builds without
  the `fixes` option; releases are built with it now.
- From upstream (`151e716`): the active rotation is scheduled from the current time after a slow refresh instead of
  from the time the tick started.

## [v218.0.0] - 2026-09-28

First release, superseded by v218.0.1 (its assets are the plain build, see above). Based on upstream `v2.18.0-27`.

### Added

- **Optional features, one flag each**: `python build.py --with <name>[,<name>...]` or `--all-features` switches
  on Telegram, weather/headline overlays, an agenda screen (ToDo + calendars), chimes, a climate history chart, a
  bedside alarm clock, stopping that alarm by voice, battery/display history charts, HTTPS, an offline hotspot
  mode, an on-screen error banner, an OTA release channel, WiFi cold-boot resilience, face-aware cropping, and a
  set of general robustness fixes — 16 in total. Asking for one a board's hardware can't run is a build error that
  names the missing capability and the boards that have it; `--all-features` skips what a board can't run instead,
  with a warning.
- **Byte-for-byte proof that "no flags" means upstream**: with every feature off, the firmware's Kconfig symbols,
  ELF symbols and `.bin` size are identical to an unmodified upstream build, and the web UI bundle is identical
  down to the byte (`scripts/verify_baseline.py`, `scripts/migrate/alloff_source.py`). Every feature also builds
  and links on its own, on a board with the hardware it needs and one without.
- **A CI build matrix**: every push builds a `plain` (no features) and a `full` (`--all-features`) firmware for
  every supported board, plus a compile-only job for every feature on its own. `plain` is what gets released.
- **A web flasher and demo page** for installing a release straight from the browser, with a toggle between the
  plain and the full build and a channel choice (stable / dev / pre-release once published).
- **This repository is its own OTA release feed** and, from here on, a real fork of upstream: future upstream
  changes can be pulled in with an ordinary `git fetch upstream && git merge upstream/main`.
- Host-side unit tests for the added modules (agenda parsing, the alarm's pattern generator, keyword spotting,
  microphone level detection, ...), alongside the existing tests for the upstream code.

### Changed

- Nothing in the upstream feature set changed — only added to, behind flags that default to off.

## Upstream

Everything before this project's own first commit is `aitjcize/esp32-photoframe`'s own history and its own
changelog, not repeated here.
