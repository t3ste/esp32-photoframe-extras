# General Fixes

> **Build option:** compiled in only with `python build.py --with fixes`; without it the firmware is the upstream
> firmware (see [FEATURES.md](FEATURES.md)). Releases build it, like every other feature the board's hardware
> supports.

Unlike the other optional features, `fixes` adds no new setting or screen - it's a bundle of small, surgical
corrections and hardening changes over upstream's own behavior, gated behind one flag (`FORK_FIXES` /
`CONFIG_FORK_FIXES`) so upstream's original code stays intact and diffable when the flag is off (see
[MAINTAINING.md](MAINTAINING.md) section 6's equality proofs). Grouped by theme, not exhaustive.

**Since the merge of upstream v2.19.0** a part of this list is upstream's own code and no longer sits behind the flag (it was sent upstream as a pull request, which upstream took in
five of its six fixes, with changes of its own): the RTC drivers' daylight-saving read, a config PATCH that applies every valid field and reports the rejected ones (a WiFi network that cannot be joined
included), the 8 KiB cap of the settings POSTs, the lock around the enabled-album list, the OTA check's chunked read / `https://`-only download / no race with a running check, and the JPEG header walk
(`main/jpeg_header.c`; our own size check was replaced by it). They are listed below only where this option goes beyond them.


- **Web UI robustness**: the gallery's thumbnail listing reads a directory once (`readdir()`)
  instead of one `stat()` syscall per image, and image-serving chunks are larger (4 KB vs 1 KB) - both avoid tying
  up the single HTTP server task (and therefore the whole Web UI) for longer than necessary on a large album.
  `GET /api/images` also takes an optional `offset`/`limit` page instead of always listing an entire album in one
  request - a large facecrop-enabled album (each photo's `.jpg` thumbnail and `.facecrop.json` sidecar roughly
  triple the real directory-entry count) could otherwise make the device walk hundreds to thousands of directory
  entries in one request, occasionally hitting a transient SD DMA-allocation failure that stalled the request
  indefinitely rather than just slowly (confirmed live 2026-09-29). Enabling a nonexistent album now reports a
  proper 404 instead of a generic 500.
- **Time zone / DST correctness**: the Agenda calendar's ICS date parser (`--with agenda`) lets `mktime()` work
  out daylight saving itself (`tm_isdst = -1`) instead of a zero-initialized `struct tm` forcing standard time -
  without this, a local event time during DST landed exactly one hour off. (The external RTC drivers had the
  same bug; upstream fixed them itself in v2.19.0.)
- **A photo read that fails halfway is no longer shown as a finished picture**: a BMP is stored bottom row first and read straight into the display buffer; when the
  read failed partway through (seen live: a one-off SD card I/O error, `sdmmc_read_sectors_dma`), the decoder logged it and stopped like a normal end, so everything
  above the last row read stayed white - the panel showed only the lower part of the photo and the log said "Image displayed successfully". The decoder
  (`components/epaper_src/GUI_BMPfile.c`) now reports the failure, and both album rotations (`main/display_manager.c`, sequential and random) record a photo as shown
  only once it really was: after a failure the sequential one goes on with the next photo, the random one tries one other photo of the same pool, and both give
  up cleanly (the next scheduled rotation tries again) if that fails too. PNG was never affected (libpng's own error handling fails the whole decode).
- **Cross-site requests**: a request that carries an `Origin` header naming another host than the one it was sent to is
  refused with `403` before anything else happens (`main/http_origin.h`, 10 host tests; see
  [API.md](API.md#access-control)). Without it any web page open in a browser on the same network could send a `POST` to the
  frame - `/api/factory-reset` wipes the settings with one. Requests without an `Origin` (curl, Home Assistant) and the
  Web UI itself pass. An `.epdgz` that unpacks to less than the panel needs is refused instead of showing the memory
  it did not fill.
- **OTA update check with the update channel** (`--with ota-channel`): reads GitHub's releases API response the same
  way the weather/headline overlays already do (accumulated via the HTTP client's event callback), which also picks
  the newest release for the pre-release channel; upstream's own chunked read is what runs without the channel.
- **Home Assistant integration**: `ha_is_configured()` also checks the enabled toggle, not just whether a URL is
  saved, so a disabled integration doesn't send (or log an intent to send) notifications.
- **Diagnostics**: free/used NVS entry counts are logged on every boot, so a slow drift toward exhaustion is
  visible before it actually triggers a full NVS erase; that erase itself now logs loudly when it happens,
  instead of silently. (The exception cause and fault address of a crash, which this option used to add to the
  boot log, are part of upstream's own crash record now - the Maintenance tab's **Last crash**.)
- **Memory safety**: the buffer
  holding the currently-displayed image's path was widened from 64 to 256 bytes after real-world filenames
  (e.g. exported Pixel Motion Photos) combined with an album subdirectory prefix were found silently truncating it.
- **Power management stability** (board-specific): on boards that share their SPI bus between the e-paper panel
  and an SD card, automatic light sleep is disabled as before, but the fix pins the CPU's minimum frequency equal
  to its maximum - fully disabling dynamic frequency scaling, not just light sleep, after coredumps showed a
  `esp_pm` spinlock assert crash still happening whenever a WiFi interrupt landed mid frequency-transition.
- **Kconfig-level tuning** (`features/sdkconfig.defaults.fixes`): a larger lwIP socket pool (24 vs the default 16 -
  the default ran dry under several concurrent Web UI connections) and mbedTLS allocating its TLS buffers from
  PSRAM instead of failing once internal RAM is fragmented.

None of this changes behavior with the flag off - `fixes` exists because upstream's own review cadence is slow,
and holding every one of these behind a single flag keeps them easy to build (or skip) as one unit while the
per-fix reasoning stays next to the code (`#if FORK_FIXES` / `#if defined(CONFIG_FORK_FIXES)`, grep for it).
