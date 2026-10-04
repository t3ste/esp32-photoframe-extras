import { defineConfig } from "vite";
import vue from "@vitejs/plugin-vue";
import vuetify from "vite-plugin-vuetify";
import { resolve } from "path";
import { gzipSync } from "node:zlib";
import { existsSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { featureDirectives, flagsFor } from "./feature-directives.js";

// The firmware embeds only the .gz files (main/CMakeLists.txt EMBED_FILES)
// and serves the stored bytes verbatim with Content-Encoding: gzip
// (http_server.c, wifi_provisioning.c) -- raw, the assets would waste
// ~800 KB of the 3.5 MB app partition. The originals stay in the output so
// `npm run preview` still serves the app.
function gzipOutput(outDir) {
  return {
    name: "gzip-output",
    apply: "build",
    closeBundle() {
      for (const dir of [outDir, resolve(outDir, "assets")]) {
        for (const name of readdirSync(dir)) {
          if (!/\.(html|js|css|svg)$/.test(name)) continue;
          const path = resolve(dir, name);
          writeFileSync(`${path}.gz`, gzipSync(readFileSync(path), { level: 9 }));
        }
      }
    },
  };
}

// Files copied verbatim from public/ that belong to an optional feature (see
// feature-directives.js): they are dropped from the output when it is off.
function dropDisabledPublicFiles(outDir, featureList) {
  const flags = flagsFor(featureList);
  const files = { "profile-editor.html": "FEATURE_AGENDA" };
  return {
    name: "drop-disabled-public-files",
    apply: "build",
    closeBundle() {
      for (const [name, flag] of Object.entries(files)) {
        const path = resolve(outDir, name);
        if (!flags.has(flag) && existsSync(path)) rmSync(path);
      }
    },
  };
}

const features = process.env.VITE_FEATURES;
// VITE_OUT_DIR lets a check build into a scratch directory instead of main/webapp.
const outDir = resolve(__dirname, process.env.VITE_OUT_DIR || "../main/webapp");

export default defineConfig({
  plugins: [
    featureDirectives(features),
    vue(),
    vuetify({ autoImport: true }),
    dropDisabledPublicFiles(outDir, features),
    gzipOutput(outDir),
  ],
  build: {
    outDir,
    emptyOutDir: true,
    rollupOptions: {
      external: ["/measurement_sample.jpg"],
      output: {
        entryFileNames: "assets/[name].js",
        chunkFileNames: "assets/[name].js",
        assetFileNames: "assets/[name].[ext]",
      },
    },
  },
  server: {
    proxy: {
      "/api": {
        target: "http://192.168.0.140",
        changeOrigin: true,
        // A firmware built with the `fixes` option refuses a request whose Origin is not its own
        // host (docs/API.md, Access control), and the dev server's Origin is not: drop it.
        configure: (proxy) => {
          proxy.on("proxyReq", (proxyReq) => proxyReq.removeHeader("origin"));
        },
      },
    },
    fs: {
      allow: [resolve(__dirname, "..")],
    },
  },
});
