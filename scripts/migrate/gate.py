#!/usr/bin/env python3
"""Turn the difference between upstream and the old fork into feature-gated source.

One-off migration aid used to build this fork's feature-gated files from the old fork's
diff against upstream (see docs/MAINTAINING.md section 5 - never re-run `apply` on a file
that was later hand-edited). For one file it

  1. diffs the upstream baseline (--base, default: the root commit) against the
     old fork's version (--fork, default: branch fork-import) into hunks,
  2. proposes the feature each hunk belongs to (keyword vote; overridable in a
     map file),
  3. writes the file so that every hunk is `#if <tag> fork-lines #else
     upstream-lines #endif`, and proves that the result is the fork file with
     all tags true and the upstream file with all tags false.

Map file (one line per override, `#` starts a comment):

    <hunk>            <tag>              # whole hunk
    <hunk>.<block>    <tag>              # one top-level block of a hunk

where <tag> is a preprocessor expression over FEATURE_*/FORK_* names, `drop`
(keep the upstream text: the fork only reverted upstream code, or the change is
not wanted) or `always` (take the fork text unconditionally).

    gate.py analyze FILE [--map MAP]   list hunks and proposed tags
    gate.py apply FILE [--map MAP]     write the gated file
"""

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Keyword vote per feature. A hunk goes to the feature with the most matching
# lines; a hunk without any match has no proposal and needs a map entry.
KEYWORDS = {
    "FEATURE_TELEGRAM": r"telegram|tg_bot|orientation_pair|rotation_pairing|rot_pairing",
    "FEATURE_OVERLAYS": r"overlay|weather|headline|\brss\b|caption|lowbatt|low_battery|"
    r"forecast|glyph|wttr|open-meteo|yr\.no",
    "FEATURE_AGENDA": r"agenda|todo|calendar|\bics\b|rrule|color_profile|\bcal_",
    "FEATURE_CHIMES": r"chime|\bbeep",
    "FEATURE_CLIMATE": r"climate|humidity|shtc|sht[34]|temperature",
    "FEATURE_ALARMCLOCK": r"alarm",
    "FEATURE_VOICE_STOP": r"kws|voice|mic_|microphone|keyword",
    "FEATURE_BATTERY_HISTORY": r"battery_history|batt_hist|batteryhistory",
    "FEATURE_DISPLAY_HISTORY": r"history_manager|display_history|no_repeat|displayhistory|"
    r"historyreset|/api/history|resethistory",
    "FEATURE_HTTPS": r"https_|https_cert|esp_https_server|httpd_ssl|httpsenabled|enable https",
    "FEATURE_OFFLINE_HOTSPOT": r"hotspot|offline_mode|offlinemode",
    "FEATURE_ERROR_BANNER": r"error_overlay|internet_health|record_internet|error_banner|"
    r"erroroverlay|error overlay",
    "FEATURE_WIFI_RESILIENCE": r"wifi_?(perf|ext|reprov|tx_?power|coldboot)|"
    r"wifi(performance|extended|reprovision|txpower)",
    "FEATURE_OTA_CHANNEL": r"ota_channel|prerelease|variant_switch|ota_set_options|"
    r"ota_get_options|ota_options|otacheck|ota_check",
    "FORK_EXIF": r"exif",
    "FEATURE_FACECROP": r"facecrop|face_crop|crop_variant|organize_crop|"
    r"resolve_display_variant|render_variant|\bcover\b",
}


class Hunk:
    """One changed region: base lines replaced by fork lines (either may be empty)."""

    def __init__(self, index, base, fork):
        self.index = index
        self.base = base
        self.fork = fork
        self.blocks = split_blocks(fork) if fork else [fork]

    @property
    def kind(self):
        if not self.base:
            return "insert"
        return "delete" if not self.fork else "replace"


def split_blocks(lines):
    """Split inserted lines into top-level blocks: chunks separated by a blank
    line where the running brace depth is zero (whole functions, defines...)."""
    blocks, current, depth = [], [], 0
    for line in lines:
        current.append(line)
        code = re.sub(r'"(\\.|[^"\\])*"|//.*', "", line)
        depth += code.count("{") - code.count("}")
        if depth <= 0 and not line.strip() and len(current) > 1:
            blocks.append(current)
            current, depth = [], 0
    if current:
        blocks.append(current)
    return blocks


def git_show(ref, path):
    result = subprocess.run(
        ["git", "show", f"{ref}:{path}"], cwd=ROOT, capture_output=True
    )
    return result.stdout.decode("utf-8") if result.returncode == 0 else None


def diff_hunks(base_lines, fork_lines):
    with tempfile.TemporaryDirectory() as tmp:
        a, b = Path(tmp, "a"), Path(tmp, "b")
        a.write_bytes("".join(base_lines).encode())
        b.write_bytes("".join(fork_lines).encode())
        out = subprocess.run(
            ["git", "diff", "--no-index", "--histogram", "-U0", str(a), str(b)],
            capture_output=True,
        ).stdout.decode("utf-8")
    hunks = []
    for match in re.finditer(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@", out, re.M):
        a_start, a_len, b_start, b_len = match.groups()
        a_start, b_start = int(a_start), int(b_start)
        a_len = 1 if a_len is None else int(a_len)
        b_len = 1 if b_len is None else int(b_len)
        # -U0: a side with length 0 is positioned after the given line
        a_from = a_start - 1 if a_len else a_start
        b_from = b_start - 1 if b_len else b_start
        hunks.append((a_from, a_from + a_len, b_from, b_from + b_len))
    return hunks


VOTE = "majority"


def propose(lines):
    if VOTE == "first":
        for line in lines:
            best, count = None, 0
            for tag, pattern in KEYWORDS.items():
                n = len(re.findall(pattern, line.lower()))
                if n > count:
                    best, count = tag, n
            if best:
                return best, {best: count}
        return None, {}
    votes = {}
    text = [l.lower() for l in lines]
    for tag, pattern in KEYWORDS.items():
        count = sum(1 for l in text if re.search(pattern, l))
        if count:
            votes[tag] = count
    if not votes:
        return None, votes
    best = max(votes, key=votes.get)
    return best, votes


def load_map(path):
    overrides = {}
    if path and Path(path).is_file():
        for raw in Path(path).read_text(encoding="utf-8").splitlines():
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            key, tag = line.split(None, 1)
            overrides[key] = tag.strip()
    return overrides


def build(base_lines, fork_lines, overrides, default=None, remap=None):
    """Return (hunk list, per-hunk tags list-of-(block_lines, tag))."""
    raw = diff_hunks(base_lines, fork_lines)
    hunks = [
        Hunk(i, base_lines[a1:a2], fork_lines[b1:b2])
        for i, (a1, a2, b1, b2) in enumerate(raw)
    ]
    plan = []
    for hunk in hunks:
        blocks = []
        whole = overrides.get(str(hunk.index))
        for n, block in enumerate(hunk.blocks):
            tag = overrides.get(f"{hunk.index}.{n}", whole)
            proposed = None
            if tag is None:
                proposed, _ = propose(block if hunk.kind != "delete" else hunk.base)
                tag = (remap or {}).get(proposed, proposed) if proposed else default
            # HUNK.BLOCK:FIRST-LAST (HUNK:FIRST-LAST is short for block 0)
            names = f"{hunk.index}[.]{n}" + (f"|{hunk.index}" if n == 0 else "")
            ranges = sorted(
                (int(m.group(1)), int(m.group(2)), value)
                for key, value in overrides.items()
                for m in [re.fullmatch(rf"(?:{names}):(\d+)-(\d+)", key)]
                if m
            )
            last_of_replace = hunk.kind == "replace" and n == len(hunk.blocks) - 1
            if not ranges or hunk.kind == "delete":
                blocks.append((block, tag, proposed is not None))
                continue
            # line-range overrides split one block into segments with own tags
            position = 1
            for first, last, value in ranges:
                if first > position:
                    blocks.append((block[position - 1 : first - 1], tag, False))
                blocks.append((block[first - 1 : last], value, False))
                position = last + 1
            if position <= len(block):
                blocks.append((block[position - 1 :], tag, False))
            elif last_of_replace:
                # ranges cover the whole block: an empty part with the block's own
                # tag still has to stand in for the upstream lines
                blocks.append(([], tag, False))
        plan.append((hunk, blocks))
    return raw, plan


def normalize_fork(text):
    """The old fork gated the alarm clock with CONFIG_ALARM_CLOCK_ENABLED; the
    same switch is FEATURE_ALARMCLOCK here."""
    text = re.sub(
        r"#ifdef\s+CONFIG_ALARM_CLOCK_ENABLED", "#if FEATURE_ALARMCLOCK", text
    )
    text = re.sub(
        r"#ifndef\s+CONFIG_ALARM_CLOCK_ENABLED", "#if !FEATURE_ALARMCLOCK", text
    )
    text = re.sub(
        r"defined\(\s*CONFIG_ALARM_CLOCK_ENABLED\s*\)", "FEATURE_ALARMCLOCK", text
    )
    return re.sub(r"\bCONFIG_ALARM_CLOCK_ENABLED\b", "FEATURE_ALARMCLOCK", text)


def strip_wrapper(block, tag):
    """Drop a `#if <tag>` ... `#endif` pair that wraps the whole block anyway."""
    first = next((i for i, l in enumerate(block) if l.strip()), None)
    last = next((i for i in range(len(block) - 1, -1, -1) if block[i].strip()), None)
    if first is None or block[first].strip() != f"#if {tag}":
        return block
    if not re.match(r"#endif\b", block[last].strip()):
        return block
    depth = 0
    for line in block[first + 1 : last]:
        stripped = line.strip()
        if stripped.startswith("#if"):
            depth += 1
        elif stripped.startswith("#endif"):
            depth -= 1
            if depth < 0:
                return block
        elif depth == 0 and (stripped == "#else" or stripped.startswith("#elif")):
            return block
    return block[:first] + block[first + 1 : last] + block[last + 1 :]


def merge_adjacent(blocks):
    """Join neighbouring insert blocks that carry the same tag."""
    merged = []
    for block, tag, auto in blocks:
        if merged and merged[-1][1] == tag:
            merged[-1] = (merged[-1][0] + block, tag, merged[-1][2] and auto)
        else:
            merged.append((list(block), tag, auto))
    return merged


def compose(base_lines, raw, plan):
    out, position = [], 0
    for (a1, a2, b1, b2), (hunk, blocks) in zip(raw, plan):
        out.extend(base_lines[position:a1])
        position = a2
        if hunk.kind == "replace":
            merged = merge_adjacent(blocks)
            for block, tag, _ in merged[
                :-1
            ]:  # extra lines in front of the replaced part
                if tag == "drop":
                    continue
                if tag == "always":
                    out.extend(block)
                    continue
                out.append(f"#if {tag}\n")
                out.extend(strip_wrapper(block, tag))
                out.append("#endif\n")
            block, tag, _ = merged[-1]  # this part stands in for the upstream lines
            if tag == "drop":
                out.extend(hunk.base)
            elif tag == "always":
                out.extend(block)
            else:
                out.append(f"#if {tag}\n")
                out.extend(strip_wrapper(block, tag))
                out.append("#else\n")
                out.extend(hunk.base)
                out.append("#endif\n")
        elif hunk.kind == "delete":
            tag = blocks[0][1]
            if tag == "drop":
                out.extend(hunk.base)
            elif tag != "always":
                out.append(f"#if !({tag})\n")
                out.extend(hunk.base)
                out.append("#endif\n")
        else:  # insert, possibly several blocks with their own tags
            for block, tag, _ in merge_adjacent(blocks):
                if tag == "drop":
                    continue
                if tag == "always":
                    out.extend(block)
                    continue
                out.append(f"#if {tag}\n")
                out.extend(strip_wrapper(block, tag))
                out.append("#endif\n")
    out.extend(base_lines[position:])
    return out


GATE_NAME = r"(?:CONFIG_)?(?:FEATURE|FORK)_[A-Z0-9_]+"
GATE_ONLY = re.compile(
    rf"^(?:defined\(\s*{GATE_NAME}\s*\)|{GATE_NAME}|[\s()!]|&&|[|][|]|\d)+$"
)


def is_gate(expression):
    """True if the #if expression only combines FEATURE_/FORK_ names."""
    return bool(re.search(GATE_NAME, expression)) and bool(GATE_ONLY.match(expression))


def truth_value(expression, value):
    """Evaluate a tag expression with every FEATURE_/FORK_ name set to 'value'."""
    text = re.sub(rf"defined\(\s*{GATE_NAME}\s*\)", "1" if value else "0", expression)
    text = re.sub(rf"\b{GATE_NAME}\b", "1" if value else "0", text)
    text = text.replace("||", " or ").replace("&&", " and ")
    text = re.sub(r"!(?!=)", " not ", text)
    return bool(eval(text, {"__builtins__": {}}, {}))


def evaluate(lines, value):
    """Resolve the #if/#else/#endif lines this tool added, with all tags = value."""
    out, stack = [], []
    for line in lines:
        stripped = line.strip()
        match = re.match(r"#if (.*)$", stripped)
        expression = (
            re.sub(r"\s*(//.*|/[*].*[*]/)\s*$", "", match.group(1)) if match else ""
        )
        if match and is_gate(expression):
            stack.append(["gate", truth_value(expression, value)])
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
        if all(v for k, v in stack if k == "gate"):
            out.append(line)
    return out


def expected_all_on(base_lines, raw, plan):
    """The fork file, except that dropped hunks keep the upstream text."""
    out, position = [], 0
    for (a1, a2, b1, b2), (hunk, blocks) in zip(raw, plan):
        out.extend(base_lines[position:a1])
        position = a2
        merged = merge_adjacent(blocks)
        for number, (block, tag, _) in enumerate(merged):
            last = number == len(merged) - 1
            if hunk.kind == "delete":
                if tag == "drop":
                    out.extend(hunk.base)
            elif hunk.kind == "replace" and last:
                out.extend(hunk.base if tag == "drop" else block)
            elif tag != "drop":
                out.extend(block)
    out.extend(base_lines[position:])
    return out


def expected_all_off(base_lines, raw, plan):
    """Upstream, except that hunks tagged `always` carry the fork text."""
    out, position = [], 0
    for (a1, a2, b1, b2), (hunk, blocks) in zip(raw, plan):
        out.extend(base_lines[position:a1])
        position = a2
        merged = merge_adjacent(blocks)
        for number, (block, tag, _) in enumerate(merged):
            last = number == len(merged) - 1
            if hunk.kind == "delete":
                if tag != "always":
                    out.extend(hunk.base)
            elif hunk.kind == "replace" and last:
                out.extend(block if tag == "always" else hunk.base)
            elif tag == "always":
                out.extend(block)
    out.extend(base_lines[position:])
    return out


def show_difference(expected, actual, what):
    import difflib

    print(f"check failed: {what}")
    diff = list(difflib.unified_diff(expected, actual, "expected", "actual", n=2))
    print("".join(diff[:60]), end="")


def check(base_lines, raw, plan, composed):
    """Prove: all tags off gives upstream, all tags on gives the fork (with the
    dropped hunks staying upstream)."""
    off = evaluate(composed, False)
    expected_off = evaluate(expected_all_off(base_lines, raw, plan), False)
    if off != expected_off:
        show_difference(expected_off, off, "all-off result differs from upstream")
        raise SystemExit(1)
    expected = evaluate(expected_all_on(base_lines, raw, plan), True)
    on = evaluate(composed, True)
    if on != expected:
        show_difference(expected, on, "all-on result differs from the fork file")
        raise SystemExit(1)


DIRECTIVE = re.compile(r"^(\s*)#(if\b.*|else\b.*|endif\b.*)$")
WRAPPED = re.compile(
    r"^(\s*)(?:<!--\s*|//\s*|/[*]\s*)#(if\b.*?|else\b.*?|endif\b.*?)\s*(?:-->|[*]/)?\s*$"
)


def wrap_directives(lines, flavor):
    """Turn `#if X` lines into comments a web file can carry: an HTML comment in a
    template or html file, // in scripts, /* */ in styles. The Vite plugin
    (webapp/feature-directives.js) removes them again at build time."""
    if flavor == "c":
        return lines
    out, region = [], "html" if flavor == "html" else "script"
    for line in lines:
        stripped = line.strip()
        if flavor == "vue":
            if stripped.startswith("<template") and not line.startswith(" "):
                region = "html"
            elif stripped.startswith("<script") and not line.startswith(" "):
                region = "script"
            elif stripped.startswith("<style") and not line.startswith(" "):
                region = "style"
        match = DIRECTIVE.match(line.rstrip("\n"))
        if match and (is_directive_text(match.group(2))):
            indent, body = match.group(1), match.group(2)
            if region == "html":
                line = f"{indent}<!-- #{body} -->\n"
            elif region == "style":
                line = f"{indent}/* #{body} */\n"
            else:
                line = f"{indent}// #{body}\n"
        out.append(line)
    return out


def is_directive_text(body):
    if body.startswith("if"):
        return is_gate(body[2:].strip())
    return body in ("else", "endif")


def unwrap_directives(lines):
    """Inverse of wrap_directives, for the equivalence check."""
    out = []
    for line in lines:
        match = WRAPPED.match(line.rstrip("\n"))
        if match:
            line = f"{match.group(1)}#{match.group(2)}\n"
        out.append(line)
    return out


def to_config_style(lines):
    """FEATURE_X -> defined(CONFIG_FEATURE_X) in the directives (for components,
    which cannot include main/feature_config.h)."""
    out = []
    for line in lines:
        match = re.match(r"(#if )(.*)$", line.rstrip("\n"))
        if match and re.search(GATE_NAME, match.group(2)) and "CONFIG_" not in line:
            expr = re.sub(rf"\b({GATE_NAME})\b", r"defined(CONFIG_\1)", match.group(2))
            line = f"#if {expr}\n"
        out.append(line)
    return out


def add_feature_include(lines):
    """Include feature_config.h in the include block that precedes the first gate."""
    if any('#include "feature_config.h"' in l for l in lines):
        return lines
    gate = re.compile(rf"#if .*{GATE_NAME}")
    first_gate = next((i for i, l in enumerate(lines) if gate.match(l)), len(lines))
    head = lines[:first_gate]
    line = '#include "feature_config.h"' + chr(10)
    quoted = next((i for i, l in enumerate(head) if l.startswith('#include "')), None)
    if quoted is not None:
        return lines[:quoted] + [line] + lines[quoted:]
    angled = [i for i, l in enumerate(head) if l.startswith("#include ")]
    if angled:
        at = angled[-1] + 1
        return lines[:at] + [chr(10), line] + lines[at:]
    return lines[:first_gate] + [line, chr(10)] + lines[first_gate:]


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("command", choices=["analyze", "apply", "show"])
    parser.add_argument(
        "--hunk", action="append", default=[], help="for show: HUNK or HUNK.BLOCK"
    )
    parser.add_argument("file", help="path relative to the repository root")
    parser.add_argument("--map")
    parser.add_argument("--default", help="tag for blocks without a keyword proposal")
    parser.add_argument("--vote", choices=["majority", "first"], default="majority")
    parser.add_argument(
        "--remap",
        help="FROM=TO,FROM=TO: replace proposed tags (e.g. FEATURE_OVERLAYS=FORK_IMAGE_PIPELINE)",
    )
    parser.add_argument(
        "--only",
        choices=["unresolved", "mixed", "cross"],
        help="analyze: list only these blocks",
    )
    parser.add_argument(
        "--style",
        choices=["macro", "config"],
        default="macro",
        help="macro: #if FEATURE_X (main/, needs feature_config.h); config: "
        "#if defined(CONFIG_FEATURE_X) (components/).",
    )
    parser.add_argument(
        "--flavor",
        choices=["auto", "c", "vue", "js", "html"],
        default="auto",
        help="comment style of the directives (auto: from the file extension)",
    )
    parser.add_argument("--base")
    parser.add_argument("--fork", default="fork-import")
    args = parser.parse_args()

    # 1347744 = aitjcize/esp32-photoframe @ v2.18.0-27, grafted as the parent
    # of this repository's own history.
    base_ref = args.base or "1347744414364110f96d9c121b4cc6e13b2364f2"
    base = git_show(base_ref, args.file)
    fork = git_show(args.fork, args.file)
    fork = normalize_fork(fork) if fork is not None else None
    if base is None or fork is None:
        sys.exit(f"{args.file} must exist in both {base_ref[:7]} and {args.fork}")
    base_lines, fork_lines = base.splitlines(True), fork.splitlines(True)
    global VOTE
    VOTE = args.vote
    overrides = load_map(args.map)
    remap = (
        dict(pair.split("=") for pair in args.remap.split(",")) if args.remap else None
    )
    raw, plan = build(base_lines, fork_lines, overrides, args.default, remap)

    if args.command == "show":
        for wanted in args.hunk:
            for hunk, blocks in plan:
                for n, (block, tag, auto) in enumerate(blocks):
                    label = f"{hunk.index}.{n}" if len(blocks) > 1 else f"{hunk.index}"
                    if wanted in (label, str(hunk.index)) and (
                        wanted == label or "." not in wanted
                    ):
                        print(f"--- {label} ({hunk.kind}, tag {tag})")
                        if hunk.base:
                            print("".join("- " + l for l in hunk.base), end="")
                        for number, line in enumerate(block, 1):
                            best, _ = propose([line])
                            mark = (best or "")[8:14]
                            print(f"{number:4} {mark:<6}+ {line}", end="")
        return 0

    if args.command == "analyze":
        unresolved = 0
        for hunk, blocks in plan:
            for n, (block, tag, auto) in enumerate(blocks):
                label = f"{hunk.index}.{n}" if len(blocks) > 1 else f"{hunk.index}"
                shown = tag if tag else "??"
                unresolved += tag is None
                size = f"-{len(hunk.base)}/+{len(block)}"
                first = next((l.strip() for l in block if l.strip()), "")[:90]
                _, votes = propose(block if hunk.kind != "delete" else hunk.base)
                mixed = len(votes) > 1
                if args.only == "cross":
                    if tag is None or not re.fullmatch(GATE_NAME, tag):
                        continue
                    crossing = []
                    for number, text in enumerate(block, 1):
                        other, _ = propose([text])
                        if other and other != tag:
                            crossing.append(f"{number}:{other[8:14]}")
                    if crossing:
                        print(f"{label:>6} {shown:<24} " + " ".join(crossing))
                    continue
                if args.only == "unresolved" and tag is not None:
                    continue
                if args.only == "mixed" and not (mixed and auto):
                    continue
                extra = (
                    "  votes=" + ",".join(f"{k[8:]}:{v}" for k, v in votes.items())
                    if mixed
                    else ""
                )
                print(f"{label:>6} {hunk.kind:<7} {size:<9} {shown:<26} {first}{extra}")
        print(f"\n{len(plan)} hunks, {unresolved} without a proposal")
        return 1 if unresolved else 0

    if any(tag is None for _, blocks in plan for _, tag, _ in blocks):
        sys.exit("unresolved hunks: run `analyze` and add them to the map")
    composed = compose(base_lines, raw, plan)
    check(base_lines, raw, plan, composed)
    flavor = args.flavor
    if flavor == "auto":
        flavor = {".vue": "vue", ".js": "js", ".html": "html"}.get(
            Path(args.file).suffix, "c"
        )
    if flavor != "c":
        composed = wrap_directives(composed, flavor)
    elif args.style == "config":
        composed = to_config_style(composed)
    else:
        composed = add_feature_include(composed)
    target = ROOT / args.file
    target.write_bytes("".join(composed).encode("utf-8"))
    print(f"wrote {args.file}: {len(plan)} hunks gated, checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
