// Which schedule draws in which minute when several overlap (build option schedule-pages).
//
// A port of main/sched_pick.c, the firmware's own decision, so the timeline the Web UI draws is the
// one the frame follows. The entities are in priority order (index 0 is Schedule 1, the last one is
// the photo rotation when it takes part). A fire of entity k at minute t is DRAWN unless a higher
// entity j < k draws
//   - within the hold of j before it (t - t_j < hold_j; the same minute always counts), or
//   - within the hold of k after it (0 < t_j - t < hold_k): k's display would be cut short.
// "Draws" is recursive (only fires that are themselves drawn count), an entity never holds off
// itself, and at most one entity draws in a minute. host_tests/test_sched_pick.cpp tests the C
// version against a literal reference; schedPick.test.js checks this port against that C code.

import { parseCron } from "./cron";

export const HOLD_MAX_MIN = 240;

export function clampHold(value) {
  const minutes = Math.round(Number(value));
  return Number.isFinite(minutes) ? Math.max(0, Math.min(HOLD_MAX_MIN, minutes)) : 0;
}

// match[k][g] = 1 when an entity's rule matches the minute g of a timeline whose minute 0 is
// 00:00 of the weekday `startDow` (0 = Sunday). `ruleLists` holds one list of cron strings per
// entity; a rule that does not parse never fires.
export function matchTable(ruleLists, length, startDow) {
  return ruleLists.map((rules) => {
    const parsed = rules.map(parseCron).filter(Boolean);
    const out = new Uint8Array(length);
    if (!parsed.length) return out;
    for (let g = 0; g < length; g++) {
      const dow = (startDow + Math.floor(g / 1440)) % 7;
      const inDay = g % 1440;
      const hour = Math.floor(inDay / 60);
      const minute = inDay % 60;
      for (const r of parsed) {
        if (r.minute.has(minute) && r.hour.has(hour) && r.dow.has(dow)) {
          out[g] = 1;
          break;
        }
      }
    }
    return out;
  });
}

// drawn[k][g] = 1 when entity k draws in minute g. `holds` are the effective holds in minutes (0:
// only the same minute is resolved). The timeline needs room before and after the part that is
// looked at: a fire depends on higher ones up to one hold away, those on still higher ones.
export function pickDrawn(match, holds) {
  const count = match.length;
  const length = count ? match[0].length : 0;
  const drawn = match.map(() => new Uint8Array(length));
  const hold = holds.map(clampHold);
  for (let k = 0; k < count; k++) {
    const blocked = new Uint8Array(length);
    for (let j = 0; j < k; j++) {
      const theirs = Math.max(1, hold[j]); // the same minute is always resolved
      // a higher fire at or before this minute, within its own hold
      let last = -1e9;
      for (let g = 0; g < length; g++) {
        if (drawn[j][g]) last = g;
        if (g - last < theirs) blocked[g] = 1;
      }
      // a higher fire after this minute, before the hold of this entity has passed
      let next = 1e9;
      for (let g = length - 1; g >= 0; g--) {
        if (next - g < hold[k] && next > g) blocked[g] = 1;
        if (drawn[j][g]) next = g;
      }
    }
    for (let g = 0; g < length; g++) {
      if (match[k][g] && !blocked[g]) drawn[k][g] = 1;
    }
  }
  return drawn;
}
