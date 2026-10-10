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
- **The gallery ignored the frame's answer** when it was asked to create or delete an album, delete an image or show one on the display: the dialog closed as if it had worked (an album called `a/b`, or one that exists, simply did not appear). It says "Could not create the album ..." / "Could not delete ..." / "Could not show the image on the display" now, and the New album dialog stays open so the name can be corrected (its Create button is off while the name is empty).
- **Upload of a file the browser cannot read as an image** (damaged, empty, a format it cannot decode): the preview stayed blank and the Upload button did nothing, with the reason only in the browser console. The Web UI says "This file could not be read as an image" and drops the file.
- **Import of a file that is JSON but not a config export** (`null`, a number, a text, a list, `{}`, a settings block that is not an object): the "overwrite your settings" dialog opened and the import ended in "Config imported successfully!" although nothing was sent (with `null` in silence). It is refused with "This file is not a config export" now.
- **The gallery's confirmation dialogs for an image without a thumbnail** (Display image / Delete image) asked the frame for `<album>/undefined`: a 404 in the browser console and a broken picture in the dialog. They show no picture then. (The grid itself already had the placeholder.)
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
- **Network**: the frame always seeds the second and third DNS server slot with public resolvers (`1.1.1.1`,
  `8.8.8.8`; lwIP only tries them after the first one timed out), because a single flaky resolver - typically the
  router's own, handed out by DHCP - made every hostname fail (Telegram and the weather API alike) for a whole wake
  cycle; the optional DNS override fills the first slot. On the provisioning page, the scan for networks first
  cancels the association attempt that switching to access-point-plus-station mode starts on its own, which made
  the scan fail with "STA is connecting" and show "0 networks found" at random.
- **Albums**: deleting an album removes its subdirectories as well (e.g. the face-crop `crop` folder) instead of failing
  on the first directory that is not empty.
- **Switching Auto Rotate mode** clears the "Last fetch error" of the mode it leaves; a stale "Connection failed" from
  URL mode no longer sits on the page after the frame is switched to Telegram.
- **Settings and gallery pages**: the settings export leaves the credentials out unless "Include credentials and URLs
  in export" is ticked (the WiFi password never leaves the device at all), and it carries which albums are enabled;
  the gallery lists photos in pages ("Load more") and shows thumbnails only when asked (a switch remembered in the
  browser, off by default - many thumbnails at once slow the frame's web server down); uploads can be encoded as
  `.epdgz` (default, already palette-indexed and compressed) or PNG; the Home Assistant integration has its own
  enabled switch.
- **Cross-site requests**: a request that carries an `Origin` header naming another host than the one it was sent to is
  refused with `403` before anything else happens (`main/http_origin.h`, 10 host tests; see
  [API.md](API.md#access-control)). Without it any web page open in a browser on the same network could send a `POST` to the
  frame - `/api/factory-reset` wipes the settings with one. Requests without an `Origin` (curl, Home Assistant) and the
  Web UI itself pass. An `.epdgz` that unpacks to less than the panel needs is refused instead of showing the memory
  it did not fill.
- **OTA update check**: the releases API response is read through the shared `http_fetch_get()` (the way the
  weather/headline overlays read theirs: accumulated via the HTTP client's event callback, 64 KB cap), which also
  serves the update channel's "newest release" lookup (`--with ota-channel`); upstream's own reader (chunked-aware
  since v2.19.0) is what runs without this option. Three more corrections sit next to it: versions compare on
  `major.minor.patch` and then the pre-release suffix (`-rc2` is older than `-rc10`, and any `-rcN` is older than the
  same version without one, so a frame that installed a release candidate is still offered the final release); a
  saved "update available" is dropped at boot once the running version is that very release (it was offered again
  right after installing it); and the check enters its "checking" state before the task starts, so the Web UI's
  request that waits for the result no longer sees the old state and answers "no update" at once.
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
