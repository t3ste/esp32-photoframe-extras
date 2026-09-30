import { describe, expect, it } from "vitest";
import { isMarketKey } from "./marketKey";

describe("isMarketKey", () => {
  it("accepts letters and digits, 4 to 64 of them", () => {
    expect(isMarketKey("abcdef12")).toBe(true);
    expect(isMarketKey("demo")).toBe(true);
    expect(isMarketKey("ABCDEFGH12345678")).toBe(true);
    expect(isMarketKey("a".repeat(64))).toBe(true);
  });

  it("refuses what is too short or too long", () => {
    expect(isMarketKey("abc")).toBe(false);
    expect(isMarketKey("")).toBe(false);
    expect(isMarketKey("a".repeat(65))).toBe(false);
  });

  it("refuses characters that could change a request", () => {
    expect(isMarketKey("abcdef12&x=1")).toBe(false);
    expect(isMarketKey("abcdef12 34")).toBe(false);
    expect(isMarketKey("abcdef12\n34")).toBe(false);
    expect(isMarketKey("abcdef12/../")).toBe(false);
    expect(isMarketKey("abcdef12ä")).toBe(false);
  });

  it("refuses what is not a string", () => {
    expect(isMarketKey(undefined)).toBe(false);
    expect(isMarketKey(null)).toBe(false);
    expect(isMarketKey(12345678)).toBe(false);
  });
});
