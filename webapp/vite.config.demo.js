import { defineConfig } from "vite";
import vue from "@vitejs/plugin-vue";
import vuetify from "vite-plugin-vuetify";
import { resolve } from "path";
import { rename } from "fs/promises";
import { featureDirectives } from "./feature-directives.js";

// Plugin to rename index-demo.html to index.html after build
function renameHtmlPlugin() {
  return {
    name: "rename-html",
    closeBundle: async () => {
      const outDir = resolve(__dirname, "../demo");
      try {
        await rename(`${outDir}/index-demo.html`, `${outDir}/index.html`);
      } catch (_e) {
        // File might not exist or already renamed
      }
    },
  };
}

// The repository this site is published from. The canonical repository is the default; another
// repository that publishes its own site from this code (the extended line, docs/EXTENDED_LINE.md)
// sets SITE_REPO=<owner>/<name> (the deploy workflow passes github.repository): the site is then
// served under /<name>/, and the landing page's links and its "latest release" lookup name that
// repository instead of the canonical one. With SITE_REPO unset the output is exactly as before.
const CANONICAL_REPO = "t3stier/esp32-photoframe-rebuild";
const siteRepo = process.env.SITE_REPO || CANONICAL_REPO;
const siteBase = `/${siteRepo.split("/")[1]}/`;

function siteRepoPlugin() {
  const active = siteRepo !== CANONICAL_REPO;
  return {
    name: "site-repo",
    enforce: "pre",
    transform(code, id) {
      if (active && id.endsWith("LandingPage.vue")) {
        return { code: code.replaceAll(CANONICAL_REPO, siteRepo), map: null };
      }
      return null;
    },
    transformIndexHtml(html) {
      return active ? html.replaceAll("/esp32-photoframe-rebuild/", siteBase) : html;
    },
  };
}

// Demo build config - outputs to demo folder for GitHub Pages. The demo page
// only shows the image-processing preview, not a real device's settings UI, so
// the shared store modules it pulls in (../stores) are stripped to the plain
// (no-feature) code path, same as VITE_FEATURES="" would for the main app - see
// feature-directives.js. "site" switches on what only this site has: the fork's
// links and release channels in views/LandingPage.vue.
export default defineConfig({
  plugins: [
    siteRepoPlugin(),
    featureDirectives("site"),
    vue(),
    vuetify({ autoImport: true }),
    renameHtmlPlugin(),
  ],
  base: siteBase,
  publicDir: resolve(__dirname, "../demo"), // Serve demo folder as public (for sample.jpg, manifests)
  build: {
    outDir: resolve(__dirname, "../demo"),
    emptyOutDir: false, // Don't delete existing demo files (manifests, sample.jpg, etc.)
    rollupOptions: {
      input: resolve(__dirname, "index-demo.html"),
      output: {
        entryFileNames: "assets/[name]-[hash].js",
        chunkFileNames: "assets/[name]-[hash].js",
        assetFileNames: "assets/[name]-[hash].[ext]",
      },
    },
  },
  server: {
    open: "/esp32-photoframe/index.html",
    fs: {
      allow: [resolve(__dirname, "..")],
    },
  },
});
