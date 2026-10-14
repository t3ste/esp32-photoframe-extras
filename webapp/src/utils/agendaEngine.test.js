import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import * as engine from "./agendaEngine.js";
import { CASES, PROFILES, SIZES, sceneFor } from "./agendaEngine.scenes.js";
import hashes from "./agendaEngine.hashes.json";

// The colour profile editor draws its preview with this engine, a JavaScript port of the firmware's Agenda
// renderer. agendaEngine.hashes.json holds the SHA-256 of what the real agenda_renderer.c drew for the same
// scenes (scripts/gen_agenda_engine_hashes.py), so a preview that stops matching the frame fails here.

const sha = (rgb) => createHash("sha256").update(rgb).digest("hex");
const keyOf = (c, profile, size) => `${c.harness}|${profile}|${size[0]}x${size[1]}`;

// the engine as the editor page carries it (scripts/sync_profile_editor.py copies it between two markers)
const page = readFileSync(
  resolve(dirname(fileURLToPath(import.meta.url)), "../../public/profile-editor.html"),
  "utf8"
);
const block = page.slice(
  page.indexOf("// ==== ENGINE BEGIN"),
  page.indexOf("// ==== ENGINE END ====")
);
const inline = new Function(`${block}\nreturn AgendaEngine;`)();

const cases = [];
for (const profile of Object.keys(PROFILES)) {
  for (const size of SIZES) {
    if (profile !== "default" && size[0] !== 800) continue; // the other sizes: built-in profile only
    for (const c of CASES) cases.push([keyOf(c, profile, size), c, profile, size]);
  }
}

describe.each([
  ["the module", engine],
  ["the copy in profile-editor.html", inline],
])("Agenda engine: %s", (_name, impl) => {
  it("has a reference hash for every scene it draws", () => {
    expect(Object.keys(hashes).sort()).toEqual(cases.map((c) => c[0]).sort());
  });

  it.each(cases)("draws %s like the firmware", (key, c, profile, size) => {
    const picture = impl.renderAgenda(sceneFor(c.harness, profile, size));
    expect(picture.width).toBe(size[0]);
    expect(picture.height).toBe(size[1]);
    expect(sha(picture.rgb)).toBe(hashes[key]);
  });
});

describe("stateColors", () => {
  const colors = (profile) => engine.stateColors(PROFILES[profile]);

  it("lists the colours of the built-in profile", () => {
    const s = colors("default");
    expect(s.top).toEqual({ bg: "#ffffff", fg: "#000000" });
    expect(s.head.normal).toEqual({ bg: "#000000", fg: "#ffffff" });
    expect(s.body.normal).toEqual({ bg: "#ffffff", fg: "#000000" });
    // yellow shift model: the day area turns yellow, the text stays readable
    expect(s.body.marked).toEqual({ bg: "#ffff00", fg: "#000000" });
    expect(s.calendars[0].normal).toEqual({ bg: "#ffffff", fg: "#0000ff" });
    expect(s.calendars[0].marked).toEqual({ bg: "#ffff00", fg: "#0000ff" });
    expect(s.icon.normal).toBe("#000000");
  });

  it("marks only the header when 'markColorsHeader' is on", () => {
    const s = colors("b");
    expect(s.head.marked.bg).toBe("#0000ff");
    expect(s.body.marked).toEqual(s.body.normal);
    expect(s.calendars[0].marked).toEqual(s.calendars[0].normal);
    expect(s.icon.marked).toBe("#0000ff");
  });

  it("follows the entry colours in the header when asked to", () => {
    const s = colors("bw");
    expect(s.head.normal).toEqual({ bg: s.body.normal.bg, fg: s.body.normal.fg });
    expect(s.head.marked.bg).toBe("#ffffff");
  });

  it("is the same in the copy in profile-editor.html", () => {
    for (const profile of Object.keys(PROFILES)) {
      expect(inline.stateColors(PROFILES[profile])).toEqual(engine.stateColors(PROFILES[profile]));
    }
  });

  it("draws black and white only for a monochrome profile", () => {
    const mono = {
      ...PROFILES.default,
      mode: "mono-normal",
      mark: "black",
      headerFollowsEntries: false,
    };
    const s = engine.stateColors(mono);
    expect(s.mono).toBe(true);
    expect(s.head.normal).toEqual({ bg: "#000000", fg: "#ffffff" });
    expect(s.body.marked).toEqual({ bg: "#000000", fg: "#ffffff" });
    const picture = engine.renderAgenda({
      ...sceneFor("agenda-grid-a-marked", "default", [800, 480]),
      profile: mono,
    });
    const seen = new Set();
    for (let i = 0; i < picture.rgb.length; i += 3)
      seen.add(picture.rgb[i] * 65536 + picture.rgb[i + 1] * 256 + picture.rgb[i + 2]);
    expect([...seen].sort()).toEqual([0, 0xffffff]);
  });
});

describe("parseColor and isoWeek", () => {
  it("reads palette names and #rrggbb only", () => {
    expect(engine.parseColor("red")).toEqual([255, 0, 0]);
    expect(engine.parseColor("#0a141e")).toEqual([10, 20, 30]);
    expect(engine.parseColor("orange")).toBeNull();
    expect(engine.parseColor("#12345")).toBeNull();
    expect(engine.parseColor(5)).toBeNull();
  });

  it("numbers the ISO weeks like the firmware", () => {
    expect(engine.isoWeek(2026, 8, 30)).toBe(40); // 2026-09-30
    expect(engine.isoWeek(2027, 0, 1)).toBe(53); // 2027-01-01 belongs to the last week of 2026
    expect(engine.isoWeek(2024, 11, 30)).toBe(1); // 2024-12-30 belongs to week 1 of 2025
  });
});
