#!/usr/bin/env python3
"""Regenerate webapp/src/utils/schedPick.fixtures.json: what the firmware's scheduler decides.

The Web UI draws a timeline of the schedules with a JavaScript port of main/sched_pick.c
(webapp/src/utils/schedPick.js). schedPick.test.js checks that port against the answers of the real C
code, which this script collects: it builds scripts/sched_oracle.c with main/cron.c and
main/sched_pick.c (needs gcc), runs a set of hand-made and random schedule configurations over a week
(Monday..Sunday, TZ=UTC0) and writes every fire and every draw to the JSON file.

    python scripts/gen_sched_fixtures.py
    (cd webapp && npx prettier --write src/utils/schedPick.fixtures.json)   # the format check wants it

Only needed when main/sched_pick.c, main/cron.c or the way the Web UI turns settings into schedules
changes; the committed JSON file is what the tests read.
"""

import calendar
import json
import os
import random
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "webapp", "src", "utils", "schedPick.fixtures.json")
MONDAY = calendar.timegm((2026, 10, 12, 0, 0, 0))  # a Monday, 00:00 UTC

# hand-made cases: collisions, holds on the edge, a hold across midnight, all eight entities
HAND = [
    {"agenda": [["0 7 *", 0], ["0 7 *", 0]], "rotation": ["0 7 *"], "gap": 0},
    {"agenda": [["0 7 *", 0]], "rotation": ["10 7 *", "15 7 *", "20 7 *"], "gap": 15},
    {"agenda": [["30 23 *", 60]], "rotation": ["10 0 *", "40 0 *"], "gap": 30},
    {
        "agenda": [["0 6 *", 20], ["10 6 *", 0], ["30 6 *", 0]],
        "rotation": [],
        "gap": 10,
    },
    {
        "agenda": [["0 8-18 1-5", 0], ["30 8-18 *", 0]],
        "rotation": ["*/20 9-10 *"],
        "gap": 5,
    },
    {
        "agenda": [["%d 12 *" % (i * 5), 0] for i in range(7)],
        "rotation": ["0 12 *", "35 12 *", "40 12 *"],
        "gap": 12,
    },
    {"agenda": [["0 3 6,0", 240]], "rotation": ["0 4-9 *"], "gap": 0},
    {"agenda": [["59 23 *", 5], ["0 0 *", 0]], "rotation": ["5 0 *"], "gap": 5},
]
RULES = [
    "{m} {h} *",
    "{m} {h} 1-5",
    "{m} {h} 0,6",
    "{m} {h},{h2} *",
    "0 {h}-{h2}/3 *",
    "*/{step} {h}-{h2} *",
    "{m} {h} {d}",
]


def random_rule(rng):
    h = rng.randrange(0, 22)
    return rng.choice(RULES).format(
        m=rng.choice([0, 5, 10, 15, 20, 30, 45, 55]),
        h=h,
        h2=h + rng.randrange(1, 3),
        step=rng.choice([20, 30, 45]),
        d=rng.randrange(0, 7),
    )


def random_case(rng):
    return {
        "agenda": [
            [random_rule(rng), rng.choice([0, 0, 0, 5, 10, 20, 45, 90])]
            for _ in range(rng.randrange(0, 8))
        ],
        "rotation": [random_rule(rng) for _ in range(rng.randrange(0, 4))],
        "gap": rng.choice([0, 0, 5, 10, 15, 30, 60]),
    }


def build_oracle(tmp):
    exe = os.path.join(tmp, "sched_oracle.exe" if os.name == "nt" else "sched_oracle")
    cmd = ["gcc", "-O1", "-I", os.path.join(ROOT, "main")]
    if os.name == "nt":
        cmd += ["-include", os.path.join(ROOT, "scripts", "sched_oracle_shim.h")]
    cmd += [
        "-o",
        exe,
        os.path.join(ROOT, "scripts", "sched_oracle.c"),
        os.path.join(ROOT, "main", "cron.c"),
        os.path.join(ROOT, "main", "sched_pick.c"),
    ]
    subprocess.run(cmd, check=True)
    return exe


def run_day(exe, case, day):
    lines = ["tz UTC0", "gap %d" % case["gap"]]
    lines += ["sched %d %s" % (hold, rule) for rule, hold in case["agenda"]]
    lines += ["rot " + rule for rule in case["rotation"]]
    lines += ["from %d" % (MONDAY + day * 86400), "minutes 1440"]
    out = subprocess.run(
        [exe], input="\n".join(lines), capture_output=True, text=True, check=True
    ).stdout.splitlines()
    fires = []
    for line in out:
        m = re.match(r"^(\d\d):(\d\d) fires\[(.*?)\] draws=(\S+)", line)
        minute = int(m.group(1)) * 60 + int(m.group(2))
        for who in m.group(3).split():
            fires.append([minute, who, 1 if who == m.group(4) else 0])
    return fires


def main():
    rng = random.Random(20261013)
    cases = HAND + [random_case(rng) for _ in range(40)]
    with tempfile.TemporaryDirectory() as tmp:
        exe = build_oracle(tmp)
        for case in cases:
            case["days"] = [run_day(exe, case, d) for d in range(7)]
    with open(OUT, "w") as f:
        json.dump({"monday": "2026-10-12", "cases": cases}, f, separators=(",", ":"))
        f.write("\n")
    print("%d cases written to %s (%d bytes)" % (len(cases), OUT, os.path.getsize(OUT)))


if __name__ == "__main__":
    sys.exit(main())
