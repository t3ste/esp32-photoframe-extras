// The week the frame will follow, from the schedule settings as they stand in the editor (saved or
// not): which schedule draws when, what stays on the display and for how long. Build option
// schedule-pages. The decision itself is schedPick.js (a port of the firmware's sched_pick.c).
//
// Times are wall-clock times of the frame (its time zone setting), a week from Monday to Sunday.
// Daylight saving time changes are not modelled.

import { clampHold, matchTable, pickDrawn } from "./schedPick";

export const DAY_NAMES = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
// weekday of the first drawn day (Monday); the timeline starts PAD_DAYS earlier and ends as many later
const FIRST_DOW = 1;
const PAD_DAYS = 2;

export const PAGE_TITLES = {
  agenda: "ToDo & Calendar",
  "chore-wheel": "Chore wheel",
  weather: "Weather",
  fact: "Fact of the day",
  finance: "Exchange rates",
  fuel: "Fuel prices",
  markets: "Markets",
  recipe: "Recipe",
};

const PALETTE = [
  "#1f77b4",
  "#d62728",
  "#2ca02c",
  "#9467bd",
  "#ff7f0e",
  "#17becf",
  "#8c564b",
  "#e377c2",
];

// The colour of a schedule in the timeline: S1, S2, ... take the palette in turn, the photo rotation is grey.
export function colorOf(id) {
  return id === "R" ? "#8a8a8a" : PALETTE[(Number(id.slice(1)) - 1) % PALETTE.length];
}

const asList = (value) => (Array.isArray(value) ? value : []);

// The entities in priority order: the Agenda's schedules (one per stored cron rule) and, when Auto
// Rotate is on, the photo rotation last.
export function buildEntities(s) {
  const infoScreens = asList(s.infoScreens);
  const agendaActive = Boolean(
    s.agendaTodoEnabled ||
    s.agendaCalEnabled ||
    infoScreens.some((name) => name !== "agenda") ||
    asList(s.agendaCronPages).some((pages) => asList(pages).length)
  );
  const inUse = asList(s.agendaCronPages).some((pages) => asList(pages).length > 0);
  // without a page on any schedule nothing of schedule-pages is in force: no minimum time, no holds
  const gap = inUse ? clampHold(s.agendaGapMin) : 0;
  const entities = [];
  if (agendaActive) {
    asList(s.agendaCron).forEach((rule, i) => {
      const pages = asList(asList(s.agendaCronPages)[i]);
      const own = inUse ? clampHold(asList(s.agendaCronHold)[i]) : 0;
      entities.push({
        id: `S${i + 1}`,
        kind: "agenda",
        index: i,
        rules: [rule],
        pages,
        title: pages.length ? pages.map((p) => PAGE_TITLES[p] || p).join(", ") : "Shared rotation",
        hold: own > 0 ? own : gap,
        ownHold: own,
      });
    });
  }
  const rotationRules = asList(s.rotateCron);
  if (s.autoRotate && rotationRules.length) {
    entities.push({
      id: "R",
      kind: "rotation",
      index: -1,
      rules: rotationRules,
      pages: [],
      title: "Photo rotation",
      hold: gap,
      ownHold: 0,
    });
  }
  return { entities, gap, inUse, agendaActive };
}

// { entities, gap, inUse, agendaActive, days } with one entry per day Monday..Sunday:
//   fires: every fire of an entity { minute, id, drawn }, draws: the drawn ones in time order,
//   carry: the entity whose display is still up at midnight (null when none was drawn lately).
export function computeWeek(s) {
  const base = buildEntities(s);
  const { entities } = base;
  const total = (7 + 2 * PAD_DAYS) * 1440;
  const startDow = (FIRST_DOW - PAD_DAYS + 7 * PAD_DAYS) % 7;
  const match = matchTable(
    entities.map((e) => e.rules),
    total,
    startDow
  );
  const drawn = pickDrawn(
    match,
    entities.map((e) => e.hold)
  );
  const days = [];
  for (let d = 0; d < 7; d++) {
    const from = (PAD_DAYS + d) * 1440;
    const fires = [];
    const draws = [];
    for (let m = 0; m < 1440; m++) {
      entities.forEach((e, k) => {
        if (match[k][from + m]) {
          fires.push({ minute: m, id: e.id, drawn: Boolean(drawn[k][from + m]) });
          if (drawn[k][from + m]) draws.push({ minute: m, id: e.id });
        }
      });
    }
    let carry = null;
    for (let g = from - 1; g >= 0 && !carry; g--) {
      const k = entities.findIndex((_, i) => drawn[i][g]);
      if (k >= 0) carry = entities[k].id;
    }
    days.push({ dow: (FIRST_DOW + d) % 7, fires, draws, carry });
  }
  return { ...base, days };
}

// The display strip of one day: [{ id, from, to, hold }] in minutes; `from` 0 is the carry-over.
export function stripOf(day, entities) {
  const holdOf = (id) => entities.find((e) => e.id === id)?.hold ?? 0;
  const segments = [];
  const first = day.draws.length ? day.draws[0].minute : 1440;
  if (day.carry && first > 0) {
    segments.push({ id: day.carry, from: 0, to: first, hold: 0, carried: true });
  }
  day.draws.forEach((draw, i) => {
    const to = i + 1 < day.draws.length ? day.draws[i + 1].minute : 1440;
    segments.push({
      id: draw.id,
      from: draw.minute,
      to,
      hold: Math.min(holdOf(draw.id), to - draw.minute),
      carried: false,
    });
  });
  return segments;
}
