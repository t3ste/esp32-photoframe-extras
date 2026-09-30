#!/usr/bin/env python3
"""Static cross-reference check for the feature guards (migration aid).

For a feature set it resolves the `#if FEATURE_X` guards of every source file that
the set compiles, and reports identifiers that are defined *only* in guarded-out
regions but used in code that stays: the compile errors a build of that set would
show, found in seconds instead of minutes.

    xref.py                     check every single feature, `fixes`, `all` and `off`
    xref.py telegram agenda     check these sets (a set is a feature or a+b)
"""

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from gate import GATE_NAME, evaluate  # noqa: E402

import features as feature_registry  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]

# sources of the components that are always built (main/*.c minus feature modules)
MODULES = {
    "telegram": ["telegram_bot"],
    "overlays": ["overlay_manager", "headlines"],
    "agenda": [
        "agenda_color_profile",
        "agenda_manager",
        "agenda_renderer",
        "calendar_ics",
        "todo",
    ],
    "chimes": ["chime"],
    "climate": ["climate_history"],
    "alarmclock": ["alarm_manager", "alarm_pattern", "alarm_setting_ui"],
    "voice-stop": ["kws", "kws_service", "mic_detect", "mic_level", "mic_monitor"],
    "battery-history": ["battery_history"],
    "display-history": ["history_manager"],
    "https": ["https_cert"],
    "facecrop": ["facecrop_metadata"],
    "source-auth": ["source_auth"],
    "caldav": ["caldav", "caldav_fetch"],
    "caldav-todo": ["vtodo"],
    "glyphs": ["glyph_extras"],
    "info-screens": ["screen_canvas", "info_screens_core", "info_screens"],
    "chore-wheel": ["screen_chore_wheel"],
    "upload-dedup": ["dedup", "dedup_payload", "dedup_service"],
}
ALWAYS_HELPERS = {
    "http_fetch": "FORK_HTTP_FETCH",
    "exif_reader": "FORK_EXIF",
    "weather": "FORK_WEATHER",
    "climate": "FORK_CLIMATE_CORE",
}
SELECTS = {
    "FORK_HTTP_FETCH": ["overlays", "agenda", "fixes"],
    "FORK_EXIF": ["telegram", "overlays"],
    "FORK_WEATHER": ["overlays", "agenda"],
    "FORK_CLIMATE_CORE": ["climate", "overlays", "agenda"],
    "FORK_IMAGE_PIPELINE": [
        "telegram",
        "overlays",
        "agenda",
        "climate",
        "error-banner",
        "facecrop",
        "glyphs",
    ],
    "FORK_AUDIO_HAL": ["chimes", "alarmclock"],
}

KEYWORDS = set(
    "if else for while do switch case default return break continue goto sizeof "
    "static const extern inline volatile register typedef struct union enum void "
    "char short int long float double signed unsigned bool true false NULL "
    "uint8_t uint16_t uint32_t uint64_t int8_t int16_t int32_t int64_t size_t "
    "esp_err_t ESP_OK ESP_FAIL defined include define ifdef ifndef endif elif "
    "undef pragma once TAG".split()
)
IDENT = re.compile(r"(?<!\w)[A-Za-z_][A-Za-z0-9_]*")


COMMENTS_AND_LITERALS = re.compile(
    r"""("(?:\\.|[^"\\\n])*")|('(?:\\.|[^'\\\n])*')|/\*.*?\*/|//[^\n]*""", re.S
)


def strip_code(text):
    """Drop comments and blank string/char literals in one left-to-right pass, so
    a `/*` inside a string ("image/*") is not taken for a comment start."""

    def replace(match):
        if match.group(1) is not None:
            return '""'
        if match.group(2) is not None:
            return "''"
        return ""

    return COMMENTS_AND_LITERALS.sub(replace, text)


def flags_for(feature_names):
    flags = set()
    for name in feature_names:
        flags.add(feature_registry.FEATURES_BY_NAME[name].symbol)
    if "voice-stop" in feature_names:
        flags.add("FEATURE_ALARMCLOCK")
    enabled = {n for n in feature_names}
    if "voice-stop" in enabled:
        enabled.add("alarmclock")
    for symbol, users in SELECTS.items():
        if enabled & set(users):
            flags.add(symbol)
    if flags:
        flags.add("FORK_ANY")
    return flags


def split_lines(lines, flags):
    """(active, inactive) lines for `flags`; only guards made of FEATURE_/FORK_ names
    are resolved, everything else is kept active."""
    import gate

    def value(expression):
        text = re.sub(
            rf"\b{GATE_NAME}\b",
            lambda m: "1" if m.group(0).replace("CONFIG_", "", 1) in flags else "0",
            expression.replace("BOARD_HAL_VOICE_ENABLED", "FEATURE_VOICE_STOP"),
        )
        text = re.sub(r"defined\(\s*(\d)\s*\)", r"\1", text)
        text = text.replace("||", " or ").replace("&&", " and ")
        text = re.sub(r"!(?!=)", " not ", text)
        return bool(eval(text, {"__builtins__": {}}, {}))

    active, inactive, stack = [], [], []
    for line in lines:
        stripped = line.strip().replace("BOARD_HAL_VOICE_ENABLED", "FEATURE_VOICE_STOP")
        match = re.match(r"#if (.*)$", stripped)
        expression = (
            re.sub(r"\s*(//.*|/[*].*[*]/)\s*$", "", match.group(1)) if match else ""
        )
        if match and gate.is_gate(expression):
            stack.append(["gate", value(expression)])
            continue
        if stack and stack[-1][0] == "gate" and re.match(r"#else\b", stripped):
            stack[-1][1] = not stack[-1][1]
            continue
        if re.match(r"#if", stripped):
            stack.append(["other", True])
        if re.match(r"#endif\b", stripped) and stack:
            kind, _ = stack.pop()
            if kind == "gate":
                continue
        (active if all(v for k, v in stack if k == "gate") else inactive).append(line)
    return active, inactive


def definitions(text, in_header):
    """Identifiers a chunk of active code defines, with file-local ones flagged."""
    local, shared = set(), set()
    for match in re.finditer(r"^#define\s+([A-Za-z_]\w*)", text, re.M):
        (shared if in_header else local).add(match.group(1))
    for match in re.finditer(
        r"^(?!#)([A-Za-z_][^\n;{}=]*?)\b([A-Za-z_]\w*)\s*\(", text, re.M
    ):
        prefix = match.group(1)
        if "static" in prefix.split():
            local.add(match.group(2))
        else:
            shared.add(match.group(2))
    for match in re.finditer(
        r"^static\s+(?:const\s+)?[\w \*]*?\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*(?:=[^=]|;)",
        text,
        re.M,
    ):
        local.add(match.group(1))
    for match in re.finditer(r"\}\s*([A-Za-z_]\w*)\s*;", text):
        shared.add(match.group(1))
    for match in re.finditer(r"typedef\s+[^;{]*?\b([A-Za-z_]\w*)\s*;", text):
        shared.add(match.group(1))
    for block in re.finditer(r"enum\s*[A-Za-z_]*\s*\{(.*?)\}", text, re.S):
        for member in re.finditer(r"([A-Za-z_]\w*)\s*(?:=|,|$)", block.group(1)):
            shared.add(member.group(1))
    for block in re.finditer(r"struct\s+([A-Za-z_]\w*)\s*\{", text):
        shared.add(block.group(1))
    return local, shared


def sources_for(feature_names):
    names = set(feature_names)
    if "voice-stop" in names:
        names.add("alarmclock")
    flags = flags_for(feature_names)
    main = ROOT / "main"
    module_files = {m for f in MODULES.values() for m in f} | set(ALWAYS_HELPERS)
    files = [p for p in sorted(main.glob("*.c")) if p.stem not in module_files]
    for feature, stems in MODULES.items():
        if feature in names:
            files += [main / f"{s}.c" for s in stems]
    for stem, flag in ALWAYS_HELPERS.items():
        if flag in flags:
            files.append(main / f"{stem}.c")
    headers = sorted(main.glob("*.h")) + sorted((ROOT / "components").rglob("*.h"))
    comp = [ROOT / "components/board_hal/src/driver_waveshare_photopainter_73.c"]
    if "FORK_AUDIO_HAL" in flags:
        comp.append(ROOT / "components/board_hal/src/audio_chime.c")
    return flags, files + comp, headers


def strip_macro_arguments(text, macros):
    """Blank the arguments of calls to function-like macros: the no-op stubs never
    compile them, so names that only appear there need not exist."""
    out, i = [], 0
    pattern = (
        re.compile(r"\b(" + "|".join(map(re.escape, sorted(macros))) + r")\s*\(")
        if macros
        else None
    )
    while pattern:
        match = pattern.search(text, i)
        if not match:
            break
        out.append(text[i : match.end()])
        depth, j = 1, match.end()
        while j < len(text) and depth:
            depth += {"(": 1, ")": -1}.get(text[j], 0)
            j += 1
        out.append(")")
        i = j
    out.append(text[i:])
    return "".join(out)


NOT_A_TYPE = {"return", "else", "goto", "case", "sizeof", "do", "typedef", "defined"}
DECLARATION = re.compile(
    r"(?<![\w.>])([A-Za-z_]\w*)[ \t]+(?:const[ \t]+)?\**[ \t]*([A-Za-z_]\w*)[ \t]*(?:=|;|\[|,|\))"
)


def declared_names(text):
    """Names a file declares itself, indented locals included ("type name = ...")."""
    return {
        match.group(2)
        for match in DECLARATION.finditer(text)
        if match.group(1) not in NOT_A_TYPE
    }


def include_closure(path, active_raw, headers_by_name):
    """Project headers `path` reaches through includes that are active for the set."""
    seen, stack = set(), [path]
    while stack:
        current = stack.pop()
        for name in re.findall(r'#include\s+"([^"]+)"', active_raw.get(current, "")):
            header = headers_by_name.get(name)
            if header and header not in seen:
                seen.add(header)
                stack.append(header)
    return seen


def with_requirements(feature_names):
    """The set as build.py builds it: a feature brings the features it requires."""
    names = list(feature_names)
    pending = list(names)
    while pending:
        for dependency in feature_registry.FEATURES_BY_NAME[pending.pop()].requires:
            if dependency not in names:
                names.append(dependency)
                pending.append(dependency)
    return names


def check(feature_names):
    feature_names = with_requirements(feature_names)
    flags, sources, headers = sources_for(feature_names)
    active_text, active_raw, file_defs, inactive_defs = {}, {}, {}, set()
    for path in sources + headers:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines(True)
        active, inactive = split_lines(lines, flags)
        active_raw[path] = "".join(active)
        active_text[path] = strip_code(active_raw[path])
        loc, sha = definitions(active_text[path], path.suffix == ".h")
        file_defs[path] = loc | sha
        iloc, isha = definitions(strip_code("".join(inactive)), path.suffix == ".h")
        inactive_defs |= iloc | isha
    macros = set()
    for path in headers + sources:
        macros |= set(re.findall(r"^#define\s+(\w+)\(", active_text[path], re.M))
    headers_by_name = {h.name: h for h in headers}
    # a name is a candidate for an error when the project defines it somewhere (a
    # guarded-out region or a header) - a file must then see it through its own
    # active definitions or an active include chain
    candidates = set(inactive_defs)
    for header in headers:
        candidates |= file_defs[header]
    problems = {}
    for path in sources:
        text = strip_macro_arguments(active_text[path], macros)
        provided = set(file_defs[path]) | declared_names(active_text[path])
        for header in include_closure(path, active_raw, headers_by_name):
            provided |= file_defs[header]
        for token in set(IDENT.findall(text)):
            if token in KEYWORDS or token in provided or token not in candidates:
                continue
            problems.setdefault(path.name, set()).add(token)
    # link level: functions declared by the header of a module that is not built
    # but called from code that is (unless a stub macro/inline function covers them)
    proto = re.compile(
        r"^(?!#|static|typedef)[A-Za-z_][\w \*]*?\b(\w+)\s*\((?:[^;{}()]|\([^()]*\))*\)\s*;",
        re.M,
    )
    built = {s.stem for s in sources}
    inline = re.compile(r"^static\s+inline[^\n]*?\b(\w+)\s*\(", re.M)
    for header in headers:
        if header.parent.name != "main" or header.stem in built:
            continue
        text = active_text[header]
        stubs = macros | set(inline.findall(text))
        declared = set(proto.findall(text)) - stubs
        if not declared:
            continue
        for path in sources:
            body = strip_macro_arguments(active_text[path], macros)
            for token in set(IDENT.findall(body)) & declared:
                problems.setdefault(path.name, set()).add("LINK:" + token)
    return problems


def main():
    args = sys.argv[1:]
    sets = []
    if args:
        sets = [(a, [] if a == "off" else a.split("+")) for a in args]
    else:
        sets = [("off", [])] + [(f.name, [f.name]) for f in feature_registry.FEATURES]
        sets.append(("all", [f.name for f in feature_registry.FEATURES]))
    failed = 0
    for name, names in sets:
        problems = check([n for n in names if n])
        if not problems:
            print(f"{name:<18} ok")
            continue
        failed += 1
        print(f"{name:<18} {sum(len(v) for v in problems.values())} unresolved")
        for file, tokens in sorted(problems.items()):
            print(f"    {file}: {' '.join(sorted(tokens))[:220]}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
