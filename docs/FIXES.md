# General Fixes

> **Build option:** compiled in only with `python build.py --with fixes`; without it the firmware is the upstream
> firmware (see [FEATURES.md](FEATURES.md)). Releases build it, like every other feature the board's hardware
> supports.

Unlike the other optional features, `fixes` adds no new setting or screen - it's a bundle of small, surgical
corrections and hardening changes over upstream's own behavior, gated behind one flag (`FORK_FIXES` /
`CONFIG_FORK_FIXES`) so upstream's original code stays intact and diffable when the flag is off (see
[MAINTAINING.md](MAINTAINING.md) section 6's equality proofs). Grouped by theme, not exhaustive:

- **Web UI robustness**: importing/PATCHing a config no longer discards every other field in the request just
  because one unrelated field failed validation (`main/utils.c`) - each field is validated independently and the
  response still reports the specific error (a WiFi network that cannot be joined included; the login limiter is reset only after a password was really saved). The gallery's thumbnail listing reads a directory once (`readdir()`)
  instead of one `stat()` syscall per image, and image-serving chunks are larger (4 KB vs 1 KB) - both avoid tying
  up the single HTTP server task (and therefore the whole Web UI) for longer than necessary on a large album.
  `GET /api/images` also takes an optional `offset`/`limit` page instead of always listing an entire album in one
  request - a large facecrop-enabled album (each photo's `.jpg` thumbnail and `.facecrop.json` sidecar roughly
  triple the real directory-entry count) could otherwise make the device walk hundreds to thousands of directory
  entries in one request, occasionally hitting a transient SD DMA-allocation failure that stalled the request
  indefinitely rather than just slowly (confirmed live 2026-09-29). Enabling a nonexistent album now reports a
  proper 404 instead of a generic 500.
- **Time zone / DST correctness**: the external RTC drivers (PCF85063, PCF8563) and the Agenda calendar's ICS
  date parser (`--with agenda`) let `mktime()` work out daylight saving itself (`tm_isdst = -1`) instead of a
  zero-initialized `struct tm` forcing standard time - without this, a time read back during DST landed exactly
  one hour ahead of the real time.
- **JPEG decoding**: the photo pipeline works out the size of a decoded JPEG again in 64 bit (`main/jpeg_size_check.h`) and refuses a header whose size does not add
  up - esp_jpeg multiplies the sides of the header in 32 bit, so a header of 40000 x 35792 pixels came out as a 72 KB buffer that the decoder then wrote 4.3 GB into. It
  also looks at the result of the header read, which it used to ignore. The frame size is also read with a header walk that follows the decoder's own rule (the **last** SOF0 before SOS - `esp_jpeg_get_image_info()` reports the first, so a file with a small first and a huge second frame passed the check above and was decoded far past its buffer); a file where the two disagree is refused (`main/jpeg_header.c`, `host_tests/test_jpeg_header.cpp`; found by upstream's review of our pull request).
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
  Web UI itself pass. The settings POSTs for the processing settings and the colour palette read at most 8 KiB, in as many
  pieces as they arrive (upstream allocates whatever `Content-Length` claims and reads once). The OTA update follows only
  an `https://` download address from the release feed, and no update starts while a check is still writing its result.
  An `.epdgz` that unpacks to less than the panel needs is refused instead of showing the memory it did not fill.
- **OTA update check**: reads GitHub's releases API response the same way the weather/headline overlays already
  do (accumulated via the HTTP client's event callback), instead of a fixed-length read that failed whenever
  GitHub answered with `Transfer-Encoding: chunked` rather than a fixed `Content-Length`.
- **Home Assistant integration**: `ha_is_configured()` also checks the enabled toggle, not just whether a URL is
  saved, so a disabled integration doesn't send (or log an intent to send) notifications.
- **Diagnostics**: free/used NVS entry counts are logged on every boot, so a slow drift toward exhaustion is
  visible before it actually triggers a full NVS erase; that erase itself now logs loudly when it happens,
  instead of silently. (The exception cause and fault address of a crash, which this option used to add to the
  boot log, are part of upstream's own crash record now - the Maintenance tab's **Last crash**.)
- **Concurrency / memory safety**: the enabled-albums list is now protected by a mutex (several tasks - the HTTP
  server, the Telegram bot, auto-rotate - could otherwise race a read against a concurrent change); the buffer
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
