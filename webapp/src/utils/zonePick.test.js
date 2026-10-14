import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { TIMEZONES } from "../data/timezones";
import {
  PREFERRED_ZONES,
  pickZone,
  preferredZoneFor,
  rememberZone,
  readRememberedZone,
} from "./zonePick";

const CET = "CET-1CEST,M3.5.0,M10.5.0/3";

describe("the zone name the picker shows for a stored rule", () => {
  // the tests run without a DOM: a small storage stands in for the browser's
  beforeEach(() => {
    const data = new Map();
    vi.stubGlobal("localStorage", {
      getItem: (k) => (data.has(k) ? data.get(k) : null),
      setItem: (k, v) => data.set(k, String(v)),
    });
  });
  afterEach(() => vi.unstubAllGlobals());

  it("is not Africa/Ceuta for the Central European rule, whatever the browser", () => {
    expect(pickZone(CET, "")).toBe("Europe/Berlin");
    expect(pickZone(CET, "America/New_York")).toBe("Europe/Berlin");
  });

  it("is the zone that was picked in this browser", () => {
    rememberZone("Europe/Paris");
    expect(readRememberedZone()).toBe("Europe/Paris");
    expect(pickZone(CET, "")).toBe("Europe/Paris");
  });

  it("prefers the browser's own zone when it fits the rule", () => {
    rememberZone("Europe/Paris");
    expect(pickZone(CET, "Europe/Madrid")).toBe("Europe/Madrid");
  });

  it("forgets a remembered zone whose rule is not the stored one", () => {
    rememberZone("Europe/Paris");
    expect(pickZone("GMT0BST,M3.5.0/1,M10.5.0", "")).toBe("Europe/London");
  });

  it("keeps only names of the table", () => {
    rememberZone("Mars/Olympus");
    rememberZone(42);
    expect(readRememberedZone()).toBe("");
  });

  it("falls back to the table's own answer for a rule without a preferred zone", () => {
    expect(pickZone("GMT0", "")).toBe("Africa/Abidjan");
    expect(pickZone("UTC0", "")).toBe("Etc/UTC");
    expect(pickZone("UTC-8", "")).toBe("__fixed_offset__");
    expect(pickZone("not a rule", "")).toBe("__custom__");
  });

  it("only lists zones of the table, each the first of its rule that is listed", () => {
    for (const name of PREFERRED_ZONES) expect(TIMEZONES[name], name).toBeTypeOf("string");
    for (const name of PREFERRED_ZONES) {
      const rule = TIMEZONES[name];
      expect(preferredZoneFor(rule)).toBe(PREFERRED_ZONES.find((n) => TIMEZONES[n] === rule));
    }
  });

  it("lets the picker survive a storage that throws", () => {
    vi.stubGlobal("localStorage", {
      getItem: () => {
        throw new Error("blocked");
      },
      setItem: () => {
        throw new Error("blocked");
      },
    });
    rememberZone("Europe/Paris");
    expect(pickZone(CET, "")).toBe("Europe/Berlin");
  });

  it("works where there is no storage at all", () => {
    vi.unstubAllGlobals();
    expect(pickZone(CET, "")).toBe("Europe/Berlin");
  });
});
