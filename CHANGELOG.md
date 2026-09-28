# Changelog

All notable changes to this project are documented here. See [docs/FEATURES.md](docs/FEATURES.md) for the full
list of optional features and [docs/FEATURE_FLAGS_PLAN.md](docs/FEATURE_FLAGS_PLAN.md) for how they're built and
verified; this file covers what changed and when.

This is a rebuild of [aitjcize/esp32-photoframe](https://github.com/aitjcize/esp32-photoframe) (starting from
`v2.18.0-27`) — not a GitHub fork, so that an earlier fork of the same project could stay as it is — carried
forward as a real fork of its own from here on. With no build option chosen, this firmware *is* the upstream
firmware.

**Versions** are `v<upstream>.<minor>.<patch>`: the first number is the upstream version without its dot
(upstream 2.18 → `218`) and only changes once this repository has taken over everything of that upstream
version; the last two numbers count this repository's own releases (`v218.0.0`, `v218.0.1`, ...; when upstream
has a 2.19 and it is merged in: `v219.0.0`, then `v219.0.1`, ...).

## [Unreleased]

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
