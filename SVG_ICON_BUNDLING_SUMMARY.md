# SVG Icon Bundling Implementation

## Overview
This implementation replaces the font-based Material Design Icons (MDI) approach with SVG icon bundling to improve performance, styling flexibility, and offline capabilities.

## Changes Made

### 1. Created SVG Sprite (`webapp/src/icons/sprite.svg`)
- Contains SVG symbols for commonly used icons:
  - mdi-upload
  - mdi-rotate-right  
  - mdi-cursor-move
  - mdi-sleep
  - mdi-battery
  - mdi-delete
  - mdi-check-circle
  - mdi-alert
  - mdi-cog
  - mdi-wifi
- Each icon is defined as a `<symbol>` with unique ID for `<use>` referencing
- External sprite enables browser caching

### 2. Created Icon Component (`webapp/src/components/Icon.vue`)
- Drop-in replacement for Vuetify's `v-icon` component
- Supports core props: `icon`, `size`, `color`, `start`, `end`, `left`, `right`
- Uses external SVG sprite via `<use>` references
- Includes proper accessibility attributes (aria-hidden, focusable, title)
- Handles missing icons gracefully with error logging and fallback
- Size classes for common dimensions (20px, 24px, 28px, 36px, 40px)
- Start/end spacing for proper text alignment

### 3. Updated Main Entry (`webapp/src/main.js`)
- Added import: `import Icon from "./components/Icon.vue";`
- Registered component globally: `app.component('v-icon', Icon);`
- Preserved existing FORK_FIXES font imports for backward compatibility

## Benefits
- **Performance**: Single sprite request vs multiple font requests
- **Styling**: Full CSS control over icon properties (fill, stroke, animations)
- **Flexibility**: Easy to add/remove icons from sprite
- **Quality**: Crisp rendering at any resolution and pixel density
- **Offline**: No external dependencies after initial load
- **Bundle Size**: Potential reduction compared to loading full icon font

## Extension Instructions
To add more icons to the sprite:
1. Obtain SVG path data for desired MDI icon (from @mdi/svg package)
2. Add `<symbol>` element to `sprite.svg` with:
   - Unique ID: `icon-mdi-{icon-name}`
   - Proper viewBox (typically "0 0 24 24")
   - SVG path data
3. The Icon component will automatically support the new icon

## Integration Notes
- Existing `v-icon` usage requires no changes (backward compatible)
- Component preserves v-icon API for props like start/end/left/right
- Fallback error handling prevents silent failures
- Works with Vuetify 3.x and Vue 3.x