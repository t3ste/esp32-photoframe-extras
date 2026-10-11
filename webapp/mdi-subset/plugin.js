// The icon font of the Web UI, cut down to the icons the UI uses (build option `fixes`).
//
// `@mdi/font` holds 7,448 icons - 403 KB of font plus 7,462 CSS rules - and the UI uses about a hundred of
// them, so the stylesheet that the firmware embeds was 596 KB for the icons alone. This module
//
//   * finds the icons the sources use (`usedNames`): every `mdi-<name>` in src/, index.html and public/*.html,
//     and the names Vuetify's own icon set (`vuetify/lib/iconsets/mdi.js`) asks for,
//   * `mdiSubset()` is a Vite plugin that rewrites `@mdi/font/css/materialdesignicons.css` to the rules of the
//     icons in `icons.json` and a font-face with `mdi-subset.woff2` inlined (the file is a copy of the font
//     with only these glyphs), and refuses to build when the sources use an icon that is not in the subset.
//
// Adding an icon to the UI therefore needs `npm run mdi-subset` (generate.mjs) once; the build and
// `src/utils/mdiSubset.test.js` say so when it was forgotten.

import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const WEBAPP = resolve(HERE, "..");

export const SUBSET_FONT = join(HERE, "mdi-subset.woff2");
export const SUBSET_LIST = join(HERE, "icons.json");

const MDI_CSS = /[\\/]@mdi[\\/]font[\\/]css[\\/]materialdesignicons\.css$/;
const ICON_RULE = /\.mdi-([a-z0-9-]+)::before\s*\{\s*content:\s*"\\([0-9A-Fa-f]+)";\s*\}\s*/g;
const FONT_FACE = /@font-face\s*\{[^}]*\}/;
const NAME = /\bmdi-[a-z0-9]+(?:-[a-z0-9]+)*/g;

function walk(dir, out = []) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) walk(path, out);
    else if (/\.(vue|js|html)$/.test(entry.name) && !/\.test\.js$/.test(entry.name)) out.push(path);
  }
  return out;
}

// name -> code point (hex) of every icon rule of the font's stylesheet text
export function glyphsOf(css) {
  const glyphs = new Map();
  for (const m of css.matchAll(ICON_RULE)) glyphs.set(m[1], m[2].toUpperCase());
  return glyphs;
}

export function mdiPackage(webapp = WEBAPP) {
  const dir = join(webapp, "node_modules", "@mdi", "font");
  return {
    version: JSON.parse(readFileSync(join(dir, "package.json"), "utf-8")).version,
    css: readFileSync(join(dir, "css", "materialdesignicons.css"), "utf-8"),
    woff2: join(dir, "fonts", "materialdesignicons-webfont.woff2"),
  };
}

// Names (without the `mdi-` prefix) the sources of the Web UI refer to. Includes modifier classes
// (`mdi-spin`, `mdi-18px`) and anything else that merely looks like a name: filter with `glyphsOf`.
export function usedNames(webapp = WEBAPP) {
  const files = walk(join(webapp, "src"));
  files.push(join(webapp, "index.html"));
  const publicDir = join(webapp, "public");
  if (existsSync(publicDir)) {
    for (const f of readdirSync(publicDir)) if (f.endsWith(".html")) files.push(join(publicDir, f));
  }
  const vuetifyIcons = join(webapp, "node_modules", "vuetify", "lib", "iconsets", "mdi.js");
  if (!existsSync(vuetifyIcons)) throw new Error(`${vuetifyIcons} is missing: run npm ci`);
  files.push(vuetifyIcons);
  const names = new Set();
  for (const file of files) {
    for (const m of readFileSync(file, "utf-8").matchAll(NAME)) names.add(m[0].slice(4));
  }
  return names;
}

export function readSubset() {
  return JSON.parse(readFileSync(SUBSET_LIST, "utf-8"));
}

// The icons the sources use that the subset font lacks, and what else is wrong with it.
export function subsetProblems(webapp = WEBAPP) {
  const { version, css } = mdiPackage(webapp);
  const subset = readSubset();
  const known = glyphsOf(css);
  const problems = [];
  if (subset.mdiVersion !== version) {
    problems.push(
      `the subset was cut from @mdi/font ${subset.mdiVersion}, ${version} is installed`
    );
  }
  const have = new Set(subset.icons);
  const missing = [...usedNames(webapp)].filter((n) => known.has(n) && !have.has(n)).sort();
  if (missing.length)
    problems.push(`not in the subset: ${missing.map((n) => `mdi-${n}`).join(", ")}`);
  const gone = subset.icons.filter((n) => !known.has(n));
  if (gone.length)
    problems.push(`not in @mdi/font any more: ${gone.map((n) => `mdi-${n}`).join(", ")}`);
  return problems;
}

// The stylesheet text of @mdi/font with the rules of the subset's icons and the subset font inlined
export function slimCss(css, icons, fontBytes) {
  const have = new Set(icons);
  const rules = css.replace(ICON_RULE, (rule, name) => (have.has(name) ? rule : ""));
  const face =
    '@font-face{font-family:"Material Design Icons";' +
    `src:url(data:font/woff2;base64,${Buffer.from(fontBytes).toString("base64")}) format("woff2");` +
    "font-weight:normal;font-style:normal}";
  return rules.replace(FONT_FACE, face);
}

export function mdiSubset(webapp = WEBAPP) {
  return {
    name: "mdi-subset",
    enforce: "pre",
    transform(code, id) {
      if (!MDI_CSS.test(id.split("?")[0])) return null;
      const problems = subsetProblems(webapp);
      if (problems.length) {
        this.error(
          `The icon subset (webapp/mdi-subset) does not fit the sources: ${problems.join("; ")}. ` +
            "Run `npm run mdi-subset` in webapp/ (needs Python with fonttools and brotli)."
        );
      }
      return { code: slimCss(code, readSubset().icons, readFileSync(SUBSET_FONT)), map: null };
    },
  };
}
