#!/usr/bin/env python3
"""Compare a build of this tree against a reference build of the same board.

Acceptance check for the feature flags (docs/MAINTAINING.md section 6):
with no feature selected, the firmware must be the upstream firmware. The script
builds the reference (by default the upstream baseline this repository's history is
grafted onto) and the current working tree in scratch trees under .verify/<board>/
and compares

  a) the enabled Kconfig symbols of both sdkconfig files,
  b) the symbols of both ELF files (nm, sorted),
  c) the size of both .bin files.

The same script proves the opposite direction: pass --baseline <ref> to compare
against another commit, e.g. the old fork's import branch with --all-features.

Run it inside an activated ESP-IDF shell. Both trees embed the same web app and
splash screen (generated once in this checkout and copied), so the size
comparison is about the firmware code only. Arguments the script does not know
are passed to build.py of the candidate, e.g. `--with agenda` or `--all-features`.

The trees persist between runs: the reference is built once per (ref, args) and
the candidate is rebuilt incrementally (from scratch when a Kconfig or
sdkconfig.defaults input changed, or with --fullclean).
"""

import argparse
import filecmp
import hashlib
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

from boards import BOARD_TARGET, SUPPORTED_BOARDS
from features import ROOT

# Symbols that exist only because of the feature-flag infrastructure itself.
IGNORED_CONFIG_PREFIXES = ("FEATURE_", "FORK_")

NM_TOOL = {"esp32": "xtensa-esp32-elf-nm", "esp32s3": "xtensa-esp32s3-elf-nm"}


def parse_sdkconfig(text):
    """Map every set CONFIG_ symbol to its value ('y', a number or a string)."""
    values = {}
    for line in text.splitlines():
        match = re.match(r"CONFIG_(\w+)=(.*)$", line)
        if match:
            values[match.group(1)] = match.group(2)
    return values


def diff_sdkconfig(reference, candidate, ignored_prefixes=IGNORED_CONFIG_PREFIXES):
    """Return (only_reference, only_candidate, changed, ignored_count)."""
    ignored = 0
    only_reference, only_candidate, changed = {}, {}, {}
    for name in sorted(set(reference) | set(candidate)):
        if name.startswith(ignored_prefixes):
            ignored += 1
            continue
        if name not in candidate:
            only_reference[name] = reference[name]
        elif name not in reference:
            only_candidate[name] = candidate[name]
        elif reference[name] != candidate[name]:
            changed[name] = (reference[name], candidate[name])
    return only_reference, only_candidate, changed, ignored


def parse_nm(text):
    """Set of (type, name) from `nm` output; gcc's uniquifying suffixes removed."""
    symbols = set()
    for line in text.splitlines():
        parts = line.split()
        if len(parts) < 3:
            continue
        kind, name = parts[-2], parts[-1]
        symbols.add((kind, re.sub(r"(\.\d+)+$", "", name)))
    return symbols


def diff_symbols(reference, candidate):
    return sorted(reference - candidate), sorted(candidate - reference)


def git(*args, cwd=ROOT):
    return subprocess.run(
        ["git", *args], cwd=cwd, check=True, capture_output=True, text=True
    ).stdout.strip()


def sync_file(source, target):
    """Copy 'source' to 'target' unless it is already identical.

    Unchanged files keep their timestamp, so ninja only rebuilds what changed.
    """
    if target.is_file() and filecmp.cmp(source, target, shallow=False):
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def sync_working_tree(destination):
    """Mirror tracked and untracked (not ignored) files of the checkout.

    Files that disappeared since the last sync are removed again; generated
    files (build/, sdkconfig, embedded assets) are left alone.
    """
    listing = subprocess.run(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        cwd=ROOT,
        check=True,
        capture_output=True,
    ).stdout.decode()
    names = sorted(n for n in listing.split("\0") if n and (ROOT / n).is_file())
    manifest = destination.parent / f"{destination.name}.files"
    previous = manifest.read_text().splitlines() if manifest.is_file() else []
    for name in set(previous) - set(names):
        (destination / name).unlink(missing_ok=True)
    for name in names:
        sync_file(ROOT / name, destination / name)
    manifest.write_text("\n".join(names) + "\n")


def sync_tree(source, destination):
    for path in source.rglob("*"):
        if path.is_file():
            sync_file(path, destination / path.relative_to(source))


def config_signature(tree):
    """Hash of everything that feeds sdkconfig; idf.py does not re-read the
    sdkconfig.defaults files of an existing build, so a change needs a clean."""
    digest = hashlib.sha256()
    patterns = ("sdkconfig.defaults*", "boards/sdkconfig.defaults.*", "features/*")
    files = {f for pattern in patterns for f in tree.glob(pattern)}
    files |= set(tree.glob("**/Kconfig*")) - set(tree.glob("build/**"))
    for path in sorted(files):
        digest.update(path.relative_to(tree).as_posix().encode())
        digest.update(path.read_bytes())
    return digest.hexdigest()


def prepare_shared_assets(board):
    """Generated inputs both builds embed: the web app and the board's splash.

    Both trees get the same copies so the size comparison is about the firmware
    code only. Returns the directories to copy.
    """
    webapp = ROOT / "main" / "webapp"
    if not (webapp / "index.html.gz").is_file():
        subprocess.run(
            [sys.executable, "build.py", "--step", "webapp"], cwd=ROOT, check=True
        )
    splash = ROOT / "main" / "splash_data"
    if not (splash / "splash.epdgz").is_file():
        # Needs the qrcode module, which the ESP-IDF Python environment may lack:
        # generate it beforehand with `python build.py --board <b> --step splash`.
        subprocess.run(
            [sys.executable, "build.py", "--board", board, "--step", "splash"],
            cwd=ROOT,
            check=True,
        )
    return [webapp, splash]


def build(tree, board, extra_args, fullclean):
    """Build the firmware in 'tree' (from scratch if 'fullclean')."""
    command = [sys.executable, "build.py", "--board", board]
    command += ["--fullclean"] if fullclean else []
    command += ["--step", "firmware", *extra_args]
    print(f"\n### {tree.name}: {' '.join(command)}", flush=True)
    subprocess.run(command, cwd=tree, check=True)


def nm_symbols(elf, board):
    tool = shutil.which(NM_TOOL[BOARD_TARGET[board]])
    if tool is None:
        sys.exit(f"{NM_TOOL[BOARD_TARGET[board]]} not found: activate ESP-IDF first")
    output = subprocess.run(
        [tool, "--defined-only", str(elf)], check=True, capture_output=True, text=True
    ).stdout
    return parse_nm(output)


def report(board, reference_tree, candidate_tree, out):
    """Print the comparison; return True if the two builds are equivalent."""

    def emit(line=""):
        print(line)
        out.append(line)

    equivalent = True

    reference_config = parse_sdkconfig((reference_tree / "sdkconfig").read_text())
    candidate_config = parse_sdkconfig((candidate_tree / "sdkconfig").read_text())
    only_ref, only_cand, changed, ignored = diff_sdkconfig(
        reference_config, candidate_config
    )
    emit(f"== a) Kconfig symbols ({ignored} FEATURE_/FORK_ symbols ignored)")
    for name, value in only_ref.items():
        emit(f"  only in reference: CONFIG_{name}={value}")
    for name, value in only_cand.items():
        emit(f"  only in candidate: CONFIG_{name}={value}")
    for name, (was, now) in changed.items():
        emit(f"  changed: CONFIG_{name}: {was} -> {now}")
    if not (only_ref or only_cand or changed):
        emit("  identical")
    equivalent &= not (only_ref or only_cand or changed)

    elf = "esp32-photoframe.elf"
    gone, new = diff_symbols(
        nm_symbols(reference_tree / "build" / elf, board),
        nm_symbols(candidate_tree / "build" / elf, board),
    )
    emit("== b) ELF symbols")
    for kind, name in gone:
        emit(f"  only in reference: {kind} {name}")
    for kind, name in new:
        emit(f"  only in candidate: {kind} {name}")
    if not (gone or new):
        emit("  identical")
    equivalent &= not (gone or new)

    ref_size = (reference_tree / "build" / "esp32-photoframe.bin").stat().st_size
    cand_size = (candidate_tree / "build" / "esp32-photoframe.bin").stat().st_size
    emit("== c) firmware image size")
    emit(
        f"  reference {ref_size} B, candidate {cand_size} B, "
        f"difference {cand_size - ref_size:+d} B ({(cand_size - ref_size) / ref_size:+.2%})"
    )
    equivalent &= ref_size == cand_size
    return equivalent


def remove_reference(tree):
    if tree.exists():
        git("worktree", "remove", "--force", str(tree))
        shutil.rmtree(tree, ignore_errors=True)
    git("worktree", "prune")


def assets_signature(assets):
    """Hash of the generated inputs (web app, splash) both trees embed. The
    reference must be rebuilt when they change, or it keeps the old bytes and the
    size comparison is off by whatever the assets differ by."""
    digest = hashlib.sha256()
    for asset in assets:
        for path in sorted(p for p in asset.rglob("*") if p.is_file()):
            digest.update(path.relative_to(asset).as_posix().encode())
            digest.update(path.read_bytes())
    return digest.hexdigest()[:16]


def ensure_reference(tree, work, board, ref, baseline_args, assets):
    """Build the reference once per (ref, args, assets); later runs reuse it."""
    key = f"{git('rev-parse', ref)} {baseline_args} {assets_signature(assets)}"
    key_file = work / "reference.key"
    built = (tree / "build" / "esp32-photoframe.elf").is_file()
    if built and key_file.is_file() and key_file.read_text() == key:
        print(f"Reference {ref} already built, reusing it")
        return
    remove_reference(tree)
    print(f"Reference: {ref} ({git('log', '-1', '--format=%s', ref)})")
    git("worktree", "add", "--detach", str(tree), ref)
    for asset in assets:
        sync_tree(asset, tree / asset.relative_to(ROOT))
    build(tree, board, baseline_args.split(), fullclean=True)
    key_file.write_text(key)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--board", choices=list(SUPPORTED_BOARDS), required=True)
    parser.add_argument(
        "--baseline",
        help="Git ref of the reference build (default: aitjcize/esp32-photoframe "
        "@ v2.18.0-27, i.e. the unmodified upstream commit this repository's own "
        "history is grafted onto).",
    )
    parser.add_argument(
        "--baseline-args",
        default="",
        help="Extra build.py arguments for the reference build.",
    )
    parser.add_argument(
        "--work-dir",
        default=str(ROOT / ".verify"),
        help="Directory for the build trees (kept between runs, one per board).",
    )
    parser.add_argument(
        "--fullclean",
        action="store_true",
        help="Rebuild the candidate from scratch (default: incremental, with an "
        "automatic clean when sdkconfig inputs changed).",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Remove the trees of this board and exit.",
    )
    args, feature_args = parser.parse_known_args()

    work = Path(args.work_dir) / args.board
    reference_tree, candidate_tree = work / "reference", work / "candidate"
    if args.clean:
        remove_reference(reference_tree)
        shutil.rmtree(work, ignore_errors=True)
        return 0
    work.mkdir(parents=True, exist_ok=True)

    assets = prepare_shared_assets(args.board)
    # The newest aitjcize/esp32-photoframe commit merged into this repository:
    # 495e0b6 (after v2.18.0-27, the commit the history is grafted onto). Move it
    # with every upstream merge (docs/MAINTAINING.md section 12).
    upstream_sha = "495e0b6900d63ab2d75877d4e269d101834c379e"
    ref = args.baseline or upstream_sha
    ensure_reference(reference_tree, work, args.board, ref, args.baseline_args, assets)

    if ref == upstream_sha and "--ota-repo" not in feature_args:
        # The reference is upstream's own commit, which hardcodes its own repository
        # as the OTA feed (no build option there at all). This repository's default
        # is its own repository (main/Kconfig), so without this the two builds would
        # never compare byte-identical even with every feature off - not because the
        # code differs, but because the embedded feed string does. Anyone comparing
        # against a different --baseline (e.g. the old fork) should pass a matching
        # --ota-repo themselves if that comparison needs it too.
        feature_args = [*feature_args, "--ota-repo", "aitjcize/esp32-photoframe"]

    sync_working_tree(candidate_tree)
    for asset in assets:
        sync_tree(asset, candidate_tree / asset.relative_to(ROOT))
    signature = config_signature(candidate_tree)
    signature_file = work / "candidate.config"
    unchanged = signature_file.is_file() and signature_file.read_text() == signature
    build(
        candidate_tree,
        args.board,
        feature_args,
        fullclean=args.fullclean or not unchanged,
    )
    signature_file.write_text(signature)

    lines = []
    equivalent = report(args.board, reference_tree, candidate_tree, lines)
    (work / "report.txt").write_text("\n".join(lines) + "\n")
    print("\nRESULT:", "equivalent" if equivalent else "DIFFERENT (see above)")
    return 0 if equivalent else 1


if __name__ == "__main__":
    os.chdir(ROOT)
    sys.exit(main())
