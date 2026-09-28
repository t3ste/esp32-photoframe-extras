#!/usr/bin/env python3
"""Source-level proof that "no feature" is the upstream web UI.

The web counterpart of alloff_source.py. Resolves the build-time directives
(webapp/feature-directives.js, `#if FEATURE_*` / `#if FORK_*`) of every web source that
also exists in the upstream baseline with every flag off - so the `#else` parts stay -
by running the very plugin code the build uses, and compares the result with the
upstream file, ignoring blank lines and whitespace. It covers what ends up in the
firmware's bundle: webapp/src, index.html and public/. Only files listed in EXPECTED may
differ.

It exists because views/LandingPage.vue is part of the device bundle (router/index.js):
changes made there for the project's own demo page changed the "no flags" bundle
without anything noticing, until a byte comparison of the built bundles showed it. What
belongs to the demo page only is fenced with `#if FORK_SITE`, a flag only
vite.config.demo.js switches on.

Needs git (with the baseline commit) and node.

    alloff_web.py
"""

import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from alloff_source import BASELINE  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]

# Intended differences (files that are not part of the device bundle).
EXPECTED = {
    "webapp/index-demo.html": "entry page of the demo site only (its own icon path)",
}

RESOLVE = """
import { applyDirectives, flagsFor } from './webapp/feature-directives.js';
let input = '';
process.stdin.on('data', (chunk) => (input += chunk));
process.stdin.on('end', () => {
  const flags = flagsFor('');
  const out = {};
  for (const [name, text] of Object.entries(JSON.parse(input))) out[name] = applyDirectives(text, flags);
  process.stdout.write(JSON.stringify(out));
});
"""


def git(*args):
    return subprocess.run(
        ["git", "-c", "core.autocrlf=false", *args],
        cwd=ROOT,
        check=True,
        capture_output=True,
    ).stdout.decode("utf-8", errors="replace")


def normalise(text):
    lines = (re.sub(r"\s+", " ", line).strip() for line in text.splitlines())
    return [line for line in lines if line]


def in_bundle_scope(name):
    return name in ("webapp/index.html", "webapp/index-demo.html") or name.startswith(
        ("webapp/src/", "webapp/public/")
    )


def main():
    names = [
        n
        for n in git(
            "ls-tree", "-r", "--name-only", BASELINE, "--", "webapp"
        ).splitlines()
        if in_bundle_scope(n) and re.search(r"\.(vue|js|html|css|json|svg)$", n)
    ]
    ours = {}
    unexpected = []
    for name in names:
        path = ROOT / name
        if path.is_file():
            ours[name] = path.read_bytes().decode("utf-8")
        else:
            unexpected.append((name, "file is missing"))
    resolved = json.loads(
        subprocess.run(
            ["node", "--input-type=module", "-e", RESOLVE],
            cwd=ROOT,
            input=json.dumps(ours).encode("utf-8"),
            check=True,
            capture_output=True,
        ).stdout
    )
    unused = set(EXPECTED)
    for name, text in resolved.items():
        upstream = git("show", f"{BASELINE}:{name}")
        if normalise(text) == normalise(upstream):
            continue
        if name in EXPECTED:
            unused.discard(name)
        else:
            unexpected.append((name, "differs from upstream with every feature off"))
    for name in sorted(unused):
        unexpected.append(
            (name, "listed in EXPECTED but equal to upstream - remove it")
        )
    print(
        f"{len(names)} web files checked against {BASELINE[:7]}, "
        f"{len(unexpected)} unexpected difference(s)"
    )
    for name, why in sorted(unexpected):
        print(f"  {name}: {why}")
    return 1 if unexpected else 0


if __name__ == "__main__":
    sys.exit(main())
