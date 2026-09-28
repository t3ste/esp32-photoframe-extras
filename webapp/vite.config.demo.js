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

// Demo build config - outputs to demo folder for GitHub Pages. The demo page
// only shows the image-processing preview, not a real device's settings UI, so
// the shared store modules it pulls in (../stores) are stripped to the plain
// (no-feature) code path, same as VITE_FEATURES="" would for the main app - see
// feature-directives.js. "site" switches on what only this site has: the fork's
// links and release channels in views/LandingPage.vue.
export default defineConfig({
  plugins: [featureDirectives("site"), vue(), vuetify({ autoImport: true }), renameHtmlPlugin()],
  base: "/esp32-photoframe-rebuild/",
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
