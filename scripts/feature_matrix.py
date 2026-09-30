#!/usr/bin/env python3
"""Build several feature sets of one board side by side and report which compile.

Every set gets its own build directory (build-matrix/<board>/<name>) and therefore
its own sdkconfig, so the second run of a set is an incremental build. Run it in
an activated ESP-IDF shell, after `python build.py --board <b> --step splash` (the splash is
shared); every set builds the web app for its own feature set first, in `main/webapp`, so
sets run one after the other.

    python scripts/feature_matrix.py --board waveshare_photopainter_73 off fixes all
    python scripts/feature_matrix.py --board seeedstudio_xiao_ee02 single

Set names: `off` (nothing), `all` (--all-features), a feature name (that feature,
plus what it requires; a bundle name such as `extras` works too), `a+b` (a combination), `single` (every feature of the
board on its own, `off` and `all` included).
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

import features
from boards import SUPPORTED_BOARDS

ROOT = Path(__file__).resolve().parent.parent
COMPILE_TARGETS = ["__idf_main", "__idf_board_hal"]
ERROR_PATTERN = re.compile(r"(error:|undefined reference to|Error \d|FAILED:)")


def expand(names, board):
    """Turn set names into (name, build.py feature arguments)."""
    sets = []
    for name in names:
        if name == "single":
            sets.append(("off", []))
            for feature in features.FEATURES:
                if features.is_supported(feature.name, board):
                    sets.append((feature.name, ["--with", feature.name]))
            sets.append(("all", ["--all-features"]))
        elif name == "off":
            sets.append((name, []))
        elif name == "all":
            sets.append((name, ["--all-features"]))
        else:
            sets.append((name, ["--with", ",".join(name.split("+"))]))
    seen, unique = set(), []
    for name, args in sets:
        if name not in seen:
            seen.add(name)
            unique.append((name, args))
    return unique


def run_logged(command, log, mode="w"):
    with open(log, mode, encoding="utf-8") as out:
        result = subprocess.run(
            command, cwd=ROOT, stdout=out, stderr=subprocess.STDOUT, text=True
        )
    return result.returncode == 0


def build_set(board, name, feature_args, work, full, webapp):
    log = work / f"{name}.log"
    build_dir = str(work / name)
    base = [sys.executable, "build.py", "--board", board, "--build-dir", build_dir]
    print(f"### {name}: {' '.join(feature_args) or '(no features)'}", flush=True)
    # the firmware embeds the web app, whose content depends on the feature set
    ok = True
    if webapp != "skip":
        ok = run_logged(base + ["--step", "webapp", *feature_args], log)
    if webapp == "only":
        pass
    elif ok and full:
        ok = run_logged(base + ["--step", "firmware", *feature_args], log, "a")
    elif ok:
        # configure, then compile only the components the features touch
        ok = run_logged(base + ["--step", "configure", *feature_args], log, "a")
        if ok:
            cmake = ["cmake", "--build", build_dir, "--target"] + COMPILE_TARGETS
            ok = run_logged(cmake, log, "a")
    errors = []
    for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
        if ERROR_PATTERN.search(line) and line not in errors:
            errors.append(line.strip())
    return ok, errors[:12], log


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--board", choices=list(SUPPORTED_BOARDS), required=True)
    parser.add_argument("sets", nargs="+", help="set names, see the module docstring")
    parser.add_argument(
        "--full",
        action="store_true",
        help="Build the whole firmware (default: configure and compile main/ and board_hal only).",
    )
    parser.add_argument(
        "--webapp",
        choices=["yes", "skip", "only"],
        default="yes",
        help="Build the web app of each set first (yes, needs Node), leave it to the "
        "caller (skip: inside a container without Node) or do nothing else (only).",
    )
    args = parser.parse_args()

    work = ROOT / "build-matrix" / args.board
    work.mkdir(parents=True, exist_ok=True)
    failed = 0
    for name, feature_args in expand(args.sets, args.board):
        ok, errors, log = build_set(
            args.board, name, feature_args, work, args.full, args.webapp
        )
        print(f"    {'OK' if ok else 'FAILED'}  (log: {log.relative_to(ROOT)})")
        for line in errors if not ok else []:
            print(f"      {line[:200]}")
        failed += not ok
    print(f"\n{failed} set(s) failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
