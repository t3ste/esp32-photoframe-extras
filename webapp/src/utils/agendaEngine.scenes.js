// The scenes agendaEngine.test.js draws and compares with the firmware's own renderer. They are the sample data of the
// host render harness that draws the real agenda_renderer.c (the pictures of docs/SCREENSHOTS.md: a made-up
// household, work, sport, school and chores calendar, Wednesday 2026-09-30, 08:02), written down once, so the
// engine and the renderer are fed the same Agenda. agendaEngine.hashes.json holds the SHA-256 of what the real
// renderer drew for each scene (see scripts/gen_agenda_engine_hashes.py).

const WEEK = [false, false, false, false, false, false, false];

// [calendar A..E = 0..4, days after today, "HH:MM" (null: all day), minutes (all-day: days), summary]
const EVENTS = [
  [0, 0, "07:30", 30, "Breakfast together"],
  [0, 0, "18:30", 60, "Cook dinner"],
  [0, 1, "16:00", 60, "Piano lesson"],
  [0, 1, "20:00", 120, "Movie night"],
  [0, 2, "17:00", 45, "Library visit"],
  [0, 3, "09:00", 60, "Farmers market"],
  [0, 4, "13:00", 120, "Sunday roast lunch"],
  [0, 5, "19:00", 60, "Video call with family"],
  [0, 6, "08:30", 60, "Morning walk"],
  [1, 0, "09:00", 15, "Daily stand-up"],
  [1, 0, "14:00", 60, "Project review"],
  [1, 1, "09:00", 15, "Daily stand-up"],
  [1, 1, "10:30", 90, "Sprint planning"],
  [1, 2, "09:00", 15, "Daily stand-up"],
  [1, 2, "15:00", 45, "Weekly retro"],
  [1, 5, null, 2, "Team offsite"],
  [2, 1, "18:00", 90, "Football training"],
  [2, 3, "11:30", 90, "Weekend match"],
  [3, 0, "08:00", 300, "School"],
  [3, 1, "08:00", 300, "School"],
  [3, 2, "08:00", 240, "School (short day)"],
  [3, 5, "08:00", 300, "School"],
  [3, 6, "08:00", 300, "School"],
  [4, 1, "07:00", 15, "Recycling bin collection"],
  [4, 2, "17:30", 60, "Weekly shopping"],
].map(([cal, day, start, minutes, summary]) =>
  start === null
    ? { cal, day, start, days: minutes, summary }
    : { cal, day, start, minutes, summary }
);

const HIGHS = [19, 16, 21, 21, 22, 20, 17];
const LOWS = [11, 11, 8, 11, 11, 10, 7];
const CODES = [95, 61, 45, 2, 1, 3, 80];

export const BASE_SCENE = {
  width: 800,
  height: 480,
  layout: "gridA",
  startDate: "2026-09-30",
  nowTime: "08:02",
  marked: WEEK,
  calendars: [
    { name: "Home", show: true },
    { name: "Work", show: true },
    { name: "C", show: true },
    { name: "D", show: true },
    { name: "E", show: true },
  ],
  events: EVENTS,
  weather: HIGHS.map((max, i) => ({ min: LOWS[i], max, code: CODES[i] })),
  iconSet: "flaticon",
  iconColored: true,
  weatherRight: false,
  language: "en",
  showWeek: true,
  timeMode: "duration",
  multiday: "compact",
  priority: "ABCDE",
  orderTime: false,
  climate: null,
};

const COMMON = {
  type: "spectra6-firmware-profile",
  version: 1,
  headerFollowsEntries: true,
  headerFollowsEntriesColorSafeInk: true,
  dayheadLeaderLine: true,
  headerDividerFilled: true,
};
const WHITE_CALS = { aBg: "white", bBg: "white", cBg: "white", dBg: "white", eBg: "white" };

// the profile built into the firmware (main/resources/default_agenda_color_profile.json) and the three examples
// of examples/waveshare_photopainter_73/color_profiles/
export const PROFILES = {
  default: {
    ...COMMON,
    name: "s2_BETA",
    mode: "color",
    mark: "yellow",
    headerFollowsEntriesColor: false,
    markColorsHeader: false,
    colors: {
      text: "black",
      textBg: "white",
      headerText: "white",
      headerBg: "black",
      topText: "black",
      topBg: "white",
      a: "blue",
      b: "green",
      c: "red",
      d: "red",
      e: "red",
      ...WHITE_CALS,
      iconBg: "#000000",
      iconBgMarked: "#000000",
    },
  },
  bw: {
    ...COMMON,
    name: "s1_spectra6",
    mode: "color",
    mark: "white",
    headerFollowsEntriesColor: true,
    markColorsHeader: false,
    colors: {
      text: "white",
      textBg: "black",
      headerText: "#000000",
      headerBg: "#ffffff",
      topText: "#ffffff",
      topBg: "#000000",
      a: "#0000ff",
      b: "#ff0000",
      c: "#ffc840",
      d: "#00ff00",
      e: "#00ff00",
      aBg: "black",
      bBg: "black",
      cBg: "black",
      dBg: "black",
      eBg: "black",
      iconBg: "#000000",
      iconBgMarked: "#ffffff",
    },
  },
  y: {
    ...COMMON,
    name: "s2_spectra6",
    mode: "color",
    mark: "yellow",
    headerFollowsEntriesColor: false,
    markColorsHeader: false,
    colors: {
      text: "black",
      textBg: "white",
      headerText: "white",
      headerBg: "black",
      topText: "black",
      topBg: "white",
      a: "blue",
      b: "red",
      c: "green",
      d: "green",
      e: "green",
      ...WHITE_CALS,
      iconBg: "#000000",
      iconBgMarked: "#000000",
    },
  },
  b: {
    ...COMMON,
    name: "s3_spectra6",
    mode: "color",
    mark: "blue",
    headerFollowsEntriesColor: false,
    markColorsHeader: true,
    colors: {
      text: "black",
      textBg: "white",
      headerText: "white",
      headerBg: "black",
      topText: "#000000",
      topBg: "#ffffff",
      a: "blue",
      b: "#ff0000",
      c: "#00ff00",
      d: "#00ff00",
      e: "#00ff00",
      ...WHITE_CALS,
      iconBg: "#000000",
      iconBgMarked: "#0000ff",
    },
  },
};

// the cases of the harness (its names) and what they change in the base scene; the shift model marks these days
// of the week from today: 3/4 model from Monday 2026-09-28
const MARKED_3_4 = [false, true, true, true, true, true, true];
export const CASES = [
  { harness: "agenda-grid-a", changes: { layout: "gridA" } },
  { harness: "agenda-grid-b", changes: { layout: "gridB" } },
  { harness: "agenda-grid-a-marked", changes: { layout: "gridA", marked: MARKED_3_4 } },
  { harness: "agenda-grid-a-prio-d", changes: { layout: "gridA", priority: "DEBAC" } },
  { harness: "agenda-grid-a-noweek", changes: { layout: "gridA", showWeek: false } },
];

export const SIZES = [
  [800, 480],
  [480, 800],
  [960, 540],
  [1600, 1200],
  [1200, 1600],
];

export function sceneFor(harnessCase, profileKey, size) {
  const c = CASES.find((x) => x.harness === harnessCase);
  const [width, height] = size;
  return { ...BASE_SCENE, ...c.changes, width, height, profile: PROFILES[profileKey] };
}
