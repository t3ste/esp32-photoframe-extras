// A JavaScript port of the firmware's Agenda renderer for the Calendar's 7-day grid (main/agenda_renderer.c,
// main/agenda_order.c, and the text and icon drawing of main/image_processor.c): the colour profile editor
// (webapp/public/profile-editor.html) draws its preview with it, so the preview is the picture the frame draws -
// same 17x24 bitmap font, same cells, same dashes, same truncation, same colours - not a look-alike.
//
// Pure functions, no DOM: renderAgenda(scene) returns { width, height, rgb } (RGB888). agendaEngine.test.js
// compares the result with the firmware's own renderer (a hash of every picture, see scripts/
// gen_agenda_engine_hashes.py); scripts/sync_profile_editor.py copies this file and agendaEngineData.js into
// the editor page between its ENGINE markers, because that page is one standalone file.

import { FONT24_HEX, ICON_HEX } from "./agendaEngineData.js";

const FONT_W = 17;
const FONT_H = 24;
const PAD = 4; // AGENDA_PADDING
const ICON_W = 24;
const ICON_COUNT = 17;
const MARKER_BASE = 1; // WEATHER_ICON_MARKER_BASE: a text byte 1..17 is an icon

const hexBytes = (hex) => Uint8Array.from(hex.match(/../g) || [], (b) => parseInt(b, 16));
const FONT = hexBytes(FONT24_HEX);
const ICONS = {
  flaticon: ICON_HEX.flaticon.map(hexBytes),
  metno: ICON_HEX.metno.map(hexBytes),
};

const NAMED = {
  black: [0, 0, 0],
  white: [255, 255, 255],
  red: [255, 0, 0],
  yellow: [255, 255, 0],
  blue: [0, 0, 255],
  green: [0, 255, 0],
};

// A colour of a profile: one of the six palette names or "#rrggbb" (agenda_color_profile.c parse_color_value)
export function parseColor(value) {
  if (typeof value !== "string") return null;
  if (value[0] === "#") {
    return /^#[0-9a-fA-F]{6}$/.test(value)
      ? [
          parseInt(value.slice(1, 3), 16),
          parseInt(value.slice(3, 5), 16),
          parseInt(value.slice(5, 7), 16),
        ]
      : null;
  }
  return NAMED[value] ? NAMED[value].slice() : null;
}

const same = (a, b) => a[0] === b[0] && a[1] === b[1] && a[2] === b[2];
const isLight = (c) => c[0] * 299 + c[1] * 587 + c[2] * 114 > 127500;
const safeText = (bg) => (isLight(bg) ? [0, 0, 0] : [255, 255, 255]);

// ------------------------------------------------------------------------------------------ the profile
// What agenda_color_profile.c makes of a profile document (type "spectra6-firmware-profile")
export function resolveProfile(doc) {
  const colors = doc.colors || {};
  const get = (key, fallback) => parseColor(colors[key]) || fallback;
  const mode = typeof doc.mode === "string" ? doc.mode : "color";
  const mark = parseColor(typeof doc.mark === "string" && doc.mark !== "none" ? doc.mark : "");
  const calKeys = ["a", "b", "c", "d", "e"];
  const iconBg = get("iconBg", [255, 255, 255]);
  return {
    mono: mode.startsWith("mono"),
    monoInvert: mode === "mono-invert",
    hasMark: Boolean(mark),
    mark: mark || [0, 0, 0],
    markColorsHeader: doc.markColorsHeader === true,
    headerDividerFilled: doc.headerDividerFilled === true,
    headerFollowsEntries: doc.headerFollowsEntries === true,
    headerFollowsEntriesColor: doc.headerFollowsEntriesColor === true,
    headerFollowsEntriesColorSafeInk: doc.headerFollowsEntriesColorSafeInk === true,
    dayheadLeaderLine: doc.dayheadLeaderLine === true,
    text: get("text", [0, 0, 0]),
    textBg: get("textBg", [255, 255, 255]),
    headerText: get("headerText", [255, 255, 255]),
    headerBg: get("headerBg", [0, 0, 0]),
    topText: get("topText", [255, 255, 255]),
    topBg: get("topBg", [0, 0, 0]),
    calInk: calKeys.map((k) => get(k, [0, 0, 0])),
    calBg: calKeys.map((k) => get(`${k}Bg`, [255, 255, 255])),
    iconBg,
    iconBgMarked: parseColor(colors.iconBgMarked) || iconBg,
  };
}

// ------------------------------------------------------------------------------------------ colours
function monoPair(baseInvert, invertArea) {
  const inv = invertArea ? !baseInvert : baseInvert;
  return inv ? { bg: [0, 0, 0], fg: [255, 255, 255] } : { bg: [255, 255, 255], fg: [0, 0, 0] };
}

function topHeaderColors(p) {
  if (p.mono) return monoPair(p.monoInvert, true);
  return { bg: p.topBg, fg: p.topText };
}

function dayHeaderColors(p, dayMarked) {
  const marked = dayMarked && p.hasMark;
  const follows = p.mono ? p.headerFollowsEntries : p.headerFollowsEntriesColor;
  if (p.mono) return monoPair(p.monoInvert, !(follows && !marked));
  if (p.markColorsHeader && marked) return { bg: p.mark, fg: safeText(p.mark) };
  if (follows && marked) {
    return { bg: p.mark, fg: p.headerFollowsEntriesColorSafeInk ? safeText(p.mark) : p.text };
  }
  if (follows) return { bg: p.textBg, fg: p.text };
  return { bg: p.headerBg, fg: p.headerText };
}

function dayBodyColors(p, dayMarked) {
  const markedHere = dayMarked && p.hasMark && !p.markColorsHeader;
  if (p.mono) return monoPair(p.monoInvert, markedHere);
  if (markedHere) return { bg: p.mark, fg: safeText(p.mark) };
  return { bg: p.textBg, fg: p.text };
}

function eventColors(p, calIndex, bodyMarked, bodyInk) {
  if (p.mono) return { chip: null, ink: bodyInk };
  const ink = p.calInk[calIndex];
  if (bodyMarked) return { chip: null, ink };
  return { chip: p.calBg[calIndex], ink };
}

function iconBackground(p, marked, headBg) {
  if (p.mono) return headBg[0] > 127 ? [0, 0, 0] : [255, 255, 255];
  return marked && p.hasMark ? p.iconBgMarked : p.iconBg;
}

export const toHex = (c) => `#${c.map((v) => v.toString(16).padStart(2, "0")).join("")}`;

// What the frame draws in each state, for the colour table of the editor: the top bar, the day header, the day
// area, the weather icon box and the five calendars, on a normal day and on a day the shift model marks. Every
// entry is { bg, fg } as "#rrggbb"; the same functions draw the picture, so the table cannot disagree with it.
export function stateColors(doc) {
  const p = resolveProfile(doc);
  const pair = (c) => ({ bg: toHex(c.bg), fg: toHex(c.fg) });
  const headNormal = dayHeaderColors(p, false);
  const headMarked = dayHeaderColors(p, true);
  const bodyNormal = dayBodyColors(p, false);
  const bodyMarked = dayBodyColors(p, true);
  const markedHere = p.hasMark && !p.markColorsHeader; // the day area changes colour on a marked day
  const calendar = (i, marked) => {
    const body = marked ? bodyMarked : bodyNormal;
    const e = eventColors(p, i, marked && markedHere, body.fg);
    return { bg: toHex(e.chip || body.bg), fg: toHex(e.ink) };
  };
  return {
    mono: p.mono,
    hasMark: p.hasMark,
    top: pair(topHeaderColors(p)),
    head: { normal: pair(headNormal), marked: pair(headMarked) },
    body: { normal: pair(bodyNormal), marked: pair(bodyMarked) },
    icon: {
      normal: toHex(iconBackground(p, false, headNormal.bg)),
      marked: toHex(iconBackground(p, true, headMarked.bg)),
    },
    calendars: [0, 1, 2, 3, 4].map((i) => ({
      normal: calendar(i, false),
      marked: calendar(i, true),
    })),
  };
}

// ------------------------------------------------------------------------------------------ drawing
const isIconMarker = (code) => code >= MARKER_BASE && code < MARKER_BASE + ICON_COUNT;
const advance = (code) => (isIconMarker(code) ? ICON_W : FONT_W);

function measureText(text) {
  let w = 0;
  for (let i = 0; i < text.length; i++) w += advance(text.charCodeAt(i));
  return w;
}

class Canvas {
  constructor(width, height, opts) {
    this.width = width;
    this.height = height;
    this.rgb = new Uint8Array(width * height * 3);
    this.opts = opts;
  }

  fillRect(x, y, w, h, c) {
    const x0 = Math.max(0, x);
    const y0 = Math.max(0, y);
    const x1 = Math.min(this.width, x + w);
    const y1 = Math.min(this.height, y + h);
    for (let py = y0; py < y1; py++) {
      for (let px = x0; px < x1; px++) {
        const i = (py * this.width + px) * 3;
        this.rgb[i] = c[0];
        this.rgb[i + 1] = c[1];
        this.rgb[i + 2] = c[2];
      }
    }
  }

  blit(x, y, bytes, offset, width, height, c) {
    const perRow = Math.ceil(width / 8);
    for (let row = 0; row < height; row++) {
      for (let col = 0; col < width; col++) {
        if (bytes[offset + row * perRow + (col >> 3)] & (0x80 >> (col & 7))) {
          const px = x + col;
          const py = y + row;
          if (px >= 0 && px < this.width && py >= 0 && py < this.height) {
            const i = (py * this.width + px) * 3;
            this.rgb[i] = c[0];
            this.rgb[i + 1] = c[1];
            this.rgb[i + 2] = c[2];
          }
        }
      }
    }
  }

  glyph(x, y, code, c) {
    if (code < 32 || code > 0x7e) return;
    this.blit(x, y, FONT, (code - 32) * FONT_H * 3, FONT_W, FONT_H, c);
  }

  // image_processor.c draw_weather_icon(): the icon is drawn in the text colour, or - on a colour panel, in
  // colour mode, with coloured icons on - in the traffic-light colour of its condition, unless that is the colour
  // of the box it sits on (avoidBg)
  icon(x, y, id, c, avoidBg) {
    if (id < 0 || id >= ICON_COUNT) return;
    const set = ICONS[this.opts.iconSet] || ICONS.flaticon;
    let color = c;
    if (!this.opts.monoIcons && this.opts.iconColored && !this.opts.grayscaleBoard) {
      const hue = ICON_HUES[id];
      if (hue) {
        color = hue;
        if (avoidBg && same(color, avoidBg)) color = c;
      }
    }
    this.blit(x, y, set[id], 0, ICON_W, 24, color);
  }

  text(x, y, text, c, avoidBg) {
    let cx = x;
    for (let i = 0; i < text.length; i++) {
      const code = text.charCodeAt(i);
      if (isIconMarker(code)) {
        this.icon(cx, y, code - MARKER_BASE, c, avoidBg);
        cx += ICON_W;
      } else {
        this.glyph(cx, y, code, c);
        cx += FONT_W;
      }
    }
  }
}

// the traffic-light colour of an icon (weather_icon_color_for_id): 0 clear..16 thunderstorm with hail; null = the
// colour of the text it stands in
const YELLOW = [255, 255, 0];
const RED = [255, 0, 0];
const BLUE = [0, 0, 255];
const GREEN = [0, 255, 0];
const ICON_HUES = [
  GREEN,
  GREEN,
  null,
  null,
  YELLOW,
  RED,
  BLUE,
  YELLOW,
  RED,
  YELLOW,
  RED,
  BLUE,
  YELLOW,
  RED,
  null,
  YELLOW,
  RED,
];

// ------------------------------------------------------------------------------------------ texts
// WMO weather code -> icon id (weather.c weather_code_to_icon_id)
const CODE_ICON = {
  0: 0,
  1: 1,
  2: 2,
  3: 3,
  45: 4,
  48: 5,
  51: 6,
  61: 6,
  80: 6,
  53: 7,
  63: 7,
  81: 7,
  55: 8,
  65: 8,
  82: 8,
  56: 9,
  66: 9,
  57: 10,
  67: 10,
  71: 11,
  85: 11,
  73: 12,
  75: 13,
  86: 13,
  77: 14,
  95: 15,
  96: 16,
  99: 16,
};
const CONDITION = {
  0: ["Sonne", "sunny"],
  1: ["klar", "clear"],
  2: ["wolkig", "p.cloudy"],
  3: ["bedeckt", "cloudy"],
  45: ["Nebel", "fog"],
  48: ["Nebel", "fog"],
  51: ["Niesel", "lgt.drizzle"],
  53: ["Niesel", "drizzle"],
  55: ["st.Niesel", "hvy.drizzle"],
  61: ["Schauer", "lgt.rain"],
  63: ["Regen", "rain"],
  65: ["st.Regen", "hvy.rain"],
  71: ["Schnee", "lgt.snow"],
  73: ["Schnee", "snow"],
  75: ["st.Schnee", "hvy.snow"],
  80: ["Schauer", "lgt.showers"],
  81: ["Schauer", "showers"],
  82: ["st.Schauer", "hvy.showers"],
  95: ["Gew.", "storm"],
  96: ["Gew.+Hagel", "storm+hail"],
  99: ["st.Gew.+Hagel", "hvy.storm+hail"],
};
const WEEKDAY = {
  de: ["So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"],
  en: ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"],
};

const isLeap = (y) => (y % 4 === 0 && y % 100 !== 0) || y % 400 === 0;
const dayOfYear = (y, mon0, mday) => {
  const before = [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334];
  return before[mon0] + mday + (mon0 > 1 && isLeap(y) ? 1 : 0);
};
// 1 = Monday ... 7 = Sunday
const isoWeekday = (y, mon0, mday) => {
  const t = [0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4];
  const yy = mon0 < 2 ? y - 1 : y;
  const wd =
    (yy + Math.floor(yy / 4) - Math.floor(yy / 100) + Math.floor(yy / 400) + t[mon0] + mday) % 7;
  return wd === 0 ? 7 : wd;
};
const weeksInYear = (y) => {
  const jan1 = isoWeekday(y, 0, 1);
  return jan1 === 4 || (jan1 === 3 && isLeap(y)) ? 53 : 52;
};
export function isoWeek(y, mon0, mday) {
  const week = Math.floor((dayOfYear(y, mon0, mday) - isoWeekday(y, mon0, mday) + 10) / 7);
  if (week < 1) return weeksInYear(y - 1);
  if (week > weeksInYear(y)) return 1;
  return week;
}

function dayLabel(weekday, mday, withWeek, week, german) {
  if (!withWeek) return `${weekday} ${mday}.`;
  return `${weekday} ${mday}. ${german ? "KW" : "CW"}${week}`;
}

// 45m, 1h, 1h30m
function compactDuration(minutes) {
  if (minutes < 60) return `${minutes}m`;
  if (minutes % 60 === 0) return `${minutes / 60}h`;
  return `${Math.floor(minutes / 60)}h${String(minutes % 60).padStart(2, "0")}m`;
}

const two = (n) => String(n).padStart(2, "0");

// agenda_renderer.c build_event_line(): "HH:MM " (+ "[duration] " or the range), then the summary
function eventText(ev, timeMode) {
  let out = "";
  if (!ev.allDay) {
    const startMin = ev.startMin % 1440;
    const hh = Math.floor(startMin / 60);
    const mm = startMin % 60;
    const duration = ev.endMin > ev.startMin ? ev.endMin - ev.startMin : 0;
    if (timeMode === "range" && duration > 0) {
      const endMin = ev.endMin % 1440;
      out += `${two(hh)}:${two(mm)}-${two(Math.floor(endMin / 60))}:${two(endMin % 60)} `;
    } else {
      out += `${two(hh)}:${two(mm)} `;
    }
    if (timeMode === "duration" && duration > 0) out += `[${compactDuration(duration)}] `;
  }
  return (out + ev.summary).slice(0, 255);
}

// image_processor.c wrap_ascii_text() with one line: whole words, then a "~" where text was cut; the part of the
// next word that still fits is kept ("10:00 [1h] Fahrradw~")
function wrapOneLine(text, width) {
  const charsPerLine = Math.min(95, Math.floor((width - 2 * PAD) / FONT_W)); // OVERLAY_LINE_MAX_CHARS - 1
  if (charsPerLine < 1 || !text) return "";
  let current = "";
  let i = 0;
  while (i < text.length) {
    let end = i;
    while (end < text.length && text[end] !== " ") end++;
    const word = text.slice(i, end).slice(0, 95);
    const needed = current.length + (current.length > 0 ? 1 : 0) + word.length;
    if (needed > charsPerLine && current.length > 0) break; // stays in `rest`: the next line would start here
    current += (current.length > 0 ? " " : "") + word;
    i = text[end] === " " ? end + 1 : end;
  }
  if (current.length === 0) return "";
  if (i < text.length) {
    // truncated: mark it
    if (current.length + 1 <= charsPerLine) {
      let avail = charsPerLine - current.length - 1;
      let line = current;
      if (line.length > 0 && avail > 0) {
        line += " ";
        avail--;
      }
      let take = 0;
      while (take < avail && i + take < text.length && text[i + take] !== " ") take++;
      line += text.slice(i, i + take);
      current = line;
    } else {
      current = charsPerLine > 1 ? current.slice(0, charsPerLine - 1) : "";
    }
    current += "~";
  }
  return current;
}

// ------------------------------------------------------------------------------------------ divider / cell
// agenda_renderer.c draw_day_divider(): the day's header row of a cell
function drawDayDivider(
  cv,
  rect,
  y,
  label,
  weatherMode,
  weatherRight,
  weatherText,
  fill,
  ink,
  filled,
  iconBg,
  leader,
  page
) {
  const totalW = rect.w - 2 * PAD;
  const lineY = y + FONT_H / 2 - 1;
  const dashLen = 4;
  const gapLen = 3;
  const dashH = 2;
  if (filled) cv.fillRect(rect.x, y, rect.w, FONT_H + PAD, fill);

  if (!weatherMode) {
    let labelW = Math.min(label.length * FONT_W, totalW);
    const labelX = rect.x + PAD + Math.trunc((totalW - labelW) / 2);
    if (!filled) {
      for (let x = rect.x + PAD; x + dashLen <= labelX; x += dashLen + gapLen)
        cv.fillRect(x, lineY, dashLen, dashH, fill);
      const rightEnd = rect.x + PAD + totalW;
      for (let x = labelX + labelW; x + dashLen <= rightEnd; x += dashLen + gapLen)
        cv.fillRect(x, lineY, dashLen, dashH, fill);
    }
    if (labelW > 0) {
      cv.fillRect(labelX - 2, y, labelW + 4, FONT_H, fill);
      cv.text(labelX, y, label, ink);
    }
    return;
  }

  const labelW = Math.min(label.length * FONT_W, totalW);
  const labelX = rect.x + PAD;
  const rightEnd = rect.x + PAD + totalW;
  const iconGap = 4;
  let weatherW = 0;
  let weatherX = rightEnd;
  let weatherHasIcon = false;
  let clipped = "";
  if (weatherText) {
    const maxW = Math.max(0, totalW - labelW - (dashLen + gapLen));
    clipped = weatherText.slice(0, 39); // WEATHER_DAY_LINE_MAX_LEN - 1
    while (clipped.length > 0) {
      const hasIcon = isIconMarker(clipped.charCodeAt(clipped.length - 1));
      if (measureText(clipped) + (hasIcon ? iconGap : 0) <= maxW) break;
      clipped = clipped.slice(0, -1);
    }
    weatherHasIcon = clipped.length > 0 && isIconMarker(clipped.charCodeAt(clipped.length - 1));
    weatherW = measureText(clipped) + (weatherHasIcon ? iconGap : 0);
    weatherX = weatherRight
      ? rightEnd - weatherW
      : rect.x + PAD + Math.trunc((totalW - weatherW) / 2);
    const minX = labelX + labelW + dashLen;
    if (weatherW > 0 && weatherX < minX) weatherX = minX;
  }
  if (leader) {
    // ink, unless that is the surface the rule lies on: the header's fill, or - gaps unfilled - the page behind it
    const surface = filled ? fill : page;
    let rule = ink;
    if (same(rule, surface)) {
      rule = fill;
      if (same(rule, surface)) rule = safeText(surface);
    }
    for (let x = labelX + labelW; x + dashLen <= weatherX; x += dashLen + gapLen)
      cv.fillRect(x, lineY, dashLen, dashH, rule);
  }
  if (!filled) {
    for (let x = weatherX + weatherW; x + dashLen <= rightEnd; x += dashLen + gapLen)
      cv.fillRect(x, lineY, dashLen, dashH, fill);
  }
  if (labelW > 0) {
    cv.fillRect(labelX - 2, y, labelW + 4, FONT_H, fill);
    cv.text(labelX, y, label, ink);
  }
  if (weatherW > 0) {
    if (weatherHasIcon) {
      const bracket = clipped.slice(0, -1);
      const bracketW = measureText(bracket);
      if (bracketW > 0) {
        cv.fillRect(weatherX - 2, y, bracketW + 4, FONT_H, fill);
        cv.text(weatherX, y, bracket, ink);
      }
      const iconX = weatherX + bracketW + iconGap;
      cv.fillRect(iconX - 2, y, ICON_W + 4, FONT_H, iconBg);
      cv.text(iconX, y, clipped[clipped.length - 1], safeText(iconBg), iconBg);
    } else {
      cv.fillRect(weatherX - 2, y, weatherW + 4, FONT_H, fill);
      cv.text(weatherX, y, clipped, ink);
    }
  }
}

// weeks fits beside the forecast chip (week_fits_beside_forecast)
function weekFits(totalW, label, weatherText) {
  const labelW = label.length * FONT_W;
  if (!weatherText) return labelW <= totalW;
  const gap = isIconMarker(weatherText.charCodeAt(weatherText.length - 1)) ? 4 : 0;
  return labelW + 7 + measureText(weatherText) + gap <= totalW;
}

// the forecast chip text of one day: "[11/19]" + icon marker, or "[11/19 storm]" without icons
function forecastText(day, scene, german) {
  const w = day.weather;
  if (!w) return "";
  const tmin = Math.round(w.min);
  const tmax = Math.round(w.max);
  const iconId = scene.iconSet !== "none" ? (CODE_ICON[w.code] ?? -1) : -1;
  if (iconId >= 0) return `[${tmin}/${tmax}]${String.fromCharCode(MARKER_BASE + iconId)}`;
  const cond = CONDITION[w.code];
  return `[${tmin}/${tmax} ${cond ? cond[german ? 0 : 1] : german ? "unbekannt" : "unknown"}]`;
}

// agenda_order.c: which entries stay when there is no room: by priority rank, then in order
function selectByRank(ranks, capacity) {
  const shown = ranks.map(() => false);
  if (capacity <= 0) return shown;
  const maxRank = ranks.reduce((m, r) => Math.max(m, r), 0);
  let kept = 0;
  for (let rank = 0; rank <= maxRank && kept < capacity; rank++) {
    for (let i = 0; i < ranks.length && kept < capacity; i++) {
      if (ranks[i] === rank) {
        shown[i] = true;
        kept++;
      }
    }
  }
  return shown;
}

function drawDayCell(cv, ctx, cell, dayIndex, day, tagged, firstVisible, marked) {
  const { profile, scene, german } = ctx;
  const weekWanted = scene.showWeek && (dayIndex === 0 || day.dow === 1);
  const weekday = WEEKDAY[german ? "de" : "en"][day.dow];
  let label = dayLabel(
    weekday,
    day.mday,
    weekWanted,
    isoWeek(day.year, day.mon0, day.mday),
    german
  );
  const weatherBuf = forecastText(day, scene, german);
  const head = dayHeaderColors(profile, marked);
  const iconBg = iconBackground(profile, marked, head.bg);
  if (weekWanted && !weekFits(cell.w - 2 * PAD, label, weatherBuf))
    label = dayLabel(weekday, day.mday, false, 0, german);
  drawDayDivider(
    cv,
    cell,
    cell.y,
    label,
    ctx.weatherMode,
    scene.weatherRight,
    weatherBuf || null,
    head.bg,
    head.fg,
    profile.headerDividerFilled,
    iconBg,
    profile.dayheadLeaderLine,
    dayBodyColors(profile, false).bg
  );

  const body = dayBodyColors(profile, marked);
  const bodyMarked = marked && profile.hasMark && !profile.markColorsHeader;
  const rowH = FONT_H + PAD;
  const textWidth = cell.w - 2 * PAD;
  const contentTop = cell.y + FONT_H + PAD;
  const contentH = cell.h - FONT_H - PAD;
  const maxRows = contentH > 0 ? Math.trunc((contentH + PAD) / rowH) : 0;
  if (contentH > 0) cv.fillRect(cell.x, contentTop, cell.w, contentH, body.bg);

  const todays = tagged.filter(
    (t) =>
      touchesDay(t.ev, day.index) &&
      !(ctx.skipRepeats && t.multiday && dayIndex !== firstVisible.get(t))
  );
  const total = todays.length;
  let budget = total > maxRows ? maxRows - 1 : maxRows;
  if (budget < 0) budget = 0;
  const shownFlags = selectByRank(
    todays.map((t) => t.rank),
    budget
  );

  let rowsUsed = 0;
  let shown = 0;
  for (let i = 0; i < todays.length && rowsUsed < budget; i++) {
    if (!shownFlags[i]) continue;
    const t = todays[i];
    let prefix = "";
    if (ctx.showPrefix && t.multiday)
      prefix = `${day.index - Math.floor(t.ev.startMin / 1440) + 1}/${t.totalDays}: `;
    const avail = Math.max(FONT_W, textWidth - prefix.length * FONT_W);
    const line = wrapOneLine(t.text, avail);
    if (line.length > 0) {
      const y = contentTop + rowsUsed * rowH;
      let x = cell.x + PAD;
      if (prefix) {
        cv.text(x, y, prefix, body.fg);
        x += prefix.length * FONT_W;
      }
      const colors = eventColors(profile, t.ev.cal, bodyMarked, body.fg);
      if (colors.chip) cv.fillRect(x, y, line.length * FONT_W, FONT_H, colors.chip);
      cv.text(x, y, line, colors.ink);
      rowsUsed++;
      shown++;
    }
  }
  if (shown < total && rowsUsed < maxRows) {
    cv.text(cell.x + PAD, contentTop + rowsUsed * rowH, `+${total - shown} more`, body.fg);
  }
}

const touchesDay = (ev, dayIndex) => {
  const first = Math.floor(ev.startMin / 1440);
  const lastInstant = ev.endMin > ev.startMin ? ev.endMin - 1 : ev.startMin;
  return dayIndex >= first && dayIndex <= Math.floor(lastInstant / 1440);
};

// ------------------------------------------------------------------------------------------ the picture
// scene = {
//   width, height                 panel size (800x480, 480x800, ...)
//   layout: "gridA" | "gridB"     the 7-day grid template
//   startDate: "2026-09-30"       today; the grid shows this day and the six after it
//   nowTime: "08:02"              the clock in the header
//   profile: {...}                a profile document (the editor's export), see resolveProfile()
//   marked: [bool x 7]            which of the seven days the shift model marks
//   calendars: [{ name, show } x 5]   A..E (A and B show their name, C, D, E their letter)
//   events: [{ cal: 0..4, day: 0..6 (days after today, or earlier), start: "HH:MM" | null (all day), minutes,
//              days (all-day span), summary }]
//   weather: [{ min, max, code } | null x 7] | null    the forecast of each day; null: no forecast at all
//   iconSet: "none" | "flaticon" | "metno", iconColored: bool, weatherRight: bool
//   language: "en" | "de", showWeek: bool, timeMode: "off" | "duration" | "range"
//   multiday: "compact" | "repeat" | "numbered", priority: "ABCDE", orderTime: bool
//   climate: { temp: "26C", tempCat: "good"|"bad"|"super", hum: "49%", humCat } | null
//   grayscaleBoard: bool
// }
export function renderAgenda(scene) {
  const profile = resolveProfile(scene.profile);
  const german = scene.language === "de";
  const cv = new Canvas(scene.width, scene.height, {
    iconSet: scene.iconSet,
    iconColored: scene.iconColored,
    monoIcons: profile.mono,
    grayscaleBoard: Boolean(scene.grayscaleBoard),
  });
  const rect = { x: 0, y: 0, w: scene.width, h: scene.height };

  // the page
  const page = dayBodyColors(profile, false);
  cv.fillRect(rect.x, rect.y, rect.w, rect.h, page.bg);

  // the header bar
  const top = topHeaderColors(profile);
  const headerH = FONT_H + 2 * PAD;
  cv.fillRect(rect.x, rect.y, rect.w, headerH, top.bg);
  let hx = rect.x + PAD;
  const hy = rect.y + PAD;
  let anyShown = false;
  scene.calendars.forEach((cal, i) => {
    if (!cal.show) return;
    if (anyShown) {
      cv.text(hx, hy, ",", top.fg);
      hx += FONT_W;
    }
    const name = i < 2 ? cal.name : "CDE"[i - 2];
    cv.text(hx, hy, name, profile.mono ? top.fg : profile.calInk[i]);
    hx += name.length * FONT_W;
    anyShown = true;
  });
  const [y0, m0, d0] = scene.startDate.split("-").map(Number);
  const [nh, nm] = scene.nowTime.split(":").map(Number);
  const datetime = `${two(d0)}.${two(m0)}.${two(y0 % 100)} ${two(nh)}:${two(nm)}`;
  const climateEdge = drawHeaderClimate(cv, rect, scene.climate, Boolean(scene.grayscaleBoard));
  const datetimeX = climateEdge - datetime.length * FONT_W;
  if (datetimeX >= hx + PAD) cv.text(datetimeX, hy, datetime, top.fg);

  // the seven days
  const days = [];
  for (let i = 0; i < 7; i++) {
    const date = new Date(Date.UTC(y0, m0 - 1, d0 + i));
    days.push({
      index: i,
      year: date.getUTCFullYear(),
      mon0: date.getUTCMonth(),
      mday: date.getUTCDate(),
      dow: date.getUTCDay(),
      weather: scene.weather ? scene.weather[i] || null : null,
    });
  }
  const priority = (scene.priority || "ABCDE").toUpperCase();
  const rankOf = [0, 1, 2, 3, 4].map((c) => {
    const at = priority.indexOf("ABCDE"[c]);
    return at < 0 ? 4 : at;
  });
  const timeMode = scene.timeMode || "off";
  let tagged = scene.events.map((e, seq) => {
    const allDay = e.start == null;
    const dayStart = e.day * 1440;
    let startMin;
    let endMin;
    if (allDay) {
      startMin = dayStart;
      endMin = dayStart + (e.days || 1) * 1440;
    } else {
      const [hh, mm] = e.start.split(":").map(Number);
      startMin = dayStart + hh * 60 + mm;
      endMin = startMin + e.minutes;
    }
    const ev = { cal: e.cal, summary: e.summary, allDay, startMin, endMin };
    const lastInstant = endMin > startMin ? endMin - 1 : startMin;
    const totalDays = Math.floor(lastInstant / 1440) - Math.floor(startMin / 1440) + 1;
    return {
      ev,
      seq,
      rank: rankOf[e.cal],
      text: eventText(ev, timeMode),
      multiday: totalDays > 1,
      totalDays,
    };
  });
  // agenda_event_compare(): by rank then time (or time then rank), all-day first, end, calendar, original order
  const priorityFirst = !scene.orderTime;
  tagged = tagged.slice().sort((a, b) => {
    const cmp = (x, y) => (x > y) - (x < y);
    let c;
    if (priorityFirst && (c = cmp(a.rank, b.rank)) !== 0) return c;
    if ((c = cmp(a.ev.startMin, b.ev.startMin)) !== 0) return c;
    if (!priorityFirst && (c = cmp(a.rank, b.rank)) !== 0) return c;
    if ((c = cmp(b.ev.allDay ? 1 : 0, a.ev.allDay ? 1 : 0)) !== 0) return c;
    if ((c = cmp(a.ev.endMin, b.ev.endMin)) !== 0) return c;
    if ((c = cmp(a.ev.cal, b.ev.cal)) !== 0) return c;
    return cmp(a.seq, b.seq);
  });
  const multiday = scene.multiday || "compact";
  const ctx = {
    profile,
    scene,
    german,
    weatherMode: Boolean(scene.weather),
    skipRepeats: multiday === "compact",
    showPrefix: multiday !== "repeat",
  };
  const firstVisible = new Map();
  tagged.forEach((t) => {
    const di = days.findIndex((d) => touchesDay(t.ev, d.index));
    firstVisible.set(t, di);
  });

  // the cells (draw_calendar_grid)
  const contentTop = rect.y + headerH + PAD;
  const contentH = rect.h - headerH - PAD;
  const rowH = Math.trunc(contentH / 4);
  const colW = Math.trunc(rect.w / 2);
  const cells = [];
  if (scene.layout !== "gridB") {
    cells[0] = { x: rect.x, y: contentTop, w: rect.w, h: rowH };
    for (let i = 1; i <= 3; i++)
      cells[i] = { x: rect.x, y: contentTop + i * rowH, w: colW, h: rowH };
    for (let i = 4; i <= 6; i++)
      cells[i] = { x: rect.x + colW, y: contentTop + (i - 3) * rowH, w: colW, h: rowH };
  } else {
    cells[0] = { x: rect.x, y: contentTop, w: colW, h: rowH * 2 };
    for (let i = 1; i <= 2; i++)
      cells[i] = { x: rect.x, y: contentTop + (i + 1) * rowH, w: colW, h: rowH };
    for (let i = 3; i <= 6; i++)
      cells[i] = { x: rect.x + colW, y: contentTop + (i - 3) * rowH, w: colW, h: rowH };
  }
  days.forEach((day, i) => {
    drawDayCell(
      cv,
      ctx,
      cells[i],
      i,
      day,
      tagged,
      firstVisible,
      Boolean(scene.marked && scene.marked[i])
    );
  });
  return { width: scene.width, height: scene.height, rgb: cv.rgb };
}

// the climate readout of the header (draw_header_climate): right-aligned chips, the first one is the humidity
function drawHeaderClimate(cv, rect, climate, grayscale) {
  let edge = rect.x + rect.w - PAD;
  if (!climate) return edge;
  const y = rect.y + PAD;
  const chip = (text, category) => {
    const w = text.length * FONT_W + Math.trunc(FONT_W / 2);
    const x = Math.max(0, edge - w);
    const bg = grayscale
      ? [0, 0, 0]
      : category === "bad"
        ? [255, 0, 0]
        : category === "super"
          ? [0, 255, 0]
          : [255, 255, 0];
    cv.fillRect(x, y, w, FONT_H, bg);
    const fg =
      !grayscale && category !== "bad" && category !== "super" ? [0, 0, 0] : [255, 255, 255];
    cv.text(x + Math.trunc(FONT_W / 4), y, text, fg);
    edge = x - Math.trunc(FONT_W / 2);
  };
  if (climate.hum) chip(climate.hum, climate.humCat);
  if (climate.temp) chip(climate.temp, climate.tempCat);
  return edge;
}
