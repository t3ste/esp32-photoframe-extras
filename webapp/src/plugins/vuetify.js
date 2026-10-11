import "vuetify/styles";
import { createVuetify } from "vuetify";

export default createVuetify({
  theme: {
    defaultTheme: "light",
    themes: {
      light: {
        colors: {
          primary: "#ce9160",
          secondary: "#424242",
          accent: "#82B1FF",
          error: "#982f2f",
          info: "#2f6398",
          success: "#2f9852",
          warning: "#987e2f",
        },
      },
    },
  },
  defaults: {
    VBtn: {
      variant: "flat",
    },
    VCard: {
      elevation: 2,
    },
    // #if FORK_FIXES
    // Vuetify 3.11 writes `opacity: parseFloat(props.bgOpacity)` into the inline style of every progress
    // bar, which is NaN while the prop is not set: Firefox logs "Error in parsing value for 'opacity'.
    // Declaration dropped." for each. The value the stylesheet gives the bar anyway (--v-border-opacity,
    // 0.12 in both themes) is passed instead, so the bars look the same.
    VProgressLinear: {
      bgOpacity: 0.12,
      bufferOpacity: 0.12,
    },
    // #endif
  },
});
