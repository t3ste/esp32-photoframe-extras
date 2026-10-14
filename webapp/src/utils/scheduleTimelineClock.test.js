import { describe, expect, it } from "vitest";
import { browserWallOffsetMs, wallNow, wallOffsetMs } from "./scheduleTimeline";

// The "now" marker of the timeline follows the frame's clock, not the browser's.
describe("the frame's wall clock", () => {
  const instant = Date.UTC(2026, 9, 14, 9, 34, 12); // 09:34:12 UTC, a Wednesday

  it("reads the local time the frame reports", () => {
    // the frame is two hours ahead of UTC (CEST)
    const offset = wallOffsetMs("2026-10-14 11:34:12", instant);
    expect(offset).toBe(2 * 3600 * 1000);
    expect(wallNow(offset, instant)).toEqual({ dayIndex: 2, minute: 11 * 60 + 34 });
  });

  it("keeps ticking with the browser's clock", () => {
    const offset = wallOffsetMs("2026-10-14 11:34:12", instant);
    expect(wallNow(offset, instant + 30 * 60000).minute).toBe(12 * 60 + 4);
  });

  it("is independent of the browser's own time zone", () => {
    // the same frame time answered to browsers that are at different instants of their own: only the frame's text counts
    const a = wallNow(wallOffsetMs("2026-10-14 23:59:30", instant), instant);
    const b = wallNow(
      wallOffsetMs("2026-10-14 23:59:30", instant + 5 * 3600 * 1000),
      instant + 5 * 3600 * 1000
    );
    expect(a).toEqual(b);
    expect(a).toEqual({ dayIndex: 2, minute: 23 * 60 + 59 });
  });

  it("changes the day at the frame's midnight", () => {
    const offset = wallOffsetMs("2026-10-14 23:59:30", instant);
    expect(wallNow(offset, instant + 60000)).toEqual({ dayIndex: 3, minute: 0 });
  });

  it("takes the T form and a missing seconds field", () => {
    expect(wallOffsetMs("2026-10-14T11:34", instant)).toBe(2 * 3600 * 1000 - 12000);
  });

  it("answers null for what is not a time", () => {
    expect(wallOffsetMs("", instant)).toBeNull();
    expect(wallOffsetMs(undefined, instant)).toBeNull();
    expect(wallOffsetMs("soon", instant)).toBeNull();
  });

  it("falls back to the browser's wall clock", () => {
    const now = Date.now();
    const w = wallNow(browserWallOffsetMs(now), now);
    const d = new Date(now);
    expect(w.minute).toBe(d.getHours() * 60 + d.getMinutes());
    expect(w.dayIndex).toBe((d.getDay() + 6) % 7);
  });
});
