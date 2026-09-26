"""Build-time feature registry and validation, used by build.py.

Every optional feature is a Kconfig flag (main/Kconfig) that is off by default;
with no feature selected the firmware is the upstream firmware. build.py turns
the selected features into sdkconfig.defaults overlays (features/) and passes
the same list to the web app build.

Compatibility rules (see docs/FEATURE_FLAGS_PLAN.md, section 3):
  * a feature that was asked for by name but cannot be built for the board is an
    error, with the missing hardware or dependency spelled out;
  * --all-features silently keeps to what fits: incompatible features are
    skipped and reported;
  * dependencies between features are added automatically and reported.
"""

from collections import namedtuple
from pathlib import Path

from boards import BOARD_CAPABILITIES

ROOT = Path(__file__).resolve().parent.parent
FEATURES_DIR = ROOT / "features"

Feature = namedtuple("Feature", "name symbol hardware requires summary")

# Registry order is the order used in listings and in the overlay list.
FEATURES = (
    Feature(
        "telegram",
        "FEATURE_TELEGRAM",
        (),
        (),
        "Telegram bot photo rotation, remote commands, orientation pairing",
    ),
    Feature(
        "overlays",
        "FEATURE_OVERLAYS",
        (),
        (),
        "Weather and headline overlays, captions, low-battery badge",
    ),
    Feature(
        "agenda",
        "FEATURE_AGENDA",
        (),
        (),
        "Agenda mode: ToDo and calendars A-E, 7-day grid, colour profiles",
    ),
    Feature(
        "chimes",
        "FEATURE_CHIMES",
        ("speaker",),
        (),
        "Beep feedback for firmware events through the speaker",
    ),
    Feature(
        "climate",
        "FEATURE_CLIMATE",
        ("climate_sensor",),
        (),
        "Temperature/humidity readout, badges and history",
    ),
    Feature(
        "alarmclock",
        "FEATURE_ALARMCLOCK",
        ("speaker",),
        (),
        "Bedside alarm clock with schedule and button UI",
    ),
    Feature(
        "voice-stop",
        "FEATURE_VOICE_STOP",
        ("speaker", "microphone"),
        ("alarmclock",),
        "Stop a ringing alarm with a spoken word",
    ),
    Feature(
        "battery-history",
        "FEATURE_BATTERY_HISTORY",
        (),
        (),
        "Battery history chart and days-remaining estimate",
    ),
    Feature(
        "display-history",
        "FEATURE_DISPLAY_HISTORY",
        (),
        (),
        "No-repeat random rotation (display history)",
    ),
    Feature(
        "https",
        "FEATURE_HTTPS",
        (),
        (),
        "Optional HTTPS web UI on port 443",
    ),
    Feature(
        "offline-hotspot",
        "FEATURE_OFFLINE_HOTSPOT",
        (),
        (),
        "Offline mode and on-demand hotspot (hold BOOT for 3 s)",
    ),
    Feature(
        "error-banner",
        "FEATURE_ERROR_BANNER",
        (),
        (),
        "On-display error banner for WiFi and internet failures",
    ),
    Feature(
        "ota-channel",
        "FEATURE_OTA_CHANNEL",
        (),
        (),
        "OTA release channel (stable/pre-release) and firmware variant choice",
    ),
    Feature(
        "facecrop",
        "FEATURE_FACECROP",
        (),
        (),
        "Face-aware crop sidecars and Cover/Fit image variants",
    ),
    Feature(
        "fixes",
        "FORK_FIXES",
        (),
        (),
        "General bug fixes and robustness improvements",
    ),
)

FEATURES_BY_NAME = {f.name: f for f in FEATURES}

_HARDWARE_LABEL = {
    "speaker": "a speaker",
    "microphone": "a microphone",
    "climate_sensor": "a temperature/humidity sensor",
}

Resolution = namedtuple("Resolution", "enabled added skipped")


class FeatureError(Exception):
    """A feature selection that cannot be built."""


def normalize(name):
    return name.strip().lower().replace("_", "-")


def parse_list(values):
    """Flatten repeated, comma-separated CLI values into a de-duplicated list."""
    names = []
    for value in values or []:
        for item in value.split(","):
            item = normalize(item)
            if item and item not in names:
                names.append(item)
    return names


def boards_with(capability):
    return [b for b, caps in BOARD_CAPABILITIES.items() if capability in caps]


def overlay_files(enabled):
    """Repo-relative sdkconfig.defaults overlays for the enabled features."""
    files = []
    for name in enabled:
        path = FEATURES_DIR / f"sdkconfig.defaults.{name}"
        if not path.is_file():
            raise FeatureError(f"missing sdkconfig overlay for '{name}': {path}")
        files.append(path.relative_to(ROOT).as_posix())
    return files


def _unsupported(name, board, excluded):
    """Why 'name' cannot be built for 'board', or None if it can."""
    feature = FEATURES_BY_NAME[name]
    capabilities = BOARD_CAPABILITIES[board]
    for hardware in feature.hardware:
        if hardware not in capabilities:
            having = ", ".join(boards_with(hardware))
            return (
                f"needs {_HARDWARE_LABEL[hardware]}, but board {board} has none "
                f"(boards with one: {having})"
            )
    for dependency in feature.requires:
        if dependency in excluded:
            return f"requires '{dependency}', which was excluded with --without"
        reason = _unsupported(dependency, board, excluded)
        if reason:
            return f"requires '{dependency}', which is unavailable ({reason})"
    return None


def resolve(board, requested=(), excluded=(), all_features=False):
    """Decide which features get built.

    Returns Resolution(enabled, added, skipped): 'enabled' in registry order,
    'added' maps a dependency that was pulled in to the feature that needs it,
    'skipped' maps a feature dropped by --all-features to the reason.
    Raises FeatureError for anything that was asked for but cannot be built.
    """
    if board not in BOARD_CAPABILITIES:
        raise FeatureError(f"unknown board '{board}'")
    requested = [normalize(n) for n in requested]
    excluded = [normalize(n) for n in excluded]
    for name in requested + excluded:
        if name not in FEATURES_BY_NAME:
            valid = ", ".join(FEATURES_BY_NAME)
            raise FeatureError(f"unknown feature '{name}' (valid: {valid})")
    conflict = sorted(set(requested) & set(excluded))
    if conflict:
        raise FeatureError(
            f"both --with and --without given for: {', '.join(conflict)}"
        )

    enabled = set()
    added = {}
    skipped = {}

    if all_features:
        for feature in FEATURES:
            if feature.name in excluded:
                continue
            reason = _unsupported(feature.name, board, excluded)
            if reason and feature.name in requested:
                raise FeatureError(f"'{feature.name}' {reason}")
            if reason:
                skipped[feature.name] = reason
            else:
                enabled.add(feature.name)
    else:
        for name in requested:
            reason = _unsupported(name, board, excluded)
            if reason:
                raise FeatureError(f"'{name}' {reason}")
            enabled.add(name)
        pending = list(requested)
        while pending:
            current = pending.pop()
            for dependency in FEATURES_BY_NAME[current].requires:
                if dependency not in enabled:
                    enabled.add(dependency)
                    added[dependency] = current
                    pending.append(dependency)

    ordered = [f.name for f in FEATURES if f.name in enabled]
    return Resolution(ordered, added, skipped)


def describe(board):
    """One line per feature with its status for 'board' (for --list-features)."""
    width = max(len(f.name) for f in FEATURES)
    lines = [f"Features for board {board}:"]
    for feature in FEATURES:
        reason = _unsupported(feature.name, board, ())
        status = "ok" if reason is None else "not available"
        lines.append(f"  {feature.name:<{width}}  {status:<13}  {feature.summary}")
        if reason:
            lines.append(f"  {'':<{width}}  {'':<13}  -> {reason}")
    return "\n".join(lines)
