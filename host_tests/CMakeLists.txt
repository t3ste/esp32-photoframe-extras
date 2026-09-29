cmake_minimum_required(VERSION 3.14)
project(esp32_photoframe_tests)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# There is no board on the host: skip the hardware checks of main/feature_config.h.
# (stubs/sdkconfig.h is empty, so every optional feature is off unless a target
# defines its CONFIG_* symbols - see FORK_FEATURE_DEFS below.)
add_compile_definitions(FEATURE_CONFIG_NO_HW_CHECK)

# Every optional feature switched on, as `build.py --all-features` builds it. The fork
# tests below compile the feature modules and the shared image pipeline with it; the
# plain tests above stay on the upstream code.
set(FORK_FEATURE_DEFS
  CONFIG_FEATURE_TELEGRAM
  CONFIG_FEATURE_OVERLAYS
  CONFIG_FEATURE_AGENDA
  CONFIG_FEATURE_CHIMES
  CONFIG_FEATURE_CLIMATE
  CONFIG_FEATURE_ALARMCLOCK
  CONFIG_FEATURE_VOICE_STOP
  CONFIG_FEATURE_BATTERY_HISTORY
  CONFIG_FEATURE_DISPLAY_HISTORY
  CONFIG_FEATURE_HTTPS
  CONFIG_FEATURE_OFFLINE_HOTSPOT
  CONFIG_FEATURE_ERROR_BANNER
  CONFIG_FEATURE_OTA_CHANNEL
  CONFIG_FEATURE_WIFI_RESILIENCE
  CONFIG_FEATURE_WEBCAL
  CONFIG_FEATURE_MULTI_UPLOAD
  CONFIG_FEATURE_FACECROP
  CONFIG_FORK_FIXES
  CONFIG_FORK_HTTP_FETCH
  CONFIG_FORK_WEATHER
  CONFIG_FORK_CLIMATE_CORE
  CONFIG_FORK_EXIF
  CONFIG_FORK_AUDIO_HAL
  CONFIG_FORK_IMAGE_PIPELINE
)

# Cron engine tests
add_executable(
  cron_test
  test_cron.cpp
  ../main/cron.c
)

target_link_libraries(
  cron_test
  GTest::gtest_main
)

target_include_directories(
  cron_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Wake-schedule simulation (early-wake re-sleep loop vs. the real cron engine)
add_executable(
  wake_schedule_test
  test_wake_schedule.cpp
  ../main/cron.c
)

target_link_libraries(
  wake_schedule_test
  GTest::gtest_main
)

target_include_directories(
  wake_schedule_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Backoff between failed unattended network wakes (#121), against the real cron engine
add_executable(
  network_backoff_test
  test_network_backoff.cpp
  ../main/network_backoff.c
  ../main/cron.c
)

target_link_libraries(
  network_backoff_test
  GTest::gtest_main
)

target_include_directories(
  network_backoff_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# HTTP Basic credential checking for the optional device API password (#130)
add_executable(
  http_auth_test
  test_http_auth.cpp
  ../main/http_auth.c
)

target_link_libraries(
  http_auth_test
  GTest::gtest_main
)

target_include_directories(
  http_auth_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Microphone level maths (RMS/peak in dBFS, text bar) behind the console level test
# (voice-stop feature)
add_executable(
  mic_level_test
  test_mic_level.cpp
  test_mic_detect.cpp
  ../main/mic_level.c
  ../main/mic_detect.c
)

target_compile_definitions(
  mic_level_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
)

target_link_libraries(
  mic_level_test
  GTest::gtest_main
)

target_include_directories(
  mic_level_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Ring pattern of the alarm (notes / pauses / when the microphone must ignore the room)
# (alarmclock feature)
add_executable(
  alarm_pattern_test
  test_alarm_pattern.cpp
  ../main/alarm_pattern.c
)

target_compile_definitions(
  alarm_pattern_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
)

target_link_libraries(
  alarm_pattern_test
  GTest::gtest_main
)

target_include_directories(
  alarm_pattern_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
  ${CMAKE_CURRENT_SOURCE_DIR}/../components/board_hal/include
)

# Thinning a long history log out to a chartable number of points (climate feature)
add_executable(
  history_decimate_test
  test_history_decimate.cpp
)

target_link_libraries(
  history_decimate_test
  GTest::gtest_main
)

target_include_directories(
  history_decimate_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Stop-word (keyword) recognition: MFCC + DTW template matching, tested on
# text-to-speech samples (host_tests/data/kws) (voice-stop feature)
add_executable(
  kws_test
  test_kws.cpp
  ../main/kws.c
  ../main/alarm_pattern.c
)

target_compile_definitions(
  kws_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
  KWS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/kws"
)

target_link_libraries(
  kws_test
  GTest::gtest_main
)

target_include_directories(
  kws_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Headline/RSS extraction (pure parsing logic split out of headlines.c's
# network-calling headlines_fetch(), specifically so it's host-testable
# without a real HTTP stack) (overlays feature)
add_executable(
  headlines_test
  test_headlines.cpp
  ../main/headlines.c
  stubs/fake_http_fetch.c
  stubs/fake_image_processor_sanitize.c
)

target_compile_definitions(
  headlines_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
)

target_link_libraries(
  headlines_test
  GTest::gtest_main
)

target_include_directories(
  headlines_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# ToDo (todo.txt) parsing (pure parsing logic split out of todo.c's
# network-calling todo_fetch(), specifically so it's host-testable without
# a real HTTP stack) - same shape as headlines_test above (agenda feature)
add_executable(
  todo_test
  test_todo.cpp
  ../main/todo.c
  stubs/fake_http_fetch.c
  stubs/fake_image_processor_sanitize.c
)

target_compile_definitions(
  todo_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
)

target_link_libraries(
  todo_test
  GTest::gtest_main
)

target_include_directories(
  todo_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Calendar (ICS/iCalendar) parsing (pure parsing logic split out of
# calendar_ics.c's network-calling calendar_ics_fetch(), specifically so
# it's host-testable without a real HTTP stack) - same shape as
# headlines_test/todo_test above (agenda feature)
add_executable(
  calendar_ics_test
  test_calendar_ics.cpp
  ../main/calendar_ics.c
  stubs/fake_http_fetch.c
  stubs/fake_image_processor_sanitize.c
)

target_compile_definitions(
  calendar_ics_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
)

target_link_libraries(
  calendar_ics_test
  GTest::gtest_main
)

target_include_directories(
  calendar_ics_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Demo package (examples/waveshare_photopainter_73/, docs/DEMO_PLAN.md): the
# example calendars and todo.txt run through the firmware's own parsers, not
# a re-implementation - links calendar_ics.c and todo.c directly, same stubs
# as calendar_ics_test/todo_test above.
add_executable(
  example_calendars_test
  test_example_calendars.cpp
  ../main/calendar_ics.c
  ../main/todo.c
  stubs/fake_http_fetch.c
  stubs/fake_image_processor_sanitize.c
)

target_compile_definitions(
  example_calendars_test
  PRIVATE
  ${FORK_FEATURE_DEFS}
  EXAMPLE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../examples/waveshare_photopainter_73"
)

target_link_libraries(
  example_calendars_test
  GTest::gtest_main
)

target_include_directories(
  example_calendars_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

# Discover tests
include(GoogleTest)
gtest_discover_tests(cron_test)
gtest_discover_tests(wake_schedule_test)
gtest_discover_tests(http_auth_test)
gtest_discover_tests(network_backoff_test)
gtest_discover_tests(mic_level_test)
gtest_discover_tests(alarm_pattern_test)
gtest_discover_tests(history_decimate_test)
gtest_discover_tests(kws_test)
gtest_discover_tests(headlines_test)
gtest_discover_tests(todo_test)
gtest_discover_tests(calendar_ics_test)
gtest_discover_tests(example_calendars_test)

# On-device image pipeline characterization tests (host build of
# main/image_processor.c against ESP-IDF stubs + system libpng)
find_package(PNG REQUIRED)

add_executable(
  image_pipeline_test
  test_image_pipeline.cpp
  ../main/image_processor.c
  stubs/esp_stubs.c
  stubs/fake_display_manager.c
  stubs/fake_config_manager.c
  stubs/fake_processing_settings.c
)

target_include_directories(
  image_pipeline_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

target_link_libraries(
  image_pipeline_test
  GTest::gtest_main
  PNG::PNG
  m
)

gtest_discover_tests(image_pipeline_test)

# .current.* / thumbnail lifecycle tests for the shared display flow.
# FS_MOUNT_POINT is redirected to a local directory so the tests exercise
# the real path scheme against the host filesystem.
add_executable(
  display_flow_test
  test_display_flow.cpp
  ../main/display_flow.c
  ../main/image_processor.c
  stubs/esp_stubs.c
  stubs/fake_display_manager.c
  stubs/fake_config_manager.c
  stubs/fake_processing_settings.c
  stubs/fake_storage.c
)

target_compile_definitions(
  display_flow_test
  PRIVATE
  FS_MOUNT_POINT="pf_storage"
)

target_include_directories(
  display_flow_test
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../main
)

target_link_libraries(
  display_flow_test
  GTest::gtest_main
  PNG::PNG
  m
)

gtest_discover_tests(display_flow_test)

# The same two characterization tests once more, on the optional image pipeline
# (CONFIG_FORK_IMAGE_PIPELINE: caption/overlay drawing, streaming JPEG decode, ...).
# The plain variants above prove the upstream behaviour is untouched.
foreach(test_name image_pipeline display_flow)
  set(fork_target ${test_name}_fork_test)
  set(fork_sources
    test_${test_name}.cpp
    ../main/image_processor.c
    stubs/esp_stubs.c
    stubs/fake_display_manager.c
    stubs/fake_config_manager.c
    stubs/fake_processing_settings.c
    stubs/fork_stubs.c
    ../components/epaper_src/Fonts/font24.c
  )
  if(test_name STREQUAL display_flow)
    list(APPEND fork_sources ../main/display_flow.c stubs/fake_storage.c)
  endif()

  add_executable(${fork_target} ${fork_sources})

  target_compile_definitions(
    ${fork_target}
    PRIVATE
    ${FORK_FEATURE_DEFS}
  )
  if(test_name STREQUAL display_flow)
    target_compile_definitions(${fork_target} PRIVATE FS_MOUNT_POINT="pf_storage_fork")
  endif()

  target_include_directories(
    ${fork_target}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../main
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/epaper_src
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/epaper_src/Fonts
  )

  target_link_libraries(
    ${fork_target}
    GTest::gtest_main
    PNG::PNG
    m
  )

  gtest_discover_tests(${fork_target} TEST_SUFFIX " (fork)")
endforeach()
