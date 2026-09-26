# Optional features

This firmware is the upstream ESP32 PhotoFrame firmware plus a set of features that
are switched on at build time. **Without any option the build is the upstream
firmware**: same behaviour, same web UI, same `GET /api/config`.

```bash
python build.py --board waveshare_photopainter_73 --with agenda,telegram
python build.py --board waveshare_photopainter_73 --all-features
python build.py --board seeedstudio_xiao_ee02 --all-features --without https
python build.py --board seeedstudio_xiao_ee02 --list-features
```

| Option (`--with ...`) | What it adds | Needs |
| --- | --- | --- |
| `telegram` | Telegram bot photo rotation, remote commands, orientation pairing | - |
| `overlays` | Weather and headline overlays, captions, low-battery badge | - |
| `agenda` | Agenda mode: ToDo and calendars A-E, 7-day grid, colour profiles | - |
| `chimes` | Beep feedback for firmware events | speaker |
| `climate` | Temperature/humidity readout, badges and history | temperature/humidity sensor |
| `alarmclock` | Bedside alarm clock with schedule and button UI | speaker |
| `voice-stop` | Stop a ringing alarm with a spoken word | `alarmclock`, speaker, microphone |
| `battery-history` | Battery history chart and days-remaining estimate | - |
| `display-history` | No-repeat random rotation | - |
| `https` | HTTPS web UI on port 443 | - |
| `offline-hotspot` | Offline mode and on-demand hotspot (hold BOOT for 3 s) | - |
| `error-banner` | On-display error banner for WiFi and internet failures | - |
| `ota-channel` | OTA release channel (stable/pre-release) and firmware variant choice | - |
| `facecrop` | Face-aware crop sidecars and Cover/Fit image variants | - |
| `fixes` | General bug fixes and robustness improvements | - |

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
- A feature that needs another one pulls it in and says so (`--with voice-stop`
  adds `alarmclock`).
- Kconfig (`main/Kconfig`) and a compile-time check (`main/feature_config.h`) repeat
  these rules, so a hand-edited `sdkconfig` cannot produce an invalid combination.
- Changing the board or the feature set makes `build.py` do a full clean of `build/`
  automatically; a stale `sdkconfig` cannot survive it.

Design and status: [FEATURE_FLAGS_PLAN.md](FEATURE_FLAGS_PLAN.md).
