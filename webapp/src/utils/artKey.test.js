import { describe, expect, it } from "vitest";
import { isArtAlbumName, isArtKey } from "./artKey";

describe("isArtKey", () => {
  it("accepts letters, digits and underscores, 4 to 64 of them", () => {
    expect(isArtKey("DEMO_KEY")).toBe(true);
    expect(isArtKey("a1B2c3D4e5F6g7H8i9J0k1L2m3N4o5P6q7R8s9T0")).toBe(true);
    expect(isArtKey("abcd")).toBe(true);
    expect(isArtKey("a".repeat(64))).toBe(true);
  });

  it("refuses what is too short or too long", () => {
    expect(isArtKey("abc")).toBe(false);
    expect(isArtKey("")).toBe(false);
    expect(isArtKey("a".repeat(65))).toBe(false);
  });

  it("refuses characters that could change a request", () => {
    expect(isArtKey("abcdef12&x=1")).toBe(false);
    expect(isArtKey("abcdef 12")).toBe(false);
    expect(isArtKey("abcdef-12")).toBe(false);
    expect(isArtKey(null)).toBe(false);
    expect(isArtKey(undefined)).toBe(false);
  });
});

describe("isArtAlbumName", () => {
  it("accepts a folder name", () => {
    expect(isArtAlbumName("Art")).toBe(true);
    expect(isArtAlbumName("Old masters 2")).toBe(true);
    expect(isArtAlbumName("a-b_c")).toBe(true);
    expect(isArtAlbumName("x".repeat(31))).toBe(true);
  });

  it("refuses what is not a good name", () => {
    expect(isArtAlbumName("")).toBe(false);
    expect(isArtAlbumName(" Art")).toBe(false);
    expect(isArtAlbumName("Art ")).toBe(false);
    expect(isArtAlbumName("x".repeat(32))).toBe(false);
    expect(isArtAlbumName("a/b")).toBe(false);
    expect(isArtAlbumName("a.b")).toBe(false);
    expect(isArtAlbumName("Kunstä")).toBe(false);
    expect(isArtAlbumName(null)).toBe(false);
  });
});
