"""Unit tests for the comparison logic of scripts/verify_baseline.py."""

import tempfile
import unittest
from pathlib import Path

import verify_baseline as vb

SDKCONFIG_REFERENCE = """\
# comment
CONFIG_FOO=y
CONFIG_BAR=42
CONFIG_NAME="text"
# CONFIG_OFF is not set
"""

NM_OUTPUT = """\
3fc90000 D config_table
42000100 T app_main
42000200 t helper.1234
42000300 t helper.1234.5
"""


class SdkconfigTest(unittest.TestCase):
    def test_parse_reads_values_and_skips_unset_and_comments(self):
        values = vb.parse_sdkconfig(SDKCONFIG_REFERENCE)
        self.assertEqual(values, {"FOO": "y", "BAR": "42", "NAME": '"text"'})

    def test_identical_configs_have_no_difference(self):
        values = vb.parse_sdkconfig(SDKCONFIG_REFERENCE)
        only_ref, only_cand, changed, ignored = vb.diff_sdkconfig(values, values)
        self.assertEqual((only_ref, only_cand, changed, ignored), ({}, {}, {}, 0))

    def test_differences_are_reported_in_the_right_bucket(self):
        reference = {"A": "y", "B": "1", "C": "y"}
        candidate = {"A": "y", "B": "2", "D": "y"}
        only_ref, only_cand, changed, _ = vb.diff_sdkconfig(reference, candidate)
        self.assertEqual(only_ref, {"C": "y"})
        self.assertEqual(only_cand, {"D": "y"})
        self.assertEqual(changed, {"B": ("1", "2")})

    def test_infrastructure_symbols_are_ignored_and_counted(self):
        reference = {"A": "y"}
        candidate = {"A": "y", "FEATURE_X": "y", "FORK_HW_SPEAKER": "y"}
        only_ref, only_cand, changed, ignored = vb.diff_sdkconfig(reference, candidate)
        self.assertEqual((only_ref, only_cand, changed), ({}, {}, {}))
        self.assertEqual(ignored, 2)


class NmTest(unittest.TestCase):
    def test_parse_normalises_gcc_uniquifying_suffixes(self):
        symbols = vb.parse_nm(NM_OUTPUT)
        self.assertEqual(
            symbols, {("D", "config_table"), ("T", "app_main"), ("t", "helper")}
        )

    def test_diff_lists_both_directions(self):
        gone, new = vb.diff_symbols({("T", "a"), ("T", "b")}, {("T", "b"), ("T", "c")})
        self.assertEqual(gone, [("T", "a")])
        self.assertEqual(new, [("T", "c")])


class AssetsSignatureTest(unittest.TestCase):
    def test_signature_follows_the_content_of_the_embedded_assets(self):
        with tempfile.TemporaryDirectory() as tmp:
            asset = Path(tmp) / "webapp"
            (asset / "assets").mkdir(parents=True)
            (asset / "assets" / "index.js.gz").write_bytes(b"one")
            before = vb.assets_signature([asset])
            self.assertEqual(before, vb.assets_signature([asset]))
            (asset / "assets" / "index.js.gz").write_bytes(b"two")
            self.assertNotEqual(before, vb.assets_signature([asset]))


if __name__ == "__main__":
    unittest.main()
