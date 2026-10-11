// Cuts the icon font down to the icons the Web UI uses: writes icons.json and mdi-subset.woff2 next to this
// file (see plugin.js). Run it after an icon was added to or removed from the sources:
//
//   cd webapp && npm run mdi-subset
//
// Needs Python with fonttools and brotli (pip install fonttools brotli); PYTHON=<command> picks the interpreter.

import { spawnSync } from "node:child_process";
import { writeFileSync } from "node:fs";
import { SUBSET_FONT, SUBSET_LIST, glyphsOf, mdiPackage, usedNames } from "./plugin.js";

const { version, css, woff2 } = mdiPackage();
const glyphs = glyphsOf(css);
const icons = [...usedNames()].filter((n) => glyphs.has(n)).sort();
if (!icons.length) throw new Error("no icons found in the sources");

const python = process.env.PYTHON || (process.platform === "win32" ? "python" : "python3");
const unicodes = icons.map((n) => `U+${glyphs.get(n)}`).join(",");
const run = spawnSync(
  python,
  [
    "-m",
    "fontTools.subset",
    woff2,
    `--unicodes=${unicodes}`,
    "--flavor=woff2",
    "--layout-features=",
    "--no-hinting",
    "--desubroutinize",
    `--output-file=${SUBSET_FONT}`,
  ],
  { stdio: "inherit" }
);
if (run.status !== 0) {
  console.error("fonttools failed: pip install fonttools brotli");
  process.exit(run.status || 1);
}
writeFileSync(SUBSET_LIST, JSON.stringify({ mdiVersion: version, icons }, null, 2) + "\n");
console.log(`mdi-subset: ${icons.length} icons of @mdi/font ${version}`);
