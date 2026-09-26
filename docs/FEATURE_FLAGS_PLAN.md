# Feature-flag plan

Goal: **no flags = upstream firmware (behaviour and feature set 1:1); every old-fork feature is a separate opt-in build flag.**

Baseline: `main` = upstream `aitjcize/esp32-photoframe @ 1347744` (v2.18.0-27). Reference: branch `fork-import` = old fork `v218.7.0`
(`git diff main fork-import --`: 254 files, +45.6k/-3.1k; the trailing `--` is needed because `main/` is also a directory).

## 1. Flags (Kconfig `main/Kconfig`, all default `n`)

| Flag (`CONFIG_...`) | Contents | Needs |
| --- | --- | --- |
| `FEATURE_TELEGRAM` | `telegram_bot`, Telegram commands, orientation pairing, on-device EPDGZ encode, streaming JPEG fallback | selects `FORK_EXIF`, `FORK_IMAGE_PIPELINE` |
| `FEATURE_OVERLAYS` | `overlay_manager`, `headlines`, weather icons, caption, low-battery badge | selects `FORK_WEATHER`, `FORK_EXIF`, `FORK_IMAGE_PIPELINE` |
| `FEATURE_AGENDA` | `agenda_*`, `todo`, `calendar_ics` (A-E, RRULE), 7-day grid, colour profiles, profile editor | selects `FORK_HTTP_FETCH`; weather chip only if `FORK_WEATHER` |
| `FEATURE_CHIMES` | `chime`, speaker HAL, volume/quiet hours | `FORK_HW_SPEAKER` |
| `FEATURE_CLIMATE` | `climate`, `climate_history`, badges, Agenda chip, history tab | `FORK_HW_CLIMATE_SENSOR` |
| `FEATURE_ALARMCLOCK` | `alarm_*`, alarm tones/ramp/volume, button UI (replaces `ALARM_CLOCK_ENABLED`; `build.py --alarmclock` stays as alias) | `FORK_HW_SPEAKER` (HAL audio only, not `FEATURE_CHIMES`) |
| `FEATURE_VOICE_STOP` | `kws*`, `mic_*`, mic tools | `FEATURE_ALARMCLOCK` + speaker + microphone |
| `FEATURE_BATTERY_HISTORY` | `battery_history`, chart tab | - |
| `FEATURE_DISPLAY_HISTORY` | `history_manager` (no-repeat random rotation) | - |
| `FEATURE_HTTPS` | `https_cert`, second httpd on 443, mbedTLS X509 write | - |
| `FEATURE_FACECROP` | `facecrop_metadata`, Cover/Fit variants, "Organize Crop Folders" (the `process-cli` face-crop tool is host-side, always in the tree) | selects `FORK_IMAGE_PIPELINE` |
| `FORK_FIXES` | general fixes, see 5 | - |

Hidden helpers: `FORK_HTTP_FETCH`, `FORK_WEATHER`, `FORK_EXIF`, `FORK_IMAGE_PIPELINE`, `FORK_HW_SPEAKER`, `FORK_HW_MICROPHONE`, `FORK_HW_CLIMATE_SENSOR`.
Whether orientation pairing becomes its own flag is decided in the Telegram step, once the `image_processor.c` hunks (+3.4k lines, mostly Telegram-driven) are split.

## 2. Hardware (from `components/board_hal`, verified)

| Capability | Source of truth | Boards |
| --- | --- | --- |
| Speaker + microphone (ES8311 / ES7210) | `BOARD_HAL_HAS_SPEAKER/MICROPHONE` in the board header | `waveshare_photopainter_73` only |
| Climate sensor | Kconfig `SENSOR_DRIVER_SHTC3/SHT40/SHT3X` | waveshare (SHTC3); xiao_ee03, reterminal_e1002/e1003/e1004 (SHT40); m5paper_v11 (SHT3X). **Not** xiao_ee02, xiao_ee04 |
| Everything else | - | all 8 boards |

## 3. Compatibility (three layers)

1. `build.py` validates before `idf.py`: explicit `--with X` on an unsupported board is an **error** (exit 2, names the missing capability and the boards that have it); missing flag dependencies are added with an info line (`--with voice-stop` adds `alarmclock`); `--all-features` **skips** incompatible flags and prints a `WARNING: skipped ...` list.
2. Kconfig `depends on FORK_HW_*` / `select` (hand-made sdkconfigs cannot build an invalid combination).
3. C: `feature_config.h` defines `FEATURE_X` as 0/1 and `#error`s on `FEATURE_X && !BOARD_HAL_HAS_*`; a CI check keeps the three capability sources in agreement.

## 4. Mechanics

- `build.py --with a,b` / `--all-features` / `--without x`; each flag maps to `sdkconfig.defaults.<flag>` (existing alarmclock pattern), which also carries the flag's own sdkconfig lines (HTTPS server, cross-signed roots, sockets). `build/.features` records board + flags; a change forces a reconfigure (removes the stale-`sdkconfig` trap). The `webapp` step gets `VITE_FEATURES` from the same list.
- **Off leaves nothing behind:** modules are dropped from `SOURCES`; the header supplies `static inline` no-op stubs so callers stay `#if`-free. (The old fork kept stub `.c` files; those leave symbols in the ELF and would fail the nm comparison.)
- **Shared files** (`config_manager`, `http_server`, `main`, `utils`, `power_manager`, `display_manager`, `image_processor`, `wifi_*`, `ota_manager`): keep the old fork's hunks in place, wrapped in `#if FEATURE_X`; no restructuring, so later changes in the old fork stay re-applicable. Config keys, NVS keys, `/api/config` JSON keys, URIs and handlers of a feature exist only when it is on.
- `httpd max_uri_handlers` = 50 (upstream) + per-feature counts, with a boot-time error on `HANDLERS_FULL`. CI script rejects NVS keys > 15 chars.
- Web UI: build-time `VITE_FEATURES` removes fork tabs/panels from the bundle (off = upstream UI); the runtime `*_available` flags stay for hardware that may not answer (e.g. sensor). Gated **per feature step**, not last, since a feature without its UI is not done.

## 5. General fixes: `FORK_FIXES` (default off, part of `--all-features`)

Decision: a separate flag, not always-on. Otherwise "no flags = upstream" cannot be verified, and every fix stays one small, documented `#if` hunk that can be sent upstream. Scope: DST `mktime` in the RTC drivers, `PATCH /api/config` body limit and import robustness, WiFi reprovision safety, OTA chunked-response parse, DNS fallback, debug-log flush order, album delete with subdirectories, current-path buffer, mutexes in history/OTA, timezone combobox and webapp error handlers. A feature that *requires* a setting (HTTPS: sockets 24 + mbedTLS PSRAM; Agenda: cross-signed roots; Telegram: stack) carries it in its own `sdkconfig.defaults.<flag>`, so it never depends on `FORK_FIXES`. OTA repo and asset suffix become Kconfig strings (`FORK_OTA_REPO`, default upstream), the suffix derived from the flag set.

## 6. Order (branch `feature/<x>` -> local merge to `main`; every step: waveshare + xiao_ee02 builds + host tests)

0 infrastructure (Kconfig menu, `feature_config.h`, `build.py`, validation, `.features`, CI axes, `scripts/verify_baseline.py`) - 1 `FORK_FIXES` - 2 chimes, climate, alarmclock, voice-stop - 3 battery/display history - 4 weather + overlays - 5 agenda - 6 telegram (+ image pipeline) - 7 facecrop - 8 https - 9 OTA variants, CI matrix, web flasher, docs, final checks. Progress metric: `git diff main fork-import --` shrinks to the intentional deviations.

## 7. Acceptance

- **A. Off == upstream**, per board: enabled Kconfig symbols (`=y`/values; new `# ... is not set` lines ignored) and `sdkconfig.h`; sorted `nm` of the ELF (goal: empty diff); `.bin` size; keys of `GET /api/config`; web bundle has no fork markers, size within a few %. Residual differences (version string, timestamps) documented.
- **B. All on == old fork:** all flags + `FORK_FIXES` vs a `fork-import` build (separate worktree of this repo; the old fork stays untouched): `nm`/size/`/api/config` keys.
- **C.** Every flag alone (+ deps) on waveshare and xiao_ee02; CI matrix 8 boards x {plain, full} plus single-flag compile jobs.
- **D.** Host tests; `scripts/verify_baseline.py` automates A-C.

Open: release/OTA identity of this fork (repo, asset names) - nothing is pushed until decided.
