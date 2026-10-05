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

## [v219.0.0] - 2026-10-05

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
