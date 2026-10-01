# Developer Guide

This guide covers building the firmware from source and advanced configuration options.

## Software Requirements

- ESP-IDF v6.0 or later
- Python 3.7+ (for build tools)
- npm, vite (note: this excludes use under Wayland)
- ESP Component Manager (comes with ESP-IDF)

## Building from Source

### 1. Set up ESP-IDF

```bash
# Source the ESP-IDF environment
cd <path to esp-idf>
. ./export.sh
```

### 2. Build the Project

We provide a `build.py` helper script that handles configuration and building for different boards.

```bash
cd <path to photoframe-api>

# install npm dependencies
cd webapp
npm install
cd ..

# Build for Waveshare PhotoPainter (7.3" 6-color e-paper)
./build.py --board waveshare_photopainter_73

# Build for Seeed Studio XIAO EE02 (13.3" 6-color e-paper)
./build.py --board seeedstudio_xiao_ee02

# Build for Seeed Studio XIAO EE03 (10.3" 16-level grayscale e-paper)
./build.py --board seeedstudio_xiao_ee03

# Build for Seeed Studio XIAO EE04 (7.3" 6-color e-paper)
./build.py --board seeedstudio_xiao_ee04

# Build for Seeed Studio reTerminal E1002 (7.3" 6-color e-paper)
./build.py --board seeedstudio_reterminal_e1002

# Build for Seeed Studio reTerminal E1003 (10.3" 16-level grayscale e-paper)
./build.py --board seeedstudio_reterminal_e1003

# Build for Seeed Studio reTerminal E1004 (13.3" 6-color e-paper)
./build.py --board seeedstudio_reterminal_e1004

# M5Stack M5Paper v1.0/v1.1 (4.7" 16-level grayscale, ESP32 — not S3)
./build.py --board m5stack_m5paper_v11

# Clean build (optional)
./build.py --board waveshare_photopainter_73 --fullclean
```

The script automatically:
1. Builds the frontend webapp (`webapp/`)
2. Sets the correct `sdkconfig.defaults` for the selected board
3. Passes the board's target chip via `-DIDF_TARGET` (`esp32s3` for every board
   except the M5Paper, which is a plain `esp32`)
4. Runs `idf.py build` OR `idf.py build` with correct options

**Optional features.** Without further options the build is the upstream firmware. Extra
features (Telegram, agenda, overlays, alarm clock, HTTPS, ...) are switched on per build:

```bash
./build.py --board waveshare_photopainter_73 --with agenda,telegram
./build.py --board waveshare_photopainter_73 --all-features
./build.py --board seeedstudio_xiao_ee02 --list-features
```

See [FEATURES.md](FEATURES.md) for the list, the hardware each feature needs and how the
options are checked. To compile every feature on its own, run
`python scripts/feature_matrix.py --board <board> single` in an activated ESP-IDF shell.

### 3. Flash and Monitor

The project uses ESP Component Manager to automatically download the `esp_jpeg` component during the first build.

```bash
# Flash to device (replace PORT with your serial port, e.g., /dev/cu.usbserial-*)
idf.py -p PORT flash

# Monitor output
idf.py -p PORT monitor

# Flash and monitor in one go
idf.py -p PORT flash monitor
```

**Note:** On the first build, ESP-IDF will automatically download the `esp_jpeg` component from the component registry. This requires an internet connection.

## Configuration Options

Almost everything (timezone, rotation schedule, image source, and every optional feature's own
settings) is a **runtime** setting the device stores in NVS and exposes over the Web UI and
`GET`/`PATCH /api/config` - see [API.md](API.md). `main/config.h` only holds the handful of
values a fresh device starts with before its first configuration, plus fixed limits:

```c
#define AUTO_SLEEP_TIMEOUT_SEC      120          // Auto-sleep timeout (2 minutes)
#define DEFAULT_ROTATE_CRON         "0 */12 *"    // First-boot rotation schedule (every 12h)
```

Display dimensions are **not** a compile-time constant - each board reports its own native size
(`BOARD_HAL_DISPLAY_WIDTH`/`BOARD_HAL_DISPLAY_HEIGHT` in `components/board_hal/include/board_hal.h`,
resolved from the actual panel driver), since boards range from 800×480 up to 1872×1404.

## Development Workflow

### Serial Monitor

Monitor device logs in real-time:

```bash
idf.py -p PORT monitor
```

Press `Ctrl+]` to exit the monitor.

### Erase Flash

To completely reset the device (including WiFi credentials):

```bash
idf.py erase-flash
```

### Finding Serial Port

**macOS:**
```bash
ls /dev/cu.*
```

**Linux:**
```bash
ls /dev/ttyUSB*
```

**Windows:**
Check Device Manager for COM ports.

## Project Structure

```
esp32-photoframe-rebuild/
├── main/
│   ├── main.c                 # Entry point
│   ├── config.h               # First-boot defaults, NVS keys, fixed limits
│   ├── display_manager.c      # E-paper display control
│   ├── http_server.c          # Web server and API
│   ├── image_processor.c      # Image processing (dithering, tone mapping)
│   ├── power_manager.c        # Sleep/wake management
│   ├── Kconfig                # FEATURE_*/FORK_* build-time options
│   └── webapp/                # Built web UI output (generated, gitignored - source is webapp/)
├── components/                # E-paper/sensor/board drivers (board_hal, epaper_*, sensor_*, ...)
├── boards/                    # Per-board sdkconfig defaults, boards.json, capabilities.json
├── features/                  # Per-feature sdkconfig overlays (sdkconfig.defaults.<name>)
├── scripts/                   # Build/feature tooling, equality-proof and test scripts
├── webapp/                    # Web UI source (Vue 3 + Vuetify)
├── process-cli/               # Node.js CLI tool
├── host_tests/                # GoogleTest host-side unit tests
├── examples/                  # Importable demo package (docs/DEMO_PACKAGE.md)
└── docs/                      # Documentation
```

## Debugging

### Enable Verbose Logging

In `idf.py menuconfig`:
1. Navigate to `Component config` → `Log output`
2. Set default log level to `Debug` or `Verbose`

### Decoding a crash report

When the firmware panics it saves a core dump to the `coredump` partition. The
next boot turns it into a one-line record, logs it (`COREDUMP: ...`, also in the
debug log when that is on) and keeps it until cleared: the web UI shows it
under **Settings → Maintenance → Last Crash** (Copy Report copies the line),
and `GET /api/system-info` returns it as `last_crash`. For example:

```
LoadProhibited, vaddr 0x00000000 | task httpd, pc 0x4201a2b3, bt 0x4201a2b3 0x4201c3d4 0x4037a1b2 | fw v2.20.1, elf 1a2b3c4d, board seeedstudio_xiao_ee02 | found 2026-09-21T14:13:20Z | dump 23456 B
```

To turn the addresses into source lines:

1. Download `<board>-<version>.elf` for the reported `board` and `fw` from the
   [release](https://github.com/aitjcize/esp32-photoframe/releases) (for a
   local build, use `build/esp32-photoframe.elf` of that build).
2. Check that it is the build that crashed: its SHA-256 must start with the
   `elf` value. `fw unknown` means the frame was running a different build when
   it found the dump (e.g. it was reflashed after crash-looping); find the
   release ELF whose SHA-256 matches.
   ```bash
   shasum -a 256 seeedstudio_xiao_ee02-v2.20.1.elf
   ```
3. Resolve the PC and backtrace with the ESP-IDF toolchain (after
   `. $IDF_PATH/export.sh`). Use `xtensa-esp32-elf-addr2line` for the M5Paper,
   which is a plain ESP32:
   ```bash
   xtensa-esp32s3-elf-addr2line -pfiaC -e seeedstudio_xiao_ee02-v2.20.1.elf 0x4201a2b3 0x4201c3d4 0x4037a1b2
   ```

The dump is erased once its record is saved, so it can't be pulled over USB
afterwards; a dump the firmware can't summarise stays in flash for
`idf.py coredump-info`.

### Common Issues

**Build fails with component errors:**
- Ensure ESP Component Manager is up to date
- Delete `managed_components/` and rebuild

**Flash fails:**
- Check USB cable connection
- Try a different USB port
- Reduce baud rate: `idf.py -p PORT -b 115200 flash`

**Device not responding:**
- Press and hold BOOT button while connecting USB
- Try erasing flash: `idf.py erase-flash`
