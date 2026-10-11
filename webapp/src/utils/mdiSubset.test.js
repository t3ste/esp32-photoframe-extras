import { describe, it, expect } from "vitest";
import {
  glyphsOf,
  mdiPackage,
  readSubset,
  slimCss,
  subsetProblems,
  usedNames,
} from "../../mdi-subset/plugin.js";

// The Web UI ships a copy of the Material Design Icons font that holds only the icons it uses
// (webapp/mdi-subset). These tests fail when an icon was added to the sources without `npm run mdi-subset`.

describe("icon subset", () => {
  it("holds every icon the sources use, and nothing the font lacks", () => {
    expect(subsetProblems()).toEqual([]);
  });

  it("is a real subset", () => {
    const { icons } = readSubset();
    const all = glyphsOf(mdiPackage().css);
    expect(icons.length).toBeGreaterThan(50);
    expect(icons.length).toBeLessThan(all.size / 10);
  });

  it("finds the icons of Vuetify's own icon set", () => {
    const used = usedNames();
    expect(used.has("menu-down")).toBe(true);
    expect(used.has("checkbox-marked")).toBe(true);
  });
});

describe("slimCss", () => {
  const css = `@font-face {
  font-family: "Material Design Icons";
  src: url("../fonts/x.eot?v=1");
  font-weight: normal;
}

.mdi:before,
.mdi-set {
  display: inline-block;
}

.mdi-alpha::before {
  content: "\\F0001";
}

.mdi-beta::before {
  content: "\\F0002";
}

.mdi-spin:before {
  animation: mdi-spin 2s infinite linear;
}
`;

  it("keeps the rules of the subset, the helpers and swaps the font", () => {
    const out = slimCss(css, ["beta"], new Uint8Array([1, 2, 3]));
    expect(out).toContain(".mdi-beta::before");
    expect(out).not.toContain(".mdi-alpha::before");
    expect(out).toContain(".mdi-spin:before");
    expect(out).toContain(".mdi-set");
    expect(out).toContain("url(data:font/woff2;base64,AQID)");
    expect(out).not.toContain("x.eot");
  });
});
