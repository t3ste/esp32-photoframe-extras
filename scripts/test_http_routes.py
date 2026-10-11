"""Every route that upstream's registration list serves must also be in the fork's.

main/http_server.c registers the routes twice: upstream's own list (the `#else` branch of http_server_init(),
used when no build option is on) and the fork's register_all_handlers() (every build with an option, and
both the plain-HTTP and the HTTPS server). A route that upstream adds in a merge lands in the first list only,
so a build with any option answers 404 for it: `POST /api/crash/clear` (the Web UI's CLEAR button of the crash
report) was lost like this in the merge of upstream v2.19.0 and stayed lost in every release since (v219.0.0 to v219.0.3). The compiler
had said so all along ("'crash_clear_handler' defined but not used").
"""

import re
import unittest
from pathlib import Path

HTTP_SERVER = Path(__file__).resolve().parent.parent / "main" / "http_server.c"
ROUTE = re.compile(r'register_uri\(\s*(?:handle,\s*)?"([^"]+)",\s*(HTTP_[A-Z]+)')


def routes(text):
    return {(m.group(1), m.group(2)) for m in ROUTE.finditer(text)}


def registration_lists(text):
    """(the fork's register_all_handlers() body, upstream's list in http_server_init())"""
    start = text.index("static void register_all_handlers(httpd_handle_t handle)")
    init = text.index("esp_err_t http_server_init(void)")
    stop = text.index("esp_err_t http_server_stop(void)")
    fork = text[start:init]
    upstream = text[init:stop]
    upstream = upstream[upstream.index("#else") :]
    return fork, upstream


class HttpRoutesTest(unittest.TestCase):
    def test_the_fork_serves_every_route_of_upstreams_list(self):
        fork, upstream = registration_lists(HTTP_SERVER.read_text(encoding="utf-8"))
        missing = sorted(routes(upstream) - routes(fork))
        self.assertEqual(
            missing,
            [],
            "upstream registers these routes but register_all_handlers() does not (every build with an option "
            "answers 404): add them there",
        )

    def test_the_lists_are_found(self):
        fork, upstream = registration_lists(HTTP_SERVER.read_text(encoding="utf-8"))
        self.assertGreater(len(routes(upstream)), 30)
        self.assertGreater(len(routes(fork)), 60)

    def test_the_route_pattern_reads_both_spellings(self):
        text = 'register_uri(handle, "/a", HTTP_GET, h);\n register_uri(\n "/b", HTTP_POST, h);\n register_uri(handle, "/c",\n HTTP_PATCH, h);'
        self.assertEqual(
            routes(text),
            {("/a", "HTTP_GET"), ("/b", "HTTP_POST"), ("/c", "HTTP_PATCH")},
        )


if __name__ == "__main__":
    unittest.main()
