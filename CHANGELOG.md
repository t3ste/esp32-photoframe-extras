# Changelog

All notable changes to this project are documented here. See [docs/FEATURES.md](docs/FEATURES.md) for the full
list of optional features and [docs/FEATURE_FLAGS_PLAN.md](docs/FEATURE_FLAGS_PLAN.md) for how they're built and
verified; this file covers what changed and when.

This is a rebuild of [aitjcize/esp32-photoframe](https://github.com/aitjcize/esp32-photoframe) (starting from
`v2.18.0-27`) — not a GitHub fork, so that an earlier fork of the same project could stay as it is — carried
forward as a real fork of its own from here on. With no build option chosen, this firmware *is* the upstream
firmware.

## [Unreleased]

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
