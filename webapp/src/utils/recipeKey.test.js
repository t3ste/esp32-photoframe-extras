import { describe, expect, it } from "vitest";
import { isMealDbKey } from "./recipeKey";

describe("isMealDbKey", () => {
  it("accepts letters and digits, 1 to 24 of them", () => {
    expect(isMealDbKey("1")).toBe(true);
    expect(isMealDbKey("abc123XYZ")).toBe(true);
    expect(isMealDbKey("a".repeat(24))).toBe(true);
  });

  it("refuses what is empty or too long", () => {
    expect(isMealDbKey("")).toBe(false);
    expect(isMealDbKey("a".repeat(25))).toBe(false);
  });

  it("refuses characters that could change a request", () => {
    expect(isMealDbKey("abc/def")).toBe(false);
    expect(isMealDbKey("abc def")).toBe(false);
    expect(isMealDbKey("abc?x=1")).toBe(false);
    expect(isMealDbKey("abc-def")).toBe(false);
  });

  it("refuses what is not text", () => {
    expect(isMealDbKey(undefined)).toBe(false);
    expect(isMealDbKey(null)).toBe(false);
    expect(isMealDbKey(12345)).toBe(false);
  });
});
