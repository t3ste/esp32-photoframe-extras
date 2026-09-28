# Notes for AI coding assistants

This repository is a fork of `aitjcize/esp32-photoframe` with 16 opt-in features. **Read
[docs/MAINTAINING.md](docs/MAINTAINING.md) first** - it has the accounts, the git model, the commands, the
pitfalls and the open items. Design and procedures: [docs/FEATURE_FLAGS_PLAN.md](docs/FEATURE_FLAGS_PLAN.md).

Rules that must not be forgotten (details and reasons in MAINTAINING.md):

- **No build option = upstream firmware, 1:1** (Kconfig, ELF symbols, `.bin` size, web bundle). Guard every change to
  a file that also exists upstream with `#if FEATURE_X ... #else <exact upstream text> #endif` (web: the same in
  comments, see `webapp/feature-directives.js`; demo-site-only web changes: `FORK_SITE`). After editing a shared file
  run `python scripts/migrate/alloff_source.py`, `python scripts/migrate/alloff_web.py`,
  `python scripts/migrate/xref.py`, the host tests, and build the board you touched.
- Never re-run `scripts/migrate/gate.py apply` on a file that has manual edits.
- Never pipe the output of `scripts/verify_baseline.py`.
- Releases are the FULL build of each board. Push to BOTH `t3stier` (canonical fork) and `origin` (mirror).
  Ask before pushing, tagging, releasing, force-pushing, deleting, flashing.
- Flash tests only with the multi-part esptool command (never the merged image at 0x0: it erases settings).
  Unreachable device: check once, then ask the maintainer; no retry loops.
- Privacy: no real names, SSIDs, tokens, chat IDs, calendar URLs, LAN IPs, MACs, locations, EXIF or local user
  paths in commits, docs or test data - invented data only. Never echo secret values.
- Format before committing: clang-format **18**, black, isort, prettier (`webapp`, `process-cli`).
- Do not modify upstream or any local read-only companion checkouts.
- Talk to the maintainer in German; code, comments, commit messages and docs are English. Commit trailer:
  `Co-Authored-By: <assistant> <noreply@anthropic.com>`.
