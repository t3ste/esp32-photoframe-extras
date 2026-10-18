<script setup>
const props = defineProps({
  // Either icon name as content or as icon prop
  icon: {
    type: String,
    default: null,
  },
  // Other v-icon props we want to support
  start: {
    type: Boolean,
    default: false,
  },
  end: {
    type: Boolean,
    default: false,
  },
  left: {
    type: Boolean,
    default: false,
  },
  right: {
    type: Boolean,
    default: false,
  },
  size: {
    type: [String, Number],
    default: "24px",
  },
  color: {
    type: String,
    default: "currentColor",
  },
  // Additional props for compatibility
  disabled: {
    type: Boolean,
    default: false,
  },
});

// Get icon name from either prop or content (slots would be handled differently)
// For simplicity, we'll assume icon prop is used, or we can extract from slots in a more complex implementation
const iconName = props.icon;

// Fallback: if no icon prop, we should handle slots properly
// For this implementation, we'll require the icon prop to be explicit
if (!iconName) {
  // In a real implementation, we'd access the default slot
  // For now, we'll console error and use a visible error icon
  console.error('[Vue Warn] v-icon: Missing required prop "icon"');
  iconName = "mdi-alert-outline"; // Fallback to a visible error indicator
}

// Convert name like "mdi-upload" to "icon-mdi-upload" for the sprite ID
// Handle cases where it might not have mdi- prefix
const normalizedIconName = iconName.startsWith("mdi-") ? iconName : `mdi-${iconName}`;
const iconId = `icon-${normalizedIconName}`;

// Ensure size is in proper format
const normalizedSize = typeof props.size === "number" ? `${props.size}px` : props.size;

// Determine if we should add start/end margins for text alignment
const hasStartOrEnd = props.start || props.end || props.left || props.right;
</script>

<template>
  <!-- Wrapper for start/end positioning -->
  <span
    class="v-icon-wrapper"
    :class="{
      start: hasStartOrEnd && (props.start || props.left),
      end: hasStartOrEnd && (props.end || props.right),
    }"
  >
    <!-- SVG Icon -->
    <svg
      v-if="iconName"
      :class="['v-icon-svg', { [ 'size--' + normalizedSize ]: typeof props.size === 'number' }]"
      :style="{ width: normalizedSize, height: normalizedSize, fill: props.color }"
      aria-hidden="true"
      focusable="false"
      title="Icon: {{ iconName }}"
    >
      <!-- Reference to symbol in sprite -->
      <use :href="`#${iconId}`" />
    </svg>
  </span>
</template>

<style scoped>
.v-icon-wrapper {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  user-select: none;
  /* Vertically align with text - matches typical icon behavior */
  vertical-align: -0.125em;
  line-height: 1;
}

/* Start/end spacing for text alignment */
.v-icon-wrapper.start {
  margin-right: 8px;
}
.v-icon-wrapper.end {
  margin-left: 8px;
}

.v-icon-svg {
  /* Ensure consistent sizing and behavior */
  display: block;
  flex-shrink: 0;
  /* Override any inherited fill/stroke that might interfere */
  fill: var(--v-icon-fill, currentColor);
}

/* Size classes */
.size--20px {
  width: 20px;
  height: 20px;
}
.size--24px {
  width: 24px;
  height: 24px;
}
.size--28px {
  width: 28px;
  height: 28px;
}
.size--36px {
  width: 36px;
  height: 36px;
}
.size--40px {
  width: 40px;
  height: 40px;
}
</style>
