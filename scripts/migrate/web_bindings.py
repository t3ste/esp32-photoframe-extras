#!/usr/bin/env python3
"""Proof that no feature set leaves the web UI with a name it never defines (template or script).

The web sources fence parts of a component with `#if FEATURE_*` directives
(webapp/feature-directives.js), and the template of a component and its script are fenced
independently. When a template part stays in the build while the script part it uses is
fenced out, Vite builds without a word and Vue only warns when the page is drawn: a switch
that does nothing, a list without items, a button that fails when it is pressed. Neither the
all-off proof (alloff_web.py) nor a build sees it.

This resolves the directives of every component that has any for each feature set - every
feature alone (with what it needs), the full build, every feature left out of the full build,
and a few combinations - compiles the script and the template the way Vite does and reports
each name the template uses that the script does not define, and each name the script itself uses
without declaring it (a declaration fenced by one option, the code that uses it by another).

Needs node and `npm ci` in webapp/ (for @vue/compiler-sfc).

    web_bindings.py            all sets
    web_bindings.py -v         also the sets that are fine
"""

import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import features  # noqa: E402

# A board that has every capability, so nothing is dropped for lack of hardware.
BOARD = "waveshare_photopainter_73"

COMBINATIONS = [
    ("agenda", "info-screens"),
    ("agenda", "alarmclock"),
    ("agenda", "info-screens", "fuel-prices", "route-time"),
    ("agenda", "info-screens", "schedule-pages"),
    ("agenda", "info-screens", "recipes"),
    ("agenda", "caldav", "caldav-todo", "webcal", "source-auth"),
    ("info-screens", "market-quotes", "chore-wheel"),
    ("alarmclock", "voice-stop"),
    ("climate", "overlays"),
]

CHECK = r"""
import { readFileSync, readdirSync, statSync } from 'node:fs';
import { createRequire } from 'node:module';
import { join } from 'node:path';
import { applyDirectives, flagsFor } from './webapp/feature-directives.js';

const require = createRequire(process.cwd() + '/webapp/package.json');
let sfc;
try {
  sfc = require('@vue/compiler-sfc');
} catch {
  process.stdout.write(JSON.stringify({ error: 'run `npm ci` in webapp/ first (needs @vue/compiler-sfc)' }));
  process.exit(0);
}
const GLOBALS = new Set(['attrs', 'slots', 'emit', 'props', 'route', 'router', 'el', 'refs', 't', 'parent']);
// The script part is checked as well: a name that a function body uses but a fenced-out
// declaration was meant to provide (a ReferenceError the moment the function runs).
const { Linter } = require('eslint');
const globals = require('globals');
const linter = new Linter({ configType: 'flat' });
const LINT = [
  {
    files: ['**/*.js'],
    languageOptions: { ecmaVersion: 'latest', sourceType: 'module', globals: { ...globals.browser } },
    rules: { 'no-undef': 'error' },
  },
];
function undefinedInScript(code) {
  const messages = linter.verify(code, LINT, { filename: 'x.js' });
  const fatal = messages.find((m) => m.fatal);
  if (fatal) return { error: fatal.message };
  return { names: [...new Set(messages.filter((m) => m.ruleId === 'no-undef').map((m) => m.message.split("'")[1]))] };
}
// The script of a resolved source: the code of a .js file, the compiled <script setup> of a .vue.
function scriptOf(file, source, sfc) {
  if (file.endsWith('.js')) return source;
  const { descriptor } = sfc.parse(source, { filename: file });
  return sfc.compileScript(descriptor, { id: 'x', inlineTemplate: false }).content;
}
function sourceFiles(dir) {
  const out = [];
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) out.push(...sourceFiles(p));
    else if (name.endsWith('.vue') || name.endsWith('.js')) out.push(p);
  }
  return out;
}
const files = sourceFiles('webapp/src').filter((f) => readFileSync(f, 'utf8').includes('#if '));
// Names that the text of upstream itself (every directive off) already uses without declaring - an
// upstream defect the all-off proof keeps as it is - are not reported for a set without the `fixes`
// option (which corrects them: with it they must be defined).
const upstreamUndefined = {};
for (const file of files) {
  try {
    const source = applyDirectives(readFileSync(file, 'utf8'), flagsFor(''));
    upstreamUndefined[file] = new Set(undefinedInScript(scriptOf(file, source, sfc)).names || []);
  } catch { upstreamUndefined[file] = new Set(); }
}
let input = '';
process.stdin.on('data', (chunk) => (input += chunk));
process.stdin.on('end', () => {
  const result = {};
  for (const set of JSON.parse(input)) {
    const flags = flagsFor(set.features.join(','));
    const problems = [];
    for (const file of files) {
      const source = applyDirectives(readFileSync(file, 'utf8'), flags);
      const lintScript = (code) => {
        const lint = undefinedInScript(code);
        if (lint.error) { problems.push(`${file}: script: ${lint.error}`); return; }
        const known = flags.has('FORK_FIXES') ? new Set() : upstreamUndefined[file];
        const names = lint.names.filter((n) => !known.has(n));
        if (names.length) problems.push(`${file}: used by the script, not defined: ${names.join(', ')}`);
      };
      if (file.endsWith('.js')) { lintScript(source); continue; }
      const { descriptor, errors } = sfc.parse(source, { filename: file });
      if (errors.length) { problems.push(`${file}: ${errors[0].message}`); continue; }
      let bindings = {};
      try {
        const compiled = sfc.compileScript(descriptor, { id: 'x', inlineTemplate: false });
        bindings = compiled.bindings || {};
        lintScript(compiled.content);
      } catch (e) { problems.push(`${file}: script: ${String(e.message).split('\n')[0]}`); continue; }
      if (!descriptor.template) continue;
      const t = sfc.compileTemplate({
        source: descriptor.template.content, filename: file, id: 'x',
        compilerOptions: { bindingMetadata: bindings, prefixIdentifiers: true },
      });
      if (t.errors.length) { problems.push(`${file}: template: ${String(t.errors[0].message || t.errors[0]).split('\n')[0]}`); continue; }
      const used = new Set([...t.code.matchAll(/_ctx\.([A-Za-z_$][\w$]*)/g)].map((m) => m[1]));
      const missing = [...used].filter((n) => !GLOBALS.has(n.replace(/^\$/, '')));
      if (missing.length) problems.push(`${file}: used by the template, not defined: ${missing.join(', ')}`);
    }
    result[set.name] = problems;
  }
  process.stdout.write(JSON.stringify(result));
});
"""


def feature_sets():
    names = [f.name for f in features.FEATURES]
    sets = [
        {"name": "none", "features": []},
        {
            "name": "all",
            "features": list(features.resolve(BOARD, all_features=True).enabled),
        },
    ]
    for name in names:
        sets.append(
            {
                "name": f"only {name}",
                "features": list(features.resolve(BOARD, [name]).enabled),
            }
        )
        try:
            enabled = list(
                features.resolve(BOARD, all_features=True, excluded=[name]).enabled
            )
        except features.FeatureError:
            continue  # something else needs it
        if name not in enabled:
            sets.append({"name": f"all without {name}", "features": enabled})
    for combination in COMBINATIONS:
        if not all(name in names for name in combination):
            continue  # a line of this repository without some of the options
        sets.append(
            {
                "name": "+".join(combination),
                "features": list(features.resolve(BOARD, list(combination)).enabled),
            }
        )
    seen, unique = set(), []
    for entry in sets:
        key = tuple(sorted(entry["features"]))
        if key not in seen:
            seen.add(key)
            unique.append(entry)
    return unique


def main():
    verbose = "-v" in sys.argv[1:]
    sets = feature_sets()
    run = subprocess.run(
        ["node", "--input-type=module", "-e", CHECK],
        cwd=ROOT,
        input=json.dumps(sets).encode("utf-8"),
        capture_output=True,
    )
    if run.returncode != 0:
        print(run.stderr.decode("utf-8", errors="replace"))
        return 2
    result = json.loads(run.stdout.decode("utf-8"))
    if "error" in result:
        print(result["error"])
        return 2
    bad = 0
    for entry in sets:
        problems = result[entry["name"]]
        if problems:
            bad += 1
            print(f"FAIL {entry['name']} ({len(entry['features'])} features)")
            for problem in problems:
                print(f"       {problem}")
        elif verbose:
            print(f"ok   {entry['name']}")
    print(
        f"{len(sets)} feature sets checked, {bad} with an undefined name"
        if bad
        else f"{len(sets)} feature sets checked, every name a template or a script uses is defined"
    )
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
