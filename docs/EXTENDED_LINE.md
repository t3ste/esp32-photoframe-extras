# The extended line - maintainer's guide

How the **extended edition** ([EXTENDED_EDITION.md](EXTENDED_EDITION.md)) is kept next to the base project, written for whoever
continues the work (a person or another assistant instance) without the conversation that created it. Read
[MAINTAINING.md](MAINTAINING.md) first: its rules (no option = upstream 1:1, ask before outward-facing actions, privacy, formatting,
credentials) apply here unchanged. This page only adds what is specific to the extended line. State of 2026-09-30.

## 1. What it is and why it exists

The base project (`main`) is upstream's firmware plus 16 opt-in features. The extended line adds **14 more** opt-in features
(`webcal`, `multi-upload`, `source-auth`, `caldav`, `caldav-todo`, `upload-dedup`, `glyphs`, `info-screens`, `chore-wheel`,
`weather-screen`, `fact-of-the-day`, `finance-snapshot`, `fuel-prices`, `market-quotes`; bundle name **`extras`**) and one PC helper
(`scripts/fetch_art.py`). Decision of the maintainer (2026-09-30): these are **not merged into `main`** - the audience and target
hardware are small, the change is large, and only one board (Waveshare PhotoPainter 7.3") exists for testing. The line is published
next to `main` instead and kept in step with it.

It keeps every rule of the base: one build option per idea, no option = upstream byte-for-byte (proved by `alloff_source.py` and
`alloff_web.py`), releases are full builds. The 30 registry features and the bundle are checked by `scripts/test_features.py`.

## 2. Where everything lives

| What | Where |
| --- | --- |
| Base project (canonical) | `t3stier/esp32-photoframe-rebuild`, branch `main` - releases, update feed, web flasher of the **base** |
| Base project (mirror) | `t3ste/esp32-photoframe-rebuild` (Actions disabled) |
| **Extended repository** | `t3ste/esp32-photoframe-extras`, branch `main` = the extended line - **its own** releases, update feed and web flasher |
| Extended line, source only | branch `extras` in the canonical repository **and** in the mirror (no CI, no releases, no Pages) |
| Upstream (read-only) | `aitjcize/esp32-photoframe` |

Local checkout (`esp32-photoframe-fork`): the working branch is **`feature/ideas`** (the extended line). Remotes: `origin` = mirror,
`t3stier` = canonical, `extras` = the extended repository, `upstream` = aitjcize. The local branch `main` is the base.

Refspec map (what goes where):

```sh
git push t3stier feature/ideas:extras      # source branch next to main in the canonical fork
git push origin  feature/ideas:extras      # the same in the mirror
git push extras  feature/ideas:main        # the extended repository (its main)
```

Use `refs/heads/main` for the base in git commands - `main` alone is ambiguous because a directory `main/` exists.

**Why a repository of its own** (and not a second release line in the base repository): the frame's update check asks
`releases/latest` of the repository baked in at build time (`--ota-repo`), and `deploy-pages` force-pushes `gh-pages` on every
deploy. Two lines in one repository would offer each other's firmware to their frames and overwrite each other's web flasher.

### Accounts and credentials

As in MAINTAINING.md section 3: `gh` is logged in as `t3stier` and `t3ste`; the `t3ste` token lacks the `workflow` scope, so every
push that touches `.github/workflows/` must be made as `t3stier`. The extended repository belongs to `t3ste`; `t3stier` is a
collaborator with write access there. Push with the `gh` credential helper and the right account active:

```sh
gh auth switch --user t3stier
git -c credential.helper= -c "credential.helper=!gh auth git-credential" push <remote> <refspec>
```

Repository settings of the extended repository: Actions on; Pages from branch `gh-pages`, folder `/` (switch on after the first
successful `deploy-pages` has created the branch); **no** repository variable `OTA_REPO` (so the build bakes in the repository itself
as update feed - the base repositories set it to the canonical fork on purpose); no secrets; Issues on (hardware test reports).

## 3. Keeping it in step with `main`

Rules:

1. **The base is the source of truth.** A change that is useful for the base too (a fix in shared code, a doc correction) is made on
   `main` first, then merged here. Example: the Agenda timer fix exists as `3298659` on `main`; on this line it came earlier as
   `b95c483` and the comment text was then made identical (`21c8a7b`), so the merge has nothing to resolve in that hunk.
2. **Merge, do not cherry-pick**, in one direction: `git merge refs/heads/main` into `feature/ideas`. Nothing is merged from here to
   `main` except by a deliberate decision (single generic fixes go through `main`-first).
3. **Upstream goes into the base first** (MAINTAINING.md section 12), then into this line via the merge above. Note on 2026-09-30:
   `upstream/main` is **ahead** of the base (`495e0b6` -> `f5e3ec9`, six commits: core dump partition on every layout, the last crash kept
   in NVS and shown in the Maintenance tab, ELF attached to releases, a doc). They are not merged yet; the partition change needs care
   for OTA compatibility.

Procedure after the base changed:

```sh
git switch feature/ideas && git status                     # must be clean
git fetch --all --prune
git merge refs/heads/main                                   # resolve, see the hot spots below
# then the whole check set (section 5) - all must pass
git push extras feature/ideas:main                          # first: its CI is the real test
# after CI is green:
git push t3stier feature/ideas:extras && git push origin feature/ideas:extras
```

Conflict hot spots (both lines edit them): `CHANGELOG.md` (keep both entries in `[Unreleased]`), the feature tables in `README.md`
and `docs/FEATURES.md`, `main/Kconfig`, `main/feature_config.h` (the `FORK_ANY` list), `main/CMakeLists.txt`, `scripts/features.py`,
`scripts/migrate/xref.py` (`MODULES`), `host_tests/CMakeLists.txt` (`FORK_FEATURE_DEFS`), `Makefile` (the fork's test loop),
`.github/workflows/build.yml` (`feature_set`), `webapp/src/components/SettingsPanel.vue`, `webapp/src/stores/settings.js`,
`main/config_manager.{c,h}`, `main/config.h`, `main/http_server.c`, `main/utils.c`. The changes here sit in `#if FEATURE_X` blocks
and are additive, so a textual conflict usually means "both added lines at the same place: keep both".

## 4. Releases of the extended repository

- Releases are **full builds** of every board (`--all-features` = the base's 16 + these 14), as in the base. The workflow is the same
  file; the update feed is the repository itself.
- **Tags** `vMAJOR.MINOR.PATCH`, numeric, in the repository's own sequence. `version_compare()` in `main/ota_manager.c` understands
  only three numbers and an `-rc<n>` suffix - a fourth number (`v218.7.0.1`) or another suffix would not be seen as a new version. Start
  from the base version the release contains; a second release on the same base bumps the patch. Say "extended edition" and the base
  version in the release notes.
- A **first release for any board that has not been flashed** is a pre-release (`-rc1` in the tag, the workflow turns that into a
  GitHub pre-release; frames on the pre-release channel get it).
- Procedure: `git tag -a vX.Y.Z -m "..."` on the extended repository's `main`, push the tag there **as `t3stier`**; the workflow builds
  16 firmware files, publishes them and redeploys the web flasher. Nothing is released from the source branches.
- The web flasher site is `https://t3ste.github.io/esp32-photoframe-extras/` (base path derived from the repository name by
  `SITE_REPO` in `webapp/vite.config.demo.js`; the deploy step passes `github.repository`).

## 5. The check set (what "good" means)

Run before every push (all green on 2026-09-30):

| Check | Command | Expected now |
| --- | --- | --- |
| C/C++ format | `clang-format-18 --dry-run --Werror` on `main/`, `components/`, `host_tests/` | clean |
| Web format, tests, lint | `cd webapp && npx prettier --check src && npx vitest run && npx eslint src` | 74 tests |
| Python format, tooling tests | `python -m black --check scripts build.py`, `python -m isort --check-only scripts`, `python -m unittest discover scripts -p "test_*.py"` | 91 tests |
| Host tests (WSL/Linux) | cmake `host_tests/`, build, `ctest` (serial - a few dedup tests collide in parallel) | 681 tests |
| Proofs | `alloff_source.py` (170 files), `alloff_web.py` (31 files), `xref.py`, `check_capabilities.py` | 0 differences |
| Compile matrix | IDF shell: `python scripts/feature_matrix.py --board <b> extras all` for at least Waveshare, M5Paper and XIAO EE02 | OK |
| Firmware + flash | only with the maintainer's explicit OK, multi-part esptool command (MAINTAINING.md section 13) | |

Notes: the host tests fetch cJSON themselves when `managed_components/` is missing. After a feature matrix run,
`main/webapp` holds the last set's web bundle - regenerate it (`python build.py --board <b> --all-features --step webapp --step splash`,
outside the IDF shell) before a `--step firmware` build. A sporadic GCC internal compiler error in an IDF file is cured by running the
build again. `make` may not exist on Windows: run the parts by hand.

## 6. Where the knowledge is

- Per feature: `docs/<FEATURE>.md` (settings, limits, fixtures, how it was verified); for users
  [EXTRAS_USER_GUIDE.md](EXTRAS_USER_GUIDE.md); the option matrix in [FEATURES.md](FEATURES.md); the changelog.
- Fixtures of real service answers: `host_tests/data/{ecb,tankerkoenig,market,caldav,...}` with a README each (no secrets; invented data).
- Design notes per page: the information pages are pure drawing functions (`main/screen_*.c`, host-tested for five panel sizes, pictures
  via `host_tests/render_screens.cpp`); data fetching lives in `info_screens.c` / `market_service.c`; settings in `config_manager.c`,
  `http_server.c`, `utils.c`, `SettingsPanel.vue`, `settings.js` - follow the `fuel-prices` or `market-quotes` commit as the template for a new page
  ("Checklist for adding a build feature" in MAINTAINING.md section 5).

## 7. Open items (what has not been seen on real hardware)

Only the Waveshare PhotoPainter 7.3" was flashed. Not done, and how to check:

| Item | Check |
| --- | --- |
| Other boards (M5Paper first: plain ESP32, no PSRAM assumptions; flash-only XIAO: index files on internal flash; the two 16-grey and the big panels) | flash a full build, tick each page, upload a batch, run a duplicate index; look at colours/greys |
| `market-quotes` with personal Twelve Data / Alpha Vantage keys, Yahoo over days at a short schedule, a wake without network, the quota counter, the 12 KB wake-task stack with a page, the look on the e-paper | real keys, a day of the debug log, unplug the network once |
| `fuel-prices` with a personal Tankerkoenig key (only the public demo key was used) | real key, a real place in Germany; keep the schedule >= 5 minutes (the service asks for it) |
| `caldav` / `caldav-todo` against Nextcloud, Baikal, iCloud-like servers; long task lists; `source-auth` Digest/https on a real server | see the per-feature docs |
| Web UI in Safari/Firefox/Android | batch upload uses `CompressionStream`/`DecompressionStream` |
| Import of a config exported by an older build | export, import into a build without the new keys: unknown keys must be ignored |
| CI on the extended repository | the first runs may show timing/resource problems (8 boards x plain/full, feature-compile matrix) |

## 8. Ideas that were decided against or left for later

- **Not built:** tree/bird/dinosaur "of the day" packs (the content does not exist in the repository and cannot be generated by
  code; the picture part would need a JPEG-into-canvas path) and the newsstand (needs WebP; the ESP registry has no decoder; front pages
  are copyrighted, and the one free JPEG source found serves *progressive* JPEGs that the firmware's decoder - baseline only - cannot read).
- **Alternatives worked out** (none started): a tiny "daily content" feed that a scheduled job publishes (an image or a text per day,
  per board) for the frame's existing URL rotation mode - needs a date placeholder in the image URL (~0.5 day); "picture of the day =
  album image number (day mod count)"; the fact list fetched from a URL instead of uploaded; fact/bird packs generated by a PC script from
  Wikipedia summaries (CC BY-SA, attribution needed).
- **Symbol alias table for markets** (a JSON file mapping Yahoo symbols to the other sources' vocabulary): judged not stable enough - the
  pattern rules would need an interpreter, the proxies (futures -> spot, index -> ETF) change the level of a price, forex history must be
  `FX_DAILY` (not `CURRENCY_EXCHANGE_RATE`, which has no history), and Twelve Data's non-US `mic_code` could not be verified with the public demo
  key. Worth doing later, in stages: table-driven mapping equal to today's behaviour, Alpha Vantage crypto via `DIGITAL_CURRENCY_DAILY`
  (verified live) and currency pairs via `FX_DAILY` (verified live), then Twelve Data suffixes with a real key and a "plan not sufficient" status.
- **Markets input:** only the first four symbols are used and the rest is ignored silently - a hint next to the field would help.
- **Fuel prices:** a fallback provider is of little value (all providers carry the same MTS-K data). kraftstoffbilliger.de has a JSON API but
  gives keys on request only; TankPuls' documented API answered 404 on 2026-09-30 and the service is not on the Bundeskartellamt's list of
  approved providers. The MTS-K itself is only available to approved consumer-information services via the Mobilithek (XML containers, a
  server-side job). A better safeguard: keep the last good list on the storage and show it with the "Stand" note, and a 5-minute minimum between fetches.
- **Stale sentence** in `docs/CALDAV_TODO.md`: it says umlauts become ae/oe/ue; with `glyphs` (always present where `info-screens` is) they are real glyphs.

## 9. Pitfalls collected on the way

- A shell heredoc can turn `'\0'` into a real NUL byte and backslash escapes into characters when a patch script is passed through a
  tool - write patch scripts as files (an editor/`Write` tool) and run them.
- `git add -A` picks up new build directories; list them in `.git/info/exclude` first (`build-*`, `build-matrix`).
- `make` is not installed on the maintainer's Windows machine; the CI steps are run by hand (table in section 5).
- Flashing the merged image at offset 0 erases settings - use the part-wise command. A frame that is unreachable: check once and ask.
- The test frame's settings must be snapshotted and restored around live checks (schedule, ticked pages, language, keys); never leave demo
  keys or a demo place on it.
- `git log --format=%h main..feature/ideas` is ambiguous (directory `main/`); write `refs/heads/main`.
- Never echo a key; the firmware never logs keys or login URLs, tests check this.

## 10. Keeping this page true

Update section 2 when a repository or branch moves, section 5 when counts change, section 7 when something has been verified on hardware,
and section 3 after every upstream merge. The maintainer's private planning notes (`IDEAS.md`, git-ignored) may exist on his machine; nothing in
them is needed to continue - this page and the feature documents carry everything.
