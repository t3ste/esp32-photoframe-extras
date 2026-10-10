# Maintaining this fork - handover guide

Written 2026-09-28 for whoever continues this project next (a person, or an AI coding assistant that starts
with **no memory of earlier sessions**). It collects what was learned while building the fork: how the pieces
fit, the commands that work, the mistakes that were made once and must not be repeated. Read it top to bottom
once; afterwards use the table of contents as a lookup.

This guide is meant to be self-sufficient for day-to-day maintenance. What changed and when is in
[../CHANGELOG.md](../CHANGELOG.md). What the features do for a user is in [FEATURES.md](FEATURES.md).

1. [The ten golden rules](#1-the-ten-golden-rules)
2. [What this repository is](#2-what-this-repository-is)
3. [Repositories, accounts, remotes](#3-repositories-accounts-remotes)
4. [The git model](#4-the-git-model)
5. [How the feature system works](#5-how-the-feature-system-works)
6. [The equality invariant and its proofs](#6-the-equality-invariant-and-its-proofs)
7. [Working environment and commands](#7-working-environment-and-commands)
8. [Continuous integration](#8-continuous-integration)
9. [Releasing](#9-releasing)
10. [Web flasher and the Pages site](#10-web-flasher-and-the-pages-site)
11. [Over-the-air updates](#11-over-the-air-updates)
12. [Merging upstream changes](#12-merging-upstream-changes)
13. [Testing on a real device](#13-testing-on-a-real-device)
14. [Privacy and safety rules](#14-privacy-and-safety-rules)
15. [Pitfalls: symptom, cause, fix](#15-pitfalls-symptom-cause-fix)
16. [Open items](#16-open-items)
17. [File map](#17-file-map)
18. [Conventions](#18-conventions)

## 1. The ten golden rules

1. **No build option = upstream firmware, 1:1.** Kconfig symbols, ELF symbols, `.bin` size and the web bundle
   must be identical to `aitjcize/esp32-photoframe` at the baseline commit. This is the reason for the whole
   design and it is checked by scripts (section 6). Any edit to a file that also exists upstream must be fenced
   with a feature guard so that "everything off" restores upstream's exact text.
2. **Releases are the FULL build of every board** (all features the hardware supports, `--all-features`).
   Whoever wants the plain firmware gets it from upstream. The frame updates itself over OTA from this
   repository's releases.
3. **Never modify the reference material**: upstream (`aitjcize/esp32-photoframe`) and any local read-only
   companions (an older fork of the same project, an upstream snapshot) - only read-only git commands there.
4. **Ask before outward-facing or irreversible actions**: pushing, tagging, publishing a release, force-pushing,
   deleting, changing repository settings, flashing a device. Approval for one action does not carry over.
5. **Push to BOTH repositories** (canonical fork and mirror, section 3) and keep them identical.
6. **No private data in commits, docs, tests or fixtures** (section 14). Only invented data (`Alex`, `Sam`,
   `example.com`). Never echo a secret; check for presence or length only.
7. **Format before every commit** (clang-format **18**, black, isort, prettier); CI fails otherwise (section 7).
8. **Never re-run `scripts/migrate/gate.py apply` on a file that has manual edits** - it regenerates from
   scratch and destroys them (list in section 5).
9. **Do not poll or reset an unreachable device in a loop**: check once, then ask the maintainer to power-cycle
   or reconnect it. Never flash the merged image at offset 0 for tests (it erases WiFi and settings).
10. **Never pipe `scripts/verify_baseline.py` output** (`| tail`, `| Select-Object`): the nested build then fails
    with a misleading `exit status 2`. Run it unpiped, in the background, and read the captured output file.

## 2. What this repository is

- A rebuild of [aitjcize/esp32-photoframe](https://github.com/aitjcize/esp32-photoframe) (an ESP32 e-paper
  photoframe: ESP-IDF 6.0 firmware in C, Vue 3 + Vuetify web UI, a host-side `process-cli`). It started from upstream
  `v2.18.0-27` (commit `1347744414364110f96d9c121b4cc6e13b2364f2`) and is run as a real fork that keeps adopting
  upstream commits.
- It adds **16 opt-in features** that an earlier, always-on fork contained (Telegram bot, weather/headline
  overlays, Agenda with ToDo and calendars A-E, chimes, climate sensor, alarm clock, voice stop, battery history,
  no-repeat rotation, HTTPS, offline hotspot, error banner, OTA channel, WiFi resilience, face crop, general
  fixes). List and hardware requirements: [FEATURES.md](FEATURES.md). Each is chosen at build time
  (`python build.py --with a,b` / `--all-features`).
- 8 boards (`boards/boards.json`): `waveshare_photopainter_73` (the only one with speaker + microphone, and an
  SHTC3 climate sensor), `seeedstudio_xiao_ee02/ee03/ee04`, `seeedstudio_reterminal_e1002/e1003/e1004`,
  `m5stack_m5paper_v11` (ESP32, all others ESP32-S3).
- **Versions** are `v<upstream>.<minor>.<patch>`: upstream 2.18 becomes `218`, and that number only changes once
  everything of the new upstream version has been merged; the last two numbers count this fork's own releases
  (`v218.0.1`, `v218.0.2`, ... then `v219.0.0` after upstream 2.19 is merged). The firmware compares them
  numerically. Decided by the maintainer; do not change the scheme.
- What exists in the tree besides the firmware: the web UI (`webapp/`), the demo/flasher site build, the
  `process-cli` image tool, host unit tests (`host_tests/`), the feature tooling (`scripts/`), CI (`.github/`).

## 3. Repositories, accounts, remotes

| Repository | Role | Actions | Pages | Releases |
| --- | --- | --- | --- | --- |
| `t3stier/esp32-photoframe-rebuild` | **Canonical**, a real GitHub fork of upstream | on | on (branch `gh-pages`) | yes - the OTA feed |
| `t3ste/esp32-photoframe-rebuild` | **Mirror** (the maintainer insists it stays); the project started here | disabled | - | v218.0.0-v218.0.2 only |
| `aitjcize/esp32-photoframe` | Upstream, read-only | - | - | - |

Why two: GitHub allows one fork of a repository per account, and the maintainer's first account already used
its slot for the older fork, so the project began as a standalone repository there. A second account
(`t3stier`) made the real fork; the first repository was kept as a mirror.

Local remotes: `origin` = mirror, `t3stier` = canonical fork, `upstream` = aitjcize. **Every push goes to both**:

```sh
git push t3stier main            # and tags by name: git push t3stier vX.Y.Z
git push origin main             # and: git push origin vX.Y.Z
```

**Credentials.** `gh` is logged in with both accounts; `gh auth switch --user t3stier|t3ste` selects one and
`gh auth status` shows which is active. On Windows, git itself uses Git Credential Manager with ONE stored
github.com credential, which may belong to the wrong account for a given push (HTTP 403). Use the `gh`
credential helper explicitly, with the account that has rights active:

```sh
git -c credential.helper= -c "credential.helper=!gh auth git-credential" push <remote> main
```

Each account is a collaborator (write) of the other's repository. The `t3ste` token lacks the `workflow` scope,
so **a push that changes a file under `.github/workflows/` must be done as `t3stier`**.

Repository settings that matter (canonical fork): Actions enabled; Pages deploys from branch `gh-pages`,
folder `/`; repository variable `OTA_REPO` = `t3stier/esp32-photoframe-rebuild` (also set in the mirror, so a
build made there would still point frames at the fork). The mirror's Actions are disabled
(`PUT /repos/t3ste/esp32-photoframe-rebuild/actions/permissions` with `enabled=false`) - it only receives code
and tags. There are no repository secrets; workflows use the built-in `GITHUB_TOKEN`.

The fork also carries copies of upstream's other branches (`cdr`, `cdr2`, `dev`, ...) and upstream's own
`gh-pages`; harmless (our deploy force-pushes a single-commit `gh-pages`).

Companions that may exist next to the working copy on the maintainer's machine (not part of the repository,
read-only): the older fork `t3ste/Tlg-esp32-photoframe` (the always-on predecessor with the full old history; the
reference branch `fork-import` was imported from its tag `v218.7.0`) and a plain snapshot of upstream. Do not touch
them: no fetch, pull, checkout, commit, gc or new files there.

## 4. The git model

- `main` descends from the **real upstream commit** `1347744414364110f96d9c121b4cc6e13b2364f2` (`v2.18.0-27`).
  The repository originally began as a squashed import with no shared ancestry, so a `git replace --graft` plus
  `git filter-branch` made upstream's commit the actual parent. The result is that
  `git fetch upstream && git merge upstream/main` is a normal three-way merge. The graft is finished; do not redo
  it. How it was done (for understanding, and for a repeat in a similar situation): fetch the upstream commit and
  its ancestors; confirm the trees are identical (they differed only in the executable bit of 8 files, a byte-import
  artifact); on a scratch branch `git replace --graft <old-root> <upstream-sha>` and
  `git filter-branch -f --tree-filter '<fix exec bits with git update-index --chmod=+x>' -- <upstream-sha>..<branch>`
  (the `A..B` range keeps upstream's own commits untouched); verify the diff at the graft point is empty and the
  merge-base equals the upstream SHA; `git switch main && git reset --hard <scratch>`; delete the scratch branch, the
  replace ref (`git replace -d`) and filter-branch's `refs/original/`; `git reflog expire --expire=now --all &&
  git gc --prune=now`. The rewritten `main` was force-pushed once; that is why a force push is a classifier-sensitive
  action (section 14).
- Because history now reaches upstream's true root, `git rev-list --max-parents=0 HEAD` is **not** the baseline.
  The scripts use fixed SHAs: the **graft point** `1347744...` (never moves; `gate.py` uses it) and the
  **equality baseline**, the newest merged upstream commit (`186ebaf3b470305824d238c2d2dabf2c5bc59a7a` at the
  time of writing), stored in `scripts/verify_baseline.py` (`upstream_sha`) and `scripts/migrate/alloff_source.py`
  (`BASELINE`, imported by `alloff_web.py`). Move the baseline with each upstream merge (section 12).
- Branches: `main` (release line), `feature/<name>` for work (merged locally into `main` after the checks,
  `feature/upstream-<date>` for upstream merges). `fork-import` is the old fork's tag `v218.7.0` as a reference
  for acceptance test B only; it is never a base for `main`.
- Gotchas:
  - There is a directory `main/`, so the branch name `main` is ambiguous in some commands: write
    `git diff refs/heads/main fork-import --` (trailing `--`).
  - `core.autocrlf=false` and `.gitattributes` `eol=lf`: the baseline is byte-exact LF. LF/CRLF warnings on
    Windows are harmless; **do not** let a tool write CRLF into tracked files (Python `Path.write_text()` does;
    use `write_bytes` or `newline="\n"`).
  - `core.fileMode=false` (Windows). A plain `chmod +x` is invisible to git; use `git update-index --chmod=+x`.
  - **Never** run `git config core.filemode --show-scope`: it is accepted as a WRITE and corrupts `.git/config`
    (every git command then dies with `bad boolean config value`; repair by editing `.git/config` as text).
    The valid read is `git config --show-scope core.filemode`.
  - `git fetch upstream` also pulls all upstream **tags** into the local repository. Harmless: tags are only ever
    pushed by name.
  - `git commit -F -` with a PowerShell here-string does not work (the message becomes a pathspec). Write the
    message to a temporary file and use `git commit -F <file>` (same for `git merge -F`).
  - Never `--no-verify`; never amend published commits.

## 5. How the feature system works

- **Registry**: `scripts/features.py` (name, Kconfig symbol, dependencies, hardware needs, sdkconfig overlay).
  Hardware capabilities: `boards/capabilities.json` (speaker, microphone, climate sensor per board); a CI check
  (`scripts/check_capabilities.py`) keeps `boards.json`, `capabilities.json` and the Kconfig tables consistent.
- **Build**: `build.py --with ... | --all-features | --without x | --list-features | --ota-repo owner/name`.
  Each feature maps to `features/sdkconfig.defaults.<name>` and to Kconfig `FEATURE_*` (`main/Kconfig`); `FORK_FIXES`
  is the general-fixes flag; hidden helper symbols `FORK_*` are selected by features. `main/feature_config.h`
  turns them into 0/1 macros and `#error`s on impossible combinations. `build.py` records board + features in
  `build/.features` and **cleans `build/` automatically** when they change. `--ota-repo` writes
  `build.ota-repo.defaults` next to the build directory (gitignored).
- **C code**: shared upstream files carry `#if FEATURE_X ... #else <exact upstream text> #endif`. Modules of a
  disabled feature are dropped from `main/CMakeLists.txt` SOURCES and their headers provide `static inline` or
  macro no-op stubs, so callers stay `#if`-free. `config.h` and `config_manager.*` are guarded as a whole with
  `FORK_ANY` (any feature or `FORK_FIXES` on). NVS keys must be <= 15 characters. `PRIV_REQUIRES` cannot depend on
  `CONFIG_*`; use `idf_component_optional_requires(PRIVATE x)` after `idf_component_register`.
- **Web UI**: `webapp/feature-directives.js` is a Vite `pre` plugin that removes fenced parts before Vue compiles
  a file. Directive forms: `// #if FEATURE_X && FORK_EXIF` (script), `<!-- #if ... -->` (template),
  `/* #if ... */` (style), with `#else`/`#endif`. `build.py` passes the enabled features as `VITE_FEATURES`
  (comma separated); `VITE_OUT_DIR` redirects the output for scratch builds. Files containing `#else`
  alternatives cannot be parsed by Prettier/ESLint and are listed in `webapp/.prettierignore` and
  `webapp/eslint.config.js` - **format those by hand**.
- **`FORK_SITE`** is a pseudo-flag that only `webapp/vite.config.demo.js` switches on (`featureDirectives("site")`).
  Everything that belongs to the project's demo/flasher site but sits in a file that is also part of the device
  bundle (`views/LandingPage.vue`) is fenced `#if FORK_SITE ... #else <upstream> #endif`. Getting this wrong
  once silently broke bundle equality (section 15).
- **`scripts/migrate/gate.py`** generated the guards from the old-fork diff (`analyze|show|apply FILE --map
  scripts/migrate/maps/<f>.map`). It is a one-off migration tool now. **Files with manual edits (never re-`apply`):**
  `main/http_server.c` (init tail, config/urls handler, includes), `main/display_manager.c/.h` (face-crop stubs,
  random-pick block), `main/ota_manager.c` (`api_url`), `main/main.c` (button task, alarm/agenda wake, WiFi
  bring-up, rotate-button wake, offline/`wifi_skipped`), `main/config.h` (OTA repo URL),
  all stubified headers (`chime`, `agenda_manager`, `alarm_manager`, `alarm_setting_ui`, `history_manager`,
  `battery_history`, `climate_history`, `overlay_manager`), `webapp/src/components/SettingsPanel.vue`.
- **Adding a feature or fix**: create `feature/<name>`; guard every change to a shared file with a flag and put the
  exact upstream text in the `#else`; give a new module a stub header; register it in `scripts/features.py`,
  `main/Kconfig`, `main/feature_config.h`, `features/sdkconfig.defaults.<name>`, `docs/FEATURES.md`; gate its web
  UI in the same step; then run every check of section 6. Small fixes to upstream behaviour belong under
  `FORK_FIXES` (one small documented hunk each, so they can be offered upstream).
- The HTTP server (`esp_http_server`) serves requests in ONE task: a handler must not do many syscalls per
  element (this is why the climate history answer is bounded, see `main/history_decimate.h`). All sockets share
  one lwIP pool; `httpd max_uri_handlers` must stay above the number of registered handlers.

## 6. The equality invariant and its proofs

"No flags == upstream" is verified at several levels. **Each proof sees something the others cannot**; know
which to run after which change.

| Check | Command | Proves | Blind spot |
| --- | --- | --- | --- |
| Feature tooling unit tests | `python -m unittest discover -s scripts -p "test_*.py"` | registry, `generate_manifests.py`, `verify_baseline.py` helpers | - |
| Capability tables | `python scripts/check_capabilities.py` | boards/capabilities/Kconfig agree | - |
| Cross-reference | `python scripts/migrate/xref.py [set ...]` | a symbol defined only in a guarded-out region, or in a header included conditionally (seconds instead of a build) | not a compile |
| All-off C sources | `python scripts/migrate/alloff_source.py` | with every guard resolved "off", each C/H file equals the upstream text at `BASELINE` (175 files; intended differences listed in the script) | build wiring, web |
| All-off web sources | `python scripts/migrate/alloff_web.py` | same for the 33 web sources that reach the device bundle (runs the real directive plugin through node; only `webapp/index-demo.html` may differ) | build output |
| Names in the web sources | `python scripts/migrate/web_bindings.py` | for every feature set (each alone, the full build, the full build without each, some combinations) every name a template uses is defined in its script, and every name a script uses is declared in it (ESLint `no-undef` on the resolved script; what upstream's own text leaves undefined is not counted) - a fenced-out script part under a template part or a function that stays builds without a word and fails in the browser | needs node and `npm ci --ignore-scripts` in `webapp/`; run by CI (`feature-tooling`) |
| Binary acceptance A | `python scripts/verify_baseline.py --board <b>` (IDF shell) | Kconfig symbols, ELF `nm`, `.bin` size equal to an upstream build; `--all-features` variants compare against the old fork | **feeds both builds the same prebuilt web assets - cannot see web differences** |
| Web bundle byte compare | manual, below | the all-off web bundle is byte-identical to a build of upstream's own webapp | - |
| Compile matrix | `python scripts/feature_matrix.py --board <b> off fixes single ...` (IDF shell) | every flag alone, none, all compile (`--full` also links) | runtime |
| Host tests | section 7 | 314 tests: upstream tests on the all-off code + module tests + "(fork)" image variants | hardware |

After **any** edit to a shared file: `alloff_source.py`, `alloff_web.py`, `xref.py`, host tests (and `web_bindings.py` after a web change), plus a real build
of the board you touched. After an **upstream merge** or a **web change**: also the manual bundle compare and
`verify_baseline.py` for one board with the hardware (waveshare) and one without (`seeedstudio_xiao_ee02`).

Manual web bundle compare: `git worktree add --detach <tmp> <BASELINE sha>`, junction/symlink
`webapp/node_modules` into it, `npx vite build` there (writes `main/webapp/`), then in this repo
`VITE_FEATURES="" VITE_OUT_DIR=<dir> npx vite build`; compare the SHA-256 of every file (16 files including `.gz`).

`verify_baseline.py` needs `main/webapp` and `main/splash_data` (both gitignored) generated first, OUTSIDE the IDF
shell: `python build.py --board <b> --step webapp --step splash` (the IDF Python lacks `qrcode`; `rsvg-convert` is
needed). Scratch trees go to `.verify/` (gitignored), the report to `.verify/report-<board>.txt`. It auto-pins the
candidate's OTA repo to upstream's own hardcoded value so the comparison stays apples-to-apples (the shipped
default differs, a single build-time string, listed as an intended exception in `alloff_source.py`).

## 7. Working environment and commands

The maintainer works on Windows 11; PowerShell is the primary shell, Git Bash is available, WSL Ubuntu is used
for the host tests and clang-format. Equivalent Linux/macOS commands work the same.

**ESP-IDF v6.0** (mbedTLS 4.x / PSA - classic mbedTLS tutorials often do not fit, check the installed headers).
**The local install and the CI are not the same IDF**: the local one is a checkout of `release-v6.0` from 2026-01 (6.0.0), the CI's `espressif/idf:release-v6.0` image follows the branch (the firmware of
v219.0.0 reports `v6.0.3-489-g71722673c52`). Code that depends on IDF internals can pass every local test and break only in the released binary - see the pitfall about `tls_ca_cb_fix.c` below. The IDF
version a binary was built with is in its first 4 KiB (`esp_app_desc`) and in `GET /api/system-info` (`idf_version`); before a release, flash the **CI build** of the commit to a test frame and run what the
change touches (for anything TLS: the update check, `POST /api/ota/check`).
On Windows the environment does not persist between shell invocations; activate it in each call:

```powershell
& "C:\Espressif\tools\Microsoft.release-v6.0.PowerShell_profile.ps1" | Out-Null; <command>
```

**Build**

```sh
python build.py --board waveshare_photopainter_73 --all-features --step webapp --step firmware
python build.py --board <b> --list-features            # what the board supports
```

- After changing Kconfig or any `sdkconfig.defaults*`: `--fullclean` is mandatory (otherwise the old `sdkconfig`
  silently stays; the version string is also only refreshed at configure time).
- Changes under `webapp/` need `--step webapp`. `build/` belongs to one board/feature combination (auto-cleaned
  when they change).
- A compiler "Segmentation fault" inside a foreign IDF file: retry once.
- Only building another board exercises its `#else` stub branches - both test devices are waveshare boards.

**Format** (CI runs `make format-check`; `make` is not installed on Windows, replicate by hand)

- C/C++: **clang-format 18** (`pip install clang-format==18.1.8`, or `clang-format-18` in WSL) over `main`,
  `components`, `host_tests`: `-i`, then `--dry-run --Werror`. GitHub's Ubuntu 24.04 `clang-format-18` can differ
  from 18.1.8 on edge cases; where it does, wrap the lines in `// clang-format off` / `// clang-format on` instead of
  guessing.
- Python: `python -m black` and `python -m isort` on `main/*.py scripts/**/*.py` (isort prints a harmless charmap
  warning).
- Web: `npm run format` / `npm run format:check` in `webapp` and, separately (own Prettier config), in `process-cli`.
- `make format-check` stops at the first failing target; after a red CI run replicate **all** targets locally.
- Optional: `make install-hooks` runs the check as a git pre-commit hook.

**Host tests** (`host_tests/`, GoogleTest): the POSIX-only tests (`localtime_r`, `setenv`, `mkdir`) do not build with
a Windows mingw gcc - use WSL (Ubuntu with cmake, g++, libpng-dev):

```sh
cmake -S <repo>/host_tests -B ~/pf-host-build -DCMAKE_BUILD_TYPE=Debug \
      -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<local googletest source copy> -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build ~/pf-host-build -j16
ctest --test-dir ~/pf-host-build            # SERIAL: tests of one binary share a working directory
```

Keep the build directory outside the repository. `-j` with ctest makes the DisplayFlow tests flake. Result at the
time of writing: 339/339.

**Tooling traps on Windows**: `json.dumps` reformats files (edit JSON as text); backslash escapes and odd numbers
of quotes/backticks in shell-tool heredocs are unreliable (write patch scripts to a file and run them); `which`
may dump a huge PATH; `H` is a PowerShell alias (do not name a function `H`); pipe `git archive | tar` in Bash,
not PowerShell.

## 8. Continuous integration

- `ci.yml` (push/PR to `main`/`dev`, also callable): **format-check** (Node 18, Python 3.11, clang-format-18),
  **host-tests** (`make test`), **feature-tooling** (unit tests, `check_capabilities`, `xref`, `alloff_source`,
  `alloff_web`, `web_bindings` after an `npm ci --ignore-scripts` of `webapp/`; needs `fetch-depth: 0` because the baseline is a commit in history). About 2 minutes.
- `build.yml` "Build Firmware" (push to `main`, tags `v*`, PRs, manual `workflow_dispatch`, and the `release:
  published` event): calls `ci`, then **16 builds** (8 boards x `plain`/`full`), ~16 **feature-compile** jobs,
  the **release** job and **deploy-pages**. About 13 minutes.
  - `full` = `--all-features` and owns the **standard file names**: `esp32-photoframe-<board>.bin` (what OTA
    installs) and `photoframe-firmware-<board>-merged.bin`; artifact `photoframe-firmware-<board>`. `plain` =
    upstream-equivalent, compile proof only: `-plain` file names, artifact `plain-photoframe-firmware-<board>`.
  - The build bakes the update feed: `--ota-repo ${{ vars.OTA_REPO || github.repository }}`. The bootloader
    offset differs per chip (0x0 ESP32-S3, 0x1000 ESP32) - encoded in the matrix.
  - **release** (`push` or `workflow_dispatch` on a ref `refs/tags/v*`): creates a **draft** release with the
    full builds of every board.
  - **deploy-pages** (ref `main` or a tag): builds the demo site (`npm run build:demo`), copies static assets from
    `.img/`, cuts the manifests with `scripts/generate_manifests.py`, and force-pushes a single-commit `gh-pages`.
- **Skipping CI**: put `[skip ci]` in the commit message of docs-only or tooling-only commits (a full run is 13
  minutes of a slow runner; the repository is public so minutes are free, but queues are slow). The commit that
  gets a **release tag must not contain `[skip ci]`** - GitHub then skips the tag's workflows and no release is built.
- Confirm a run with `gh run view <id> --json status,conclusion` (and `--json jobs` grouped by conclusion).
  Do **not** trust the exit code of `gh run watch --exit-status` (it printed 1 for a green run). Download with
  `gh run download <id> -n <artifact> -D <dir>`. A binary's baked-in OTA target can be checked by grepping it for
  `api.github.com/repos/<owner>/<repo>`.
- **Flaky**: a build job once failed with `docker: 502 Bad Gateway` pulling `espressif/idf:release-v6.0` from Docker
  Hub - `gh run rerun <id> --failed`. (An optional retry step around the pull is an open idea.)
- **Fork quirk**: in the fresh fork a `push` started **no** workflow run (branch and tags, even after re-pushing a
  tag); only manual and release-event runs worked. The first push that changed a workflow file (`9cfb2b8`) made branch
  pushes start runs. Whether a **tag** push now starts the build is unconfirmed. After pushing a release tag check
  `gh run list -R t3stier/esp32-photoframe-rebuild`; if nothing started run
  `gh workflow run build.yml -R t3stier/esp32-photoframe-rebuild --ref vX.Y.Z` (the release job accepts that).
  Never let both a push run and a manual run build the same tag.
- The release feed answer must stay **below 64 KB** (the firmware's buffer): 16 assets are about 33 KB. Do not
  attach many more or long-named assets to a release.
- GitHub's runners: `actions/checkout@v5`, Node 18 in `ci.yml`. Keep the Node version in mind when web
  dependencies are updated.

## 9. Releasing

Only when the maintainer asks:

1. Move `[Unreleased]` in `CHANGELOG.md` into `## [vX.Y.Z] - <date>`; commit on `main` (no `[skip ci]`).
2. `git tag -a vX.Y.Z -m "..."`; push `main` and the tag to the fork and the mirror (section 3; workflow-file
   changes only as `t3stier`). Check that the fork started the build (section 8).
3. The `release` job creates a draft with 16 assets (full builds). Edit the notes
   (`gh release edit vX.Y.Z --notes-file <file>`), then publish (`gh release edit vX.Y.Z --draft=false`);
   publishing runs the workflow once more, which refreshes the Pages site's "stable" entry.
4. Verify: the release has the assets, `manifest.json` on Pages lists the new version with 4 parts per board,
   and a frame's update check finds it.

Release-notes rules: the **web flasher keeps settings** (part-wise flashing, section 10), but writing
`photoframe-firmware-<board>-merged.bin` with `esptool` at offset 0 **erases WiFi and settings**; the frame's own
OTA update keeps them. Never claim otherwise.

History:

| Tag | Date | What it is |
| --- | --- | --- |
| `v218.0.0` | 2026-09-28 | First release; its assets are the PLAIN build (plan before the "releases = full" decision). Kept, notes say superseded. |
| `v218.0.1` | 2026-09-28 | First **full** release; part-wise web flasher, climate-history fix, OTA fixes. Published in the mirror (then the operational repo). |
| `v218.0.2` | 2026-09-28 | Transition release: update feed, web flasher and links move to the fork; OTA stale-state fix. Published in BOTH repositories (built once in the mirror with the fork's feed baked in via `OTA_REPO`, the 16 assets re-uploaded to the fork's release). From now on only the fork needs releases. |
| `v218.0.3` | 2026-09-29 | Second upstream merge, the landing-page `FORK_SITE` fix, Calendar color-profile export, the Calendar-header-active-name fix. Confirmed live: a tag push alone starts the fork's CI (section 16's long-standing open item is closed for good). |
| `v219.0.0` | 2026-10-05 | First release on upstream v2.19.0 (WiFi policy, crash reports, demo package, the two `wifi-resilience` settings removed). **Withdrawn the same day** (set back to a draft): the full builds crashed on their HTTPS requests - `tls_ca_cb_fix.c` freed pointers the CI's newer ESP-IDF no longer allocates (section 15). The tag stays. |
| `v219.0.1` | 2026-10-05 | `v219.0.0` with that fix. Before it: the CI build of the fix commit was flashed to a test frame and the update check run. |

Pre-releases (the `ota-channel` feature's channel) use the tag suffix `-rc1` (e.g. `v218.0.4-rc1`); the `release`
job marks a `-rc` tag's release as a pre-release, and it must be **published** (not left as a draft) before a frame
on the pre-release channel can see it. A frame compares versions on `major.minor.patch` plus, under `fixes`, the
`-rc<n>` suffix (`-rcN` sorts before the same version without one, so the final release is still offered to a frame
that installed its release candidate; frames on builds without that comparison treat `-rc1` and the final as equal).
The Pages site keeps offering the newest published pre-release until a newer stable release exists (section 10).

## 10. Web flasher and the Pages site

- Site: `https://t3stier.github.io/esp32-photoframe-rebuild/` (Vite base path `/esp32-photoframe-rebuild/` in
  `webapp/vite.config.demo.js`; `index-demo.html` is the entry). It has the image-processing demo and an ESP Web
  Tools flasher. Sources: `webapp/src/views/LandingPage.vue`, `scripts/generate_manifests.py`,
  `scripts/launch_demo.py` (local preview; derives the repo from the git remote).
- **Why parts, not the merged image.** `photoframe-firmware-<board>-merged.bin` is one contiguous image from flash
  offset 0 with 0xFF between the pieces, **including the NVS partition (0x9000-0xF000: WiFi credentials and every
  setting)**. Writing it wiped them (found on a real device). `generate_manifests.py` reads the image's own
  partition table and cuts bootloader / partition table / OTA data / app parts (`*-boot.bin`, `-partitions.bin`,
  `-otadata.bin`, `-app.bin`); the manifests list those at their offsets and NVS is covered by none.
  `new_install_prompt_erase: true` offers the erase. The app part equals the OTA binary; the OTA-data part is the
  erased state (boot the first app slot - necessary when a device was OTA-updated into the second slot).
  Tests: `scripts/test_generate_manifests.py` (synthetic S3 and ESP32 images). `--keep-merged` keeps the merged file.
- Manifest names: `manifest.json` (stable, latest release), `manifest-dev.json` (build of `main`),
  `manifest-prerelease.json` (newest published pre-release, only while it is newer than the stable one); per board
  in `<board>/`. The stable entry follows the published release (that is why publishing re-runs the workflow).
  Every deploy rebuilds `demo/` from scratch and force-pushes it, so `deploy-pages` **restores** both entries
  from the published releases on every run (`gh release download`, the pre-release one via a scratch `.pre`
  directory because the asset keeps the stable file's name); only a run for a pre-release tag stages its own build.
- Pre-release detection: the release's own `isPrerelease` flag decides, but a **draft** is invisible to that lookup
  and the `release` job creating it runs in parallel with `deploy-pages` - so a tag-push run falls back to the
  `-rc` tag convention, and the run the "published" event triggers later sees the real flag and corrects the site.
- The demo package: `deploy-pages` copies `examples/` to `<site>/examples/` and, as its last step, checks that every
  URL the `examples/*/demo-config-*.json` files point at on this site answers `200` (retrying for 5 minutes; a
  throw-away query string bypasses a CDN-cached 404). `scripts/test_example_config.py` checks the same URLs resolve
  to files in the tree. GitHub's CDN may serve a cached 404 for a new URL for up to ten minutes.
- `deploy-pages` runs only for `main` and tags; before a release the site shows the dev build.
- Landing-page edits: everything that is fork-specific must sit in `#if FORK_SITE` fences (section 5).

## 11. Over-the-air updates

- The firmware asks `https://api.github.com/repos/<CONFIG_FORK_OTA_REPO>/releases/latest` (default from
  `main/Kconfig` `FORK_OTA_REPO` = the fork; overridable with `build.py --ota-repo`, CI uses `OTA_REPO`) and looks for
  the asset `esp32-photoframe-<board>.bin`. `version_compare` in `main/ota_manager.c` is numeric on
  `major.minor.patch`.
- Frames that ran v218.0.0/v218.0.1 asked the mirror; that is why v218.0.2 was published there as well.
- Upstream's own update check cannot parse GitHub's **chunked** API answers ("Invalid content length"): the fix
  lives under `FORK_FIXES`, so the plain build could never check for updates - another reason releases are full.
- The saved "update available" state used to survive installing that very release (stale banner). Under
  `FORK_FIXES`, `ota_manager_init` resets it when the offered version is no longer newer. Verified on a device.
- `ota-channel` is only stable vs pre-release. The old fork's "Alarm Clock firmware" variant switch does not exist
  here (an `-alarmclock` asset never existed in this fork's releases).
- A build with only some features that installs an update becomes the full firmware; keep the automatic check off
  for such builds (`build.py` prints a note).

## 12. Merging upstream changes

Do it when the maintainer asks, never on your own initiative. Experience from the three merges so far
(`151e716`, `7ccabe0`, `495e0b6`, `f5e3ec9`, and now `186ebaf` = v2.19.0):

1. `git fetch upstream`; `git log refs/heads/main..upstream/main --oneline`; branch `feature/upstream-<date>`;
   `git merge upstream/main` (message file, `[skip ci]` optional for the merge commit).
2. Small upstream commits usually merge cleanly. Conflicts, if any, sit inside the `#else` branch of a guard
   (take upstream's new code) or in a "manually edited" file (section 5; resolve by hand, keep the fences). The
   third merge (`495e0b6`) conflicted twice in `main/main.c`, where an upstream change rewrote a function this
   fork had added a `FORK_FIXES` line to (take upstream's structure, re-add the guarded fork line) and removed
   calls next to a fork-only `FEATURE_CLIMATE` line (drop the calls, keep the fork line).
3. Re-run the whole section 6 set, **including the manual web bundle byte compare** - the second merge is what
   revealed that the web bundle equality had broken.
4. Move the baselines (`upstream_sha` in `verify_baseline.py`, `BASELINE` in `alloff_source.py`), add a
   `CHANGELOG.md` entry, merge into `main`, push to both repositories (with the maintainer's OK).
5. New upstream **release** (e.g. 2.19): after everything of it is merged, the next release becomes `v219.0.0`.

## 13. Testing on a real device

- The maintainer has two `waveshare_photopainter_73` boards attached over USB (serial port + LAN). Ask which port and
  address to use; do not write addresses into files. mDNS names do not resolve in the Bash tool.
- **Flash only with the maintainer's permission**, always the multi-part command, which leaves the NVS partition
  alone (WiFi and settings survive):

  ```powershell
  python -m esptool --chip esp32s3 -p <PORT> -b 460800 --before default-reset --after hard-reset write-flash `
    --flash-mode dio --flash-size 16MB --flash-freq 80m `
    0x0 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin `
    0xf000 build\ota_data_initial.bin 0x20000 build\esp32-photoframe.bin
  ```

  `0xf000 ota_data_initial.bin` is mandatory (otherwise a previously OTA-updated device silently boots the old
  slot). **Never** flash `...-merged.bin` at `0x0` for tests: it erases settings.
- If the device does not answer: check **once** (HTTP, then serial), then stop and ask the maintainer to press
  RESET/BOOT or replug USB or check the SD card. No retry loops, no ping sweeps, no serial resets of your own.
- Debug log: `GET /api/debug/log` (persists across boots including crash reboots; covers the current and previous
  boot - use the **last** `Firmware:` line). Its content may contain WiFi SSID and location strings: **act on it,
  never quote it** into replies, files or commits.
- Useful endpoints: `GET /api/config`, `PATCH /api/config`, `GET /api/climate-history`, `POST /api/rotate`,
  OTA endpoints; documented in [API.md](API.md). What a configuration file for the web UI's import may and may not
  contain: [DEMO_PACKAGE.md](DEMO_PACKAGE.md) and `main/utils.c`'s `apply_config_from_json()`.
- **Web UI changes without flashing**: serve the sources with Vite's dev server and proxy `/api` to the frame - a few lines with the
  JS API (`createServer({ root, configFile, server: { proxy: { '/api': { target: 'http://<frame>', changeOrigin: true } } } })`) in a
  helper outside the repository, with `VITE_FEATURES` set to the options of the build (an empty string is a build with none) - and
  drive the page with Playwright and the local Chrome. `page.route()` answers or alters requests on their way (a `GET /api/config`
  with other values, an endpoint with a scripted answer), so nothing of it has to exist on the frame; do not press *Save*. The dev
  server prints Vue's warnings for names a template uses but the script lacks, which a build does not. The frame itself serves the
  web UI of the last firmware it was flashed with.
- OTA end-to-end test recipe: flash a dev build that is older than the latest release, open Updates, check, install,
  verify the frame reboots into the release with settings kept and state `idle`.
- Measure before you guess: the climate-history slowness was first blamed on "50,000 readings" and the device had
  747; timing the old and new build on the same device found the real cause.

## 14. Privacy and safety rules

- Nothing personal in commits, docs, test data or fixtures: no real names, WiFi SSIDs or passwords, Telegram bot
  tokens or chat IDs, calendar URLs (secret iCal addresses), LAN IP addresses, MAC addresses, locations, EXIF
  data, or local user-profile paths. Use invented data. If a debugging session produced such data, it must not
  leak into a commit message, a doc or a test.
- Never print secret values while verifying a live configuration; report only that a key is present or its length.
  `GET /api/config` returns some credentials in plain text (`access_token`, `http_header_value`,
  `openai_api_key`, `google_api_key`); `wifi_password`, the device password, the Telegram bot token/chat ID and the
  agenda/ToDo URLs are write-only.
- The web UI shows what the frame stores (calendar names, addresses and the like). A screenshot of a real frame's settings, or a
  printed `GET /api/config`, is personal data: replace the values in the request on its way (`page.route`) before capturing,
  report only whether a setting is set, and delete captures that show real data.
- Outward-facing and irreversible actions need the maintainer's confirmation each time (section 1, rule 4).
- **Claude Code specifics**: its permission classifier has denied real flashes ("Real-World Transactions"), force
  pushes ("Git Destructive") and edits of its own permission settings ("Self-Modification"). When a call is denied,
  explain, and let the maintainer grant a permission rule or run it; do not reword the command to slip past it.
  A push piped through `| tail` was denied for that reason while the bare command worked.
- The maintainer communicates in German; keep code, comments, commit messages and documentation in English.
- Commit messages: imperative, explain the why; last line `Co-Authored-By: <assistant> <noreply@anthropic.com>`
  when an assistant wrote the change.

## 15. Pitfalls: symptom, cause, fix

| Symptom | Cause | Fix |
| --- | --- | --- |
| HTTPS requests crash the frame (`assert failed: heap_caps_free ... free() target pointer is outside heap areas`, task of the request, frame `adopt` in `tls_ca_cb_fix.c`); the Web UI shows "Failed to fetch" / `ERR_CONNECTION_RESET` after a long wait | `tls_ca_cb_fix.c` (agenda builds) freed the buffers of the certificate the bundle callback returns. ESP-IDF 6.0.0 allocates them (that was the leak); the CI's newer IDF (`v6.0.3-489`) points `subject_raw` into the flash bundle and the name entries into the peer certificate, so they are not ours to free. It passed all local tests (local IDF 6.0.0) and crashed in every CI build from v218.7.1-rc3 to v219.0.0-rc1 | The wrapper only packs a certificate whose `subject_raw` is in RAM and whose name entries are not the child's own (`owns_its_buffers()`); decode a crash record with `xtensa-esp32s3-elf-addr2line -e <board>-<version>.elf <backtrace>` (the ELF is a release asset). Always test the CI build of a commit on a frame before releasing |
| Web flasher install lost WiFi and all settings | Merged image covers NVS with 0xFF | Manifests list parts (`generate_manifests.py`); test with the multi-part esptool command |
| "No flags" web bundle differs from upstream although `verify_baseline.py` passes | `LandingPage.vue` is in the device bundle; fork changes were ungated; `verify_baseline.py` gives both builds the same prebuilt web assets | Fence with `#if FORK_SITE`; `alloff_web.py` (in CI) and the manual byte compare |
| `verify_baseline.py` fails with `exit status 2` | Its output was piped; the nested build's output fills the pipe | Run unpiped/in background, read the output file |
| `git` commands all fail: `bad boolean config value` | `git config core.filemode --show-scope` wrote a value | Edit `.git/config` as text; use `git config --show-scope <key>` |
| Executable bit not recorded | `core.fileMode=false` | `git update-index --chmod=+x <path>` |
| Files got CRLF | Python `write_text` on Windows | `write_bytes` / `newline="\n"`; `.gitattributes` has `eol=lf` |
| `git diff main fork-import` errors | `main` is also a directory | `refs/heads/main` and a trailing `--` |
| CI format job red, local clean | GitHub's clang-format-18 differs from 18.1.8; `make format-check` stops at the first failing target | Replicate all targets; `// clang-format off/on` around the line |
| Old `sdkconfig` survives a Kconfig change | `build/` reused | `--fullclean` |
| Update check says "Invalid content length" | GitHub answers chunked; upstream's parser cannot | `FORK_FIXES` build |
| The free internal heap of an always-on frame falls by a few hundred bytes with every HTTPS request (never with plain HTTP) | The `agenda` option's cross-signed bundle verification: ESP-IDF's callback hands mbedtls a certificate whose name buffers `mbedtls_x509_crt_free()` does not free (seen on IDF 6.0) | `main/tls_ca_cb_fix.c` (a `--wrap` of `mbedtls_ssl_conf_ca_cb`). To look for a leak like it: build with `CONFIG_HEAP_TRACING_STANDALONE=y`, run `heap_trace_start(HEAP_TRACE_LEAKS)` around one request, dump the surviving records with `heap_trace_get()` and resolve the addresses with `xtensa-esp32s3-elf-addr2line -pfiaC -e build/esp32-photoframe.elf`; lwIP's TIME_WAIT blocks (a TCP pcb of 208 bytes, FIN segments, timers) are free again after about two minutes, so judge a leak by the level *before* each request over more than ten requests |
| OTA says "update available" for the version just installed | Saved state restored unchecked at boot | Fixed under `FORK_FIXES` in `ota_manager_init` |
| A frame never finds its OTA asset | Wrong asset name / feed repo | Asset is `esp32-photoframe-<board>.bin`; feed = `CONFIG_FORK_OTA_REPO` |
| Frames stopped seeing releases | Feed JSON > 64 KB | Fewer or shorter assets |
| Push to the fork starts no CI run | Fresh-fork quirk | See section 8 (workflow-file push; manual run fallback) |
| Push to the mirror: HTTP 403 or "refusing to allow ... without `workflow` scope" | Git Credential Manager uses the other account; `t3ste` token lacks `workflow` | `gh` credential helper with the right account; workflow-file changes as `t3stier` |
| Tag pushed but no release appeared | Tagged commit contained `[skip ci]`, or fork quirk | Section 8/9 |
| `docker: 502 Bad Gateway` in a build job | Docker Hub flake | `gh run rerun <id> --failed` |
| "Build webapp" fails in `npm ci` at `canvas`: `prebuild-install warn install Request timed out`, then a `node-gyp` error | Network flake fetching the prebuilt binary, the source-build fallback lacks the cairo headers (seen once, 1 of 16 jobs) | `gh run rerun <id> --failed` |
| `gh run watch --exit-status` returns 1 for a green run | Unreliable | `gh run view <id> --json status,conclusion` |
| Climate History shows only a spinner | Handler built up to ~50k readings as pretty JSON | Bounded to 1000 points (`history_decimate.h`), compact JSON |
| Browser console "Password field is not contained in a form" | Vuetify password fields with a placeholder | Harmless, left as is |
| A switch, list or button of the Settings page does nothing in a build with only some options (no build error) | The template part and the script part it uses are fenced by different options; Vite builds it, Vue only warns when the page is drawn; `SettingsPanel.vue` is in `.prettierignore` and the ESLint ignores, so no formatter or linter looks at it - the same holds for a function that uses a name another option declares (`exportIncludeSecrets`: the Settings export failed with a ReferenceError in a build with `agenda` but not `fixes`) | `python scripts/migrate/web_bindings.py` (template names and script names); put the script part under the same option as the template part, or fence the declaration with every option that uses it (`// #if A \|\| B`) |
| Hand-edited file lost its edits | `gate.py apply` re-run | Restore from git; never re-apply on manually edited files |
| `PRIV_REQUIRES` ignores a Kconfig condition | Early requirement expansion | `idf_component_optional_requires(PRIVATE x)` |
| Handler registration fails at boot (`HANDLERS_FULL`) | `max_uri_handlers` too small | Raise the base + per-feature count |
| NVS write fails silently | Key longer than 15 characters | Shorten (CI rejects it) |
| Time off by an hour after DST | `mktime()` with `tm_isdst` unset | Set `tm_isdst = -1` |
| Wrong weather location after config import | An old geocoded latitude/longitude counts as a manual one and wins over the name | Set `weather_lat`, `weather_lon` and the name together (DEMO_PACKAGE.md) |
| A series in the Agenda is an hour off for half the year (or a host test cannot show a time-zone bug at all) | Every host test runs with `TZ=UTC0`, where "n x 86400 s" and "the same wall-clock time on the n-th day" are the same thing; the expander used 86400 s steps over `mktime()` local time | Test time arithmetic in a zone with daylight saving time: `CalendarIcsLocalTime` in `test_calendar_ics.cpp` sets the POSIX rule `CET-1CEST,M3.5.0,M10.5.0/3` (no tzdata needed). Step by calendar day (`local_time_on_day()`), not by seconds, and build test windows from calendar days too (`mktime()` normalizes the day of the month) |
| Test device gone silent | Deep sleep, WiFi, SD card | One check, then ask the maintainer (section 13) |

## 16. Open items

Nothing here is started without the maintainer's decision. Each actionable one below has a concrete proposed
fix; the rest are standing notes, not work items.

### Actionable

- ~~**Release `v218.0.3`**~~ **Published 2026-09-29**: <https://github.com/t3stier/esp32-photoframe-rebuild/releases/tag/v218.0.3>,
  16 assets, the tag-push run completed green (publishing then re-triggers the workflow once more to refresh the
  Pages site's stable entry, per the `release: types: [published]` trigger at the top of `build.yml`).
- ~~**Tag-push CI trigger unconfirmed**~~ **Closed 2026-09-29.** Pushing the `v218.0.3` tag started a "Build
  Firmware" run on `ref: v218.0.3` immediately (confirmed via `gh run list`) - a plain tag push alone does start a
  run in the fork now, matching the fix already made for branch pushes (`8c24444`). The documented fallback
  (`gh workflow run build.yml -R t3stier/esp32-photoframe-rebuild --ref vX.Y.Z`) is no longer needed but is kept
  in section 8 as a safety net in case this regresses.
- **Pre-release CI wiring**: implemented and first exercised 2026-09-29 with `v218.0.4-rc1`. Confirmed: the
  tag-push run produced a draft with 16 assets marked as a pre-release and left `manifest.json` on the previous
  stable version while writing `manifest-prerelease.json` (the draft is invisible to `gh release view`, the tag
  convention decided); publishing it as a pre-release triggered a run with the same result; the update feed showed
  the release candidate as the newest release and `releases/latest` still the stable one; a real frame on the
  pre-release channel installed it over OTA; a later push to `main` kept `manifest-prerelease.json` for every
  board (the restore path). Checked 2026-10-05 with headless Chrome: the landing pages of both sites show the Stable /
  Dev / Pre-release radios with their versions and no script error. **Still to confirm:** that a frame running a
  release candidate is offered the final release (the `-rc<n>` comparison is covered by host tests; the live part needs
  a frame on an `-rc` build and a later final of the same line).
- ~~**Docker-pull retry**~~ **Done 2026-09-29.** `build.yml`'s `build` job pre-pulls `espressif/idf:release-v6.0`
  with a 3-attempt retry loop right before `Setup ESP-IDF`, so a Docker Hub 502 there is now usually absorbed
  before the action's own pull runs. `feature-compile` (the per-flag compile-only job) was left as is - its
  existing `gh run rerun --failed` workaround is enough for how rarely it fails, and it doesn't ship a release.
- **Demo package** for the `waveshare_photopainter_73` (`examples/waveshare_photopainter_73/`,
  `scripts/generate_demo_photos.py`, `scripts/test_example_config.py`, `host_tests/test_example_calendars.cpp`).
  User-facing overview: [DEMO_PACKAGE.md](DEMO_PACKAGE.md) (the fuller internal planning notes are intentionally not
  part of this repository - see the note on `docs/DEMO_PLAN.md` below). Status of the steps:
  1. ~~CI~~ Done: `scripts/test_example_config.py` is picked up by `ci.yml`'s feature-tooling job through
     `python -m unittest discover -s scripts -p "test_*.py"` (no separate wiring needed); the calendars/ToDo through
     the firmware's own parsers by the host tests.
  2. ~~Publish~~ Implemented 2026-09-29: `deploy-pages` copies `examples/` into the site and its last step checks
     that every URL the two configs use answers `200`. Confirmed 2026-10-05: all six answer `200` and both served
     configs are byte-identical to the files in the repository (a device that imported the config before the first
     deploy saw HTTP 404 for calendars A-E and the ToDo list, because nothing served them).
  3. ~~Landing page and docs~~ Done: a `#if FORK_SITE` "demo configuration" step in the landing page's "How it goes"
     list, README and FEATURES.md links.
  4. ~~Live device test~~ Done 2026-10-05 on the extended-line test frame (Waveshare, all-features build of the same
     base): the served `demo-config-storage.json` imported through the real Web UI (Settings -> Maintenance -> Import
     Config; every setting the frame reports matches the file, the six URLs are write-only), calendars C, D and E were
     fetched from the site at import (C had been pointed at a placeholder address first, D and E were unset), the next
     Agenda render drew them (A 16 events, B 7, the three extra sources expanded, no "no saved URL" warning left), then
     `demo-config-url.json` imported the same way, and the three color profiles were imported and read back
     byte-identical. Not covered: a frame with a completely empty settings memory - a factory reset also erases the
     WiFi credentials and the frame cannot be reached again without re-entering them, so the frame kept its WiFi.
  5. A release that ships it: `v219.0.0` (the base) and `v219.0.0-rc1` (the extended edition).
  Known gap: importing a config whose Calendar C/D/E URL equals the stored one never re-fetches (only a changed URL
  or "Refresh now" does), so an import made while the files were unreachable leaves C-E without a cached file. A
  fetch-if-the-cache-file-is-missing rule in `apply_extra_ics_url()` would close it (not done).

- ~~**New upstream pull request for the photo-read fix**~~ **Opened 2026-10-05 as [aitjcize/esp32-photoframe#145](https://github.com/aitjcize/esp32-photoframe/pull/145)** after the maintainer read the draft and
  approved it: one commit (`ad17d8c`, branch `fix-bmp-partial-read` of the canonical fork) on upstream `main` = v2.19.0 (`186ebaf`), disclosed as a finding of Claude with a link to the two repositories. The first pull request
  (#143) was closed on 2026-10-05 after upstream took five of its six fixes into v2.19.0. What this one carries: a BMP that fails to read halfway was shown as a half-blank picture with "Image displayed successfully"
  in the log (`read_bmp24_mapped()` returned success after a failed `fread`; `rotate_sequential()`/`rotate_random()` ignored `display_manager_show_image()`'s result). It builds clean for the Waveshare board on v2.19.0 and
  is `clang-format-18` clean; there is no host test (no host build of the display path) and the failure branch could not be provoked on a device. **Follow-up:** answer the review; when upstream merges it, the next upstream
  merge drops the `fixes` copy of this fix (`GUI_BMPfile.c`, `display_manager.c`) like the others (section 12).
- **Upstream pull requests from the Web UI test (2026-10-10), opened as #147-#152 on the maintainer's go (#145 is the BMP one); #153 (`config-report-ignored-fields`, the firmware answer that names ignored fields) followed**: six branches on top of upstream's `main` (v2.19.0, `186ebaf`), one concern each, written without directives (`fixes` code of this
  repository, section 12 drops each when upstream merges it): `webapp-gallery-missing-thumbnail` (the gallery and its two dialogs asked for `<album>/undefined`), `webapp-gallery-tell-failures` (album and image actions
  ignored the frame's refusal), `webapp-unreadable-image` (a file the browser cannot read as an image gave a blank preview and dead buttons), `webapp-import-robustness` (a JSON file that is no config export was
  "imported successfully", mistyped values the frame skips were not named, the frame's own message on a 400 was dropped), `webapp-settings-store-catch` (`catch (_error)` that prints `error`: a ReferenceError in the failure
  case, three places) and `webapp-offline-assets` (icons and Roboto from CDNs: a frame without internet access, e.g. in its own hotspot, shows a page without icons; bundled instead, +540 KB of `index.css.gz`, woff2 only,
  inlined). Each passes upstream's own `prettier --check`, `eslint`, `vitest` and `vite build` on v2.19.0. The drafts (disclosure as in #145, what/why/fix/validation) are in the maintainer's `local-tools/scratch/upstream_prs/`,
  the branches are in the git worktree `C:/pfpr/wt-up` (`local-tools/scratch/make_upstream_prs.py` rebuilds them). The branches are on the canonical fork. Opening a pull request needs the maintainer's go each time (as with #145).
- **A library issue behind upstream's JPEG fix goes to Espressif privately**: upstream's maintainer suggested reporting the library side of his fix. Espressif's `SECURITY.md` asks that
  vulnerabilities are **not** reported as public issues but through its security incident response process (coordinated disclosure; the private forms or the bug bounty address named there). Nothing is
  published by this repository about it; the report is the maintainer's to submit. **Decided 2026-10-05: not submitted.**
- ~~**Retire the settings of `wifi-resilience` that upstream v2.19.0 made inert**~~ **Done 2026-10-05.** *Extended retry* and *reprovision when attempts run out* (Web UI switches, the two API fields,
  the config getters/setters, `cold_boot_wifi_retry()` and `wifi_manager_last_failure_is_credential_reject()`) are gone; a short note in `main/config.h` names the three retired NVS entries
  (`wifi_ext_retry`, `wifi_cb_fail`, `wifi_reprov_en`), which an older device may still hold and nothing reads. A config export of an older build that carries the two fields is still accepted (unknown
  fields are ignored). The option itself stays: TX cap, performance mode, the Telegram power-save budget and MIC/802.1X-as-rejection are not upstream's.
- **Calendar recurrence engine (decided 2026-10-06, steps 0-1 done 2026-10-09).** The Agenda's own reader (`main/calendar_ics.c`) takes DAILY/WEEKLY rules only ([CALENDAR_RRULE_SUPPORT.md](CALENDAR_RRULE_SUPPORT.md)). A read-only review of three candidates (uICAL, GoogleCalendarClient, libical;
  host-measured against a `python-dateutil` reference, 25 rule cases, 10 broken-rule cases, a 2.4 MB feed) chose **libical** (v4.0.6; LGPL-2.1 *or* MPL-2.0 - **MPL-2.0 is the accepted choice**): 24 of 25 valid cases
  right, daylight saving time and `TZID` right with the feed's own `VTIMEZONE`, about 93 KB of flash (cross-compile estimate, not a device measurement). uICAL was out (fixed UTC offset per zone - a Berlin event is an hour
  off in winter -, its loader drops `EXDATE`/`RDATE`/`RECURRENCE-ID`, it throws C++ exceptions, walks from `DTSTART` linearly and hangs on `INTERVAL=0`); GoogleCalendarClient is no recurrence engine at all (ESP8266/Arduino,
  no `singleEvents`, certificate checks off, Google's device flow does not allow the Calendar scope). Done: steps 0-1 (the library-independent fixes: DST on the wall clock, `EXDATE`/`RDATE`/`RECURRENCE-ID`/`STATUS`/`DURATION`,
  earliest-48 list; tests, fuzzing and a differential run against libical on 97 public fixtures). **Still open** (maintainer decided: go ahead with the option, name `agenda-rrule`, a sub-option of `agenda`; target limits were proposed - flash growth at most 150 KB, a
  2 MB / 30-day window in about 2 s on the ESP32-S3 (today's reader needs 4.5 s, so this needs checking), the internal heap left for a following TLS connection, the M5Paper must run - and the maintainer answered yes; the numbers are fixed after the proof of concept): (2) a proof of concept on a device - libical must be used **one VEVENT at a time** (parsing the whole feed takes about ten times its size in RAM: 25 MB for 2.4 MB),
  measured against today's 4.5 s for a 2 MB / 4000-event feed on the ESP32-S3; (3) the vendored component (generated sources checked in, a hand-written `config.h`, `UPSTREAM.md` with tag/commit/SHA256, MPL-2.0 text and notice under
  `docs/third_party/`, no Kconfig symbol so the all-off proofs stay); (4) an adapter that keeps the scanner for unfolding, window prefilter and `EXDATE` handling, hands only repeating events to libical, drops events libical reports
  errors for (`icalcomponent_count_errors()`, so fail-closed stays) and bounds iterations and results itself (`foreach_recurrence` cannot be stopped from its callback; `FREQ` below DAILY is refused; `RDATE` with `TZID` must be
  resolved by hand); (5)-(7) a temporary parallel comparison of old and new expansion in the log, rollout through an `extras` pre-release, docs and support matrix, removal of the old expander only after an acceptance cycle.
  Whether the option lives in the base or the extended line is not decided. Public feeds, generated feeds or invented ones are the test data (the maintainer has no feeds that may be used).

### Standing notes (not action items)

- Documentation drift to keep an eye on: several feature docs were written for the old fork (an always-on
  firmware with a separate "Alarm Clock firmware" variant). [ALARMCLOCK_USER_GUIDE.md](ALARMCLOCK_USER_GUIDE.md) was
  corrected on 2026-09-28; when touching another feature doc, check for claims about variants, the old repository
  or menu names that no longer exist.
- `docs/GUIDE.html` (an end-user guide draft) and `docs/DEMO_PLAN.md` (this demo package's internal planning
  notes - research, decisions, the full verification/CI checklist the "Demo package" item above summarizes) are
  both deliberately **gitignored, local-only** on the maintainer's machine - `docs/DEMO_PLAN.md` was untracked on
  2026-09-29 specifically because it isn't meant to be public; its user-facing counterpart is the tracked
  [DEMO_PACKAGE.md](DEMO_PACKAGE.md). Do not track either without being asked.
- Upstream's `gh-pages` and side branches copied into the fork are ignored; do not "clean up" without asking.

## 17. File map

| Path | What |
| --- | --- |
| `build.py` | Build driver (board, features, webapp/splash/firmware steps, `--ota-repo`) |
| `main/` | Firmware application; `Kconfig`, `feature_config.h`, `config*.{h,c}`, `http_server.c`, `main.c`, `utils.c`, `ota_manager.c`, modules per feature |
| `components/` | Drivers and HAL: `board_hal`, e-paper drivers, sensors, RTC, PMIC, SD card |
| `boards/` | `boards.json`, `capabilities.json`, per-board sdkconfig defaults |
| `features/` | `sdkconfig.defaults.<feature>` overlays |
| `sdkconfig.defaults` | Base sdkconfig (note `CONFIG_ESP_TLS_INSECURE`: without a pinned certificate, HTTPS image/ICS fetches do not verify the server) |
| `scripts/features.py`, `boards.py`, `check_capabilities.py`, `feature_matrix.py` | Feature registry, board tables, checks, compile matrix |
| `scripts/verify_baseline.py`, `scripts/migrate/` | Equality proofs (`alloff_source.py`, `alloff_web.py`, `xref.py`), the per-feature-set check of the web sources (`web_bindings.py`), the one-off `gate.py` and its `maps/` |
| `scripts/generate_manifests.py`, `launch_demo.py` | Web flasher manifests, local demo server |
| `scripts/test_*.py` | Tooling unit tests (41) |
| `webapp/` | Vue web UI; `feature-directives.js`, `vite.config.js`, `vite.config.demo.js`, `index-demo.html`, `src/` |
| `process-cli/` | Host-side image processing tool (Node) |
| `host_tests/` | GoogleTest host tests (339) |
| `demo/` | Tracked stubs (`.nojekyll`, `_headers`, `favicon.svg`); the rest is generated site output (gitignored) |
| `.github/workflows/ci.yml`, `build.yml` | CI, builds, release, Pages deploy |
| `docs/FEATURES.md` | User-facing feature list and OTA notes |
| `docs/API.md`, `OVERLAYS.md`, `TELEGRAM.md`, `ALARMCLOCK_USER_GUIDE.md`, `CALENDAR_RRULE_SUPPORT.md`, `FACE_CROP.md`, `SCALE_MODE.md`, `MEASURED_PALETTE.md` | Feature and API documentation |
| `docs/DEMO_PACKAGE.md`, `examples/waveshare_photopainter_73/` | User-facing demo package overview and its files (section 16) |
| `CHANGELOG.md` | Fresh changelog of this fork |
| `CLAUDE.md` | Short rules file for AI coding assistants; points here |

## 18. Conventions

- Match the surrounding code: comment density, naming, idioms. C follows the repository's `.clang-format`.
- Comments explain the *why* (constraints, incidents), as the existing code does; do not restate the code.
- Every behaviour change to a shared file is guarded (section 5) and gets a `CHANGELOG.md` entry under
  `[Unreleased]`.
- Docs are English and tracked; keep [FEATURES.md](FEATURES.md) in step with the code, and update this guide
  when a procedure changes.
- Tests: a bug fix gets a host test where the logic can be isolated (pure helpers such as `history_decimate.h`
  exist for that reason); tooling changes get a `scripts/test_*.py` test.
