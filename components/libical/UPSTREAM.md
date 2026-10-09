# libical, vendored

| | |
| --- | --- |
| Project | [libical/libical](https://github.com/libical/libical) - the iCalendar library (RFC 5545 and friends) |
| Version | **v4.0.6**, commit `9cfca65c122fa23b9680a28e0c9225a2fd015fc5` |
| License | `LGPL-2.1-only OR MPL-2.0` - **this project uses it under the MPL-2.0** (see `LICENSE.txt`, `LICENSES/`, and [../../docs/third_party/LIBICAL-NOTICE.md](../../docs/third_party/LIBICAL-NOTICE.md)) |
| Vendored tree | `libical/`: 69 files, SHA-256 `819ec96c98c98d65bc61c985a8cd0e37af08406ae0c0134bf15516f62ee2f6f3` (sorted file hashes, see `vendor.sh`) |
| Used by | the build option `agenda-rrule` only (`main/calendar_rrule.c`); without it the component is empty |

## What is here

- `libical/` - the sources of libical's core (`src/libical/` of the tag) that the component compiles, the headers
  they include, and the files libical's build generates (`icalderived*.c/.h`, `icalrestriction.c`, `icaltime_p.h`; its
  Perl generators ran when this was vendored, so building the firmware needs no Perl). **Unmodified.** There is no
  patch list: if one is ever needed it goes here, with the reason.
- `port/config.h` - ours: replaces the `config.h` that libical's CMake build generates for a system (a single task,
  64-bit `time_t`, no file system, no time zone database).
- `CMakeLists.txt` - ours: an empty component unless `CONFIG_FEATURE_AGENDA_RRULE`.
- `vendor.sh` - how the tree was made, and how to update it: `components/libical/vendor.sh <tag>` (needs git, cmake >= 3.20,
  perl; set `CMAKE=` if the default cmake is older). It prints the commit and the tree's SHA-256 for this table.

## What is used

Only the recurrence iterator (`icalrecur.c`) is called, on wall-clock fields and a rule that `main/calendar_rrule.c`
has checked first. The parser, the components and the time zone code are linked because libical's files refer to each
other, and dropped again by the linker where nothing uses them (`-ffunction-sections`, `--gc-sections`).

## Updating

1. `components/libical/vendor.sh <new tag>`; put the printed commit and SHA-256 above.
2. `git diff --stat components/libical/libical` - read what changed in `icalrecur.c`, `icaltime.c` and `icalerror*`.
3. Host tests (`calendar_rrule_test`, `calendar_ics_rrule_test`) and the fuzzer in `host_tests/` - they compile this tree.
4. A build of every board with `agenda-rrule` (flash growth in the CHANGELOG), then a run on a frame.
