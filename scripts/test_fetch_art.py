"""Unit tests for scripts/fetch_art.py - no network, invented API answers only."""

import io
import json
import tempfile
import unittest
import urllib.error
from contextlib import redirect_stdout
from pathlib import Path
from unittest import mock

import fetch_art


def item(
    number, source="wikimedia", public_domain=True, resized=None, url=None, **more
):
    """An invented API item."""
    image = {}
    if resized is not False:
        image["resizedUrls"] = resized or {
            "512": f"https://img.example/{number}/w512.jpg",
            "1024": f"https://img.example/{number}/w1024.jpg",
            "3000": f"https://img.example/{number}/w3000.jpg",
        }
    if url:
        image["url"] = url
    entry = {
        "id": f"{source}:{number}",
        "sourceId": str(number),
        "source": source,
        "title": f"Picture {number}",
        "artist": "Some Painter",
        "license": "Public domain",
        "sourceUrl": f"https://commons.example/wiki/{number}",
        "isPublicDomain": public_domain,
        "image": image,
    }
    entry.update(more)
    return entry


class FakeResponse:
    def __init__(self, body):
        self._body = io.BytesIO(body)

    def read(self, size=-1):
        return self._body.read(size)

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


def json_opener(pages):
    """An urlopen stand-in answering successive API pages, then an empty page."""
    calls = []

    def opener(request, timeout=None):
        calls.append(request.full_url)
        index = len(calls) - 1
        page = pages[index] if index < len(pages) else []
        return FakeResponse(json.dumps({"items": page}).encode())

    opener.calls = calls
    return opener


class QueryAndSelection(unittest.TestCase):
    def test_query_url_asks_for_curated_public_domain_items(self):
        url = fetch_art.build_query_url(
            "https://api.example/", ["met", "artic"], "sea", 5, True, 100, 200
        )
        self.assertTrue(url.startswith("https://api.example/api/artworks?"))
        for part in (
            "selected=true",
            "rating=5",
            "publicDomain=true",
            "q=sea",
            "offset=200",
        ):
            self.assertIn(part, url)
        self.assertNotIn("source=", url)  # several sources: filtered client-side

    def test_query_url_single_source_and_any_rating(self):
        url = fetch_art.build_query_url("https://a", ["met"], "", 0, False, 10, 0)
        self.assertIn("source=met", url)
        self.assertNotIn("rating=", url)
        self.assertNotIn("publicDomain", url)

    def test_pick_image_url_smallest_that_covers(self):
        entry = item(1)
        self.assertTrue(fetch_art.pick_image_url(entry, 800).endswith("w1024.jpg"))
        self.assertTrue(fetch_art.pick_image_url(entry, 1872).endswith("w3000.jpg"))
        self.assertTrue(fetch_art.pick_image_url(entry, 5000).endswith("w3000.jpg"))
        self.assertTrue(fetch_art.pick_image_url(entry, 100).endswith("w512.jpg"))

    def test_pick_image_url_fallbacks(self):
        no_variants = item(2, resized=False, url="https://img.example/2/full.png")
        self.assertEqual(
            fetch_art.pick_image_url(no_variants, 800), "https://img.example/2/full.png"
        )
        self.assertIsNone(fetch_art.pick_image_url(item(3, resized=False), 800))

    def test_svg_is_skipped(self):
        svg = item(4, resized={"1024": "https://img.example/4/icon.SVG"})
        self.assertIsNone(fetch_art.pick_image_url(svg, 800))
        svg_only = item(5, resized=False, url="https://img.example/5/x.svg?v=1")
        self.assertIsNone(fetch_art.pick_image_url(svg_only, 800))

    def test_select_items_filters_and_dedupes(self):
        items = [
            item(1),
            item(1),  # duplicate id
            item(2, public_domain=False),
            item(3, resized=False),
            item(4),
        ]
        chosen = fetch_art.select_items(items, 10, "s", 800, True)
        self.assertEqual(
            sorted(i["id"] for i, _ in chosen), ["wikimedia:1", "wikimedia:4"]
        )
        with_cc = fetch_art.select_items(items, 10, "s", 800, False)
        self.assertEqual(len(with_cc), 3)

    def test_select_items_is_deterministic_and_seed_dependent(self):
        items = [item(n) for n in range(30)]
        first = fetch_art.select_items(items, 8, "seed", 800, True)
        again = fetch_art.select_items(list(reversed(items)), 8, "seed", 800, True)
        self.assertEqual([i["id"] for i, _ in first], [i["id"] for i, _ in again])
        other = fetch_art.select_items(items, 8, "another", 800, True)
        self.assertNotEqual([i["id"] for i, _ in first], [i["id"] for i, _ in other])
        self.assertEqual(len(first), 8)


class BoardsAndCommand(unittest.TestCase):
    def test_board_targets_from_the_real_boards_file(self):
        boards = fetch_art.load_boards()
        self.assertEqual(
            fetch_art.board_target("waveshare_photopainter_73", boards),
            (800, 480, False),
        )
        self.assertEqual(
            fetch_art.board_target("seeedstudio_xiao_ee03", boards), (1872, 1404, True)
        )
        self.assertEqual(
            fetch_art.board_target("seeedstudio_xiao_ee02", boards), (1200, 1600, False)
        )
        self.assertEqual(
            fetch_art.board_target("m5stack_m5paper_v11", boards), (960, 540, True)
        )
        with self.assertRaises(fetch_art.ArtError):
            fetch_art.board_target("no_such_board", boards)

    def test_cli_command(self):
        command = fetch_art.build_cli_command(
            "node", "cli.js", "in", "out", "b1", True, "fit", "frame.local", False
        )
        self.assertEqual(command[:3], ["node", "cli.js", "in"])
        for flag in ("--grayscale", "--auto-orient"):
            self.assertIn(flag, command)
        self.assertNotIn("--upload", command)
        upload = fetch_art.build_cli_command(
            "node", "cli.js", "in", "out", "b1", False, "cover", "frame.local", True
        )
        self.assertEqual(upload[-3:], ["--upload", "--host", "frame.local"])
        self.assertNotIn("--grayscale", upload)
        self.assertEqual(upload[upload.index("--scale-mode") + 1], "cover")


class FilesAndText(unittest.TestCase):
    def test_safe_name(self):
        self.assertEqual(
            fetch_art.safe_name(item(7), "https://x.example/a/b/w1024.JPG"),
            "wikimedia-7.jpg",
        )
        odd = item(8, sourceId="a b/../c")
        self.assertRegex(
            fetch_art.safe_name(odd, "https://x.example/p"), r"^[A-Za-z0-9._-]+$"
        )

    def test_download_respects_the_size_cap(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / "a.jpg"
            body = b"x" * 5000
            opener = lambda request, timeout=None: FakeResponse(body)  # noqa: E731
            self.assertEqual(
                fetch_art.download("https://x/a", target, 10_000, opener), 5000
            )
            with self.assertRaises(fetch_art.ArtError):
                fetch_art.download("https://x/a", target, 1_000, opener)

    def test_download_network_error_is_reported(self):
        def failing(request, timeout=None):
            raise urllib.error.URLError("boom")

        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(fetch_art.ArtError):
                fetch_art.download("https://x/a", Path(tmp) / "a", 100, failing)

    def test_attribution_lists_every_picture_and_escapes_pipes(self):
        tricky = item(9, title="A | B\nC")
        text = fetch_art.attribution_markdown([("f1.jpg", item(1)), ("f9.jpg", tricky)])
        self.assertIn("| f1.jpg | Picture 1 | Some Painter | Public domain |", text)
        self.assertIn("A / B C", text)
        self.assertEqual(text.count("\n| f"), 2)


class EndToEnd(unittest.TestCase):
    def args(self, *extra):
        return fetch_art.parse_args(
            ["--board", "waveshare_photopainter_73", "--count", "3", *extra]
        )

    def test_argument_validation(self):
        for bad in (
            ["--count", "0"],
            ["--count", "999"],
            ["--source", "svgrepo"],
            ["--min-rating", "6"],
        ):
            with self.assertRaises(SystemExit), redirect_stdout(
                io.StringIO()
            ), mock.patch("sys.stderr", io.StringIO()):
                self.args(*bad)

    def test_dry_run_lists_and_downloads_nothing(self):
        opener = json_opener([[item(n) for n in range(10)]])
        out = io.StringIO()
        with redirect_stdout(out):
            self.assertEqual(fetch_art.run(self.args("--dry-run"), opener), 0)
        self.assertIn(
            "3 artwork(s) for waveshare_photopainter_73 (800x480)", out.getvalue()
        )
        self.assertEqual(out.getvalue().count("w1024.jpg"), 3)
        self.assertEqual(len(opener.calls), 1)

    def test_nothing_usable_is_an_error(self):
        with self.assertRaises(fetch_art.ArtError):
            fetch_art.run(
                self.args("--dry-run"), json_opener([[item(1, public_domain=False)]])
            )

    def test_full_run_with_a_fake_process_cli(self):
        api = json_opener([[item(n) for n in range(5)]])
        downloads = []

        def opener(request, timeout=None):
            if "/api/artworks" in request.full_url:
                return api(request, timeout)
            downloads.append(request.full_url)
            return FakeResponse(b"\xff\xd8fake-jpeg")

        seen = {}

        def runner(command, cwd=None):
            seen["command"] = command
            seen["input_files"] = sorted(
                p.name for p in (Path(command[2]) / "Art").iterdir()
            )
            return mock.Mock(returncode=0)

        with tempfile.TemporaryDirectory() as tmp:
            cli_dir = Path(tmp) / "process-cli"
            (cli_dir / "node_modules").mkdir(parents=True)
            cli = cli_dir / "cli.js"
            cli.write_text("// fake")
            out_dir = Path(tmp) / "out"
            args = self.args("--out", str(out_dir))
            with mock.patch.object(fetch_art, "PROCESS_CLI", cli), mock.patch.object(
                fetch_art.shutil, "which", return_value="/usr/bin/node"
            ), mock.patch.object(fetch_art.time, "sleep"), redirect_stdout(
                io.StringIO()
            ):
                self.assertEqual(fetch_art.run(args, opener, runner), 0)
            attribution = (out_dir / "Art" / "ATTRIBUTION.md").read_text(
                encoding="utf-8"
            )

        self.assertEqual(len(downloads), 3)
        self.assertEqual(len(seen["input_files"]), 3)
        self.assertIn("--board", seen["command"])
        self.assertEqual(attribution.count("| wikimedia-"), 3)

    def test_failing_process_cli_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            cli_dir = Path(tmp) / "process-cli"
            (cli_dir / "node_modules").mkdir(parents=True)
            cli = cli_dir / "cli.js"
            cli.write_text("// fake")
            opener = json_opener([[item(n) for n in range(3)]])

            def routed(request, timeout=None):
                if "/api/artworks" in request.full_url:
                    return opener(request, timeout)
                return FakeResponse(b"jpeg")

            with mock.patch.object(fetch_art, "PROCESS_CLI", cli), mock.patch.object(
                fetch_art.shutil, "which", return_value="node"
            ), mock.patch.object(fetch_art.time, "sleep"), redirect_stdout(
                io.StringIO()
            ):
                with self.assertRaises(fetch_art.ArtError):
                    fetch_art.run(
                        self.args("--out", str(Path(tmp) / "o")),
                        routed,
                        lambda command, cwd=None: mock.Mock(returncode=3),
                    )


if __name__ == "__main__":
    unittest.main()
