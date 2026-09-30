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
- **The demo package is online.** The site's deploy now serves `examples/` at
  `https://t3stier.github.io/esp32-photoframe-rebuild/examples/waveshare_photopainter_73/` (the calendars and ToDo list
  the two importable demo configs point at - importing one used to leave calendars A-E and the ToDo list failing
  with HTTP 404 in the frame's log) and, as its last step, checks that every URL those configs use answers `200`.
  Three example Calendar color profiles (`color_profiles/`) join the package, and the landing page's "How it goes"
  list, `README.md` and `docs/FEATURES.md` link to it ([docs/DEMO_PACKAGE.md](docs/DEMO_PACKAGE.md)).
- The web flasher's pre-release entry (`manifest-prerelease.json`) now survives the site's next deploy: the site is
  rebuilt from scratch on every deploy, so the newest published pre-release (while it is newer than the stable
  release) is restored from its release like the stable one already was.

### Changed

- Upstream `495e0b6` is merged (two commits, both in `main/main.c`): waking a frame with its CLEAR button no longer
  re-runs `board_hal_init()`/`display_manager_init()` (the second `spi_bus_initialize()` aborted, so the frame
  panicked and rebooted instead of clearing the screen - every ESP32-S3 board was affected; the climate reading
  taken on that wake is kept), and the boot-time coredump log now also prints the panic reason and keeps a dump it
  cannot summarise in flash instead of erasing it (debug builds only; the `fixes` option's extra `exc_cause`/
  `exc_vaddr` line is kept on top of it).

### Fixed

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
