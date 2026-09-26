#!/usr/bin/env python3
"""Check that the places that describe board hardware and features agree.

The same facts live in several places, each for a different consumer:
  * boards/boards.json           "capabilities" - build.py / CI (scripts/features.py)
  * board headers                BOARD_HAL_HAS_SPEAKER / _MICROPHONE - C code
  * components/board_hal/Kconfig SENSOR_DRIVER_* selects - climate sensor
  * main/Kconfig                 FORK_HW_* helpers and the FEATURE_* flags
  * main/feature_config.h        FEATURE_* macros
  * features/                    one sdkconfig overlay per feature

Exits non-zero and lists every disagreement.
"""

import re
import sys
from pathlib import Path

from boards import BOARD_CAPABILITIES
from features import FEATURES, ROOT

HAL_KCONFIG = ROOT / "components" / "board_hal" / "Kconfig"
MAIN_KCONFIG = ROOT / "main" / "Kconfig"
FEATURE_HEADER = ROOT / "main" / "feature_config.h"
INCLUDE_DIR = ROOT / "components" / "board_hal" / "include"

# hardware name -> (Kconfig helper symbol, board header macro or None)
HARDWARE = {
    "speaker": ("FORK_HW_SPEAKER", "BOARD_HAL_HAS_SPEAKER"),
    "microphone": ("FORK_HW_MICROPHONE", "BOARD_HAL_HAS_MICROPHONE"),
    "climate_sensor": ("FORK_HW_CLIMATE_SENSOR", None),
}


def kconfig_blocks(path):
    """Map each `config SYMBOL` in a Kconfig file to the text that follows it."""
    text = path.read_text(encoding="utf-8")
    pattern = re.compile(
        r"^[ \t]*config[ \t]+(\w+)[ \t]*\n(.*?)"
        r"(?=^[ \t]*(?:config|menuconfig|choice|endchoice|menu|endmenu|comment)\b|\Z)",
        re.M | re.S,
    )
    return {m.group(1): m.group(2) for m in pattern.finditer(text)}


def board_symbol(board):
    return f"BOARD_DRIVER_{board.upper()}"


def check_headers(errors):
    for board, capabilities in BOARD_CAPABILITIES.items():
        header = INCLUDE_DIR / f"board_{board}.h"
        text = header.read_text(encoding="utf-8") if header.is_file() else ""
        for hardware, (_, macro) in HARDWARE.items():
            if macro is None:
                continue
            defined = re.search(rf"^#define {macro} 1\b", text, re.M) is not None
            if defined != (hardware in capabilities):
                errors.append(
                    f"{board}: boards.json says {hardware}="
                    f"{hardware in capabilities}, {header.name} says {defined}"
                )


def check_sensor_kconfig(errors):
    hal = kconfig_blocks(HAL_KCONFIG)
    helper = kconfig_blocks(MAIN_KCONFIG).get("FORK_HW_CLIMATE_SENSOR", "")
    sensors = set(re.findall(r"\b(SENSOR_DRIVER_\w+)", helper))
    if not sensors:
        errors.append("main/Kconfig: FORK_HW_CLIMATE_SENSOR lists no SENSOR_DRIVER_*")
    for board, capabilities in BOARD_CAPABILITIES.items():
        selects = set(
            re.findall(r"select (SENSOR_DRIVER_\w+)", hal[board_symbol(board)])
        )
        has = bool(selects & sensors)
        if has != ("climate_sensor" in capabilities):
            errors.append(
                f"{board}: boards.json says climate_sensor="
                f"{'climate_sensor' in capabilities}, components/board_hal/Kconfig "
                f"selects {sorted(selects) or 'no sensor driver'}"
            )


def check_hardware_helpers(errors):
    blocks = kconfig_blocks(MAIN_KCONFIG)
    for hardware in ("speaker", "microphone"):
        symbol = HARDWARE[hardware][0]
        listed = set(re.findall(r"\b(BOARD_DRIVER_\w+)", blocks.get(symbol, "")))
        expected = {
            board_symbol(b)
            for b, caps in BOARD_CAPABILITIES.items()
            if hardware in caps
        }
        if listed != expected:
            errors.append(
                f"main/Kconfig {symbol}: default y if {sorted(listed)}, "
                f"boards.json expects {sorted(expected)}"
            )


def check_features(errors):
    blocks = kconfig_blocks(MAIN_KCONFIG)
    header = FEATURE_HEADER.read_text(encoding="utf-8")
    for feature in FEATURES:
        block = blocks.get(feature.symbol)
        if block is None:
            errors.append(f"main/Kconfig: no 'config {feature.symbol}'")
            continue
        if re.search(r"^\s*default n\b", block, re.M) is None:
            errors.append(f"main/Kconfig: {feature.symbol} must be 'default n'")
        depends = " ".join(re.findall(r"^\s*depends on (.*)$", block, re.M))
        for hardware in feature.hardware:
            if HARDWARE[hardware][0] not in depends:
                errors.append(
                    f"main/Kconfig: {feature.symbol} needs {hardware} but does not "
                    f"'depends on {HARDWARE[hardware][0]}'"
                )
        for required in feature.requires:
            symbol = next(f.symbol for f in FEATURES if f.name == required)
            if symbol not in depends:
                errors.append(
                    f"main/Kconfig: {feature.symbol} requires {required} but does "
                    f"not 'depends on {symbol}'"
                )
        overlay = ROOT / "features" / f"sdkconfig.defaults.{feature.name}"
        if not overlay.is_file():
            errors.append(f"{overlay.relative_to(ROOT)} is missing")
        elif f"CONFIG_{feature.symbol}=y" not in overlay.read_text(encoding="utf-8"):
            errors.append(f"{overlay.name} does not set CONFIG_{feature.symbol}=y")
        if f"#define {feature.symbol} 1" not in header:
            errors.append(f"main/feature_config.h does not define {feature.symbol}")


def main():
    errors = []
    check_headers(errors)
    check_sensor_kconfig(errors)
    check_hardware_helpers(errors)
    check_features(errors)
    if errors:
        print("Capability/feature tables disagree:")
        for error in errors:
            print(f"  - {error}")
        return 1
    print(
        f"OK: {len(BOARD_CAPABILITIES)} boards and {len(FEATURES)} features consistent"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
