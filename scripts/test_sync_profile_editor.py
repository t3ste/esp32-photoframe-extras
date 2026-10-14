"""The colour profile editor page carries an up-to-date copy of the Agenda renderer port.

Run: python -m unittest discover scripts. If this fails, run python scripts/sync_profile_editor.py.
"""

import unittest

import sync_profile_editor as sync


class ProfileEditorSyncTest(unittest.TestCase):
    def test_page_is_up_to_date(self):
        self.assertEqual(sync.read(sync.PAGE), sync.synced_page())

    def test_exports_are_listed(self):
        block = sync.engine_block()
        for name in ("renderAgenda", "resolveProfile", "stateColors", "parseColor"):
            self.assertIn(name, block.rsplit("return {", 1)[1], name)

    def test_no_module_syntax_is_left(self):
        block = sync.engine_block()
        self.assertNotRegex(block, r"(?m)^(import|export) ")


if __name__ == "__main__":
    unittest.main()
