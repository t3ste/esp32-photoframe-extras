// Build-time feature switches for the web app.
//
// Parts of the UI that belong to an optional firmware feature are fenced with
// directives in comments, in the same spirit as the C preprocessor:
//
//   <!-- #if FEATURE_TELEGRAM -->   (in a <template> or html file)
//   // #if FEATURE_TELEGRAM && FEATURE_OVERLAYS   (in a script)
//   /* #if FORK_ANY */   (in a style)
//   ... #else ... #endif
//
// build.py hands the enabled features over as VITE_FEATURES (comma separated, the
// names of `build.py --with`). This plugin removes the fenced parts of every source
// file that is not enabled *before* Vue compiles it, so a build without features
// contains exactly the upstream UI - not a hidden copy of it.

const DIRECTIVE = /^\s*(?:<!--\s*|\/\/\s*|\/\*\s*)#(if|else|endif)\b(.*?)\s*(?:-->|\*\/)?\s*$/;
const NAME = /\b(?:FEATURE|FORK)_[A-Z0-9_]+\b/g;

// Flags of the features that are on, plus the derived ones main/feature_config.h has.
export function flagsFor(featureList) {
  const names = (featureList || "")
    .split(",")
    .map((n) => n.trim())
    .filter(Boolean);
  const flags = new Set(
    names.map((n) =>
      n === "fixes" ? "FORK_FIXES" : `FEATURE_${n.toUpperCase().replace(/-/g, "_")}`
    )
  );
  if (flags.size > 0) flags.add("FORK_ANY");
  if (flags.has("FEATURE_TELEGRAM") || flags.has("FEATURE_OVERLAYS")) flags.add("FORK_EXIF");
  return flags;
}

function evaluate(expression, flags) {
  const source = expression.replace(NAME, (name) => (flags.has(name) ? "true" : "false"));
  if (!/^[\s()!&|truefals]*$/.test(source)) {
    throw new Error(`unsupported feature expression: ${expression}`);
  }
  return Function(`return (${source});`)();
}

// Returns the text with every fenced part of a disabled feature removed.
export function applyDirectives(code, flags) {
  if (!code.includes("#if ")) return code;
  const out = [];
  const stack = []; // { parent, condition }
  const active = () => stack.every((s) => s.parent && s.branch);
  for (const line of code.split("\n")) {
    const match = DIRECTIVE.exec(line);
    if (!match) {
      if (active()) out.push(line);
      continue;
    }
    const [, kind, rest] = match;
    if (kind === "if") {
      stack.push({ parent: true, branch: evaluate(rest.trim(), flags) });
    } else if (kind === "else") {
      const top = stack[stack.length - 1];
      top.branch = !top.branch;
    } else {
      stack.pop();
    }
  }
  return out.join("\n");
}

export function featureDirectives(featureList) {
  const flags = flagsFor(featureList);
  return {
    name: "feature-directives",
    enforce: "pre",
    transform(code, id) {
      if (!/\.(vue|js|html)$/.test(id.split("?")[0]) || id.includes("node_modules")) return null;
      const result = applyDirectives(code, flags);
      return result === code ? null : { code: result, map: null };
    },
  };
}
