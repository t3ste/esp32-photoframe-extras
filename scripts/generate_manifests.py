#!/usr/bin/env python3
"""
Generate ESP Web Tools manifests for firmware flashing.

The merged firmware image is one file that covers the flash from offset 0 - the
bootloader, the partition table and the app, and 0xFF in between. Written as it
is, it also overwrites the settings partition (WiFi credentials, all settings)
that sits in that gap. The manifests therefore list the individual parts at
their own offsets instead - bootloader, partition table, OTA data and app - and
leave the settings partition alone; the merged image is only cut into these
parts here (and removed afterwards, see --keep-merged).

Usage:
    python generate_manifests.py                    # Generate manifests
    python generate_manifests.py --dev              # Generate both stable and dev
    python generate_manifests.py --no-copy          # Skip copying firmware
"""

import argparse
import json
import os
import struct
import subprocess
import sys
from pathlib import Path

# Import version detection functions from get_version module
import get_version as version_module

from boards import SUPPORTED_BOARDS, board_chip_family, board_flash_args

PARTITION_TABLE_OFFSET = 0x8000
PARTITION_TABLE_SIZE = 0x1000
PARTITION_ENTRY_SIZE = 32
PARTITION_ENTRY_MAGIC = b"\xaa\x50"
PARTITION_TYPE_APP = 0
PARTITION_TYPE_DATA = 1
PARTITION_SUBTYPE_DATA_OTA = 0
IMAGE_MAGIC = 0xE9


def check_firmware_exists(firmware_path):
    """Check if firmware file exists."""
    if not os.path.exists(firmware_path):
        print(f"Warning: Firmware file not found: {firmware_path}")
        print("Please build the firmware first with: idf.py build")
        return False
    return True


def parse_partition_table(table):
    """[(name, type, subtype, offset, size)] of a partition table image."""
    entries = []
    for pos in range(0, len(table) - PARTITION_ENTRY_SIZE + 1, PARTITION_ENTRY_SIZE):
        entry = table[pos : pos + PARTITION_ENTRY_SIZE]
        if entry[:2] != PARTITION_ENTRY_MAGIC:
            break  # the MD5 checksum entry or the erased rest
        ptype, subtype, offset, size = struct.unpack("<BBII", entry[2:12])
        name = entry[12:28].split(b"\0", 1)[0].decode("ascii", "replace")
        entries.append((name, ptype, subtype, offset, size))
    return entries


def firmware_parts(merged, bootloader_offset):
    """The flashable parts of a merged image as [(name, flash_offset, data)].

    Everything the image holds that a device needs replaced for an update - the
    bootloader, the partition table, the OTA data (erased state: boot the first
    app slot) and the app - and nothing of the gaps in between, in particular not
    the settings (NVS) partition.
    """
    table = merged[
        PARTITION_TABLE_OFFSET : PARTITION_TABLE_OFFSET + PARTITION_TABLE_SIZE
    ]
    entries = parse_partition_table(table)
    otadata = next(
        (
            e
            for e in entries
            if e[1] == PARTITION_TYPE_DATA and e[2] == PARTITION_SUBTYPE_DATA_OTA
        ),
        None,
    )
    apps = [e for e in entries if e[1] == PARTITION_TYPE_APP]
    if otadata is None or not apps:
        raise ValueError("partition table has no OTA data or no app partition")
    app_offset = min(e[3] for e in apps)
    if len(merged) <= app_offset or merged[app_offset] != IMAGE_MAGIC:
        raise ValueError(f"no app image at {app_offset:#x} in the merged firmware")
    if merged[bootloader_offset] != IMAGE_MAGIC:
        raise ValueError(f"no bootloader image at {bootloader_offset:#x}")

    regions = [
        ("boot", bootloader_offset, PARTITION_TABLE_OFFSET),
        (
            "partitions",
            PARTITION_TABLE_OFFSET,
            PARTITION_TABLE_OFFSET + PARTITION_TABLE_SIZE,
        ),
        ("otadata", otadata[3], otadata[3] + otadata[4]),
        ("app", app_offset, len(merged)),
    ]
    return [(name, start, merged[start:end]) for name, start, end in regions]


def split_merged_image(merged_path, out_dir, stem, board):
    """Cut a merged image into <stem>-<part>.bin files.

    Returns the manifest parts: [{"path": file name, "offset": flash offset}].
    """
    _, bootloader_offset = board_flash_args(board)
    with open(merged_path, "rb") as f:
        merged = f.read()
    parts = []
    for name, offset, data in firmware_parts(merged, int(bootloader_offset, 16)):
        filename = f"{stem}-{name}.bin"
        with open(Path(out_dir) / filename, "wb") as out:
            out.write(data)
        parts.append({"path": filename, "offset": offset})
    return parts


def copy_firmware_to_demo(build_dir, demo_dir, board):
    """Copy firmware files from build directory to demo."""
    # Source files
    bootloader = os.path.join(build_dir, "bootloader", "bootloader.bin")
    partition_table = os.path.join(build_dir, "partition_table", "partition-table.bin")
    app_bin = os.path.join(build_dir, "esp32-photoframe.bin")

    # Check if files exist
    if not all(os.path.exists(f) for f in [bootloader, partition_table, app_bin]):
        print("Error: Firmware files not found. Please build first with: idf.py build")
        return False

    # Create merged firmware using esptool
    merged_bin = os.path.join(demo_dir, f"photoframe-firmware-{board}-merged.bin")

    # The target chip decides both the esptool chip name and where the
    # 2nd-stage bootloader lives (0x1000 on the ESP32, 0x0 on the S3).
    chip, bootloader_offset = board_flash_args(board)

    try:
        subprocess.run(
            [
                "esptool",
                "--chip",
                chip,
                "merge-bin",
                "-o",
                merged_bin,
                "--flash-mode",
                "dio",
                "--flash-freq",
                "80m",
                "--flash-size",
                "16MB",
                bootloader_offset,
                bootloader,
                "0x8000",
                partition_table,
                "0x20000",
                app_bin,
            ],
            check=True,
        )

        print(f"Created merged firmware: {merged_bin}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"Error creating merged firmware: {e}")
        return False


def generate_manifest(
    output_path, version, parts, board, is_dev=False, is_prerelease=False
):
    """Generate a manifest.json file."""

    board_display = SUPPORTED_BOARDS.get(board, board)

    manifest = {
        "name": (
            f"ESP32 PhotoFrame {board_display}"
            f"{' (Development)' if is_dev else ''}"
            f"{' (Pre-release)' if is_prerelease else ''}"
        ),
        "version": version,
        "home_assistant_domain": "esphome",
        "new_install_prompt_erase": True,
        "new_install_improv_wait_time": 15,
        "builds": [
            {
                "chipFamily": board_chip_family(board),
                "parts": parts,
            }
        ],
    }

    with open(output_path, "w") as f:
        json.dump(manifest, f, indent=2)

    print(f"Generated manifest: {output_path}")
    print(f"  Version: {version}")
    print(f"  Parts: {', '.join(p['path'] for p in parts)}")


def split_if_present(merged_path, demo_path, stem, board, keep_merged):
    """Parts of the merged image if there is one (and remove it), else None."""
    if not check_firmware_exists(merged_path):
        return None
    parts = split_merged_image(merged_path, demo_path, stem, board)
    if not keep_merged:
        os.remove(merged_path)
    return parts


def generate_manifests(
    demo_dir,
    board,
    build_dir=None,
    dev_mode=False,
    stable_version=None,
    prerelease_version=None,
    keep_merged=False,
):
    """Generate manifest files for web flasher."""

    demo_path = Path(demo_dir)
    demo_path.mkdir(exist_ok=True)

    # Get stable version (use provided version or auto-detect)
    if not stable_version:
        stable_version = version_module.get_stable_version()

    # Copy firmware if build_dir provided
    if build_dir:
        if not copy_firmware_to_demo(build_dir, demo_dir, board):
            return False

    # Stable manifest, from the merged firmware of the release
    stable_stem = f"photoframe-firmware-{board}"
    stable_parts = split_if_present(
        demo_path / f"{stable_stem}-merged.bin",
        demo_path,
        stable_stem,
        board,
        keep_merged,
    )
    if stable_parts:
        generate_manifest(
            demo_path / "manifest.json", stable_version, stable_parts, board
        )
    else:
        print(
            f"  Warning: Stable firmware {stable_stem}-merged.bin not found, "
            "skipping stable manifest generation"
        )

    # Generate dev manifest if in dev mode
    if dev_mode:
        # Get dev version (commit hash)
        dev_version = version_module.get_dev_version()
        # Dev manifest points to the dev firmware
        dev_stem = f"photoframe-firmware-{board}-dev"
        dev_parts = split_if_present(
            demo_path / f"{dev_stem}.bin", demo_path, dev_stem, board, keep_merged
        )
        if not dev_parts:
            print(
                f"  Warning: Dev firmware {dev_stem}.bin not found, using stable firmware instead"
            )
            dev_parts = stable_parts
        if dev_parts:
            generate_manifest(
                demo_path / "manifest-dev.json",
                dev_version,
                dev_parts,
                board,
                is_dev=True,
            )

    # Pre-release manifest: the newest published pre-release, hosted next to
    # the other firmware files (release assets can't be fetched cross-origin).
    if prerelease_version:
        pre_stem = f"photoframe-firmware-{board}-prerelease"
        pre_parts = split_if_present(
            demo_path / f"{pre_stem}-merged.bin",
            demo_path,
            pre_stem,
            board,
            keep_merged,
        )
        if pre_parts:
            generate_manifest(
                demo_path / "manifest-prerelease.json",
                prerelease_version,
                pre_parts,
                board,
                is_prerelease=True,
            )
        else:
            print(
                f"  Warning: Pre-release firmware {pre_stem}-merged.bin not found, "
                "skipping pre-release manifest generation"
            )

    return True


def main():
    parser = argparse.ArgumentParser(
        description="Generate ESP Web Tools manifests for firmware flashing"
    )
    parser.add_argument(
        "--demo-dir", default="demo", help="Demo directory (default: demo)"
    )
    parser.add_argument(
        "--build-dir",
        default="build",
        help="Build directory with firmware files (default: build)",
    )
    parser.add_argument(
        "--dev",
        action="store_true",
        help="Generate development manifest in addition to stable",
    )
    parser.add_argument(
        "--no-copy",
        action="store_true",
        help="Skip copying firmware from build directory",
    )
    parser.add_argument(
        "--board",
        required=True,
        choices=list(SUPPORTED_BOARDS.keys()),
        help="Board type to build",
    )
    parser.add_argument(
        "--prerelease-version",
        help="Tag of the published pre-release whose firmware is in the demo dir",
    )
    parser.add_argument(
        "--stable-version",
        help="Override stable version (default: auto-detect from git/GitHub)",
    )
    parser.add_argument(
        "--keep-merged",
        action="store_true",
        help="Keep the merged firmware images after cutting them into parts",
    )

    args = parser.parse_args()

    # Get absolute paths - resolve relative to project root (parent of scripts dir)
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    demo_dir = project_root / args.demo_dir
    build_dir = project_root / args.build_dir if not args.no_copy else None

    # Generate manifests
    print(f"Generating manifests for {args.board}...")
    if not generate_manifests(
        demo_dir,
        args.board,
        build_dir,
        args.dev,
        args.stable_version,
        args.prerelease_version,
        args.keep_merged,
    ):
        sys.exit(1)

    print("\nManifests generated successfully!")


if __name__ == "__main__":
    main()
