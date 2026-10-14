// Which time zone name the picker in Settings > General shows for the stored POSIX rule (build option `fixes`).
//
// The device stores only the rule ("CET-1CEST,M3.5.0,M10.5.0/3"); many zones share one (33 of them that one: Berlin, Paris, Madrid,
// Rome, Ceuta, ...), so the name shown has to be guessed back. The picker took the browser's zone when it fits and otherwise the first
// of the table, which is alphabetical: someone in Berlin chose Europe/Berlin, saved, and found Africa/Ceuta there after the reload.
// Now the choice made in this browser is remembered, and without one the zone people mean (the big city) comes before the rest.
import { TIMEZONES } from "../data/timezones";
import { ruleForZone, zoneForRule } from "./timezone";

const STORAGE_KEY = "photoframe.timezoneName";

// For a rule that many zones share, the first of these that has the rule is shown (a zone that is not in the table is skipped).
export const PREFERRED_ZONES = [
  "Europe/Berlin",
  "Europe/London",
  "Europe/Lisbon",
  "Europe/Athens",
  "Europe/Moscow",
  "Europe/Istanbul",
  "America/New_York",
  "America/Chicago",
  "America/Denver",
  "America/Los_Angeles",
  "America/Anchorage",
  "America/Halifax",
  "America/Sao_Paulo",
  "America/Mexico_City",
  "America/Puerto_Rico",
  "Africa/Lagos",
  "Africa/Nairobi",
  "Asia/Dubai",
  "Asia/Bangkok",
  "Asia/Shanghai",
  "Australia/Sydney",
];

export function preferredZoneFor(rule) {
  return PREFERRED_ZONES.find((name) => TIMEZONES[name] === rule) ?? null;
}

export function readRememberedZone() {
  try {
    return localStorage.getItem(STORAGE_KEY) || "";
  } catch {
    return "";
  }
}

// Remember what was picked: only a name of the table is kept (the value comes from a picker, but storage can hold anything).
export function rememberZone(name) {
  if (typeof name !== "string" || ruleForZone(name) === null) return;
  try {
    localStorage.setItem(STORAGE_KEY, name);
  } catch {
    // storage can be unavailable (private mode, blocked site data): the picker still works
  }
}

export function pickZone(rule, browserZone, remembered = readRememberedZone()) {
  if (browserZone && ruleForZone(browserZone) === rule) return browserZone;
  if (remembered && ruleForZone(remembered) === rule) return remembered;
  return preferredZoneFor(rule) ?? zoneForRule(rule, browserZone);
}
