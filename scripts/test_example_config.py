"""Unit tests for the demo package's importable configuration
(examples/waveshare_photopainter_73/, see docs/DEMO_PACKAGE.md).

Checked here, not just by hand: the two files only ever set "config" (never
processing/palette/albums - importing those would overwrite the user's own
palette calibration and album state), every key they set is one
apply_config_from_json() in main/utils.c actually reads (scraped from the
source, so a firmware rename breaks THIS test instead of silently leaving an
example file pointing at a key nothing reads any more), no key that could
carry a credential, WiFi setting or other personal data is present, every
enum-like value and cron rule is one the firmware accepts, every URL is
well-formed and within the device's 256-character limit, and none of the
package's files (including the calendars and the ToDo list) leak anything
that looks like a private IP, an email address, an embedded token or a local
filesystem path. Calendar/ToDo *content* is checked separately, through the
firmware's own parser, by host_tests/test_example_calendars.cpp. The Calendar
color profiles (color_profiles/) are checked here against the same rules
main/agenda_color_profile.c's parser enforces (its required color keys and
color grammar are scraped from the source, like the config keys above).
"""

import json
import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
EXAMPLE_DIR = REPO_ROOT / "examples" / "waveshare_photopainter_73"
UTILS_C = REPO_ROOT / "main" / "utils.c"
CONFIG_H = REPO_ROOT / "main" / "config.h"

CONFIG_FILES = ["demo-config-url.json", "demo-config-storage.json"]
COLOR_PROFILE_DIR = EXAMPLE_DIR / "color_profiles"
AGENDA_COLOR_PROFILE_C = REPO_ROOT / "main" / "agenda_color_profile.c"

# Never allowed in an example file, even though the firmware would accept
# them ("deliberately absent", docs/DEMO_PACKAGE.md): any WiFi/network
# setting, any credential, or the device's own name/password.
FORBIDDEN_KEYS = {
    "wifi_ssid",
    "wifi_password",
    "static_ip",
    "static_netmask",
    "static_gateway",
    "dns_server",
    "ip_mode",
    "device_name",
    "http_password",
    "https_enabled",
    "access_token",
    "http_header_key",
    "http_header_value",
    "ha_url",
    "ha_enabled",
    "openai_api_key",
    "google_api_key",
    "ntp_server",
}

ENUM_FIELDS = {
    "rotation_mode": {"storage", "url"},
    "sd_rotation_mode": {"random", "sequential"},
    "weather_provider": {"open-meteo", "wttr.in", "yr.no"},
    "overlay_language": {"en", "de"},
    "display_orientation": {"landscape", "portrait"},
    "agenda_cal_layout_mode": {"list", "grid_a", "grid_b"},
    "agenda_shift_model": {"none", "2-2-3", "week_week", "3-4"},
    "agenda_cal_multiday_mode": {"repeat", "compact", "repeat_numbered"},
    "agenda_cal_time_display_mode": {"off", "duration", "range"},
    "chime_speaker_mode": {"off", "battery_and_mains", "mains_only"},
    "climate_room_type": {"living_room", "bedroom", "bathroom", "kitchen", "basement"},
    "climate_temp_unit": {"celsius", "fahrenheit"},
}

TIME_RE = re.compile(r"^([01]\d|2[0-3]):[0-5]\d$")
URL_RE = re.compile(r"^https?://")
URL_MAX_LEN = 256

# Fields whose device-side buffer is smaller than the generic 256-char URL
# cap - main/config.h's own #define, scraped rather than copied so a changed
# constant fails this test instead of letting an example value silently
# truncate on the device. Each buffer holds the NUL terminator too (verified
# against config_manager.c's own `strncpy(..., X_MAX_LEN - 1)` pattern), so
# the usable text length is one less than the constant.
FIELD_MAX_LEN_DEFINE = {
    "agenda_cal_name": "AGENDA_CAL_NAME_MAX_LEN",
    "agenda_cal_name2": "AGENDA_CAL_NAME_MAX_LEN",
    "agenda_cal_c_name": "AGENDA_CAL_CDE_NAME_MAX_LEN",
    "agenda_cal_d_name": "AGENDA_CAL_CDE_NAME_MAX_LEN",
    "agenda_cal_e_name": "AGENDA_CAL_CDE_NAME_MAX_LEN",
    "weather_lat": "WEATHER_LATLON_MAX_LEN",
    "weather_lon": "WEATHER_LATLON_MAX_LEN",
    "weather_location_name": "WEATHER_LOCATION_NAME_MAX_LEN",
    "weather_provider": "WEATHER_PROVIDER_MAX_LEN",
}


def config_h_defines():
    text = CONFIG_H.read_text(encoding="utf-8")
    return {
        name: int(value)
        for name, value in re.findall(r"#define\s+([A-Z0-9_]+)\s+(\d+)\b", text)
    }


PRIVATE_DATA_PATTERNS = [
    re.compile(
        r"\b(10\.\d+\.\d+\.\d+|192\.168\.\d+\.\d+|172\.(1[6-9]|2\d|3[01])\.\d+\.\d+)\b"
    ),
    re.compile(r"(token|secret|apikey|api_key)\s*=", re.IGNORECASE),
    re.compile(r"[Cc]:\\Users\\|/home/[a-z0-9_]+|/Users/[A-Za-z0-9_ ]+"),
    re.compile(r"\bCOM\d\b"),
]

# Checked separately from the patterns above: the calendars' own VEVENT UIDs
# are deliberately shaped like an email address (standard iCalendar
# practice), e.g. "demo-a-mon-1@examples.local" - not a real address. Only
# flag a domain that is NOT one of the reserved-for-documentation/synthetic
# ones (RFC 2606 "example.*"/"test"/"invalid"/"localhost", RFC 6762 ".local").
EMAIL_RE = re.compile(r"[A-Za-z0-9._%+-]+@([A-Za-z0-9.-]+\.[A-Za-z]{2,})")
RESERVED_EMAIL_DOMAIN_SUFFIXES = (
    "example.com",
    "example.org",
    "example.net",
    "local",
    "test",
    "invalid",
    "localhost",
)


def real_looking_emails(text):
    matches = []
    for m in EMAIL_RE.finditer(text):
        domain = m.group(1).lower()
        if any(
            domain == s or domain.endswith("." + s)
            for s in RESERVED_EMAIL_DOMAIN_SUFFIXES
        ):
            continue
        matches.append(m.group(0))
    return matches


def load(name):
    return json.loads((EXAMPLE_DIR / name).read_text(encoding="utf-8"))


def accepted_config_keys():
    text = UTILS_C.read_text(encoding="utf-8")
    keys = set(re.findall(r'cJSON_GetObjectItem\(root,\s*"([a-z0-9_]+)"', text))
    # The extra ICS calendars' (C/D/E) URL fields aren't read directly - they
    # go through apply_extra_ics_url(root, "agenda_cal_c_url", ...), a small
    # helper for the "fetch once, on change or refresh" fields (see its own
    # doc comment in utils.c) - same source-of-truth principle, different call
    # shape, so it needs its own scrape.
    keys |= set(re.findall(r'apply_extra_ics_url\(root,\s*"([a-z0-9_]+)"', text))
    # The 7 chime_event_*_enabled fields are read in a loop over a small
    # table (chime_event_fields[]) rather than one cJSON_GetObjectItem() call
    # per field - same reasoning, different call shape.
    keys |= set(re.findall(r'\{"([a-z0-9_]+)",\s*CHIME_EVENT_', text))
    return keys


def cron_rules_valid(rules):
    if not isinstance(rules, list) or not (1 <= len(rules) <= 7):
        return False
    for rule in rules:
        if not isinstance(rule, str) or len(rule.split()) != 3:
            return False
    return True


class ExampleConfigFiles(unittest.TestCase):
    def test_files_exist(self):
        for name in CONFIG_FILES:
            self.assertTrue((EXAMPLE_DIR / name).is_file(), name)

    def test_only_top_level_key_is_config(self):
        for name in CONFIG_FILES:
            self.assertEqual(set(load(name).keys()), {"config"}, name)

    def test_keys_are_accepted_by_the_firmware(self):
        accepted = accepted_config_keys()
        # Sanity check on the scrape itself: main/utils.c is known to define
        # well over 100 such keys (grep count at the time of writing: 121) -
        # a much smaller number would mean the regex stopped matching, e.g.
        # after a refactor of apply_config_from_json(), and every "key is
        # accepted" assertion below would then pass for the wrong reason.
        self.assertGreater(len(accepted), 50)
        for name in CONFIG_FILES:
            for key in load(name)["config"]:
                self.assertIn(
                    key,
                    accepted,
                    f"{name}: {key!r} is not read by apply_config_from_json()",
                )

    def test_no_forbidden_or_telegram_keys(self):
        for name in CONFIG_FILES:
            keys = set(load(name)["config"])
            leaked = keys & FORBIDDEN_KEYS
            self.assertFalse(leaked, f"{name}: forbidden key(s) {leaked}")
            telegram_keys = {k for k in keys if k.startswith("telegram_")}
            self.assertFalse(telegram_keys, f"{name}: {telegram_keys}")

    def test_no_alarm_configured(self):
        # D4 (2026-09-28): the demo intentionally leaves the alarm alone -
        # it must neither arm one nor clear one the user already has.
        for name in CONFIG_FILES:
            alarm_keys = {k for k in load(name)["config"] if k.startswith("alarm_")}
            self.assertFalse(alarm_keys, f"{name}: {alarm_keys}")

    def test_no_processing_palette_or_albums(self):
        for name in CONFIG_FILES:
            data = load(name)
            for section in ("processing", "palette", "albums"):
                self.assertNotIn(section, data, f"{name}: must not import {section!r}")

    def test_enum_values(self):
        for name in CONFIG_FILES:
            config = load(name)["config"]
            for key, allowed in ENUM_FIELDS.items():
                if key in config:
                    self.assertIn(
                        config[key], allowed, f"{name}: {key}={config[key]!r}"
                    )

    def test_cron_rules(self):
        for name in CONFIG_FILES:
            config = load(name)["config"]
            for key in ("rotate_cron", "agenda_cron"):
                if key in config:
                    self.assertTrue(
                        cron_rules_valid(config[key]), f"{name}: {key}={config[key]!r}"
                    )

    def test_quiet_hours_format(self):
        for name in CONFIG_FILES:
            config = load(name)["config"]
            for key in ("chime_quiet_start", "chime_quiet_end"):
                if key in config:
                    self.assertRegex(config[key], TIME_RE, f"{name}: {key}")

    def test_urls(self):
        for name in CONFIG_FILES:
            config = load(name)["config"]
            for key, value in config.items():
                if "_url" not in key:
                    continue
                self.assertLessEqual(
                    len(value), URL_MAX_LEN, f"{name}: {key} is too long"
                )
                self.assertRegex(value, URL_RE, f"{name}: {key}={value!r}")

    def test_site_urls_point_at_files_in_this_package(self):
        # deploy-pages copies examples/ to <site>/examples/ and then checks these
        # very URLs answer 200 - a renamed or removed file must fail here first.
        marker = "/examples/waveshare_photopainter_73/"
        seen = 0
        for name in CONFIG_FILES:
            for key, value in load(name)["config"].items():
                if "_url" in key and marker in value:
                    seen += 1
                    relative = value.split(marker, 1)[1]
                    self.assertTrue(
                        (EXAMPLE_DIR / relative).is_file(),
                        f"{name}: {key} points at {relative!r}, not in the package",
                    )
        self.assertGreaterEqual(seen, 6)

    def test_field_lengths_fit_the_device_buffer(self):
        defines = config_h_defines()
        for name in CONFIG_FILES:
            config = load(name)["config"]
            for key, define_name in FIELD_MAX_LEN_DEFINE.items():
                if key not in config:
                    continue
                self.assertIn(define_name, defines, define_name)
                max_len = defines[define_name] - 1  # buffer includes the NUL
                self.assertLessEqual(
                    len(config[key]),
                    max_len,
                    f"{name}: {key}={config[key]!r} would be silently truncated "
                    f"to {max_len} characters on the device",
                )

    def test_no_leaked_private_data(self):
        paths = [EXAMPLE_DIR / n for n in CONFIG_FILES]
        paths += sorted(
            p for p in EXAMPLE_DIR.rglob("*") if p.is_file() and p.suffix != ".png"
        )
        seen = set()
        for path in paths:
            if path in seen:
                continue
            seen.add(path)
            text = path.read_text(encoding="utf-8", errors="replace")
            for pattern in PRIVATE_DATA_PATTERNS:
                match = pattern.search(text)
                self.assertIsNone(
                    match,
                    f"{path.name}: matched {pattern.pattern!r} ({match and match.group()!r})",
                )
            emails = real_looking_emails(text)
            self.assertFalse(
                emails, f"{path.name}: real-looking email address(es) {emails}"
            )


def firmware_profile_color_keys():
    """The colors the parser requires (its COLOR_KEYS[16] table) and the color
    names parse_color_value() accepts - scraped, so a change in the firmware
    breaks this test instead of leaving an example profile the device rejects."""
    text = AGENDA_COLOR_PROFILE_C.read_text(encoding="utf-8")
    table = re.search(r"COLOR_KEYS\[16\]\s*=\s*\{(.*?)\};", text, re.DOTALL).group(1)
    keys = re.findall(r'"([A-Za-z]+)"', table)
    names = set(re.findall(r'strcmp\(s,\s*"([a-z]+)"\)\s*==\s*0', text))
    return keys, names


class ExampleColorProfiles(unittest.TestCase):
    """The three Calendar color profiles: each must be a document the device
    would import into a slot (Settings -> Agenda -> Calendar color profiles)."""

    def profile_paths(self):
        return sorted(COLOR_PROFILE_DIR.glob("*.json"))

    def test_scraped_firmware_rules_are_sane(self):
        keys, names = firmware_profile_color_keys()
        self.assertEqual(len(keys), 16, keys)
        self.assertIn("text", keys)
        self.assertEqual(names, {"black", "white", "red", "yellow", "blue", "green"})

    def test_no_more_profiles_than_slots(self):
        slots = config_h_defines()["AGENDA_COLOR_PROFILE_SLOTS"]
        paths = self.profile_paths()
        self.assertGreaterEqual(len(paths), 1)
        self.assertLessEqual(len(paths), slots)

    def test_each_profile_is_importable(self):
        keys, names = firmware_profile_color_keys()
        max_bytes = config_h_defines()["AGENDA_COLOR_PROFILE_MAX_BYTES"]
        hex_re = re.compile(r"^#[0-9a-fA-F]{6}$")
        for path in self.profile_paths():
            raw = path.read_bytes()
            self.assertLessEqual(len(raw), max_bytes, f"{path.name}: too large")
            doc = json.loads(raw.decode("utf-8"))
            self.assertEqual(doc.get("type"), "spectra6-firmware-profile", path.name)
            self.assertIsInstance(doc.get("name"), str, path.name)
            self.assertTrue(doc["name"].strip(), f"{path.name}: empty name")
            colors = doc.get("colors")
            self.assertIsInstance(colors, dict, f"{path.name}: no colors object")
            for key in keys:
                value = colors.get(key)
                self.assertIsInstance(value, str, f"{path.name}: colors.{key} missing")
                self.assertTrue(
                    value in names or hex_re.match(value),
                    f"{path.name}: colors.{key}={value!r} is not a color the firmware accepts",
                )

    def test_profiles_differ(self):
        docs = [json.loads(p.read_text(encoding="utf-8")) for p in self.profile_paths()]
        names = [d["name"] for d in docs]
        self.assertEqual(len(set(names)), len(names), "profile names must differ")


if __name__ == "__main__":
    unittest.main()
