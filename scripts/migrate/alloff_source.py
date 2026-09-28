#!/usr/bin/env python3
"""Source-level proof that "no feature" is the upstream code.

Resolves the FEATURE_*/FORK_* guards of every source file that also exists in the
upstream baseline (the root commit of this repository) with every flag off - so the
`#else` branches stay - and compares the result with the upstream file, ignoring
blank lines and whitespace. Files whose difference is intended (build lists, the
board capability macros, the OTA repository option) are listed in EXPECTED.

A complement to scripts/verify_baseline.py, which proves the same for a build (Kconfig,
ELF symbols, image size) but needs the toolchain; this runs in a second, everywhere.

    alloff_source.py
"""

import difflib
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from xref import split_lines  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
SUFFIXES = (".c", ".h", ".cpp", ".hpp", ".tpp", ".txt", ".cmake")
SKIPPED_PREFIXES = ("managed_components", "webapp")

# Intended differences: what the flags need in order to exist at all.
EXPECTED = {
    "main/CMakeLists.txt": "source lists and embedded files of the features",
    "components/board_hal/CMakeLists.txt": "audio HAL source, optional requirements",
    "components/board_hal/include/board_hal.h": "capability macros, audio declarations",
    "components/board_hal/include/board_waveshare_photopainter_73.h": "capability macros",
    "main/config.h": "OTA feed built from the FORK_OTA_REPO option (upstream by default)",
    "host_tests/CMakeLists.txt": "fork tests",
    # clang-format 18 writes the one-line initializer, upstream's file predates it
    "main/image_processor.c": "GRAY() macro layout",
}


def git(*args):
    return subprocess.run(
        ["git", "-c", "core.autocrlf=false", *args],
        cwd=ROOT,
        check=True,
        capture_output=True,
    ).stdout.decode("utf-8", errors="replace")


def normalise(text):
    lines = (re.sub(r"\s+", " ", line).strip() for line in text.splitlines())
    return [line for line in lines if line]


# The newest upstream commit merged into this repository (7ccabe0, after v2.18.0-27
# which the history is grafted onto): move it with every upstream merge, see
# docs/FEATURE_FLAGS_PLAN.md section 9. alloff_web.py uses it too.
BASELINE = "7ccabe0eb2f47c6b94d1089b94c404865486cd82"


def main():
    baseline = BASELINE
    unexpected, unused = [], set(EXPECTED)
    checked = 0
    for name in git("ls-tree", "-r", "--name-only", baseline).splitlines():
        if not name.endswith(SUFFIXES) or name.startswith(SKIPPED_PREFIXES):
            continue
        path = ROOT / name
        if not path.is_file():
            unexpected.append((name, ["file is missing"]))
            continue
        active, _ = split_lines(
            path.read_text(encoding="utf-8").splitlines(True), set()
        )
        resolved = "".join(active).replace('#include "feature_config.h"\n', "")
        checked += 1
        upstream = normalise(git("show", f"{baseline}:{name}"))
        ours = normalise(resolved)
        if upstream == ours:
            continue
        if name in EXPECTED:
            unused.discard(name)
            continue
        diff = [
            line
            for line in difflib.unified_diff(upstream, ours, lineterm="", n=0)
            if not line.startswith(("---", "+++", "@@"))
        ]
        unexpected.append((name, diff))

    for name, diff in unexpected:
        print(f"DIFFERS from upstream with every feature off: {name}")
        for line in diff[:10]:
            print(f"    {line[:120]}")
    for name in sorted(unused):
        print(f"note: {name} is listed as an expected difference but is identical now")
    print(
        f"{checked} files checked against {baseline[:7]}, "
        f"{len(unexpected)} unexpected difference(s)"
    )
    return 1 if unexpected else 0


if __name__ == "__main__":
    sys.exit(main())
