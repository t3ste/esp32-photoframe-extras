"""Unit tests for the truth-expression evaluator of scripts/migrate/gate.py."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "migrate"))

import gate  # noqa: E402


class EvalTruthTest(unittest.TestCase):
    def test_plain_expressions(self):
        self.assertTrue(gate.eval_truth("1"))
        self.assertFalse(gate.eval_truth("0"))
        self.assertTrue(gate.eval_truth("1 or 0"))
        self.assertFalse(gate.eval_truth("1 and 0"))
        self.assertTrue(gate.eval_truth(" not 0"))
        self.assertTrue(gate.eval_truth("(1 or 0) and not 0"))

    def test_nothing_else_is_evaluated(self):
        for text in (
            "__import__('os').system('x')",
            "1 if 1 else 0",
            "1 + 1",
            "True",
            "[]",
            "1 and anything",
            "1;2",
            "",
        ):
            with self.subTest(text=text):
                with self.assertRaises(ValueError):
                    gate.eval_truth(text)

    def test_gate_expressions_still_resolve(self):
        self.assertTrue(gate.truth_value("FEATURE_AGENDA || FORK_FIXES", True))
        self.assertFalse(gate.truth_value("FEATURE_AGENDA && !FORK_FIXES", True))
        self.assertFalse(gate.truth_value("defined(FEATURE_AGENDA)", False))


if __name__ == "__main__":
    unittest.main()
