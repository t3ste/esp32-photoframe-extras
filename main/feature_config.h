#pragma once

// Compile-time view of the optional features declared in main/Kconfig.
//
// Every FEATURE_x is always defined, as 0 or 1, so code uses `#if FEATURE_x`
// (a misspelt name then fails to match instead of silently compiling out).
// With every feature off the firmware is the upstream firmware: a disabled
// feature contributes no source file, no config key, no HTTP handler and no web
// UI. See docs/MAINTAINING.md section 5.

#include "sdkconfig.h"

#ifdef CONFIG_FEATURE_TELEGRAM
#define FEATURE_TELEGRAM 1
#else
#define FEATURE_TELEGRAM 0
#endif

#ifdef CONFIG_FEATURE_OVERLAYS
#define FEATURE_OVERLAYS 1
#else
#define FEATURE_OVERLAYS 0
#endif

#ifdef CONFIG_FEATURE_AGENDA
#define FEATURE_AGENDA 1
#else
#define FEATURE_AGENDA 0
#endif

#ifdef CONFIG_FEATURE_CHIMES
#define FEATURE_CHIMES 1
#else
#define FEATURE_CHIMES 0
#endif

#ifdef CONFIG_FEATURE_CLIMATE
#define FEATURE_CLIMATE 1
#else
#define FEATURE_CLIMATE 0
#endif

#ifdef CONFIG_FEATURE_ALARMCLOCK
#define FEATURE_ALARMCLOCK 1
#else
#define FEATURE_ALARMCLOCK 0
#endif

#ifdef CONFIG_FEATURE_VOICE_STOP
#define FEATURE_VOICE_STOP 1
#else
#define FEATURE_VOICE_STOP 0
#endif

#ifdef CONFIG_FEATURE_BATTERY_HISTORY
#define FEATURE_BATTERY_HISTORY 1
#else
#define FEATURE_BATTERY_HISTORY 0
#endif

#ifdef CONFIG_FEATURE_DISPLAY_HISTORY
#define FEATURE_DISPLAY_HISTORY 1
#else
#define FEATURE_DISPLAY_HISTORY 0
#endif

#ifdef CONFIG_FEATURE_HTTPS
#define FEATURE_HTTPS 1
#else
#define FEATURE_HTTPS 0
#endif

#ifdef CONFIG_FEATURE_OFFLINE_HOTSPOT
#define FEATURE_OFFLINE_HOTSPOT 1
#else
#define FEATURE_OFFLINE_HOTSPOT 0
#endif

#ifdef CONFIG_FEATURE_ERROR_BANNER
#define FEATURE_ERROR_BANNER 1
#else
#define FEATURE_ERROR_BANNER 0
#endif

#ifdef CONFIG_FEATURE_OTA_CHANNEL
#define FEATURE_OTA_CHANNEL 1
#else
#define FEATURE_OTA_CHANNEL 0
#endif

#ifdef CONFIG_FEATURE_WIFI_RESILIENCE
#define FEATURE_WIFI_RESILIENCE 1
#else
#define FEATURE_WIFI_RESILIENCE 0
#endif

#ifdef CONFIG_FEATURE_FACECROP
#define FEATURE_FACECROP 1
#else
#define FEATURE_FACECROP 0
#endif

#ifdef CONFIG_FEATURE_WEBCAL
#define FEATURE_WEBCAL 1
#else
#define FEATURE_WEBCAL 0
#endif

#ifdef CONFIG_FORK_FIXES
#define FORK_FIXES 1
#else
#define FORK_FIXES 0
#endif

// Shared building blocks (selected by the features that need them).
#ifdef CONFIG_FORK_HTTP_FETCH
#define FORK_HTTP_FETCH 1
#else
#define FORK_HTTP_FETCH 0
#endif

#ifdef CONFIG_FORK_WEATHER
#define FORK_WEATHER 1
#else
#define FORK_WEATHER 0
#endif

#ifdef CONFIG_FORK_CLIMATE_CORE
#define FORK_CLIMATE_CORE 1
#else
#define FORK_CLIMATE_CORE 0
#endif

#ifdef CONFIG_FORK_EXIF
#define FORK_EXIF 1
#else
#define FORK_EXIF 0
#endif

#ifdef CONFIG_FORK_AUDIO_HAL
#define FORK_AUDIO_HAL 1
#else
#define FORK_AUDIO_HAL 0
#endif

#ifdef CONFIG_FORK_IMAGE_PIPELINE
#define FORK_IMAGE_PIPELINE 1
#else
#define FORK_IMAGE_PIPELINE 0
#endif

// True as soon as any optional feature or the general fixes are built in. Code
// that every feature relies on but that upstream does not need is gated with it:
// the config JSON handler (apply_config_from_json() in utils.c) collects errors
// instead of stopping at the first bad field, and the deep-sleep wake path runs on
// a task with a larger stack.
#define FORK_ANY                                                                                   \
    (FORK_FIXES || FEATURE_TELEGRAM || FEATURE_OVERLAYS || FEATURE_AGENDA || FEATURE_CHIMES ||     \
     FEATURE_CLIMATE || FEATURE_ALARMCLOCK || FEATURE_VOICE_STOP || FEATURE_BATTERY_HISTORY ||     \
     FEATURE_DISPLAY_HISTORY || FEATURE_HTTPS || FEATURE_OFFLINE_HOTSPOT ||                        \
     FEATURE_ERROR_BANNER || FEATURE_OTA_CHANNEL || FEATURE_WIFI_RESILIENCE || FEATURE_FACECROP || \
     FEATURE_WEBCAL)

// Second line of defence behind build.py and Kconfig: the Kconfig capability
// symbols mirror the board headers, and this makes a disagreement a build error.
// Host tests define FEATURE_CONFIG_NO_HW_CHECK because they have no board.
#ifndef FEATURE_CONFIG_NO_HW_CHECK
#include "board_hal.h"

#if (FEATURE_CHIMES || FEATURE_ALARMCLOCK) && !BOARD_HAL_HAS_SPEAKER
#error "Chimes and the alarm clock need a board with a speaker (BOARD_HAL_HAS_SPEAKER)"
#endif

#if FEATURE_VOICE_STOP && !BOARD_HAL_HAS_MICROPHONE
#error "Voice stop needs a board with a microphone (BOARD_HAL_HAS_MICROPHONE)"
#endif

#if FEATURE_VOICE_STOP && !FEATURE_ALARMCLOCK
#error "Voice stop needs the alarm clock (FEATURE_ALARMCLOCK)"
#endif

#if FEATURE_CLIMATE && !BOARD_HAL_HAS_CLIMATE_SENSOR
#error "Climate needs a board with a temperature/humidity sensor driver"
#endif
#endif  // FEATURE_CONFIG_NO_HW_CHECK
