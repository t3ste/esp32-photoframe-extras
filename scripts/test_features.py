"""Unit tests for scripts/features.py (run: python -m unittest discover scripts)."""

import unittest

import features
from boards import BOARD_CAPABILITIES

FULL_BOARD = "waveshare_photopainter_73"  # speaker, microphone, climate sensor
NO_HARDWARE_BOARD = "seeedstudio_xiao_ee02"  # none of the optional hardware
SENSOR_ONLY_BOARD = "seeedstudio_xiao_ee03"  # climate sensor, no audio
ALL_NAMES = [f.name for f in features.FEATURES]


class RepoTest(unittest.TestCase):
    def test_owner_and_name_are_accepted(self):
        self.assertEqual(
            features.check_repo("some-owner/my_repo.v2"), "some-owner/my_repo.v2"
        )

    def test_anything_else_is_an_error(self):
        for value in ("", "name-only", "a/b/c", "a b/c", 'a/b"', "a/", "/b"):
            with self.assertRaises(features.FeatureError, msg=value):
                features.check_repo(value)


class ResolveTest(unittest.TestCase):
    def test_nothing_selected_means_nothing_enabled(self):
        for board in BOARD_CAPABILITIES:
            result = features.resolve(board)
            self.assertEqual(result.enabled, [])
            self.assertEqual(result.added, {})
            self.assertEqual(result.skipped, {})

    def test_all_features_on_fully_equipped_board(self):
        result = features.resolve(FULL_BOARD, all_features=True)
        self.assertEqual(result.enabled, ALL_NAMES)
        self.assertEqual(result.skipped, {})

    def test_all_features_skips_what_the_board_cannot_do(self):
        result = features.resolve(NO_HARDWARE_BOARD, all_features=True)
        self.assertEqual(
            set(result.skipped), {"chimes", "climate", "alarmclock", "voice-stop"}
        )
        self.assertEqual(set(result.enabled), set(ALL_NAMES) - set(result.skipped))

    def test_all_features_on_sensor_only_board_keeps_climate(self):
        result = features.resolve(SENSOR_ONLY_BOARD, all_features=True)
        self.assertIn("climate", result.enabled)
        self.assertNotIn("chimes", result.enabled)

    def test_explicit_feature_without_hardware_is_an_error(self):
        cases = {
            "chimes": NO_HARDWARE_BOARD,
            "alarmclock": SENSOR_ONLY_BOARD,
            "voice-stop": SENSOR_ONLY_BOARD,
            "climate": NO_HARDWARE_BOARD,
        }
        for name, board in cases.items():
            with self.assertRaises(features.FeatureError, msg=f"{name} on {board}"):
                features.resolve(board, [name])

    def test_error_names_the_missing_hardware_and_the_boards_that_have_it(self):
        with self.assertRaises(features.FeatureError) as ctx:
            features.resolve(NO_HARDWARE_BOARD, ["chimes"])
        message = str(ctx.exception)
        self.assertIn("speaker", message)
        self.assertIn(NO_HARDWARE_BOARD, message)
        self.assertIn(FULL_BOARD, message)

    def test_explicit_request_also_fails_together_with_all_features(self):
        with self.assertRaises(features.FeatureError):
            features.resolve(NO_HARDWARE_BOARD, ["chimes"], all_features=True)

    def test_dependency_is_added_and_reported(self):
        result = features.resolve(FULL_BOARD, ["voice-stop"])
        self.assertEqual(result.enabled, ["alarmclock", "voice-stop"])
        self.assertEqual(result.added, {"alarmclock": "voice-stop"})

    def test_excluding_a_needed_dependency_is_an_error(self):
        with self.assertRaises(features.FeatureError):
            features.resolve(FULL_BOARD, ["voice-stop"], ["alarmclock"])

    def test_excluding_a_dependency_skips_dependents_of_all_features(self):
        result = features.resolve(FULL_BOARD, [], ["alarmclock"], all_features=True)
        self.assertNotIn("alarmclock", result.enabled)
        self.assertNotIn("voice-stop", result.enabled)
        self.assertIn("voice-stop", result.skipped)

    def test_without_trims_all_features(self):
        result = features.resolve(FULL_BOARD, [], ["https", "fixes"], True)
        self.assertNotIn("https", result.enabled)
        self.assertNotIn("fixes", result.enabled)
        self.assertNotIn("https", result.skipped)

    def test_with_and_without_the_same_feature_conflict(self):
        with self.assertRaises(features.FeatureError):
            features.resolve(FULL_BOARD, ["https"], ["https"])

    def test_unknown_feature_and_board(self):
        with self.assertRaises(features.FeatureError):
            features.resolve(FULL_BOARD, ["nonsense"])
        with self.assertRaises(features.FeatureError):
            features.resolve("no_such_board")

    def test_names_are_normalised(self):
        result = features.resolve(FULL_BOARD, ["Voice_Stop"])
        self.assertIn("voice-stop", result.enabled)

    def test_result_follows_registry_order(self):
        result = features.resolve(FULL_BOARD, ["fixes", "telegram", "agenda"])
        self.assertEqual(result.enabled, ["telegram", "agenda", "fixes"])


class HelperTest(unittest.TestCase):
    def test_parse_list_splits_flattens_and_deduplicates(self):
        self.assertEqual(
            features.parse_list(["a, b", "b,c", " "]),
            ["a", "b", "c"],
        )
        self.assertEqual(features.parse_list(None), [])

    def test_every_feature_has_an_overlay(self):
        files = features.overlay_files(ALL_NAMES)
        self.assertEqual(len(files), len(ALL_NAMES))
        self.assertTrue(
            all(f.startswith("features/sdkconfig.defaults.") for f in files)
        )

    def test_missing_overlay_is_reported(self):
        with self.assertRaises(features.FeatureError):
            features.overlay_files(["nonsense"])

    def test_describe_mentions_every_feature(self):
        text = features.describe(NO_HARDWARE_BOARD)
        for name in ALL_NAMES:
            self.assertIn(name, text)
        self.assertIn("not available", text)


if __name__ == "__main__":
    unittest.main()
