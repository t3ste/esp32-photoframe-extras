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

### Added

- **Calendar priority, a fixed order of the entries and the calendar week in the Calendar view** (`agenda` option; Web UI: Settings > Agenda > Calendar):
  - **Calendar priority** (`agenda_cal_priority`, default `ABCDE`): the calendars A-E from the most to the least important (`C,A,B,D,E`, `e > d > c > b > a` and so on work too). The entries of a calendar of higher priority are shown above those of a lower one, and when a column or a grid cell has no room for everything, the entries of the *lowest* priority are the ones given up (only counted in *+N more*), wherever in the window they are - a flood of entries from a minor calendar no longer pushes an important appointment out. A setting that does not name each of the five calendars once is refused with a message (`400`).
  - **Order within a day** (`agenda_cal_order`: `priority`, the default, or `time`): the priority first and then the time, or strictly by time with the priority only breaking ties. The order is a total one now: the all-day entries of several calendars all start at midnight, `qsort` put them in an arbitrary order and a different one from one refresh to the next; ties are decided by the end, the calendar and the position in the feed.
  - **Calendar week** (`agenda_cal_show_week`, default on): the ISO week on today's divider/cell and on every Monday's, e.g. `Mo 12. KW42` (`CW42` with the English overlay language). The week arithmetic is pure C (`main/agenda_order.c`) with host tests, including a 60-year walk over the year boundaries.
  - **Rotation pattern in the list layout**: the "2-2-3", "week / week" and "3-4" patterns marked days only in the 7-day grid; the list layout (1-3 days, the default) shows them now too, with the same color profile rules (the mark color on the day header, or a filled block behind the day's entries when the profile's `markColorsHeader` is off). The Web UI shows the setting for both layouts.
- **The colour profile editor draws the frame's own picture** (`agenda` option; `profile-editor.html`, guide: `docs/PROFILE_EDITOR.md`): the preview was a hand-made look-alike of the Calendar view
  (other fonts, other cell sizes, no weather icons, dividers or truncation); it is drawn now by a JavaScript copy of the firmware's renderer (`webapp/src/utils/agendaEngine.js`: the 17 x 24 text font, the
  weather icons, grid A and B, the divider dashes, the priority order, `~` and "+N more", the colours of marked days), pixel for pixel. 88 pictures (five panel sizes from 480 x 800 to 1600 x 1200, four profiles) are compared with the
  pictures of the real `agenda_renderer.c` in `agendaEngine.test.js`, and a frame's own picture matched the copy in its edges, header bars, marked days, text rows and all day labels. The page is built around it: grid A / B,
  panel size, zoom and a PNG of the picture; marked days under the picture; a colour table that shows what the frame really draws when a switch changes a colour (the same code as the picture); the six inks as a
  palette with a warning for a colour the panel would dither; the six switches explained and greyed out when they have no effect; sample content you can edit (events, weather, language, week numbers, times, order,
  priority); **Load from frame** / **Send to frame** with the names of the three slots and the one in use; **Match my frame's settings**. Profiles export and import as before. The page is still one file (the copy of
  the code is written into it by `scripts/sync_profile_editor.py`, a unit test fails when it is out of date) and grows by 14 KB compressed.
- **The Updates tab links to the repository the frame updates from** (`fixes` option): `GET /api/ota/status` reports the update feed as `repo` (`owner/name`, built in with `--ota-repo`) and the card *Firmware Update (OTA)* shows it as a link. A repository with a wiki can have a second link (`WIKI_REPOS` in `webapp/src/utils/repoLinks.js`; none is listed yet). The READMEs of the base fork and its mirror point to the extended edition's repository.

### Changed

- **Merged upstream `3e5f3f9`** ("wifi: accept 32-character SSIDs", upstream PR #146): a network whose name has the maximum 32 characters could not be joined - the buffers kept one byte for the terminator, so the name was cut to 31, and the copy into `wifi_config_t.sta.ssid` stopped one byte short as well. The equality baseline of the proofs moved to this commit.

### Fixed

- **A failed read of the SD card is tried once more, and the log says what the heap looked like** (`fixes` option): the SD driver reads into a PSRAM or unaligned buffer through a 512-byte DMA-capable buffer from the internal heap, and when the internal heap has none for a moment (WiFi, a TLS handshake and several web requests at once) it answers `allocate_dma_buf: not enough mem, err=0x101`, FATFS turns that into a failed `fread()` and the thumbnail, image or page of that one request is missing. `fread()` in the web server, the image processor, the display manager and the file loader of the display flow is `fork_fread()` now (`main/sd_read_retry.c`, 8 host tests): on a stream error it logs the internal heap (free, the lowest it has been, the largest DMA-capable block), waits 30 ms, seeks back to where the read started and reads again, once. Anything else (the end of a file, a second failure) is what `fread()` returned.
- **No more `httpd_sock_err: error in recv : 113` at every deep sleep** (`fixes` option): the frame stopped WiFi under the browsers' open keep-alive connections, which aborted all of them (one to five warnings in a few milliseconds, every sleep). The web server is stopped first (not from its own task).
- **`/favicon.ico` is answered with `204 No Content`** (`fixes` option) instead of a `404` with two warnings in the log; the page names its icon itself.
- **Gallery > Upload Image: the processing parameters were only shown with *Wide edit*, and then they did not fit** (`fixes` option; upstream's layout): Vuetify's `md` breakpoint is a width of the *window*, so the controls column beside the preview got three squeezed columns - labels ran into each other (`Exposure ● 1.00Saturation`), "Compress Dynamic Range" broke in the middle of a word, the preset buttons (Balanced ... Custom) were cut off, and at a window width of about 1100 px the whole column collapsed to a few pixels. All the controls are cells of one grid now (two or three columns of at least 220 px, one column when there is room for no more), so the columns line up from row to row; the Tone Curve card and the preset card are equally wide (the Tone Curve card kept a 20 px margin from the normal layout); the presets are equal buttons in rows of equal length (one, two, three or six per row by the width of the card) instead of a group that wrapped after "Grayscale"; "Compress Dynamic Range" has a row of its own and no longer breaks. **Everything fits beside the picture without scrolling** - the reason for *Wide edit*: the spacing is tight (no room is kept for hints that are not there, small cards, 30 px preset buttons in one row when the card is wide enough), the Tone Curve card is smaller and, in a wide card, sits to the right of the controls instead of above them; at a window width of 1500 px the whole editor, header to *Display* button, is about 650 px high instead of about 960. With *Wide edit* off a collapsible **Processing parameters** panel under the preview (open by default) gives the same controls, so they can be changed without switching layouts. The same CSS applies to Settings > Processing and the landing page, which use the same component. **The card itself was cut off as well:** *Wide edit* lets it break out of the page column to 80 % of the window, but the tab's `v-window` it sits in is as wide as the column (about 1170 px) and has `overflow: hidden`, so from a window width of about 1460 px the card lost up to 196 px on each side - the left of the preview and the right of the controls. Upstream's page has no tab window (the fork's options add the tabs), so this part is a fork bug and is not part of what is offered upstream. While the card is there the window lets it show (`.v-window:has(.wide-edit-card) { overflow: visible }`). Checked in Chrome and Firefox at window widths from 420 to 2600 px, measuring what an ancestor clips, not only what leaves the window.
- **The CLEAR button of the crash report (Settings > Maintenance) did nothing** (every build with an option; the Web UI said *Failed to clear the crash report*, `POST /api/crash/clear` answered `404`): upstream registers its routes in a list that only a build without options uses, the fork has a list of its own (`register_all_handlers()`), and this route was missing from it since the merge of upstream v2.19.0 - so `v219.0.0` to `v219.0.3`. It is in the list now, and `scripts/test_http_routes.py` (part of the CI's tooling tests) checks that every route of upstream's list is in the fork's as well, so the next merge cannot lose one silently. The compiler had pointed at it all along (`'crash_clear_handler' defined but not used`).
- **Firefox logged "Error in parsing value for 'opacity'. Declaration dropped." for every progress bar of the Web UI** (`fixes` option; harmless, the declaration was ignored): Vuetify 3.11 writes `opacity: parseFloat(props.bgOpacity)` into the bar's inline style, which is `NaN` while the prop is not set. The value the stylesheet gives the bar anyway (`--v-border-opacity`, 0.12) is passed as the default now, so the bars look the same.
- **A timer wake a few seconds before its minute is no longer taken for a photo rotation** (`agenda` and `alarmclock` options): `main.c` lets a wake that is at most `EARLY_WAKE_TOLERANCE_SEC` (5 s) early go on as if it were on time, but `agenda_manager_wake_matches_now()` and `alarm_manager_wake_matches_now()` read the plain clock. A wake at 10:49:57 for a 10:50 schedule belongs to the minute 10:49, matched nothing and ran as a photo rotation: the 10:50 page was never drawn (and an alarm would not have rung). A deep-sleep wake lands before its minute about as often as after it. `wake_minute_clock()` (`main/wake_minute.h`, host-tested) reads the schedule at the start of the next minute when the clock is within the tolerance before it. Found on a test frame whose 10:50 Agenda schedule drew a photo instead.
- **The debug log goes on after a failed write to the SD card** (`fixes` option): FatFs keeps a disk error in the file object and fails every later call on it, so one failed write (`allocate_dma_buf: not enough mem` while WiFi, TLS and a web request had the internal heap) silenced the log for the rest of the boot - usually in the middle of a line, with no hint in the log. The writer checks `fwrite`/`fflush`/`fsync`, opens the file again (not more often than every 5 s) and notes it in the log (`[debug_log: a storage write failed, the file was opened again]`).
- **The time zone picker showed "Africa/Ceuta" for Europe/Berlin** (`fixes` option): the device stores only the POSIX rule, 33 zones share the Central European one, and the picker showed the browser's zone if that fits and otherwise the first zone of the alphabetical table - so a chosen "Europe/Berlin" turned into "Africa/Ceuta" after the save. The choice made in this browser is remembered now, and without one a list of the zones people mean (Berlin, London, New York, ...) comes before the rest (`webapp/src/utils/zonePick.js`). The frame's clock was right all along; only the name shown was a guess.
- **The dotted rule between the day and the forecast was invisible on a white page** (`agenda` option; colour profile switch `dayheadLeaderLine`): it was drawn in the header's text colour, so white text on a black header with the gaps unfilled (`headerDividerFilled` off) drew white dashes on the white page - no rule at all, and one only on the days whose header colours happened to differ (marked days with *Only highlight the header of marked days*, or *Marked header: automatic high-contrast text*). Only the dashes to the right of the forecast (the gap fill, in the header colour) showed, which looked like a rule "at the wrong place" when the forecast is not right-aligned. The rule takes the header colour now when the text colour is the colour of what it lies on (the page behind an unfilled header, the header fill of a filled one), and a safe black or white if that is the surface too; a rule that was visible stays as it was (all 56 earlier reference pictures are unchanged). The colour profile editor's picture follows it; 32 more reference pictures (three profiles with unfilled gaps, with and without marked days and a right-aligned forecast) are compared with the real renderer.
- **A schedule rule was described as something it is not** (`fixes` option): the builder said "Rotate every 45 minutes" for `*/45 * *`, which fires at :00 and :45 of every hour (a cron step restarts with every hour), and "every 5 hours" for `0 */5 *`, which has a gap of four hours over midnight. The text under a rule is worked out from the minutes, hours and weekdays the rule hits now (`Rotate at :00, :45 of every hour, every day`; `45 * *` is `every hour, at :45`) and a custom rule is explained too (`webapp/src/utils/cronText.js`, a test checks every "every n" it says against the times the rule fires at).

## [v219.0.3] - 2026-10-11

The image is about 750 KB smaller, nothing was taken away; and one fix of the Telegram bot's status text. On upstream v2.19.0 as before.

### Changed

- **The firmware and the update are about 750 KB smaller** (`fixes` option). The boards with internal flash have a 3.5 MiB update slot and the offline Web UI had filled it (a full build for the
  reTerminal E1004 took 3.10 MiB of it, with the extras 3.46 MiB). Measured on the CI images with every option: the Waveshare image went from 2,902,368 to 2,171,456 bytes (-25.2 %), the E1004's from 3.10 to 2.41 MiB,
  the XIAO EE02's from 3.02 to 2.33 MiB. Four changes, none of which takes a function away:
  - the **icon font holds only the icons the Web UI uses** (105 of 7,448; the stylesheet in the firmware went from 596 KB to 130 KB): `webapp/mdi-subset/` has the Vite plugin, the cut-down font and the list,
    a build error when a source uses an icon that is not in it, and `npm run mdi-subset` to cut the font again (Python with `fonttools` and `brotli`);
  - the **example photo of the palette calibration help** is embedded re-encoded: 42 KB instead of 152 KB (JPEG quality 80, same size);
  - the firmware is built with **`-Os`** instead of ESP-IDF's default `-Og` (168 KB; the on-device processing of a JPEG took 26.6 s instead of 27.6 s, most of it the panel's refresh);
  - **Roboto 300**, which the Web UI does not use (only Vuetify's `text-h1`/`text-h2`), is no longer bundled (22 KB).

### Fixed

- **The status text of the Telegram bot lost its last switches** (`telegram` option): `/status` and the wake-up message list 17 switches (625 characters) in buffers of 384 and 900 bytes, so the list ended in the
  middle of its eleventh line. The text has room now (the message buffer is on the heap: the wake-up message is built on the main task, whose stack is small). Found by the `-Os` build, which sees what
  `-Og` did not; the same build made one more path safe that could have been cut: the one of a preserved Telegram original is skipped when it does not fit.

## [v219.0.2] - 2026-10-10

Fixes only, on upstream v2.19.0. The Web UI fixes come from driving the Web UI of a frame with a script (every tab, panel and control, the import and export, uploads, the albums); seven of them are offered upstream as pull requests ([#147](https://github.com/aitjcize/esp32-photoframe/pull/147) - [#153](https://github.com/aitjcize/esp32-photoframe/pull/153)). The Agenda fixes concern calendar feeds with repeating events.

### Fixed

- **A config request tells which of its fields the frame ignored** (`fixes` option): `PATCH /api/config` answered `{"status":"success"}` also when it had skipped a field - a value of the wrong JSON type (`"rotate_interval": "soon"`), or a key the firmware does not know. The answer (and its `400`) now has `"ignored": [...]` for the first and `"unknown": [...]` for the second, each left out when empty; JSON `null` is never listed; a server-pushed config is not checked. `docs/API.md` describes it; the Web UI's import names the ignored settings after an import. `main/config_track.c` does the bookkeeping, `utils.c` switches it on with file-local macros (no change at its 175 call sites), 13 host tests.
- **The Web UI needs no internet access any more** (`fixes` option): the icons and the font came from `cdn.jsdelivr.net` and `fonts.googleapis.com`; a frame without internet access (its own hotspot, a LAN with no outbound access) showed a page without icons in a fallback font. `@mdi/font` and `@fontsource/roboto` (woff2, latin 300-700) are part of the bundle, inlined into the stylesheet - about 540 KB more flash, nothing is loaded from another host. `index.html` and `main.js` carry the change under `#if FORK_FIXES`; a build without the option loads from the CDNs as upstream does. The Vite dev server resolves the directives of `index.html` now too.
- **An import tells what the frame did not take** (`fixes` option): a value of the wrong type is skipped by the frame, which still answers `success`, so the import ended in "Config imported successfully!". The file is compared with what the frame reports after the import (read-only status values left out) and the differing settings are named; a refusal of the frame (HTTP 400) shows the frame's own message.
- **The gallery did not tell when the frame refused an album or image action** (`fixes` option): creating an album with a name the frame does not take (`a/b`, one that exists), deleting an album or an image and showing an image on the display closed their dialogs as if they had worked. They report the failure now; the New album dialog stays open for correcting the name, and its Create button is off while the name is empty.
- **Two silent failures of the Web UI** (`fixes` option), found by driving the Web UI of a frame with a script: choosing a file the browser cannot read as an image (damaged, empty, an unsupported format) left a blank preview with an Upload button that did nothing - it says "This file could not be read as an image" now; and importing a file that is valid JSON but no config export (`null`, a number, a list, `{}`) opened the overwrite dialog and ended in "Config imported successfully!" although nothing was sent - it is refused with "This file is not a config export".
- **The "Display image?" and "Delete image?" dialogs of the gallery asked for a picture that does not exist** (`fixes` option): for an image without a thumbnail they requested `<album>/undefined` - a 404 in the browser console (`GET /api/image?filepath=Default%2Fundefined`) and a broken picture in the dialog. They show no picture now. Found by clicking through the whole Web UI with a script (every tab, panel and control, 900 in all, on a frame) and reading the browser console.
- **The Settings export failed in a build with the Agenda but without the `fixes` option** (Web UI: *Failed to export config: ReferenceError: exportIncludeSecrets is not defined*, no file was written): the code that leaves the credentials out of an export used a switch that only the `fixes` option declared. It is declared in every build that uses it now. The same mistake made the Alarm Clock's on switch fail with a ReferenceError (instead of the hint "Add a schedule below first") in a build with the Alarm Clock but without the Agenda. Releases are full builds and were not affected. `scripts/migrate/web_bindings.py` now also checks the script of every fenced component for names it uses without declaring them, in every feature set (it only looked at what the templates use), so a fence that cuts a declaration off from its users is found before a build.

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
- **The Telegram bot token and chat ID were returned in plaintext by `GET /api/config` and so sat in plain view in their own Web UI fields**, pre-existing since the Telegram option was added
  (`3a55a45b`) - unlike the HTTP API password and the Agenda's calendar URLs, which this project already made write-only on purpose for exactly this reason; the chat ID was not even on the
  known list of fields this endpoint deliberately returns in plaintext (`docs/MAINTAINING.md`), so it was a plain oversight rather than a considered choice. Found from the maintainer's report of
  the fields suddenly being visible. Both are write-only now, the same pattern as the Calendar/ToDo addresses: `GET /api/config` reports only `telegram_bot_token_configured` and
  `telegram_chat_id_configured` (plus the existing combined `telegram_configured`), the Web UI's two fields start blank and are sent only when something new was typed (an empty value
  no longer clears a token by accident on an unrelated save), and each has its own **Remove** button (`telegram_bot_token_clear`/`telegram_chat_id_clear`). A full export with secrets still
  includes both, from the existing `/api/config/urls` endpoint, unchanged.
- **The Telegram card could show a stale, unrelated fetch error as if it were its own.** `last_fetch_error` is one slot shared by every image source (URL, Telegram, Home
  Assistant, ...); nothing cleared it when `rotation_mode` changed away from the mode that had set it, so a frame once run in URL mode and since switched to Telegram kept
  showing that old "Connection failed" under the Telegram card indefinitely - found live on a test frame together with the plaintext report above (the message named `ESP_ERR_HTTP_CONNECT`,
  which only the URL-mode fetch path ever produces, while this frame has been in Telegram mode). `apply_config_from_json` now clears it whenever an incoming `rotation_mode`
  actually differs from the one stored; checked live by switching the test frame's mode away and back, which cleared it immediately. Any error from the mode actually in use
  still shows exactly as before.
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
  was only there with the offline hotspot although the error banner and the chimes use it, and the error-overlay card with its test button showed in builds
  without `error-banner` and failed when pressed. Fixed. `scripts/migrate/web_bindings.py` now checks every feature set for this (see [docs/MAINTAINING.md](docs/MAINTAINING.md)); the
  full build and a build with no option were not affected, and the all-off proof is unchanged.
- **Agenda: with the frame awake (USB power) the next render came about 25 s too early, and a second one followed at once.** After a render the seconds to the next cron match come from the wall clock, but
  they were added to the time from before the render (the fetches and the panel's ~20 s refresh), so the next render fired right before the cron boundary and, one second later, again - the Agenda was drawn
  twice per turn, a wasted refresh of a colour panel. Upstream fixed the same slip for the photo rotation; this is the same one-line change for the Agenda (`power_manager.c`).
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
