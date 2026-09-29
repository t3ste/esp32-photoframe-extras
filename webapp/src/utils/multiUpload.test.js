import { describe, it, expect } from "vitest";
import {
  BATCH_MAX_FILES,
  batchSummary,
  epdgzExpectedSize,
  fitsPanel,
  gunzippedSize,
  uploadBaseName,
} from "./multiUpload";

async function gzip(bytes) {
  const stream = new Blob([bytes]).stream().pipeThrough(new CompressionStream("gzip"));
  return new Blob([await new Response(stream).arrayBuffer()]);
}

describe("uploadBaseName", () => {
  it("keeps the user's name, without the extension", () => {
    expect(uploadBaseName("wikimedia-313183.epdgz")).toBe("wikimedia-313183");
    expect(uploadBaseName("holiday.2026.png")).toBe("holiday.2026");
  });

  it("replaces what the frame's file system dislikes", () => {
    expect(uploadBaseName("my photo (1).png")).toBe("my_photo__1_");
    expect(uploadBaseName("../../etc/passwd.png")).toBe(".._.._etc_passwd");
    expect(uploadBaseName("Bäume.png")).toBe("B_ume");
  });

  it("is never empty and never long", () => {
    expect(uploadBaseName(".png")).toBe("image");
    expect(uploadBaseName("")).toBe("image");
    expect(uploadBaseName(undefined)).toBe("image");
    expect(uploadBaseName("a".repeat(300) + ".png").length).toBe(100);
  });
});

describe("panel geometry", () => {
  it("expects two pixels per byte", () => {
    expect(epdgzExpectedSize(800, 480)).toBe(192000);
    expect(epdgzExpectedSize(1872, 1404)).toBe(1314144);
    expect(epdgzExpectedSize(3, 3)).toBe(5); // odd pixel counts round up, like createEPDGZ()
  });

  it("matches the panel in either orientation only", () => {
    expect(fitsPanel(800, 480, 800, 480)).toBe(true);
    expect(fitsPanel(480, 800, 800, 480)).toBe(true);
    expect(fitsPanel(801, 480, 800, 480)).toBe(false);
    expect(fitsPanel(4000, 3000, 800, 480)).toBe(false);
  });
});

describe("gunzippedSize", () => {
  it("counts the expanded bytes", async () => {
    const raw = new Uint8Array(192000).map((_, i) => (i * 7) % 251);
    expect(await gunzippedSize(await gzip(raw))).toBe(192000);
    expect(await gunzippedSize(await gzip(new Uint8Array(0)))).toBe(0);
  });

  it("rejects what is not gzip", async () => {
    await expect(gunzippedSize(new Blob(["this is not gzip data at all"]))).rejects.toThrow();
  });

  it("tells a matching EPDGZ from a mismatching one", async () => {
    const good = await gzip(new Uint8Array(epdgzExpectedSize(800, 480)));
    const other = await gzip(new Uint8Array(epdgzExpectedSize(1200, 1600)));
    expect(await gunzippedSize(good)).toBe(epdgzExpectedSize(800, 480));
    expect(await gunzippedSize(other)).not.toBe(epdgzExpectedSize(800, 480));
  });
});

describe("batchSummary", () => {
  it("counts finished files and the percentage", () => {
    const items = [
      { status: "done" },
      { status: "failed" },
      { status: "skipped" },
      { status: "working" },
      { status: "waiting" },
    ];
    expect(batchSummary(items)).toEqual({
      done: 1,
      failed: 1,
      skipped: 1,
      finished: 3,
      percent: 60,
    });
    expect(batchSummary([]).percent).toBe(0);
  });

  it("has a sensible limit", () => {
    expect(BATCH_MAX_FILES).toBeGreaterThan(20);
    expect(BATCH_MAX_FILES).toBeLessThanOrEqual(500);
  });
});
