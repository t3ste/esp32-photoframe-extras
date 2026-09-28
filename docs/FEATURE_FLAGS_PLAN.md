# Feature-flag plan

Goal: **no flags = upstream firmware (behaviour and feature set 1:1); every old-fork feature is a separate opt-in build flag.**

Baseline: `main` = upstream `aitjcize/esp32-photoframe @ 1347744` (v2.18.0-27). Reference: branch `fork-import` = old fork `v218.7.0`
(`git diff main fork-import --`: 254 files, +45.6k/-3.1k; the trailing `--` is needed because `main/` is also a directory).

## 1. Flags (Kconfig `main/Kconfig`, all default `n`)

| Flag (`CONFIG_...`) | Contents | Needs |
| --- | --- | --- |
| `FEATURE_TELEGRAM` | `telegram_bot`, Telegram commands, orientation pairing, on-device EPDGZ encode, streaming JPEG fallback | selects `FORK_EXIF`, `FORK_IMAGE_PIPELINE` |
| `FEATURE_OVERLAYS` | `overlay_manager`, `headlines`, weather icons, caption, low-battery badge | selects `FORK_HTTP_FETCH`, `FORK_WEATHER`, `FORK_CLIMATE_CORE`, `FORK_EXIF`, `FORK_IMAGE_PIPELINE` |
| `FEATURE_AGENDA` | `agenda_*`, `todo`, `calendar_ics` (A-E, RRULE), 7-day grid, colour profiles, profile editor | selects `FORK_HTTP_FETCH`, `FORK_WEATHER`, `FORK_CLIMATE_CORE`, `FORK_IMAGE_PIPELINE` |
| `FEATURE_CHIMES` | `chime`, speaker HAL, volume/quiet hours | `FORK_HW_SPEAKER`; selects `FORK_AUDIO_HAL` |
| `FEATURE_CLIMATE` | `climate_history`, badges, history tab (the sensor read itself is the `FORK_CLIMATE_CORE` helper) | `FORK_HW_CLIMATE_SENSOR`; selects `FORK_CLIMATE_CORE`, `FORK_IMAGE_PIPELINE` |
| `FEATURE_ALARMCLOCK` | `alarm_*`, alarm tones/ramp/volume, button UI (replaces `ALARM_CLOCK_ENABLED`; `build.py --alarmclock` stays as alias) | `FORK_HW_SPEAKER`; selects `FORK_AUDIO_HAL` (HAL audio only, not `FEATURE_CHIMES`) |
| `FEATURE_VOICE_STOP` | `kws*`, `mic_*`, mic tools | `FEATURE_ALARMCLOCK` + speaker + microphone |
| `FEATURE_BATTERY_HISTORY` | `battery_history`, chart tab | - |
| `FEATURE_DISPLAY_HISTORY` | `history_manager` (no-repeat random rotation) | - |
| `FEATURE_HTTPS` | `https_cert`, second httpd on 443, mbedTLS X509 write | - |
| `FEATURE_OFFLINE_HOTSPOT` | offline mode, on-demand AP hotspot (BOOT 3 s), setup-page offline option | - |
| `FEATURE_ERROR_BANNER` | on-display error banner incl. "no internet" tracking | selects `FORK_IMAGE_PIPELINE` |
| `FEATURE_OTA_CHANNEL` | stable/pre-release channel, firmware variant choice | - |
| `FEATURE_WIFI_RESILIENCE` | credential-reject detection, cold-boot retry policy (retries, persistent attempt count, "reprovision on failure" switch), battery TX-power cap, WiFi performance mode | - |
| `FEATURE_FACECROP` | `facecrop_metadata`, Cover/Fit variants, "Organize Crop Folders" (the `process-cli` face-crop tool is host-side, always in the tree) | selects `FORK_IMAGE_PIPELINE` |
| `FORK_FIXES` | general fixes, see 5 | - |

Found during the inventory (not in the first list): offline mode/hotspot, error banner and the OTA channel above; also small options that ride on `FORK_FIXES`
(DNS fallback, HA on/off). `process-cli` (host tool) is taken over as is - it is not part of the firmware.

Hidden helpers (no prompt, selected by the features above): `FORK_HTTP_FETCH` (HTTP GET client), `FORK_EXIF`, `FORK_WEATHER`, `FORK_CLIMATE_CORE` (sensor read shared by overlays, agenda and climate), `FORK_IMAGE_PIPELINE` (everything the fork added to `image_processor`: caption/overlay drawing, streaming JPEG decode, ...), `FORK_AUDIO_HAL` (speaker/microphone code of `board_hal`), `FORK_OTA_REPO` (string, default upstream), and the capability symbols `FORK_HW_SPEAKER`, `FORK_HW_MICROPHONE`, `FORK_HW_CLIMATE_SENSOR` derived from the board.
Orientation pairing stays part of `FEATURE_TELEGRAM` (it is Telegram-driven; the `image_processor` code it needs is `FORK_IMAGE_PIPELINE`).

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
- **Shared files** (`config_manager`, `http_server`, `main`, `utils`, `power_manager`, `display_manager`, `image_processor`, `wifi_*`, `ota_manager`): keep the old fork's hunks in place, wrapped in `#if FEATURE_X ... #else <upstream> #endif`; no restructuring, so later changes in the old fork stay re-applicable. Config keys, NVS keys, `/api/config` JSON keys, URIs and handlers of a feature exist only when it is on. The bulk of this was done with `scripts/migrate/gate.py` (a per-file map from hunk to flag, `scripts/migrate/maps/`), which proves textually that *all off* reproduces the upstream file and *all on* the old fork's; `main.c`, `http_server.c`, `display_manager.c`, `ota_manager.c` and `wifi_*` needed hand integration where the old fork and upstream changed the same code.
- **The storage layer is one guard.** `config.h`, `config_manager.{h,c}` are guarded as a whole with `FORK_ANY` (`feature_config.h`: any feature or `FORK_FIXES` on): features read each other's settings (the Telegram status text lists overlay, WiFi and EXIF options, Agenda reads the overlay language and weather icon set), so per-feature guards there would multiply the compile matrix for no gain. What a disabled feature adds to the outside world - JSON keys, handlers, UI - stays behind its own flag; storage code that nothing reaches is dropped by the linker (`--gc-sections`).
- **Cross-feature calls** go through stubs in the module header (`#else` branch of `#if FEATURE_X`): `static inline` functions or function-like macros that do nothing, so shared code stays `#if`-free wherever the call is harmless.
- **WiFi resilience on upstream's newer boot flow.** Upstream meanwhile bounds the cold-boot connect and keeps the credentials when it merely timed out (`ESP_ERR_TIMEOUT`, background reconnect, `late_wifi_task`). `FEATURE_WIFI_RESILIENCE` builds on that: an `ESP_FAIL` that is not a credential rejection gets 3 tries, then (default) a reboot loop with a persistent attempt count (max 10), and with "reprovision on failure" off the credentials are kept: deep sleep until the next wake, or - without deep sleep - the same background reconnect as a timeout. A genuine rejection is unchanged (wipe and reprovision).
- **`git diff main fork-import` is not a pure list of fork additions.** The old fork's merges of upstream lost parts of five upstream commits (bounded WiFi wake with `late_wifi_task`, HTTP API password check, power_manager backoff and debug-log flush, M5Paper CMake bits). Hunks that only undo upstream code are **not** carried over; where the old fork replaced upstream code on purpose (e.g. the WiFi credential wipe), the replacement goes under its flag. So *all on* differs from the old fork exactly at those places, and the acceptance report lists them.
- `httpd max_uri_handlers` = 50 (upstream) + per-feature counts, with a boot-time error on `HANDLERS_FULL`. CI script rejects NVS keys > 15 chars.
- Web UI: build-time `VITE_FEATURES` removes fork tabs/panels from the bundle (off = upstream UI); the runtime `*_available` flags stay for hardware that may not answer (e.g. sensor). Gated **per feature step**, not last, since a feature without its UI is not done.

## 5. General fixes: `FORK_FIXES` (default off, part of `--all-features`)

Decision: a separate flag, not always-on. Otherwise "no flags = upstream" cannot be verified, and every fix stays one small, documented `#if` hunk that can be sent upstream. Scope: DST `mktime` in the RTC drivers, `PATCH /api/config` body limit and import robustness, WiFi reprovision safety, OTA chunked-response parse, DNS fallback, debug-log flush order, album delete with subdirectories, current-path buffer, mutexes in history/OTA, timezone combobox and webapp error handlers. A feature that *requires* a setting (HTTPS: sockets 24 + mbedTLS PSRAM; Agenda: cross-signed roots; Telegram: stack) carries it in its own `sdkconfig.defaults.<flag>`, so it never depends on `FORK_FIXES`. OTA repo and asset suffix become Kconfig strings (`FORK_OTA_REPO`, default upstream), the suffix derived from the flag set.

## 6. Order (branch `feature/<x>` -> local merge to `main`; every step: waveshare + xiao_ee02 builds + host tests)

0 infrastructure (Kconfig menu, `feature_config.h`, `build.py`, validation, `.features`, CI axes, `scripts/verify_baseline.py`) - 1 `FORK_FIXES` - 2 chimes, climate, alarmclock, voice-stop - 3 battery/display history - 4 weather + overlays - 5 agenda - 6 telegram (+ image pipeline) - 7 facecrop - 8 https - 9 OTA variants, CI matrix, web flasher, docs, final checks. Progress metric: `git diff main fork-import --` shrinks to the intentional deviations.

Actual course: the shared-file gating (steps 1-8, C code and web UI) was done in one pass on `feature/gating` with the tooling above, followed by the compile matrix (`scripts/feature_matrix.py`: every flag alone + `off` + `all`, one build directory per set) and the static cross-reference check (`scripts/migrate/xref.py` finds a symbol that only exists in a guarded-out region in seconds instead of a build). Step 9 (CI matrix, web flasher, docs) follows.

## 7. Acceptance

- **A. Off == upstream**, per board: enabled Kconfig symbols (`=y`/values; new `# ... is not set` lines ignored) and `sdkconfig.h`; sorted `nm` of the ELF (goal: empty diff); `.bin` size; keys of `GET /api/config`; web bundle has no fork markers, size within a few %. Residual differences (version string, timestamps) documented.
- **B. All on == old fork:** all flags + `FORK_FIXES` vs a `fork-import` build (separate worktree of this repo; the old fork stays untouched): `nm`/size/`/api/config` keys.
- **C.** Every flag alone (+ deps) on waveshare and xiao_ee02; CI matrix 8 boards x {plain, full} plus single-flag compile jobs.
- **D.** Host tests (`host_tests/`): the upstream tests run on the upstream code (`stubs/sdkconfig.h` is empty, so every `CONFIG_*` is undefined); the feature module tests (headlines, todo, calendar, alarm pattern, mic level, keyword spotting) and a second build of the two image tests ("fork" variants: `CONFIG_FORK_IMAGE_PIPELINE` and all features defined) cover the fork code. `scripts/verify_baseline.py` automates A-C.

## 8. Status

Done and verified (2026-09-27):

- **A** all off == upstream: Kconfig symbols, ELF symbols and `.bin` size identical on `waveshare_photopainter_73` and `seeedstudio_xiao_ee02`; the web bundle is byte-identical (16 files incl. `.gz`). At source level `scripts/migrate/alloff_source.py` resolves every guard with all flags off and compares each of the 170 files that also exist upstream: only the intended build wiring differs (it runs in CI).
- **B** all on vs the old fork tip (`v218.7.0`, built from a `fork-import` worktree with `--alarmclock`) on `waveshare_photopainter_73`: the image is 608 B larger (+0.03 %). Nothing exists only in the old fork's ELF. The differences are the upstream code the old fork had lost (`GUI_ReadBmp_RGB_Gray16`, `GUI_ReadPng_Gray16`, `Paint_DrawGrayscaleCalibrationPattern`, the bounded WiFi wake: `wifi_manager_keep_reconnecting`, `late_wifi_task`, `startup_online_work`, `forget_wifi_and_reprovision`) and `cold_boot_wifi_retry`, the old fork's cold-boot retry re-implemented on that flow; the Kconfig symbol `ALARM_CLOCK_ENABLED` is now `FEATURE_ALARMCLOCK`.
- **C** every flag alone, none and all: compile + link on `waveshare_photopainter_73` (18 sets); compile on `seeedstudio_xiao_ee02` (14 sets, the board without speaker, microphone and sensor); none/all compile on all other boards; all-features links on `seeedstudio_xiao_ee02` and the ESP32 board `m5stack_m5paper_v11`. Web app builds for every set.
- **D** 292 host tests pass; formatters (clang-format 18, black, isort, prettier) clean.

**Release / OTA identity: decided.** The project lives in its own repository (`esp32-photoframe-rebuild`, not a GitHub fork, so the earlier fork stays as it is) and publishes its own releases there. `main/Kconfig`'s `FORK_OTA_REPO` defaults to that repository; `build.py --ota-repo owner/name` (the CI's release/full builds use `--ota-repo ${{ github.repository }}`) overrides it, e.g. to compare against upstream. `scripts/verify_baseline.py`'s acceptance-A run does exactly that automatically (pins the candidate's OTA repo to upstream's own hardcoded value) so "no flags == upstream" keeps holding at the source level even though the *shipped* default now differs - the difference is a single build-time string (`main/config.h`'s `GITHUB_API_URL`), never a code or behaviour change, and is listed as an intended exception in `scripts/migrate/alloff_source.py`. `fixes` (`FORK_FIXES`) stays exactly what it already was: one optional feature among the 16, opt-in via `--with fixes`, never default-on - nothing to change there.

**This repository now tracks upstream as a real fork.** Its own history (this file's "actual course" above) is grafted onto the real upstream commit `1347744414364110f96d9c121b4cc6e13b2364f2` (`aitjcize/esp32-photoframe @ v2.18.0-27`) as its parent - not merely a copy of its tree, an actual shared-ancestor link - so a later `git fetch upstream && git merge upstream/main` (remote `upstream` already configured) is a normal three-way merge, not a from-scratch reconciliation. `scripts/verify_baseline.py`, `scripts/migrate/alloff_source.py` and `scripts/migrate/gate.py` reference that same fixed SHA as the baseline (a graft moves what `git rev-list --max-parents=0` finds all the way back to upstream's own true root, so that shortcut no longer identifies the right commit).

**Web flasher / demo page: ported**, adapted from the old fork's for two build variants instead of an alarm-clock one: `scripts/generate_manifests.py --variant full` (matching the CI's `plain`/`full` build matrix), `scripts/launch_demo.py` and the landing page (`webapp/src/views/LandingPage.vue`) derive the GitHub repo from the git remote rather than a hardcoded fork URL, and the "All features" toggle applies to every board (not board-restricted, since `--all-features` itself already skips what a board can't run). `webapp/vite.config.demo.js` needed its own `featureDirectives` plugin (a pre-existing gap: the demo page pulls in the shared store modules, which carry the same fenced `#if` directives as the main app) and its GitHub Pages base path, which the old fork itself had never actually corrected to its own name.

**CI: has run** (first runs 2026-09-28): the formatting job, host tests, feature tooling, all 16 firmware builds (8 boards x plain/full), the per-feature compile jobs and `deploy-pages` are green; the baked-in OTA target of the CI-built plain and full binaries was checked and is this repository. `deploy-pages` publishes the demo site and the per-board dev manifests (plain and full) to the `gh-pages` branch; GitHub Pages is enabled (deploy from branch `gh-pages`, folder `/`); the demo page and the manifests are served at `https://t3ste.github.io/esp32-photoframe-rebuild/` (checked: page, assets under the base path, plain and full dev manifests and firmware all answer 200).

Not decided, so not done:

- OTA asset names for feature builds: only the plain `esp32-photoframe-<board>.bin` is ever published as a release asset (installing an update replaces a `full` build with the plain one, see `docs/FEATURES.md`); `full` stays a workflow artifact and only ever gets a *dev* manifest on the demo page, never a stable one.
- Cutting an actual first release (tag, version number): the `release` job has never run, it only runs for a `v*` tag push.
- The pre-release channel's CI wiring: the landing page and `generate_manifests.py` already support a `manifest-prerelease(-full)?.json`, but `deploy-pages` does not yet detect/deploy an actual GitHub pre-release; needs a tag-naming convention decided together with the first release.
- The old fork's `docs/DIFF.md` and README screenshots are not taken over (`CHANGELOG.md` *was* - written fresh for this repository's own history, not imported).
- On-device test of the hand-integrated boot paths of `main.c`: done (2026-09-27, both physical test devices) - see the runbook.

## 9. Pulling in a later upstream change

Upstream keeps moving; this repository is a real fork of it (section 8), so a later upstream commit is a normal merge,
not a from-scratch diff. Do this whenever the user asks for it - never on its own initiative (a merge changes a lot of
files at once and needs review before it's committed, let alone pushed):

1. `git fetch upstream` (remote already configured, points at `aitjcize/esp32-photoframe`).
2. See what's new: `git log main..upstream/main --oneline` (and `git diff main...upstream/main -- <file>` for any file
   worth a closer look before merging).
3. `git switch -c feature/upstream-<date>` and `git merge upstream/main` there - never merge upstream directly into
   `main`. Most files merge cleanly (this repository's own tree at the graft point is byte-identical to upstream's, so
   git has a real common ancestor to diff against). Two kinds of conflict are expected:
   - A file gate.py generated (`#if FEATURE_X ... #else <upstream> #endif`, most of `main/`): the conflict is inside
     the `#else`/ungated branch, since that's literally upstream's own code. Resolve it the ordinary way (take
     upstream's new code for that branch); the `#if FEATURE_X` branch is unaffected unless the same lines changed.
   - A file with **manual edits** (`main/main.c`, `main/display_manager.c`'s random-pick block, `main/http_server.c`'s
     init tail/includes, `main/ota_manager.c`'s `api_url` block, the stubified headers - see 7c/7d and the runbook's
     "manually integrated" list): resolve by hand, keeping the flag-gating structure; re-reading the corresponding
     hunk of this file's own history (`git log -p` on it) shows how the original hand-integration reasoned about it.
4. After resolving conflicts, re-verify from scratch - a merge can silently break "no flags == upstream" even with no
   conflicts (e.g. upstream renaming something a gate references): `scripts/migrate/xref.py`,
   `scripts/migrate/alloff_source.py`, `scripts/verify_baseline.py --board <b>` for at least one board with the
   hardware and one without, the full feature compile matrix (`scripts/feature_matrix.py --board <b> single`), and
   the host tests.
5. Update `docs/FEATURE_FLAGS_PLAN.md`'s baseline mentions only if upstream's own versioning changed (the graft point
   itself, `1347744...`, never moves - new upstream commits just land on top of it); add a `CHANGELOG.md` entry.
6. Merge `feature/upstream-<date>` into `main` locally, same as any other feature branch; push needs the user's OK
   like any push.

## 10. Versioning and releasing

**Scheme** (decided by the user, 2026-09-28): `v<upstream>.<minor>.<patch>`. The first number is upstream's version
without the dot (upstream 2.18 → `218`) and only changes once this repository has taken over everything of that
upstream version; the last two numbers count this repository's own releases: `v218.0.0` (first release, on top of
upstream `v2.18.0-27`), `v218.0.1` next while upstream has no new release, `v219.0.0` after upstream 2.19 has been merged
in (section 9), then `v219.0.1` ... The firmware's OTA compares `major.minor.patch` numerically (`main/ota_manager.c`
`version_compare`), so these sort correctly. Pre-releases (the `ota-channel` feature's channel) are not wired up in the
CI yet - choose a tag suffix such as `-rc1` together with that work.

**Steps** (only when the user asks for a release):

1. Move the `CHANGELOG.md` entries from `[Unreleased]` into a new `## [vX.Y.Z] - <date>` section, commit on `main`.
   The commit that gets tagged must NOT contain `[skip ci]` in its message (GitHub skips tag-push workflows whose head
   commit says so, and then no release is built).
2. `git tag -a vX.Y.Z -m "..."` on that commit, `git push origin main vX.Y.Z`.
3. The `Build Firmware` workflow builds all 8 boards, and its `release` job creates a **draft** release with the plain
   `esp32-photoframe-<board>.bin` (what the OTA installs) and `photoframe-firmware-<board>-merged.bin` (what the web
   flasher and `esptool` write at offset 0) of every board; `full` builds are not part of a release.
4. Edit the draft's notes (`gh release edit vX.Y.Z --notes-file ...`), then publish it (`gh release edit vX.Y.Z
   --draft=false`). Publishing triggers the workflow once more, which refreshes the GitHub Pages demo so the web
   flasher's "stable" entry follows the published release.
