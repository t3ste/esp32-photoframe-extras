<script setup>
import { computed } from "vue";
import { getDitherOptions, getPresetOptions } from "@aitjcize/epaper-image-convert";
import { useAppStore } from "../stores";

const appStore = useAppStore();

const props = defineProps({
  params: {
    type: Object,
    required: true,
  },
  preset: {
    type: String,
    default: "custom",
  },
});

const emit = defineEmits(["update:params", "update:preset", "preset-change"]);

const presetOptions = [
  ...getPresetOptions(),
  { value: "custom", title: "Custom", description: "Manually adjusted parameters" },
];

const ditherOptions = getDitherOptions();

const toneModeOptions = [
  { title: "Contrast", value: "contrast" },
  { title: "S-Curve", value: "scurve" },
];

const colorMethodOptions = [
  { title: "RGB", value: "rgb" },
  { title: "LAB", value: "lab" },
];

const scaleModeOptions = [
  { title: "Cover (crop to fill)", value: "cover" },
  { title: "Fit (letterbox)", value: "fit" },
];

const backgroundColorOptions = [
  { title: "White", value: "white" },
  { title: "Black", value: "black" },
];

const isScurveMode = computed(() => props.params.toneMode === "scurve");

function onPresetChange(value) {
  emit("update:preset", value);
  emit("preset-change", value);
}

function updateParam(key, value) {
  emit("update:params", { ...props.params, [key]: value });
}
</script>

<template>
  <div class="processing-controls">
    <!-- Preset Selection -->
    <v-row>
      <v-col cols="12">
        <v-card variant="outlined" class="mb-6">
          <v-card-subtitle class="pt-3"> Processing Preset </v-card-subtitle>
          <v-card-text>
            <v-btn-toggle
              :model-value="preset"
              mandatory
              color="primary"
              variant="outlined"
              @update:model-value="onPresetChange"
            >
              <v-btn v-for="p in presetOptions" :key="p.value" :value="p.value">
                {{ p.title }}
              </v-btn>
            </v-btn-toggle>
          </v-card-text>
        </v-card>
      </v-col>
    </v-row>

    <!-- Layout: scale mode first; background only matters in fit mode -->
    <v-row>
      <v-col cols="12" md="4">
        <v-select
          :model-value="params.scaleMode || 'cover'"
          :items="scaleModeOptions"
          item-title="title"
          item-value="value"
          label="Scale Mode"
          variant="outlined"
          density="compact"
          @update:model-value="updateParam('scaleMode', $event)"
        />
      </v-col>

      <v-col v-if="(params.scaleMode || 'cover') === 'fit'" cols="12" md="4">
        <v-select
          :model-value="params.backgroundColor || 'white'"
          :items="backgroundColorOptions"
          item-title="title"
          item-value="value"
          label="Background Color"
          variant="outlined"
          density="compact"
          @update:model-value="updateParam('backgroundColor', $event)"
        />
      </v-col>
    </v-row>

    <v-row>
      <!-- Dither Algorithm -->
      <v-col cols="12" md="4">
        <v-select
          :model-value="params.ditherAlgorithm"
          :items="ditherOptions"
          item-title="title"
          item-value="value"
          label="Dithering Algorithm"
          variant="outlined"
          density="compact"
          @update:model-value="updateParam('ditherAlgorithm', $event)"
        />
      </v-col>

      <v-col cols="12" md="4">
        <v-select
          :model-value="params.colorMethod"
          :items="colorMethodOptions"
          item-title="title"
          item-value="value"
          label="Color Matching"
          variant="outlined"
          density="compact"
          @update:model-value="updateParam('colorMethod', $event)"
        />
      </v-col>
    </v-row>

    <v-row>
      <!-- Exposure -->
      <v-col cols="12" md="4">
        <v-slider
          :model-value="params.exposure"
          :min="0.5"
          :max="2.0"
          :step="0.01"
          label="Exposure"
          thumb-label
          color="primary"
          @update:model-value="updateParam('exposure', $event)"
        >
          <template #append>
            <span class="text-body-2">{{ params.exposure.toFixed(2) }}</span>
          </template>
        </v-slider>
      </v-col>

      <!-- Saturation (hidden on grayscale panels) -->
      <v-col v-if="!appStore.isGrayscale" cols="12" md="4">
        <v-slider
          :model-value="params.saturation"
          :min="0.5"
          :max="2.0"
          :step="0.01"
          label="Saturation"
          thumb-label
          color="primary"
          @update:model-value="updateParam('saturation', $event)"
        >
          <template #append>
            <span class="text-body-2">{{ params.saturation.toFixed(2) }}</span>
          </template>
        </v-slider>
      </v-col>

      <!-- Compress Dynamic Range -->
      <v-col cols="12" md="4">
        <v-checkbox
          :model-value="params.compressDynamicRange"
          label="Compress Dynamic Range"
          hint="Map brightness to display's actual white point"
          persistent-hint
          color="primary"
          @update:model-value="updateParam('compressDynamicRange', $event)"
        />
      </v-col>
    </v-row>

    <!-- Tone Mode -->
    <v-row>
      <v-col cols="12" md="4">
        <v-select
          :model-value="params.toneMode"
          :items="toneModeOptions"
          item-title="title"
          item-value="value"
          label="Tone Mapping"
          variant="outlined"
          density="compact"
          @update:model-value="updateParam('toneMode', $event)"
        />
      </v-col>

      <!-- Contrast (for contrast mode) -->
      <v-col v-if="!isScurveMode" cols="12" md="4">
        <v-slider
          :model-value="params.contrast"
          :min="0.5"
          :max="2.0"
          :step="0.01"
          label="Contrast"
          thumb-label
          color="primary"
          @update:model-value="updateParam('contrast', $event)"
        >
          <template #append>
            <span class="text-body-2">{{ params.contrast.toFixed(2) }}</span>
          </template>
        </v-slider>
      </v-col>
    </v-row>

    <!-- S-Curve Controls -->
    <v-expand-transition>
      <v-card v-if="isScurveMode" variant="tonal" class="mt-4">
        <v-card-subtitle class="pt-3"> S-Curve Parameters </v-card-subtitle>
        <v-card-text>
          <v-row>
            <v-col cols="12" md="6">
              <v-slider
                :model-value="params.strength"
                :min="0"
                :max="1"
                :step="0.01"
                label="Strength"
                thumb-label
                color="primary"
                @update:model-value="updateParam('strength', $event)"
              >
                <template #append>
                  <span class="text-body-2">{{ params.strength.toFixed(2) }}</span>
                </template>
              </v-slider>
            </v-col>

            <v-col cols="12" md="6">
              <v-slider
                :model-value="params.shadowBoost"
                :min="0"
                :max="1"
                :step="0.01"
                label="Shadow Boost"
                thumb-label
                color="primary"
                @update:model-value="updateParam('shadowBoost', $event)"
              >
                <template #append>
                  <span class="text-body-2">{{ params.shadowBoost.toFixed(2) }}</span>
                </template>
              </v-slider>
            </v-col>

            <v-col cols="12" md="6">
              <v-slider
                :model-value="params.highlightCompress"
                :min="0.5"
                :max="5"
                :step="0.01"
                label="Highlight Compress"
                thumb-label
                color="primary"
                @update:model-value="updateParam('highlightCompress', $event)"
              >
                <template #append>
                  <span class="text-body-2">{{ params.highlightCompress.toFixed(2) }}</span>
                </template>
              </v-slider>
            </v-col>

            <v-col cols="12" md="6">
              <v-slider
                :model-value="params.midpoint"
                :min="0.3"
                :max="0.7"
                :step="0.01"
                label="Midpoint"
                thumb-label
                color="primary"
                @update:model-value="updateParam('midpoint', $event)"
              >
                <template #append>
                  <span class="text-body-2">{{ params.midpoint.toFixed(2) }}</span>
                </template>
              </v-slider>
            </v-col>
          </v-row>
        </v-card-text>
      </v-card>
    </v-expand-transition>
  </div>
</template>

<!-- #if FORK_FIXES -->
<style>
/* The controls are laid out by the room the component has, not by the width of the browser window
   (Vuetify's md breakpoint is a window width: the narrow column beside the preview in the wide-edit
   mode got three squeezed columns, labels ran into each other, words broke, the presets were cut off).
   All the rows of the template are cells of ONE grid, so the columns line up from row to row: two or
   three columns of at least 220 px, never more than three (one column when there is room for no more);
   every row of the template starts in the first column. */
.processing-controls {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(max(220px, calc((100% - 40px) / 3)), 1fr));
  gap: 14px 20px;
  align-items: center;
}
.processing-controls > .v-row {
  display: contents;
}
.processing-controls > .v-row > * {
  flex: none;
  width: auto;
  max-width: none;
  min-width: 0;
  margin: 0;
  padding: 0;
}
.processing-controls > .v-row > :first-child {
  grid-column: 1;
}
/* the preset card takes the whole row, so does the S-curve card */
.processing-controls > .v-row > .v-col-12:not([class*="v-col-md-"]),
.processing-controls > .v-card {
  grid-column: 1 / -1;
}
/* "Compress Dynamic Range" and its hint get a row of their own, so the label stays on one line */
.processing-controls > .v-row > :has(.v-checkbox) {
  grid-column: 1 / -1;
  margin-bottom: 10px;
}
.processing-controls > .v-card {
  margin-top: 8px;
}
/* the sliders of the S-curve card: its own small grid */
.processing-controls .v-card-text .v-row {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
  gap: 4px 20px;
  margin: 0;
}
.processing-controls .v-card-text .v-row > * {
  flex: none;
  width: auto;
  max-width: none;
  min-width: 0;
  padding: 0;
}
/* the presets: equal buttons in rows of equal length - one in a row in a very narrow card, two, three, and six
   when the card is very wide - instead of a group that wraps at an arbitrary place */
.processing-controls .v-card-text {
  container-type: inline-size;
}
.processing-controls .v-btn-toggle {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 8px;
  width: 100%;
  height: auto;
  border: none;
  overflow: visible;
}
.processing-controls .v-btn-toggle.v-btn-group > .v-btn {
  min-height: 40px;
  min-width: 0;
  padding: 0 8px;
  border: thin solid rgba(var(--v-border-color), var(--v-border-opacity));
  border-radius: 4px;
}
@container (max-width: 379px) {
  .processing-controls .v-btn-toggle {
    grid-template-columns: minmax(0, 1fr);
  }
}
@container (min-width: 560px) {
  .processing-controls .v-btn-toggle {
    grid-template-columns: repeat(3, minmax(0, 1fr));
  }
}
@container (min-width: 1100px) {
  .processing-controls .v-btn-toggle {
    grid-template-columns: repeat(6, minmax(0, 1fr));
  }
}
</style>
<!-- #endif -->
