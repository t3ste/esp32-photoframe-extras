import { describe, expect, it } from "vitest";
import { isRouteKey } from "./routeKey";

describe("isRouteKey", () => {
  it("accepts the keys of both providers", () => {
    expect(isRouteKey("0123456789abcdefABCDEF0123456789")).toBe(true); // 32, like TomTom
    expect(isRouteKey("aB3-dE_6gH9jK2mN5pQ8sT1vW4yZ7aB0cD3eF6gH9jK")).toBe(true); // like HERE
    expect(isRouteKey("0123456789abcdef")).toBe(true);
    expect(isRouteKey("a".repeat(104))).toBe(true);
  });

  it("refuses what is too short or too long", () => {
    expect(isRouteKey("0123456789abcde")).toBe(false);
    expect(isRouteKey("")).toBe(false);
    expect(isRouteKey("a".repeat(105))).toBe(false);
  });

  it("refuses characters that could change a request", () => {
    expect(isRouteKey("0123456789abcdef&key=x")).toBe(false);
    expect(isRouteKey("0123456789abcdef 12")).toBe(false);
    expect(isRouteKey("0123456789abcdef\n12")).toBe(false);
    expect(isRouteKey("0123456789abcdef/../")).toBe(false);
    expect(isRouteKey("0123456789abcdef%2F")).toBe(false);
    expect(isRouteKey("0123456789abcdefä")).toBe(false);
  });

  it("refuses what is not a string", () => {
    expect(isRouteKey(undefined)).toBe(false);
    expect(isRouteKey(null)).toBe(false);
    expect(isRouteKey(1234567890123456)).toBe(false);
  });
});
