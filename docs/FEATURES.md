# Optional features

This firmware is the upstream ESP32 PhotoFrame firmware plus a set of features that
are switched on at build time. **Without any option the build is the upstream
firmware**: same behaviour, same web UI, same `GET /api/config`.

To see most of them working without setting up a Telegram bot, a calendar or a
Home Assistant instance, import the ready-made [demo package](DEMO_PACKAGE.md).
For a walkthrough of the newer options (calendar logins and CalDAV, batch upload, duplicate detection, the
information pages and what each needs) see the [user guide to the extras](EXTRAS_USER_GUIDE.md); what the Agenda and the
pages look like is in [SCREENSHOTS.md](SCREENSHOTS.md).

```bash
python build.py --board waveshare_photopainter_73 --with agenda,telegram
python build.py --board waveshare_photopainter_73 --all-features
python build.py --board seeedstudio_xiao_ee02 --all-features --without https
python build.py --board seeedstudio_xiao_ee02 --list-features
python build.py --board waveshare_photopainter_73 --with extras
```

| Option (`--with ...`) | What it adds | Needs | Details |
| --- | --- | --- | --- |
| `telegram` | Telegram bot photo rotation, remote commands, orientation pairing | - | [TELEGRAM.md](TELEGRAM.md) |
| `overlays` | Weather and headline overlays, captions, low-battery badge | - | [OVERLAYS.md](OVERLAYS.md) |
| `agenda` | Agenda mode: ToDo and calendars A-E, 7-day grid, colour profiles | - | [CALENDAR_RRULE_SUPPORT.md](CALENDAR_RRULE_SUPPORT.md) |
| `chimes` | Beep feedback for firmware events | speaker | |
| `climate` | Temperature/humidity readout, badges and history | temperature/humidity sensor | |
| `alarmclock` | Bedside alarm clock with schedule and button UI | speaker | [ALARMCLOCK_USER_GUIDE.md](ALARMCLOCK_USER_GUIDE.md) |
| `voice-stop` | Stop a ringing alarm with a spoken word | `alarmclock`, speaker, microphone | [ALARMCLOCK_USER_GUIDE.md](ALARMCLOCK_USER_GUIDE.md) |
| `battery-history` | Battery history chart and days-remaining estimate | - | |
| `display-history` | No-repeat random rotation | - | |
| `https` | HTTPS web UI on port 443 | - | |
| `offline-hotspot` | Offline mode and on-demand hotspot (hold BOOT for 3 s) | - | |
| `error-banner` | On-display error banner for WiFi and internet failures | - | |
| `ota-channel` | OTA release channel (stable/pre-release) | - | |
| `wifi-resilience` | WiFi options: cold-boot retries instead of an early credential wipe, option to keep the credentials, battery TX cap, performance mode | - | |
| `facecrop` | Face-aware crop sidecars and Cover/Fit image variants | - | [FACE_CROP.md](FACE_CROP.md), [SCALE_MODE.md](SCALE_MODE.md) |
| `webcal` | `webcal://` subscription links for the Agenda calendars (fetched over https) | `agenda` | |
| `source-auth` | A login (`https://user:password@host/...`) in the Agenda calendar and ToDo addresses, answered with HTTP Basic or Digest | `agenda` | [SOURCE_AUTH.md](SOURCE_AUTH.md) |
| `caldav` | CalDAV calendars (`caldavs://user:password@host/...`): the server sends only the coming days and expands repeating events | `source-auth` | [CALDAV.md](CALDAV.md) |
| `caldav-todo` | CalDAV task lists (`caldavs://...` as the ToDo address): the open to-dos with priority and due date in the ToDo column | `caldav` | [CALDAV_TODO.md](CALDAV_TODO.md) |
| `glyphs` | Real glyphs for ä ö ü Ä Ö Ü ß ° € in the text the frame draws (overlays, Agenda, Telegram captions) instead of ae/oe/ue/ss | - | [GLYPHS.md](GLYPHS.md) |
| `info-screens` | Full-screen information pages that take turns with the Agenda on its schedule (the base for the pages below) | `agenda`, `glyphs` | [INFO_SCREENS.md](INFO_SCREENS.md) |
| `chore-wheel` | Chore wheel page: who does which chore this week - a donut wheel and one card per chore, the chores go round the members by calendar week | `info-screens` | [CHORE_WHEEL.md](CHORE_WHEEL.md) |
| `weather-screen` | Weather page: today as a big icon and temperature, and the next four days as rows | `info-screens`, `overlays` | [WEATHER_SCREEN.md](WEATHER_SCREEN.md) |
| `fact-of-the-day` | Fact-of-the-day page: 24 built-in facts in English and German or your own list, a new fact every day | `info-screens` | [FACT_OF_THE_DAY.md](FACT_OF_THE_DAY.md) |
| `finance-snapshot` | Exchange-rate page: ECB reference rates of up to four currencies with the change and a 30-day line, free and without a key | `info-screens` | [FINANCE_SNAPSHOT.md](FINANCE_SNAPSHOT.md) |
| `fuel-prices` | Fuel-price page: the cheapest petrol stations around the weather place, price like on the pump (Germany, free Tankerkoenig API key) | `info-screens`, `overlays` | [FUEL_PRICES.md](FUEL_PRICES.md) |
| `market-quotes` | Markets page: up to four stocks, ETFs, indices, futures, crypto or currency pairs with the last price, the change and a 30-day line; Yahoo Finance without a key, Twelve Data and Alpha Vantage (free keys) as fallbacks | `info-screens` | [MARKET_QUOTES.md](MARKET_QUOTES.md) |
| `route-time` | Travel time there and back between two addresses in the header of the fuel page, with the traffic of now (TomTom, HERE with a free key); a way that takes longer than usual is a red block with a "!" | `fuel-prices` | [ROUTE_TIME.md](ROUTE_TIME.md) |
| `artworks` | Artworks rotation mode: a painting, drawing or print from the Rijksmuseum, SMK or the Smithsonian (public domain / CC0) per rotation, kept in an album for wakes without network, deleting the oldest to keep free space, with a small caption | `overlays` | [ARTWORKS.md](ARTWORKS.md) |
| `schedule-pages` | Pages per Agenda schedule: each schedule draws its own pages, the smaller number wins when two overlap, a minimum time (or a hold time per schedule) keeps one display from replacing another at once, the photo rotation gives way; without any page assigned nothing changes | `info-screens` | [SCHEDULE_PAGES.md](SCHEDULE_PAGES.md) |
| `upload-dedup` | Duplicate detection at upload: a per-album MD5 index, refuse or warn, by file or by pixels, background indexing of earlier images, duplicate report | - | [UPLOAD_DEDUP.md](UPLOAD_DEDUP.md) |
| `multi-upload` | Web UI: upload several images at once - photos converted one after the other, pre-rendered EPDGZ/PNG files as they are | - | [MULTI_UPLOAD.md](MULTI_UPLOAD.md) |
| `fixes` | General bug fixes and robustness improvements | - | |

## Bundles

A bundle is one name for several options: `--with extras` asks for every member at once. It is not an option of its own - nothing is compiled
for the name, the build only ever sees its members - so a build with `extras` is the same as one that lists them all.

| Bundle | Members |
| --- | --- |
| `extras` | everything added after the first fork release: `webcal`, `multi-upload`, `source-auth`, `caldav`, `caldav-todo`, `upload-dedup`, `glyphs`, `info-screens`, `chore-wheel`, `weather-screen`, `fact-of-the-day`, `finance-snapshot`, `fuel-prices`, `market-quotes` |

What the members need is added as usual (`extras` pulls in `agenda` and `overlays`). `--without fuel-prices` takes a member out again, and
`--without extras` takes all of them out of `--all-features`. A member that the board cannot build is skipped with a notice instead of stopping the
build (all members of `extras` need no special hardware, so every board builds all of them). `extras` does not include the options that were
already in the first release (Telegram, overlays, the Agenda, chimes, alarm clock, ...) nor `fixes`; `--all-features` is still everything.

Which optional hardware each board has:

| Hardware | Boards |
| --- | --- |
| Speaker and microphone | `waveshare_photopainter_73` |
| Temperature/humidity sensor | `waveshare_photopainter_73`, `seeedstudio_xiao_ee03`, `seeedstudio_reterminal_e1002`, `seeedstudio_reterminal_e1003`, `seeedstudio_reterminal_e1004`, `m5stack_m5paper_v11` |

## How the options are checked

- Asking for a feature the board cannot run (`--with chimes` on a board without a
  speaker) is an error that names the missing hardware and the boards that have it.
- `--all-features` enables everything the board supports and prints a warning for each
  feature it skipped.
- A bundle (`--with extras`) stands for several features; a member the board cannot run is skipped with a notice,
  and `--without` trims a member.
- A feature that needs another one pulls it in and says so (`--with voice-stop`
  adds `alarmclock`).
- Kconfig (`main/Kconfig`) and a compile-time check (`main/feature_config.h`) repeat
  these rules, so a hand-edited `sdkconfig` cannot produce an invalid combination.
- Changing the board or the feature set makes `build.py` do a full clean of `build/`
  automatically; a stale `sdkconfig` cannot survive it.

## OTA updates

The firmware checks the release feed in `CONFIG_FORK_OTA_REPO`, which defaults to this
project's own repository - `python build.py --ota-repo owner/name` picks another feed (e.g.
the upstream project, for comparing against it). The CI builds every released firmware with
its own repository (`--ota-repo ${{ github.repository }}`) explicitly, so this holds even if
the default ever changes.

**The releases carry the full firmware**: every optional feature the board's hardware
supports (what `--all-features` builds for that board), as `esp32-photoframe-<board>.bin`
(the file the update installs) and `photoframe-firmware-<board>-merged.bin` (the whole flash
image for `esptool` at offset 0). Whoever wants the plain upstream firmware gets it from
upstream; the CI still builds it (`-plain` file names, workflow artifacts only) to prove
that it compiles. A frame running a release therefore updates from the next release
without losing any feature. A build with only some features that installs an update
becomes the full firmware; keep the automatic update check off in the settings for such a
build. `build.py` prints a note whenever features are enabled.

The web flasher installs the same firmware. Its manifests list the individual parts of the
image (bootloader, partition table, OTA data, app) instead of the merged file, so the
settings partition (WiFi credentials, all settings) is left alone unless "Erase device" is
ticked. Writing the merged image itself at offset 0 (`esptool write-flash 0x0 ...-merged.bin`)
does erase the settings.
