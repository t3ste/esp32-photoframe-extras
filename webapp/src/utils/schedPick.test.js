import { describe, expect, it } from "vitest";

import fixtures from "./schedPick.fixtures.json";
import { clampHold, matchTable, pickDrawn } from "./schedPick";
import { buildEntities, computeWeek, stripOf } from "./scheduleTimeline";

// The settings of the editor for one fixture case: every Agenda schedule gets a page, so the
// schedules are in force (without a page on any schedule there is no minimum time and no hold).
function settingsOf(c) {
  return {
    autoRotate: c.rotation.length > 0,
    rotateCron: c.rotation,
    agendaCron: c.agenda.map(([rule]) => rule),
    agendaCronPages: c.agenda.map(() => ["weather"]),
    agendaCronHold: c.agenda.map(([, hold]) => hold),
    agendaGapMin: c.gap,
    agendaTodoEnabled: true,
    agendaCalEnabled: false,
    infoScreens: [],
  };
}

describe("clampHold", () => {
  it("keeps whole minutes between 0 and 240", () => {
    expect(clampHold(15)).toBe(15);
    expect(clampHold("30")).toBe(30);
    expect(clampHold(-3)).toBe(0);
    expect(clampHold(999)).toBe(240);
    expect(clampHold("")).toBe(0);
    expect(clampHold(NaN)).toBe(0);
  });
});

describe("pickDrawn", () => {
  const line = (...minutes) => {
    const a = new Uint8Array(300);
    minutes.forEach((m) => (a[m] = 1));
    return a;
  };
  const drawnAt = (drawn) => drawn.map((d) => [...d.keys()].filter((g) => d[g]));

  it("lets the higher schedule win in the same minute", () => {
    expect(drawnAt(pickDrawn([line(100), line(100)], [0, 0]))).toEqual([[100], []]);
  });
  it("holds off a lower fire within the hold of a higher draw, not at exactly the hold", () => {
    expect(drawnAt(pickDrawn([line(100), line(109)], [10, 0]))).toEqual([[100], []]);
    expect(drawnAt(pickDrawn([line(100), line(110)], [10, 0]))).toEqual([[100], [110]]);
  });
  it("skips a fire whose own hold would be cut short by a higher draw after it", () => {
    expect(drawnAt(pickDrawn([line(105), line(100)], [0, 10]))).toEqual([[105], []]);
    expect(drawnAt(pickDrawn([line(110), line(100)], [0, 10]))).toEqual([[110], [100]]);
  });
  it("does not let a held-off fire hold off a lower one", () => {
    // 100 draws (hold 15), 110 is held off, 120 is 20 minutes after 100: drawn
    expect(drawnAt(pickDrawn([line(100), line(110), line(120)], [15, 15, 15]))).toEqual([
      [100],
      [],
      [120],
    ]);
  });
  it("never holds a schedule off by its own fires", () => {
    expect(drawnAt(pickDrawn([line(100, 105, 110)], [60]))).toEqual([[100, 105, 110]]);
  });
  it("handles no schedules", () => {
    expect(pickDrawn([], [])).toEqual([]);
  });
});

describe("matchTable", () => {
  it("matches minute, hour and weekday of a timeline that starts on the given weekday", () => {
    // starts on Saturday (6): minute 0 = Sat 00:00; "30 7 1" = Monday 07:30 = two days later
    const [m] = matchTable([["30 7 1"]], 4 * 1440, 6);
    const hits = [...m.keys()].filter((g) => m[g]);
    expect(hits).toEqual([2 * 1440 + 7 * 60 + 30]);
  });
  it("ignores a rule that does not parse", () => {
    const [m] = matchTable([["not a rule", "61 7 *"]], 1440, 0);
    expect(m.some(Boolean)).toBe(false);
  });
});

describe("computeWeek against the firmware's scheduler (fixtures of scripts/gen_sched_fixtures.py)", () => {
  fixtures.cases.forEach((c, n) => {
    it(`case ${n + 1}: ${c.agenda.length} schedules, ${c.rotation.length} rotation rules, gap ${c.gap}`, () => {
      const week = computeWeek(settingsOf(c));
      expect(week.days).toHaveLength(7);
      week.days.forEach((day, d) => {
        const got = day.fires.map((f) => [f.minute, f.id, f.drawn ? 1 : 0]);
        expect(got).toEqual(c.days[d]);
      });
    });
  });
});

describe("buildEntities", () => {
  const base = {
    autoRotate: true,
    rotateCron: ["0 8 *"],
    agendaCron: ["0 9 *", "0 10 *"],
    agendaCronPages: [["weather"], []],
    agendaCronHold: [30, 0],
    agendaGapMin: 15,
    agendaTodoEnabled: true,
    agendaCalEnabled: false,
    infoScreens: [],
  };
  it("gives each stored rule its own priority and puts the photo rotation last", () => {
    const { entities } = buildEntities(base);
    expect(entities.map((e) => e.id)).toEqual(["S1", "S2", "R"]);
    expect(entities.map((e) => e.hold)).toEqual([30, 15, 15]);
    expect(entities[0].title).toBe("Weather");
    expect(entities[1].title).toBe("Shared rotation");
  });
  it("applies no minimum time and no hold while no schedule has a page", () => {
    const { entities, inUse, gap } = buildEntities({ ...base, agendaCronPages: [[], []] });
    expect(inUse).toBe(false);
    expect(gap).toBe(0);
    expect(entities.map((e) => e.hold)).toEqual([0, 0, 0]);
  });
  it("leaves the Agenda's schedules out while the Agenda is off", () => {
    const off = { ...base, agendaTodoEnabled: false, agendaCronPages: [[], []] };
    expect(buildEntities(off).entities.map((e) => e.id)).toEqual(["R"]);
  });
  it("leaves the photo rotation out while Auto Rotate is off", () => {
    expect(buildEntities({ ...base, autoRotate: false }).entities.map((e) => e.id)).toEqual([
      "S1",
      "S2",
    ]);
  });
  it("takes a cleared minimum time as 0", () => {
    expect(buildEntities({ ...base, agendaGapMin: "" }).gap).toBe(0);
  });
});

describe("stripOf", () => {
  it("shows what is on the display from midnight to midnight, with the hold part of each draw", () => {
    const week = computeWeek({
      autoRotate: false,
      rotateCron: [],
      agendaCron: ["0 6 *", "0 18 *"],
      agendaCronPages: [["weather"], ["fact"]],
      agendaCronHold: [60, 0],
      agendaGapMin: 15,
      agendaTodoEnabled: true,
      agendaCalEnabled: false,
      infoScreens: [],
    });
    const strip = stripOf(week.days[2], week.entities);
    expect(strip).toEqual([
      { id: "S2", from: 0, to: 360, hold: 0, carried: true },
      { id: "S1", from: 360, to: 1080, hold: 60, carried: false },
      { id: "S2", from: 1080, to: 1440, hold: 15, carried: false },
    ]);
  });
});
