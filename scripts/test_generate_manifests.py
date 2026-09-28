"""Unit tests for scripts/generate_manifests.py (run: python -m unittest discover scripts)."""

import json
import struct
import tempfile
import unittest
from pathlib import Path

import generate_manifests as gm

S3_BOARD = "waveshare_photopainter_73"  # bootloader at 0x0
ESP32_BOARD = "m5stack_m5paper_v11"  # bootloader at 0x1000


def partition_entry(name, ptype, subtype, offset, size):
    return (
        b"\xaa\x50"
        + struct.pack("<BBII", ptype, subtype, offset, size)
        + name.encode().ljust(16, b"\0")
        + b"\0\0\0\0"
    )


def merged_image(bootloader_offset, app=b"\xe9APP" * 100):
    """A merged image laid out like esptool merge-bin makes it for these boards."""
    table = (
        partition_entry("nvs", 1, 2, 0x9000, 0x6000)
        + partition_entry("otadata", 1, 0, 0xF000, 0x2000)
        + partition_entry("phy_init", 1, 1, 0x11000, 0x1000)
        + partition_entry("ota_0", 0, 0x10, 0x20000, 0x780000)
        + partition_entry("ota_1", 0, 0x11, 0x7A0000, 0x780000)
        + b"\xeb\xeb"
        + b"\xff" * 14
        + b"\x00" * 16  # the MD5 entry
    )
    image = bytearray(b"\xff" * (0x20000 + len(app)))
    boot = b"\xe9BOOT" * 50
    image[bootloader_offset : bootloader_offset + len(boot)] = boot
    image[0x8000 : 0x8000 + len(table)] = table
    image[0x20000:] = app
    return bytes(image)


class PartitionTableTest(unittest.TestCase):
    def test_entries_are_read_up_to_the_checksum_entry(self):
        table = merged_image(0)[0x8000:0x9000]
        names = [e[0] for e in gm.parse_partition_table(table)]
        self.assertEqual(names, ["nvs", "otadata", "phy_init", "ota_0", "ota_1"])


class FirmwarePartsTest(unittest.TestCase):
    def check_layout(self, bootloader_offset):
        image = merged_image(bootloader_offset)
        parts = gm.firmware_parts(image, bootloader_offset)
        self.assertEqual(
            [(name, offset) for name, offset, _ in parts],
            [
                ("boot", bootloader_offset),
                ("partitions", 0x8000),
                ("otadata", 0xF000),
                ("app", 0x20000),
            ],
        )
        for _, offset, data in parts:
            self.assertEqual(image[offset : offset + len(data)], data)
        return parts

    def test_s3_layout(self):
        self.check_layout(0x0)

    def test_esp32_layout(self):
        self.check_layout(0x1000)

    def test_settings_partition_is_not_covered_by_any_part(self):
        parts = self.check_layout(0x0)
        nvs = range(0x9000, 0xF000)
        for _, offset, data in parts:
            end = offset + len(data)
            self.assertTrue(end <= nvs.start or offset >= nvs.stop, offset)

    def test_app_part_is_the_ota_binary(self):
        app = b"\xe9" + bytes(range(256)) * 20
        parts = dict(
            (name, data) for name, _, data in gm.firmware_parts(merged_image(0, app), 0)
        )
        self.assertEqual(parts["app"], app)

    def test_otadata_is_in_the_erased_state(self):
        parts = dict(
            (name, data) for name, _, data in gm.firmware_parts(merged_image(0), 0)
        )
        self.assertEqual(parts["otadata"], b"\xff" * 0x2000)

    def test_an_image_without_an_app_is_rejected(self):
        image = bytearray(merged_image(0))
        image[0x20000] = 0x00
        with self.assertRaises(ValueError):
            gm.firmware_parts(bytes(image), 0)

    def test_an_image_without_a_bootloader_is_rejected(self):
        image = bytearray(merged_image(0))
        image[0] = 0xFF
        with self.assertRaises(ValueError):
            gm.firmware_parts(bytes(image), 0)

    def test_a_missing_partition_table_is_rejected(self):
        image = bytearray(merged_image(0))
        image[0x8000:0x9000] = b"\xff" * 0x1000
        with self.assertRaises(ValueError):
            gm.firmware_parts(bytes(image), 0)


class ManifestTest(unittest.TestCase):
    def test_manifests_list_the_parts_and_never_the_merged_image(self):
        for board, bootloader_offset in ((S3_BOARD, 0x0), (ESP32_BOARD, 0x1000)):
            with tempfile.TemporaryDirectory() as tmp:
                demo = Path(tmp)
                (demo / f"photoframe-firmware-{board}-merged.bin").write_bytes(
                    merged_image(bootloader_offset)
                )
                (demo / f"photoframe-firmware-{board}-dev.bin").write_bytes(
                    merged_image(bootloader_offset, b"\xe9DEV" * 100)
                )
                self.assertTrue(
                    gm.generate_manifests(
                        demo, board, dev_mode=True, stable_version="v1.2.3"
                    )
                )
                stable = json.loads((demo / "manifest.json").read_text())
                dev = json.loads((demo / "manifest-dev.json").read_text())
                self.assertEqual(stable["version"], "v1.2.3")
                for manifest, stem in (
                    (stable, f"photoframe-firmware-{board}"),
                    (dev, f"photoframe-firmware-{board}-dev"),
                ):
                    parts = manifest["builds"][0]["parts"]
                    self.assertEqual(
                        [p["offset"] for p in parts],
                        [bootloader_offset, 0x8000, 0xF000, 0x20000],
                    )
                    for part in parts:
                        self.assertTrue(part["path"].startswith(stem + "-"))
                        self.assertTrue((demo / part["path"]).is_file())
                # the merged images are gone, only the parts remain
                self.assertEqual(list(demo.glob("*-merged.bin")), [])
                self.assertFalse(
                    (demo / f"photoframe-firmware-{board}-dev.bin").exists()
                )
                self.assertIn("Development", dev["name"])

    def test_keep_merged_leaves_the_merged_image(self):
        with tempfile.TemporaryDirectory() as tmp:
            demo = Path(tmp)
            merged = demo / f"photoframe-firmware-{S3_BOARD}-merged.bin"
            merged.write_bytes(merged_image(0))
            gm.generate_manifests(demo, S3_BOARD, stable_version="v1", keep_merged=True)
            self.assertTrue(merged.is_file())

    def test_dev_manifest_falls_back_to_the_stable_parts(self):
        with tempfile.TemporaryDirectory() as tmp:
            demo = Path(tmp)
            (demo / f"photoframe-firmware-{S3_BOARD}-merged.bin").write_bytes(
                merged_image(0)
            )
            gm.generate_manifests(demo, S3_BOARD, dev_mode=True, stable_version="v1")
            stable = json.loads((demo / "manifest.json").read_text())
            dev = json.loads((demo / "manifest-dev.json").read_text())
            self.assertEqual(stable["builds"][0]["parts"], dev["builds"][0]["parts"])

    def test_without_firmware_no_manifest_is_written(self):
        with tempfile.TemporaryDirectory() as tmp:
            demo = Path(tmp)
            self.assertTrue(
                gm.generate_manifests(
                    demo, S3_BOARD, dev_mode=True, stable_version="v1"
                )
            )
            self.assertEqual(list(demo.iterdir()), [])


if __name__ == "__main__":
    unittest.main()
