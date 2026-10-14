#!/usr/bin/env python3
"""Copy the Agenda renderer port into the colour profile editor page.

webapp/public/profile-editor.html is one standalone file (the frame serves it as it is, and it works from a
download too), but its preview is drawn by webapp/src/utils/agendaEngine.js - the JavaScript port of the firmware's
Agenda renderer, which the tests check against the real renderer. This script writes that code (and the font and
icon data of agendaEngineData.js) into the page between its two ENGINE markers, wrapped in a function, so the page
has one copy of the code and the tests have the other.

    python scripts/sync_profile_editor.py            rewrite the page
    python scripts/sync_profile_editor.py --check    exit 1 when the page is not up to date (CI runs this as a test)

Run it after every change of agendaEngine.js or agendaEngineData.js.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
UTILS = os.path.join(ROOT, "webapp", "src", "utils")
PAGE = os.path.join(ROOT, "webapp", "public", "profile-editor.html")

BEGIN = "// ==== ENGINE BEGIN (written by scripts/sync_profile_editor.py from webapp/src/utils/agendaEngine*.js - do not edit here) ===="
END = "// ==== ENGINE END ===="


def read(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read().replace("\r\n", "\n")


def strip_module(source):
    """The module code without its import lines; the names it exports."""
    names = re.findall(r"^export (?:function|const) (\w+)", source, re.M)
    source = re.sub(r"^import .*?;\n", "", source, flags=re.M)
    source = re.sub(r"^export (function|const) ", r"\1 ", source, flags=re.M)
    return source, names


def engine_block():
    data, _ = strip_module(read(os.path.join(UTILS, "agendaEngineData.js")))
    engine, names = strip_module(read(os.path.join(UTILS, "agendaEngine.js")))
    if "</script" in data + engine:
        sys.exit("the engine code must not contain a closing script tag")
    body = data.rstrip("\n") + "\n\n" + engine.rstrip("\n")
    return "%s\nconst AgendaEngine = (() => {\n%s\n\nreturn { %s };\n})();\n%s" % (
        BEGIN,
        body,
        ", ".join(names),
        END,
    )


def synced_page():
    page = read(PAGE)
    start = page.find(BEGIN)
    end = page.find(END)
    if start < 0 or end < start:
        sys.exit("%s: the ENGINE markers are missing" % PAGE)
    return page[:start] + engine_block() + page[end + len(END) :]


def main():
    wanted = synced_page()
    if "--check" in sys.argv[1:]:
        if wanted != read(PAGE):
            sys.exit(
                "profile-editor.html is not up to date: run python scripts/sync_profile_editor.py"
            )
        print("profile-editor.html is up to date")
        return
    with open(PAGE, "w", encoding="utf-8", newline="\n") as f:
        f.write(wanted)
    print("written %s" % PAGE)


if __name__ == "__main__":
    main()
