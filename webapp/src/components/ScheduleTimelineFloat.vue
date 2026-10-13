<script setup>
// A floating window with the timeline of the schedules, shown beside the two places where they are
// edited (Settings > Auto Rotate and Settings > Agenda > Schedule; build option schedule-pages).
// It reads the settings as they stand in the editor, saved or not, so every change shows at once.
// The window can be dragged and minimized; its place and view are remembered in this browser.
import { computed, onBeforeUnmount, onMounted, reactive, ref, watch } from "vue";
import { useSettingsStore } from "../stores";
import { colorOf, computeWeek } from "../utils/scheduleTimeline";
import ScheduleTimeline from "./ScheduleTimeline.vue";

const props = defineProps({
  // true while the Settings tab of the page is the one shown
  active: { type: Boolean, default: false },
});

const settingsStore = useSettingsStore();
const PREFS_KEY = "photoframe.scheduleTimeline";
const WIDTH = 560;

function loadPrefs() {
  try {
    return JSON.parse(localStorage.getItem(PREFS_KEY) || "{}") || {};
  } catch {
    return {};
  }
}
const saved = loadPrefs();
const narrow = ref(typeof window !== "undefined" && window.innerWidth < 768);
const prefs = reactive({
  minimized: typeof saved.minimized === "boolean" ? saved.minimized : narrow.value,
  mode: saved.mode === "week" ? "week" : "day",
  x: Number.isFinite(saved.x) ? saved.x : null,
  y: Number.isFinite(saved.y) ? saved.y : null,
});
watch(prefs, () => {
  try {
    localStorage.setItem(PREFS_KEY, JSON.stringify(prefs));
  } catch {
    // localStorage can be unavailable (private mode / disabled); ignore.
  }
});

// the two tabs where schedules are edited
const editingSchedules = computed(() =>
  ["autoRotate", "agenda"].includes(settingsStore.activeSettingsTab)
);
const visible = computed(() => props.active && editingSchedules.value);

// only what the timeline needs, so unrelated settings do not recompute it
const input = computed(() => {
  const s = settingsStore.deviceSettings;
  return {
    autoRotate: s.autoRotate,
    rotateCron: s.rotateCron,
    agendaCron: s.agendaCron,
    agendaCronPages: s.agendaCronPages,
    agendaCronHold: s.agendaCronHold,
    agendaGapMin: s.agendaGapMin,
    agendaTodoEnabled: s.agendaTodoEnabled,
    agendaCalEnabled: s.agendaCalEnabled,
    infoScreens: s.infoScreens,
  };
});
const week = computed(() => computeWeek(input.value));

// today and now, in the browser's clock (the frame's clock may differ by its time zone)
const todayIndex = ref((new Date().getDay() + 6) % 7);
const nowMinute = ref(new Date().getHours() * 60 + new Date().getMinutes());
const day = ref(todayIndex.value);
let clock = null;
onMounted(() => {
  clock = setInterval(() => {
    const d = new Date();
    todayIndex.value = (d.getDay() + 6) % 7;
    nowMinute.value = d.getHours() * 60 + d.getMinutes();
  }, 30000);
  window.addEventListener("resize", onResize);
});
onBeforeUnmount(() => {
  clearInterval(clock);
  window.removeEventListener("resize", onResize);
});

const DAY_BUTTONS = ["Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"];
const notes = computed(() => {
  const w = week.value;
  const out = [];
  if (!w.entities.length) out.push("No schedule is in force: the frame draws nothing on its own.");
  if (w.agendaActive && !w.inUse) {
    out.push(
      "No schedule has a page of its own, so there is no minimum time: only the same minute is resolved."
    );
  }
  return out;
});

// --- position: bottom right by default, free after a drag; a bottom sheet on a narrow screen
const panel = ref(null);
const panelEl = () => panel.value?.$el || panel.value;
function clamp() {
  const el = panelEl();
  if (prefs.x === null || !el) return;
  const w = el.offsetWidth;
  const h = el.offsetHeight;
  prefs.x = Math.min(Math.max(0, prefs.x), Math.max(0, window.innerWidth - w));
  prefs.y = Math.min(Math.max(0, prefs.y), Math.max(0, window.innerHeight - h));
}
function onResize() {
  narrow.value = window.innerWidth < 768;
  clamp();
}
const style = computed(() => {
  if (narrow.value) return {};
  if (prefs.x === null) return { right: "16px", bottom: "16px", width: `${WIDTH}px` };
  return { left: `${prefs.x}px`, top: `${prefs.y}px`, width: `${WIDTH}px` };
});
let drag = null;
function startDrag(event) {
  if (narrow.value || event.target.closest("button")) return;
  const rect = panelEl().getBoundingClientRect();
  drag = { dx: event.clientX - rect.left, dy: event.clientY - rect.top };
  event.currentTarget.setPointerCapture(event.pointerId);
}
function moveDrag(event) {
  if (!drag) return;
  prefs.x = event.clientX - drag.dx;
  prefs.y = event.clientY - drag.dy;
  clamp();
}
function endDrag() {
  drag = null;
}
function resetPlace() {
  prefs.x = null;
  prefs.y = null;
}

const holdText = (e) => (e.hold ? `${e.hold} min` : "");
const legend = computed(() => week.value.entities);
</script>

<template>
  <div v-if="visible" class="timeline-float-root">
    <v-btn
      v-if="prefs.minimized"
      class="timeline-pill"
      color="primary"
      size="small"
      prepend-icon="mdi-clock-outline"
      @click="prefs.minimized = false"
    >
      Timeline
    </v-btn>
    <v-card v-else ref="panel" class="timeline-float" :style="style" elevation="10">
      <div
        class="timeline-head"
        @pointerdown="startDrag"
        @pointermove="moveDrag"
        @pointerup="endDrag"
        @pointercancel="endDrag"
        @dblclick="resetPlace"
      >
        <span class="text-subtitle-2">Schedule timeline</span>
        <span class="text-caption text-medium-emphasis ml-2">live, as edited</span>
        <v-spacer />
        <v-btn-toggle
          v-model="prefs.mode"
          mandatory
          density="compact"
          variant="outlined"
          divided
          class="mr-2"
        >
          <v-btn value="day" size="x-small">Day</v-btn>
          <v-btn value="week" size="x-small">Week</v-btn>
        </v-btn-toggle>
        <v-btn
          icon="mdi-chevron-down"
          size="x-small"
          variant="text"
          title="Minimize"
          @click="prefs.minimized = true"
        />
      </div>
      <div class="timeline-body">
        <div v-if="prefs.mode === 'day'" class="timeline-days">
          <v-btn
            v-for="(label, i) in DAY_BUTTONS"
            :key="label"
            size="x-small"
            :variant="day === i ? 'flat' : 'text'"
            :color="day === i ? 'primary' : undefined"
            :class="{ 'timeline-today': i === todayIndex }"
            @click="day = i"
          >
            {{ label }}
          </v-btn>
        </div>
        <ScheduleTimeline
          :week="week"
          :mode="prefs.mode"
          :day="day"
          :today="todayIndex"
          :now-minute="nowMinute"
        />
        <div class="timeline-legend">
          <span v-for="e in legend" :key="e.id" class="timeline-key">
            <span class="timeline-swatch" :style="{ background: colorOf(e.id) }" />
            <strong>{{ e.id }}</strong> {{ e.title
            }}<span v-if="holdText(e)" class="text-medium-emphasis"> · {{ holdText(e) }}</span>
          </span>
        </div>
        <div v-for="note in notes" :key="note" class="text-caption text-medium-emphasis mt-1">
          {{ note }}
        </div>
        <div class="text-caption text-medium-emphasis mt-1">
          Strong part of a bar: the display is kept (hold or minimum time); light part: it stays
          until the next draw. Hollow circle: the schedule fires but is held off.
        </div>
      </div>
    </v-card>
  </div>
</template>

<style scoped>
.timeline-float,
.timeline-pill {
  position: fixed;
  z-index: 1100;
}
.timeline-pill {
  right: 16px;
  bottom: 16px;
}
.timeline-float {
  max-width: calc(100vw - 16px);
  border: 1px solid rgba(var(--v-border-color), var(--v-border-opacity));
}
.timeline-head {
  display: flex;
  align-items: center;
  padding: 4px 6px 4px 12px;
  cursor: move;
  user-select: none;
  touch-action: none;
  border-bottom: 1px solid rgba(var(--v-border-color), var(--v-border-opacity));
}
.timeline-body {
  padding: 8px 10px 10px;
  max-height: min(68vh, 560px);
  overflow: auto;
}
.timeline-days {
  display: flex;
  gap: 2px;
  margin-bottom: 6px;
}
.timeline-today {
  text-decoration: underline;
}
.timeline-legend {
  display: flex;
  flex-wrap: wrap;
  gap: 2px 12px;
  margin-top: 6px;
  font-size: 12px;
}
.timeline-swatch {
  display: inline-block;
  width: 10px;
  height: 10px;
  margin-right: 4px;
  border-radius: 2px;
  background: #8a8a8a;
}
@media (max-width: 767px) {
  .timeline-float {
    left: 8px;
    right: 8px;
    bottom: 8px;
    width: auto;
  }
  .timeline-pill {
    right: 8px;
    bottom: 8px;
  }
  .timeline-head {
    cursor: default;
  }
}
</style>
