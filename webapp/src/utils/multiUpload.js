// Pure helpers of the batch upload (build option `multi-upload`, see
// components/ImageUpload.vue) - kept out of the component so they can be tested
// without a browser. Nothing here is imported when the option is off.

export const BATCH_MAX_FILES = 200;

// The file name a pre-rendered upload is stored under: the user's own base name,
// reduced to characters that are safe on the frame's file system.
export function uploadBaseName(fileName) {
  const base = String(fileName ?? "")
    .replace(/\.[^/.]+$/, "")
    .replace(/[^A-Za-z0-9._-]/g, "_")
    .slice(0, 100);
  return base || "image";
}

// Size of the raw data inside an EPDGZ for a panel: 4 bits per pixel, two pixels per byte
// (see createEPDGZ() in @aitjcize/epaper-image-convert and main/image_processor.c).
export function epdgzExpectedSize(width, height) {
  return Math.ceil((width * height) / 2);
}

// True if an image of width x height is exactly the panel, in either orientation.
export function fitsPanel(width, height, panelWidth, panelHeight) {
  return (
    (width === panelWidth && height === panelHeight) ||
    (width === panelHeight && height === panelWidth)
  );
}

// Number of bytes a gzip file expands to, counted without keeping them; null where the
// browser has no DecompressionStream (the check is then skipped).
export async function gunzippedSize(blob) {
  if (typeof DecompressionStream === "undefined") return null;
  const reader = blob.stream().pipeThrough(new DecompressionStream("gzip")).getReader();
  let total = 0;
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    total += value.length;
  }
  return total;
}

// Counts for the queue's header and progress bar.
export function batchSummary(items) {
  const count = (status) => items.filter((i) => i.status === status).length;
  const done = count("done");
  const failed = count("failed");
  const skipped = count("skipped");
  const finished = done + failed + skipped;
  return {
    done,
    failed,
    skipped,
    finished,
    percent: items.length === 0 ? 0 : Math.round((finished * 100) / items.length),
  };
}
