import pluginVue from "eslint-plugin-vue";
import eslintConfigPrettier from "eslint-config-prettier";

export default [
  {
    ignores: [
      "dist/**",
      "node_modules/**",
      // These files carry `#if FEATURE_X ... #else ... #endif` alternatives in comments
      // (see feature-directives.js): both variants are in the text, so ESLint cannot
      // parse them as they are; the build strips one variant before Vue sees the file.
      "src/components/AlbumGallery.vue",
      "src/components/ImageProcessing.vue",
      "src/components/ImageUpload.vue",
      "src/components/SettingsPanel.vue",
      "src/stores/app.js",
      "src/stores/settings.js",
      "src/views/LandingPage.vue",
    ],
  },
  ...pluginVue.configs["flat/recommended"],
  eslintConfigPrettier,
  {
    rules: {
      "vue/multi-word-component-names": "off",
      "vue/no-v-html": "off",
      "no-unused-vars": [
        "warn",
        { argsIgnorePattern: "^_", caughtErrorsIgnorePattern: "^_" },
      ],
    },
  },
];
