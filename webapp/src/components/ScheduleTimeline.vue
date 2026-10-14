<script setup>
// The timeline of the schedules as an SVG: what is on the display, and for how long, over a day or a
// week (build option schedule-pages). `week` is the result of computeWeek() (utils/scheduleTimeline.js).
import { computed, onBeforeUnmount, onMounted, ref } from "vue";
import { DAY_NAMES, colorOf, stripOf } from "../utils/scheduleTimeline";

const props = defineProps({
  week: { type: Object, required: true },
  // "day": the display strip and one row per schedule with its fires; "week": one strip per day
  mode: { type: String, default: "day" },
  // the day of the day view, 0 = Monday ... 6 = Sunday
  day: { type: Number, default: 0 },
  // the index of today (0 = Monday), highlighted; -1 for none
  today: { type: Number, default: -1 },
  // minute of the day now, for the marker in the day view of today; -1 for none
  nowMinute: { type: Number, default: -1 },
});

const LABEL_W = 44;
const AXIS_H = 16;
const ROW_H = 24;
const STRIP_H = 20;

const root = ref(null);
const width = ref(480);
let observer = null;
onMounted(() => {
  width.value = Math.max(240, root.value?.clientWidth || 480);
  if (typeof ResizeObserver !== "undefined" && root.value) {
    observer = new ResizeObserver((entries) => {
      width.value = Math.max(240, Math.floor(entries[0].contentRect.width));
    });
    observer.observe(root.value);
  }
});
onBeforeUnmount(() => observer?.disconnect());

const chartW = computed(() => width.value - LABEL_W - 8);
const px = (minute) => LABEL_W + (chartW.value * minute) / 1440;
const hhmm = (m) =>
  `${String(Math.floor(m / 60) % 24).padStart(2, "0")}:${String(m % 60).padStart(2, "0")}`;
const entityOf = (id) => props.week.entities.find((e) => e.id === id);
const titleOf = (id) => entityOf(id)?.title || id;
const shorten = (text, room) => (text.length > room ? `${text.slice(0, room - 1)}…` : text);

const hourMarks = computed(() => {
  // every full hour while the two-digit labels fit (about 16 px each), else every second or sixth
  const perHour = chartW.value / 24;
  const step = perHour >= 16 ? 1 : perHour >= 8 ? 2 : 6;
  const marks = [];
  for (let h = 0; h <= 24; h += step) marks.push(h);
  return marks;
});

// strips: [{ y, label, today, segments: [{ id, x, w, holdW, tip, text }] }]
const strips = computed(() => {
  const days = props.mode === "week" ? [0, 1, 2, 3, 4, 5, 6] : [props.day];
  return days.map((d, i) => {
    const dayData = props.week.days[d];
    const segments = stripOf(dayData, props.week.entities).map((s) => {
      const w = px(s.to) - px(s.from);
      const hold = entityOf(s.id)?.hold ?? 0;
      const until = s.to >= 1440 ? "midnight" : hhmm(s.to);
      const tip = s.carried
        ? `${titleOf(s.id)} (from the day before) until ${until}`
        : `${hhmm(s.from)} ${s.id}: ${titleOf(s.id)}, on the display until ${until}` +
          (hold ? `; kept at least ${hold} min` : "");
      const room = Math.floor((w - 6) / 6);
      return {
        id: s.id,
        x: px(s.from),
        w,
        holdW: s.carried ? 0 : Math.max(px(s.from + s.hold) - px(s.from), s.hold ? 2 : 0),
        tip,
        text: room >= 4 ? shorten(titleOf(s.id), room) : "",
      };
    });
    return {
      y: AXIS_H + i * (STRIP_H + 14),
      label: props.mode === "week" ? DAY_NAMES[dayData.dow] : "Display",
      today: props.mode === "week" && d === props.today,
      segments,
    };
  });
});

// rows of the day view: one per schedule with its fires
const rows = computed(() => {
  if (props.mode !== "day") return [];
  const top = AXIS_H + STRIP_H + 22;
  const fires = props.week.days[props.day].fires;
  return props.week.entities.map((e, i) => ({
    id: e.id,
    y: top + i * ROW_H,
    title: e.title,
    dots: fires
      .filter((f) => f.id === e.id)
      .map((f) => ({
        x: px(f.minute),
        drawn: f.drawn,
        tip: `${hhmm(f.minute)} ${e.id} ${f.drawn ? "is drawn" : "fires but is held off"}`,
      })),
  }));
});

const height = computed(() =>
  props.mode === "week"
    ? AXIS_H + 7 * (STRIP_H + 14)
    : AXIS_H + STRIP_H + 22 + props.week.entities.length * ROW_H + 4
);
const showNow = computed(
  () => props.mode === "day" && props.day === props.today && props.nowMinute >= 0
);
</script>

<template>
  <div ref="root" class="schedule-timeline">
    <svg
      :width="width"
      :height="height"
      role="img"
      aria-label="Timeline of the schedules: what is on the display, and for how long"
    >
      <g v-for="h in hourMarks" :key="h">
        <line :x1="px(h * 60)" :x2="px(h * 60)" :y1="AXIS_H - 2" :y2="height" class="tl-grid" />
        <text v-if="h < 24" :x="px(h * 60) + 2" y="10" class="tl-axis">
          {{ String(h).padStart(2, "0") }}
        </text>
      </g>
      <g v-for="strip in strips" :key="strip.y">
        <text :x="2" :y="strip.y + 14" :class="['tl-label', { 'tl-today': strip.today }]">
          {{ strip.label }}
        </text>
        <rect :x="LABEL_W" :y="strip.y" :width="chartW" :height="STRIP_H" class="tl-strip-bg" />
        <g v-for="(s, i) in strip.segments" :key="i">
          <rect
            :x="s.x"
            :y="strip.y"
            :width="s.w"
            :height="STRIP_H"
            :fill="colorOf(s.id)"
            opacity="0.35"
          >
            <title>{{ s.tip }}</title>
          </rect>
          <rect
            v-if="s.holdW"
            :x="s.x"
            :y="strip.y"
            :width="s.holdW"
            :height="STRIP_H"
            :fill="colorOf(s.id)"
          >
            <title>{{ s.tip }}</title>
          </rect>
          <text v-if="s.text" :x="s.x + 3" :y="strip.y + 14" class="tl-seg">{{ s.text }}</text>
        </g>
      </g>
      <g v-for="row in rows" :key="row.id">
        <text :x="2" :y="row.y + 14" class="tl-label" :fill="colorOf(row.id)">{{ row.id }}</text>
        <line
          :x1="LABEL_W"
          :x2="LABEL_W + chartW"
          :y1="row.y + 10"
          :y2="row.y + 10"
          class="tl-grid"
        />
        <circle
          v-for="(dot, i) in row.dots"
          :key="i"
          :cx="dot.x"
          :cy="row.y + 10"
          r="5"
          :fill="dot.drawn ? colorOf(row.id) : 'rgb(var(--v-theme-surface))'"
          :stroke="colorOf(row.id)"
          stroke-width="1.5"
        >
          <title>{{ dot.tip }}</title>
        </circle>
      </g>
      <line
        v-if="showNow"
        :x1="px(nowMinute)"
        :x2="px(nowMinute)"
        :y1="AXIS_H - 2"
        :y2="height"
        class="tl-now"
      >
        <title>now</title>
      </line>
    </svg>
  </div>
</template>

<style scoped>
.schedule-timeline {
  width: 100%;
  color: rgb(var(--v-theme-on-surface));
}
.schedule-timeline svg {
  display: block;
}
.tl-grid {
  stroke: currentColor;
  stroke-opacity: 0.14;
}
.tl-axis,
.tl-label,
.tl-seg {
  fill: currentColor;
  font-size: 11px;
}
.tl-axis {
  opacity: 0.6;
  font-size: 10px;
}
.tl-label {
  font-weight: 600;
}
.tl-today {
  fill: rgb(var(--v-theme-primary));
}
.tl-seg {
  pointer-events: none;
  opacity: 0.85;
}
.tl-strip-bg {
  fill: currentColor;
  opacity: 0.05;
}
.tl-now {
  stroke: rgb(var(--v-theme-error));
  stroke-width: 1.5;
}
</style>
